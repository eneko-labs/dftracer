#ifndef DFTRACER_PERFETTO_PROTO_ZMQ_WRITER_H
#define DFTRACER_PERFETTO_PROTO_ZMQ_WRITER_H

#include <assert.h>
#include <dftracer/core/common/constants.h>
#include <dftracer/core/common/cpp_typedefs.h>
#include <dftracer/core/common/datastructure.h>
#include <dftracer/core/utils/configuration_manager.h>
#include <dftracer/core/utils/posix_internal.h>
#include <dftracer/core/utils/utils.h>
#include <dftracer/core/writer/writer_base.h>
#include <perfetto.h>
#include <unistd.h>

#include <any>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <zmq.hpp>

namespace dftracer {
class PerfettoProtoZMQWriter : public WriterBase {
 private:
  std::mutex mtx;
  std::unique_ptr<zmq::context_t> context;
  std::unique_ptr<zmq::socket_t> socket;
  std::thread stream_thread;
  std::atomic<bool> stop_stream;
  std::unique_ptr<perfetto::TracingSession> tracing_session;

  void stream_main(std::unique_ptr<perfetto::TracingSession> session);

 public:
  PerfettoProtoZMQWriter() {
    DFTRACER_LOG_DEBUG("PerfettoProtoZMQWriter.PerfettoProtoZMQWriter", "");
    auto conf =
        dftracer::Singleton<dftracer::ConfigurationManager>::get_instance();
    include_metadata = conf->metadata;
    enable_core_affinity = conf->core_affinity;
  }
  ~PerfettoProtoZMQWriter() {
    DFTRACER_LOG_DEBUG("Destructing PerfettoProtoZMQWriter", "");
  }
  void initialize(char *filename, bool throw_error, HashType hostname_hash);
  void log(int index, ConstEventNameType event_name,
           ConstEventNameType category, TimeResolution start_time,
           TimeResolution duration, Metadata *metadata, ProcessID process_id,
           ThreadID tid);
  void log_metadata(int index, ConstEventNameType name,
                    ConstEventNameType value, ConstEventNameType ph,
                    ProcessID process_id, ThreadID tid, bool is_string = true);
  void finalize(bool has_entry);
};
}  // namespace dftracer

#endif  // DFTRACER_PERFETTO_PROTO_ZMQ_WRITER_H
