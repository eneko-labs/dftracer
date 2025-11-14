//
// Created for shared Chrome writer functionality
//

#ifndef DFTRACER_PERFETTO_CHROME_WRITER_BASE_H
#define DFTRACER_PERFETTO_CHROME_WRITER_BASE_H

#include <assert.h>
#include <dftracer/core/common/constants.h>
#include <dftracer/core/common/logging.h>
#include <dftracer/core/common/cpp_typedefs.h>
#include <dftracer/core/common/datastructure.h>
#include <dftracer/core/utils/configuration_manager.h>
#include <dftracer/core/utils/posix_internal.h>
#include <dftracer/core/utils/utils.h>
#include <dftracer/core/writer/writer_base.h>
#include <unistd.h>

#include <any>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace dftracer {
class PerfettoChromeWriterBase : public WriterBase {
 protected:
  static const int MAX_LINE_SIZE = 16 * 1024L;
  std::mutex mtx;
  std::vector<char> buffer;
  size_t current_index;
  size_t write_buffer_size;
  bool is_first_write = true;

  // Pure virtual method for flushing buffer - to be implemented by derived
  // classes
  virtual size_t flush_buffer_to_destination(bool force = false) = 0;

 public:
  PerfettoChromeWriterBase() : current_index(0), write_buffer_size(0), is_first_write(true) {
    DFTRACER_LOG_DEBUG("PerfettoChromeWriterBase.PerfettoChromeWriterBase", "");
    auto conf =
        dftracer::Singleton<dftracer::ConfigurationManager>::get_instance();
    enable_compression = conf->compression;
    enable_core_affinity = conf->core_affinity;
    include_metadata = conf->metadata;
    write_buffer_size = conf->write_buffer_size;
    {
      std::unique_lock lock(mtx);
      buffer = std::vector<char>(write_buffer_size + MAX_LINE_SIZE);
      current_index = 0;
    }
  }

  virtual ~PerfettoChromeWriterBase() {
    DFTRACER_LOG_DEBUG("Destructing PerfettoChromeWriterBase", "");
  }

  void log(int index, ConstEventNameType event_name,
           ConstEventNameType category, TimeResolution start_time,
           TimeResolution duration, Metadata *metadata, ProcessID process_id,
           ThreadID tid) override;

  void log_metadata(int index, ConstEventNameType name,
                    ConstEventNameType value, ConstEventNameType ph,
                    ProcessID process_id, ThreadID tid,
                    bool is_string = true) override;

  void set_write_buffer_size(size_t buffer_size) {
    std::unique_lock lock(mtx);
    write_buffer_size = buffer_size;
    buffer = std::vector<char>(write_buffer_size + MAX_LINE_SIZE);
    current_index = 0;
  }

 protected:
  std::string convert_metadata_to_json_string(Metadata *metadata);
  void write_event_json_to_buffer(int index, ConstEventNameType event_name,
                                  ConstEventNameType category,
                                  TimeResolution start_time,
                                  TimeResolution duration,
                                  Metadata *metadata, ProcessID process_id,
                                  ThreadID thread_id);
  void write_metadata_json_to_buffer(int index, ConstEventNameType name,
                                     ConstEventNameType value,
                                     ConstEventNameType ph,
                                     ProcessID process_id, ThreadID thread_id,
                                     bool is_string);
};
}  // namespace dftracer

#endif  // DFTRACER_PERFETTO_CHROME_WRITER_BASE_H
