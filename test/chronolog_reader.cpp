/**
 * ChronoLog reader tool for DFTracer.
 *
 * ChronoLog does not use pub/sub; this tool polls the story via Client::ReplayStory()
 * in a loop and writes events to a file (or stdout). The client must be
 * constructed in reader mode (ClientPortalServiceConf + ClientQueryServiceConf).
 * Use for tests and for draining trace data into a .pfw file for DFAnalyzer/Perfetto.
 *
 * Environment variables (portal = writer connection, query = replay service):
 *   DFTRACER_CHRONOLOG_PROTOCOL, DFTRACER_CHRONOLOG_HOST, DFTRACER_CHRONOLOG_PORT
 *   DFTRACER_CHRONOLOG_PROVIDER_ID, DFTRACER_CHRONOLOG_CHRONICLE_NAME,
 *   DFTRACER_CHRONOLOG_STORY_NAME
 *   DFTRACER_CHRONOLOG_QUERY_HOST, DFTRACER_CHRONOLOG_QUERY_PORT (default: same host, port 5557)
 */

#include <chronolog_client.h>

#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

static std::string getenv_default(const char* name, const char* default_val) {
  const char* v = std::getenv(name);
  return v ? std::string(v) : std::string(default_val);
}

static uint16_t getenv_port(const char* name, uint16_t default_val) {
  const char* v = std::getenv(name);
  if (!v) return default_val;
  try {
    int p = std::stoi(v);
    if (p > 0 && p <= 65535) return static_cast<uint16_t>(p);
  } catch (...) {}
  return default_val;
}

static uint16_t getenv_provider_id(const char* name, uint16_t default_val) {
  const char* v = std::getenv(name);
  if (!v) return default_val;
  try {
    int p = std::stoi(v);
    if (p >= 0 && p <= 65535) return static_cast<uint16_t>(p);
  } catch (...) {}
  return default_val;
}

int main(int argc, char* argv[]) {
  bool once = false;
  double poll_interval_sec = 1.0;
  int delay_before_first_replay_sec = 0;
  std::string output_path;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--once") {
      once = true;
    } else if (arg == "--poll-interval" && i + 1 < argc) {
      poll_interval_sec = std::stod(argv[++i]);
    } else if (arg == "--delay" && i + 1 < argc) {
      int d = std::stoi(argv[++i]);
      delay_before_first_replay_sec = (d > 0) ? d : 0;
    } else if (arg == "--output" && i + 1 < argc) {
      output_path = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      std::cerr << "Usage: " << argv[0]
                << " [--once] [--output FILE] [--poll-interval SEC] [--delay SEC]\n"
                << "  --once           Read once and exit (default: loop forever).\n"
                << "  --output FILE    Write events to FILE (default: stdout).\n"
                << "  --poll-interval  Seconds between polls (default: 1.0).\n"
                << "  --delay SEC      Wait SEC seconds before first ReplayStory (default: 0).\n";
      return 0;
    }
  }

  std::string protocol = getenv_default("DFTRACER_CHRONOLOG_PROTOCOL", "ofi+sockets");
  std::string host = getenv_default("DFTRACER_CHRONOLOG_HOST", "127.0.0.1");
  uint16_t port = getenv_port("DFTRACER_CHRONOLOG_PORT", 5555);
  uint16_t provider_id = getenv_provider_id("DFTRACER_CHRONOLOG_PROVIDER_ID", 55);
  std::string chronicle_name = getenv_default("DFTRACER_CHRONOLOG_CHRONICLE_NAME", "dftracer_chronicle");
  std::string story_name = getenv_default("DFTRACER_CHRONOLOG_STORY_NAME", "dftracer_story");
  std::string query_host = getenv_default("DFTRACER_CHRONOLOG_QUERY_HOST", host.c_str());
  // Query service must be on a different port than the portal; same port causes hg_class init failure and crash.
  uint16_t query_port = getenv_port("DFTRACER_CHRONOLOG_QUERY_PORT", 5557);
  uint16_t query_provider_id = getenv_provider_id("DFTRACER_CHRONOLOG_QUERY_PROVIDER_ID", 57);

  chronolog::ClientPortalServiceConf portal_conf;
  portal_conf.PROTO_CONF = protocol;
  portal_conf.IP = host;
  portal_conf.PORT = port;
  portal_conf.PROVIDER_ID = provider_id;

  chronolog::ClientQueryServiceConf query_conf;
  query_conf.PROTO_CONF = protocol;
  query_conf.IP = query_host;
  query_conf.PORT = query_port;
  query_conf.PROVIDER_ID = query_provider_id;

  // Reader mode: two-argument constructor (portal + query) so the client can consume events.
  // Writing-only clients use the single-argument constructor; readers use this.
  chronolog::Client* client = new chronolog::Client(portal_conf, query_conf);
  int ret = client->Connect();
  if (ret != chronolog::CL_SUCCESS) {
    std::cerr << "ChronoLog reader: Connect failed: " << ret << std::endl;
    delete client;
    return 1;
  }

  // Reader must acquire the story before ReplayStory (writer releases the story when done).
  int story_flags = 0;
  std::map<std::string, std::string> story_attrs;
  auto acquire_result = client->AcquireStory(chronicle_name, story_name, story_attrs, story_flags);
  if (acquire_result.first != chronolog::CL_SUCCESS) {
    std::cerr << "ChronoLog reader: AcquireStory failed: " << acquire_result.first
              << " (ensure the writer has released the story)" << std::endl;
    client->Disconnect();
    delete client;
    return 1;
  }

  std::ostream* out = &std::cout;
  std::ofstream out_file;
  if (!output_path.empty()) {
    out_file.open(output_path, std::ios::out | std::ios::trunc);
    if (!out_file) {
      std::cerr << "ChronoLog reader: cannot open output file: " << output_path << std::endl;
      client->Disconnect();
      delete client;
      return 1;
    }
    out = &out_file;
  }

  uint64_t start_ts = 0;
  const uint64_t end_ts_max = UINT64_MAX;  // request up to latest available
  const int max_wait_on_not_acquired_sec = 150;  // keep retrying -5 for up to this long (writer may take ~2 min)
  const int retry_sleep_ms = 2000;

  if (delay_before_first_replay_sec > 0) {
    std::cerr << "ChronoLog reader: waiting " << delay_before_first_replay_sec
              << "s before first ReplayStory..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(delay_before_first_replay_sec));
  }

  do {
    std::vector<chronolog::Event> playback_events;
    int play_ret = client->ReplayStory(chronicle_name, story_name, start_ts, end_ts_max, playback_events);

    if (play_ret != chronolog::CL_SUCCESS) {
      if (play_ret == chronolog::CL_ERR_NOT_ACQUIRED && max_wait_on_not_acquired_sec > 0) {
        auto retry_start = std::chrono::steady_clock::now();
        auto max_wait = std::chrono::seconds(max_wait_on_not_acquired_sec);
        while (play_ret == chronolog::CL_ERR_NOT_ACQUIRED &&
               (std::chrono::steady_clock::now() - retry_start) < max_wait) {
          int elapsed_sec = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now() - retry_start).count());
          std::cerr << "ChronoLog reader: ReplayStory returned -5 (NOT_ACQUIRED), retrying in "
                    << (retry_sleep_ms / 1000) << "s (elapsed " << elapsed_sec << "s / max "
                    << max_wait_on_not_acquired_sec << "s)..." << std::endl;
          std::this_thread::sleep_for(std::chrono::milliseconds(retry_sleep_ms));
          playback_events.clear();
          play_ret = client->ReplayStory(chronicle_name, story_name, start_ts, end_ts_max, playback_events);
        }
      }
      if (play_ret == chronolog::CL_ERR_QUERY_TIMED_OUT && max_wait_on_not_acquired_sec > 0) {
        auto retry_start = std::chrono::steady_clock::now();
        auto max_wait = std::chrono::seconds(max_wait_on_not_acquired_sec);
        while (play_ret == chronolog::CL_ERR_QUERY_TIMED_OUT &&
               (std::chrono::steady_clock::now() - retry_start) < max_wait) {
          int elapsed_sec = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now() - retry_start).count());
          std::cerr << "ChronoLog reader: ReplayStory returned -12 (QUERY_TIMED_OUT), "
                       "data not yet persisted; retrying in " << (retry_sleep_ms / 1000) << "s "
                    << "(elapsed " << elapsed_sec << "s / max " << max_wait_on_not_acquired_sec
                    << "s)..." << std::endl;
          std::this_thread::sleep_for(std::chrono::milliseconds(retry_sleep_ms));
          playback_events.clear();
          play_ret = client->ReplayStory(chronicle_name, story_name, start_ts, end_ts_max, playback_events);
        }
      }
      if (play_ret != chronolog::CL_SUCCESS) {
        std::cerr << "ChronoLog reader: ReplayStory failed: " << play_ret << std::endl;
        if (play_ret == chronolog::CL_ERR_NOT_ACQUIRED) {
          std::cerr << "  (NOT_ACQUIRED) The writer exits without releasing the story to avoid a "
                    << "ChronoLog client bug; the visor may keep the story acquired.\n"
                    << "  Ensure the ChronoLog query service is running on port " << static_cast<int>(query_port)
                    << " (DFTRACER_CHRONOLOG_QUERY_PORT). Replay requires the query service; "
                    << "contact ChronoLog maintainers for deployment that supports replay when the "
                    << "writer did not call ReleaseStory." << std::endl;
        }
        break;
      }
    }

    for (const auto& ev : playback_events) {
      std::string record = ev.log_record();  // copy; don't hold reference across iterations
      if (!record.empty()) {
        *out << record;
        if (record.back() != '\n') *out << '\n';
      }
      uint64_t t = ev.time();
      if (t >= start_ts) start_ts = t + 1;
    }

    if (out == &out_file) out_file.flush();

    if (once) break;

    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<int>(poll_interval_sec * 1000)));
  } while (true);

  if (!output_path.empty()) {
    out_file.close();
  }

  // Release the story before Disconnect to avoid "Resource deadlock avoided" (EDEADLK)
  // during client teardown. Then exit without Disconnect/delete to avoid ChronoLog
  // client heap corruption (same workaround as the writer).
  // Wrap to ensure _exit(0) is always reached even if ReleaseStory throws EDEADLK.
  try {
    client->ReleaseStory(chronicle_name, story_name);
  } catch (const std::exception& e) {
    std::cerr << "ChronoLog reader: ReleaseStory threw: " << e.what()
              << " (ignoring, exiting)" << std::endl;
  } catch (...) {
    std::cerr << "ChronoLog reader: ReleaseStory threw unknown exception (ignoring, exiting)"
              << std::endl;
  }
  _exit(0);
}
