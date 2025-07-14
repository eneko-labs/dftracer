//
// Created by haridev on 3/28/23.
//

#include <dftracer/core/logging.h>
#include <dftracer/writer/perfetto_chrome_file_writer.h>
#include <fcntl.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <thread>

template <>
std::shared_ptr<dftracer::PerfettoChromeFileWriter>
    dftracer::Singleton<dftracer::PerfettoChromeFileWriter>::instance = nullptr;
template <>
bool dftracer::Singleton<
    dftracer::PerfettoChromeFileWriter>::stop_creating_instances = false;

namespace dftracer {

size_t PerfettoChromeFileWriter::flush_buffer_to_destination(bool force) {
  std::unique_lock lock(mtx);
  if (current_index == 0 || (!force && current_index < write_buffer_size))
    return 0;
  DFTRACER_LOG_DEBUG("PerfettoChromeFileWriter.write_buffer_op %s",
                     this->filename.c_str());
  size_t written_elements = 0;
  if (fh != nullptr) {
    flockfile(fh);
    written_elements = fwrite(buffer.data(), current_index, sizeof(char), fh);
    size_t expected_elements = current_index;
    current_index = 0;
    funlockfile(fh);
    if (written_elements != expected_elements) {
      DFTRACER_LOG_ERROR(
          "unable to log write only %ld of %ld trying to write %ld with error "
          "code "
          "%d",
          written_elements, expected_elements, current_index, errno);
    }
  }
  return written_elements;
}

void PerfettoChromeFileWriter::initialize(char *filename, bool throw_error,
                                          HashType hostname_hash) {
  this->hostname_hash = hostname_hash;
  this->throw_error = throw_error;
  this->filename = filename;
  if (fh == nullptr) {
    fh = fopen(filename, "ab+");
    if (fh == nullptr) {
      DFTRACER_LOG_ERROR("unable to create log file %s",
                         filename);  // GCOVR_EXCL_LINE
    } else {
      setvbuf(fh, NULL, _IOLBF, write_buffer_size + 4096);
      // Write opening bracket at the beginning
      fwrite("[\n", sizeof(char), 2, fh);
      DFTRACER_LOG_INFO("created log file %s", filename);
    }
  }
  init = true;
  DFTRACER_LOG_DEBUG("PerfettoChromeFileWriter.initialize %s",
                     this->filename.c_str());
}

void PerfettoChromeFileWriter::finalize(bool has_entry) {
  if (this->init) {
    DFTRACER_LOG_DEBUG("PerfettoChromeFileWriter.finalize", "");
    if (fh != nullptr) {
      DFTRACER_LOG_INFO("Profiler finalizing writer %s", filename.c_str());
      flush_buffer_to_destination(true);
      fflush(fh);
      int status = fclose(fh);
      if (status != 0) {
        DFTRACER_LOG_ERROR("unable to close log file %s for a+",
                           filename.c_str());  // GCOVR_EXCL_LINE
      }
      if (!has_entry) {
        DFTRACER_LOG_INFO("No trace data written deleting file %s",
                          filename.c_str());
        df_unlink(filename.c_str());
      } else {
        DFTRACER_LOG_INFO("Profiler writing the final symbol", "");
        fh = fopen(this->filename.c_str(), "a");
        if (fh != nullptr) {
          std::string data = "]";
          auto written_elements =
              fwrite(data.c_str(), sizeof(char), data.size(), fh);
          if (written_elements != data.size()) {
            DFTRACER_LOG_ERROR(
                "unable to finalize log write %s written only %zu of %zu",
                filename.c_str(), written_elements, data.size());
          }
          status = fclose(fh);
          if (status != 0) {
            DFTRACER_LOG_ERROR("unable to close log file %s for append",
                               filename.c_str());
          }
          fh = nullptr;
        }
      }
      if (enable_compression) {
        if (system("which gzip > /dev/null 2>&1")) {
          DFTRACER_LOG_ERROR("Gzip compression does not exists",
                             "");  // GCOVR_EXCL_LINE
        } else {
          DFTRACER_LOG_INFO("Applying Gzip compression on file %s",
                            filename.c_str());
          char cmd[2048];
          sprintf(cmd, "gzip -f %s", filename.c_str());
          int ret = system(cmd);
          if (ret == 0) {
            DFTRACER_LOG_INFO("Successfully compressed file %s.gz",
                              filename.c_str());
          } else {
            DFTRACER_LOG_ERROR("Unable to compress file %s", filename.c_str());
          }
        }
      }
    }
    if (enable_core_affinity) {
#if DISABLE_HWLOC == 1
      hwloc_topology_destroy(topology);
#endif
    }
    DFTRACER_LOG_DEBUG("Finished writer finalization", "");
  } else {
    DFTRACER_LOG_DEBUG("Already finalized writer", "");
  }
}

}  // namespace dftracer