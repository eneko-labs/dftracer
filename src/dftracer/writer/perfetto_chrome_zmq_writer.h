#ifndef DFTRACER_PERFETTO_CHROME_ZMQ_WRITER_H
#define DFTRACER_PERFETTO_CHROME_ZMQ_WRITER_H

#include <assert.h>
#include <dftracer/core/constants.h>
#include <dftracer/core/typedef.h>
#include <dftracer/utils/configuration_manager.h>
#include <dftracer/utils/posix_internal.h>
#include <dftracer/utils/utils.h>
#include <dftracer/writer/perfetto_chrome_writer_base.h>
#include <pthread.h>
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
class PerfettoChromeZMQWriter : public PerfettoChromeWriterBase {
 private:
  std::unique_ptr<zmq::context_t> context;
  std::unique_ptr<zmq::socket_t> socket;

 protected:
  size_t flush_buffer_to_destination(bool force = false) override;

 public:
  PerfettoChromeZMQWriter() {
    DFTRACER_LOG_DEBUG("PerfettoChromeZMQWriter.PerfettoChromeZMQWriter", "");
  }
  ~PerfettoChromeZMQWriter() {
    DFTRACER_LOG_DEBUG("Destructing PerfettoChromeZMQWriter", "");
  }
  void initialize(char *filename, bool throw_error,
                  HashType hostname_hash) override;
  void finalize(bool has_entry) override;
  /**
   * @brief Re-initializes the ZMQ socket. This is intended to be called
   * in a child process after a fork().
   */
  void reconnect();
};
}  // namespace dftracer

#endif  // DFTRACER_PERFETTO_CHROME_ZMQ_WRITER_H
