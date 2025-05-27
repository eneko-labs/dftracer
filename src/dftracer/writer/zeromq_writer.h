#ifndef DFTRACER_ZEROMQ_WRITER_H
#define DFTRACER_ZEROMQ_WRITER_H

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
#include <zmq.hpp>

namespace dftracer {
class ZeroMQWriter : public WriterBase {
 private:
  std::unique_ptr<zmq::context_t> context;
  std::unique_ptr<zmq::socket_t> socket;

 public:
  ZeroMQWriter() {
    DFTRACER_LOG_DEBUG("ZeroMQWriter.ZeroMQWriter", "");
    auto conf =
        dftracer::Singleton<dftracer::ConfigurationManager>::get_instance();
    include_metadata = conf->metadata;
    enable_core_affinity = conf->core_affinity;
  }
  ~ZeroMQWriter() { DFTRACER_LOG_DEBUG("Destructing ZeroMQWriter", ""); }
  void initialize(char *filename, bool throw_error, HashType hostname_hash);
  void log(int index, ConstEventNameType event_name,
           ConstEventNameType category, TimeResolution start_time,
           TimeResolution duration, MetadataMap *metadata, ProcessID process_id,
           ThreadID tid);
  void log_metadata(int index, ConstEventNameType name,
                    ConstEventNameType value, ConstEventNameType ph,
                    ProcessID process_id, ThreadID tid, bool is_string = true);
  void finalize(bool has_entry);
};
}  // namespace dftracer

#endif  // DFTRACER_ZEROMQ_WRITER_H
