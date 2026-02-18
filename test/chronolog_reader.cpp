/**
 * ChronoLog reader tool for DFTracer.
 *
 * ChronoLog does not use pub/sub; this tool polls the story via playback_story()
 * in a loop and writes events to a file (or stdout). Use for tests and for
 * draining trace data into a .pfw file for DFAnalyzer/Perfetto.
 *
 * Environment variables (same as ChronologWriter):
 *   DFTRACER_CHRONOLOG_PROTOCOL, DFTRACER_CHRONOLOG_HOST, DFTRACER_CHRONOLOG_PORT
 *   DFTRACER_CHRONOLOG_PROVIDER_ID, DFTRACER_CHRONOLOG_CHRONICLE_NAME,
 *   DFTRACER_CHRONOLOG_STORY_NAME
 */

#include <chronolog_client.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
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
  std::string output_path;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--once") {
      once = true;
    } else if (arg == "--poll-interval" && i + 1 < argc) {
      poll_interval_sec = std::stod(argv[++i]);
    } else if (arg == "--output" && i + 1 < argc) {
      output_path = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      std::cerr << "Usage: " << argv[0]
                << " [--once] [--output FILE] [--poll-interval SEC]\n"
                << "  --once           Read once and exit (default: loop forever).\n"
                << "  --output FILE    Write events to FILE (default: stdout).\n"
                << "  --poll-interval  Seconds between polls (default: 1.0).\n";
      return 0;
    }
  }

  std::string protocol = getenv_default("DFTRACER_CHRONOLOG_PROTOCOL", "ofi+sockets");
  std::string host = getenv_default("DFTRACER_CHRONOLOG_HOST", "127.0.0.1");
  uint16_t port = getenv_port("DFTRACER_CHRONOLOG_PORT", 5555);
  uint16_t provider_id = getenv_provider_id("DFTRACER_CHRONOLOG_PROVIDER_ID", 55);
  std::string chronicle_name = getenv_default("DFTRACER_CHRONOLOG_CHRONICLE_NAME", "dftracer_chronicle");
  std::string story_name = getenv_default("DFTRACER_CHRONOLOG_STORY_NAME", "dftracer_story");

  chronolog::ClientPortalServiceConf conf;
  conf.PROTO_CONF = protocol;
  conf.IP = host;
  conf.PORT = port;
  conf.PROVIDER_ID = provider_id;

  chronolog::Client* client = new chronolog::Client(conf);
  int ret = client->Connect();
  if (ret != chronolog::CL_SUCCESS) {
    std::cerr << "ChronoLog reader: Connect failed: " << ret << std::endl;
    delete client;
    return 1;
  }

  std::map<std::string, std::string> story_attrs;
  int story_flags = 0;
  auto story_result = client->AcquireStory(chronicle_name, story_name, story_attrs, story_flags);
  if (story_result.first != chronolog::CL_SUCCESS) {
    std::cerr << "ChronoLog reader: AcquireStory failed: " << story_result.first << std::endl;
    client->Disconnect();
    delete client;
    return 1;
  }
  chronolog::StoryHandle* story_handle = story_result.second;

  std::ostream* out = &std::cout;
  std::ofstream out_file;
  if (!output_path.empty()) {
    out_file.open(output_path, std::ios::out | std::ios::app);
    if (!out_file) {
      std::cerr << "ChronoLog reader: cannot open output file: " << output_path << std::endl;
      client->ReleaseStory(chronicle_name, story_name);
      client->Disconnect();
      delete client;
      return 1;
    }
    out = &out_file;
  }

  uint64_t start_ts = 0;
  const uint64_t end_ts_max = UINT64_MAX;  // request up to latest available
  int total_events = 0;

  do {
    std::vector<chronolog::Event> playback_events;
    int play_ret = story_handle->playback_story(start_ts, end_ts_max, playback_events);

    if (play_ret != chronolog::CL_SUCCESS && play_ret != chronolog::CL_ERR_OUT_OF_RANGE) {
      std::cerr << "ChronoLog reader: playback_story failed: " << play_ret << std::endl;
      break;
    }

    for (const auto& ev : playback_events) {
      const std::string& record = ev.log_record();
      if (!record.empty()) {
        *out << record;
        if (record.back() != '\n') *out << '\n';
        total_events++;
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

  client->ReleaseStory(chronicle_name, story_name);
  client->Disconnect();
  delete client;

  if (once && output_path.empty()) {
    /* When --once and stdout, nothing to report; exit 0. */
  }
  return 0;
}
