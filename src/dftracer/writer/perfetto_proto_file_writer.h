#ifndef DFTRACER_PERFETTO_PROTO_FILE_WRITER_H
#define DFTRACER_PERFETTO_PROTO_FILE_WRITER_H

#include <assert.h>
#include <dftracer/core/constants.h>
#include <dftracer/core/typedef.h>
#include <dftracer/utils/configuration_manager.h>
#include <dftracer/utils/posix_internal.h>
#include <dftracer/utils/utils.h>
#include <dftracer/writer/writer_base.h>
#include <perfetto.h>
#include <unistd.h>

#include <any>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace dftracer {
class PerfettoProtoFileWriter : public WriterBase {
 private:
  std::unique_ptr<perfetto::TracingSession> tracing_session;
  size_t write_buffer_size;

  void stream_main(std::unique_ptr<perfetto::TracingSession> session);

 public:
  PerfettoProtoFileWriter() {
    DFTRACER_LOG_DEBUG("PerfettoProtoFileWriter.PerfettoProtoFileWriter", "");
    auto conf =
        dftracer::Singleton<dftracer::ConfigurationManager>::get_instance();
    enable_compression = conf->compression;
    enable_core_affinity = conf->core_affinity;
    include_metadata = conf->metadata;
    write_buffer_size = conf->write_buffer_size;
  }
  ~PerfettoProtoFileWriter() {
    DFTRACER_LOG_DEBUG("Destructing PerfettoProtoFileWriter", "");
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
  void set_write_buffer_size(size_t buffer_size) {
    write_buffer_size = buffer_size;
  }
};
}  // namespace dftracer

#endif  // DFTRACER_PERFETTO_PROTO_FILE_WRITER_H
