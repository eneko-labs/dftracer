#include <any>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "dftracer/core/typedef.h"
#include "dftracer/writer/perfetto_chrome_file_writer.h"

static dftracer::PerfettoChromeFileWriter writer_instance;

int main(int argc, char** argv) {
  std::string tmp_output_file = "perf_chrome_trace.json";
  int num_events_to_log = 10000;

  if (argc > 1) {
    tmp_output_file = argv[1];
  }
  if (argc > 2) {
    try {
      num_events_to_log = std::stoi(argv[2]);
    } catch (const std::invalid_argument& ia) {
      std::cerr << "Invalid argument for number of events: " << argv[2]
                << std::endl;
      return EXIT_FAILURE;
    } catch (const std::out_of_range& oor) {
      std::cerr << "Number of events out of range: " << argv[2] << std::endl;
      return EXIT_FAILURE;
    }
  }

  writer_instance.initialize(const_cast<char*>(tmp_output_file.c_str()), false,
                             const_cast<char*>("test_host_hash_perf"));

  auto start_time = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < num_events_to_log; ++i) {
    std::unordered_map<std::string, std::any> m;
    m["iteration"] = i;
    m["detail"] = std::string(
        "performance_test_event_detail_long_enough_to_test_string_handling");
    m["another_key"] = std::string("another_value_for_metadata_map_testing");
    writer_instance.log(
        i, ("event_name_perf_" + std::to_string(i)).c_str(), "perf_category",
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count(),
        100 + (i % 75), &m, 12345, 54321);
    if (i % 1000 == 0) {
      writer_instance.log_metadata(i, "metadata_event_name",
                                   "metadata_event_value", "process_name_perf",
                                   12345, 54321);
    }
  }

  writer_instance.finalize(true);

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         end_time - start_time)
                         .count();
  double duration_s = static_cast<double>(duration_ms) / 1000.0;

  std::cout << std::endl;
  std::cout << "[ PERFORMANCE ] PerfettoChromeFileWriter: Logged "
            << num_events_to_log
            << " main events (plus some metadata events) in " << duration_s
            << " seconds." << std::endl;
  if (duration_s > 0) {
    std::cout << "[ PERFORMANCE ] PerfettoChromeFileWriter: Throughput: "
              << static_cast<double>(num_events_to_log) / duration_s
              << " main events/sec." << std::endl;
  } else {
    std::cout
        << "[ PERFORMANCE ] PerfettoChromeFileWriter: Duration too short to "
           "calculate throughput accurately."
        << std::endl;
  }

  std::ifstream file(tmp_output_file);
  if (!file.good()) {
    std::cerr << "Error: Output file was not created: " << tmp_output_file
              << std::endl;
    return EXIT_FAILURE;
  }
  file.seekg(0, std::ios::end);
  if (file.tellg() <= 0) {
    std::cerr << "Error: Output file is empty: " << tmp_output_file
              << std::endl;
    file.close();
    return EXIT_FAILURE;
  }
  file.close();

  return EXIT_SUCCESS;
}