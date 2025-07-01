//
// Created by haridev on 3/28/23.
//

#ifndef DFTRACER_PERFETTO_CHROME_FILE_WRITER_H
#define DFTRACER_PERFETTO_CHROME_FILE_WRITER_H

#include <assert.h>
#include <dftracer/core/constants.h>
#include <dftracer/core/typedef.h>
#include <dftracer/utils/configuration_manager.h>
#include <dftracer/utils/posix_internal.h>
#include <dftracer/utils/utils.h>
#include <dftracer/writer/writer_base.h>
#include <unistd.h>

#include <any>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace dftracer {
class PerfettoChromeFileWriter : public WriterBase {
 protected:
  static const int MAX_LINE_SIZE = 16 * 1024L;
  bool enable_compression;
  bool is_first_write = true;
  FILE *fh;
  size_t current_index = 0;
  size_t write_buffer_size;
  std::vector<char> buffer;
  std::mutex mtx;

  inline size_t flush_buffer_to_file(bool force = false) {
    std::unique_lock lock(mtx);
    if (current_index == 0 || (!force && current_index < write_buffer_size))
      return 0;
    DFTRACER_LOG_DEBUG("PerfettoChromeFileWriter.write_buffer_op %s",
                       this->filename.c_str());
    size_t written_elements = 0;
    flockfile(fh);
    written_elements = fwrite(buffer.data(), current_index, sizeof(char), fh);
    current_index = 0;
    funlockfile(fh);
    if (written_elements != 1) {
      DFTRACER_LOG_ERROR(
          "unable to log write only %ld of %d trying to write %ld with error "
          "code "
          "%d",
          written_elements, 1, current_index, errno);
    }
    return written_elements;
  }

 public:
  PerfettoChromeFileWriter()
      : enable_compression(false),
        is_first_write(false),
        fh(nullptr),
        write_buffer_size(0) {
    DFTRACER_LOG_DEBUG("PerfettoChromeFileWriter.PerfettoChromeFileWriter", "");
    auto conf =
        dftracer::Singleton<dftracer::ConfigurationManager>::get_instance();
    enable_core_affinity = conf->core_affinity;
    enable_compression = conf->compression;
    write_buffer_size = conf->write_buffer_size;
    {
      std::unique_lock lock(mtx);
      buffer = std::vector<char>(write_buffer_size + MAX_LINE_SIZE);
      current_index = 0;
    }
  }
  ~PerfettoChromeFileWriter() {
    DFTRACER_LOG_DEBUG("Destructing PerfettoChromeFileWriter", "");
  }
  void initialize(char *filename, bool throw_error, HashType hostname_hash);
  void log(int index, ConstEventNameType event_name,
           ConstEventNameType category, TimeResolution start_time,
           TimeResolution duration, MetadataMap *metadata, ProcessID process_id,
           ThreadID tid);
  void log_metadata(int index, ConstEventNameType name,
                    ConstEventNameType value, ConstEventNameType ph,
                    ProcessID process_id, ThreadID tid, bool is_string = true);
  void finalize(bool has_entry);

 private:
  std::string convert_metadata_to_json_string(MetadataMap *metadata);
  void write_event_json_to_buffer(int index, ConstEventNameType event_name,
                                  ConstEventNameType category,
                                  TimeResolution start_time,
                                  TimeResolution duration,
                                  MetadataMap *metadata, ProcessID process_id,
                                  ThreadID thread_id);
  void write_metadata_json_to_buffer(int index, ConstEventNameType name,
                                     ConstEventNameType value,
                                     ConstEventNameType ph,
                                     ProcessID process_id, ThreadID thread_id,
                                     bool is_string);
};
}  // namespace dftracer

#endif  // DFTRACER_PERFETTO_CHROME_FILE_WRITER_H
