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
#include <dftracer/writer/perfetto_chrome_writer_base.h>
#include <unistd.h>

#include <any>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace dftracer {
class PerfettoChromeFileWriter : public PerfettoChromeWriterBase {
 private:
  FILE *fh;

 protected:
  size_t flush_buffer_to_destination(bool force = false) override;

 public:
  PerfettoChromeFileWriter() : fh(nullptr) {
    DFTRACER_LOG_DEBUG("PerfettoChromeFileWriter.PerfettoChromeFileWriter", "");
  }
  ~PerfettoChromeFileWriter() {
    DFTRACER_LOG_DEBUG("Destructing PerfettoChromeFileWriter", "");
  }
  void initialize(char *filename, bool throw_error, Hostname hostname) override;
  void finalize(bool has_entry) override;
};
}  // namespace dftracer

#endif  // DFTRACER_PERFETTO_CHROME_FILE_WRITER_H
