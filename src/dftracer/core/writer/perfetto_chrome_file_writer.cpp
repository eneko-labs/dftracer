//
// Created by haridev on 3/28/23.
//

#include <dftracer/core/common/logging.h>
#include <dftracer/core/writer/perfetto_chrome_file_writer.h>
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
      DFTRACER_LOG_INFO("created log file %s", filename);
    }
  }
  init = true;
  DFTRACER_LOG_DEBUG("PerfettoChromeFileWriter.initialize %s",
                     this->filename.c_str());
}


void PerfettoChromeFileWriter::log(int index, ConstEventNameType event_name,
                                   ConstEventNameType category,
                                   TimeResolution start_time,
                                   TimeResolution duration, Metadata *metadata,
                                   ProcessID process_id, ThreadID thread_id) {
  DFTRACER_LOG_DEBUG("PerfettoChromeFileWriter.log", "");
  if (fh != nullptr) {
    PerfettoChromeWriterBase::log(index, event_name, category, start_time, duration, metadata, process_id, thread_id);
  } else {
    DFTRACER_LOG_ERROR("PerfettoChromeFileWriter.log invalid", "");
  }
  is_first_write = false;
}


void PerfettoChromeFileWriter::log_metadata(int index, ConstEventNameType name,
                                            ConstEventNameType value,
                                            ConstEventNameType ph,
                                            ProcessID process_id, ThreadID tid,
                                            bool is_string) {
  DFTRACER_LOG_DEBUG("PerfettoChromeFileWriter.log_metadata", "");
  if (fh != nullptr) {
    PerfettoChromeWriterBase::log_metadata(index, name, value, ph, process_id, tid, is_string);
  } else {
    DFTRACER_LOG_ERROR("PerfettoChromeFileWriter.log_metadata invalid", "");
  }
  is_first_write = false;
}

size_t PerfettoChromeFileWriter::flush_buffer_to_destination(bool force) {
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
  if (written_elements != 1) {  // GCOVR_EXCL_START
    DFTRACER_LOG_ERROR(
        "unable to log write only %ld of %d trying to write %ld with error "
        "code "
        "%d",
        written_elements, 1, current_index, errno);
  }  // GCOVR_EXCL_STOP
  return written_elements;
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
        fh = fopen(this->filename.c_str(), "r+");
        if (fh != nullptr) {
          std::string data = "[\n";
          auto written_elements =
              fwrite(data.c_str(), sizeof(char), data.size(), fh);
          if (written_elements != data.size()) {  // GCOVR_EXCL_START
            DFTRACER_LOG_ERROR(
                "unable to finalize log write %s for O_WRONLY written only %ld "
                "of %ld",
                filename.c_str(), data.size(), written_elements);
          }  // GCOVR_EXCL_STOP
          data = "]";
          fseek(fh, 0, SEEK_END);
          written_elements =
              fwrite(data.c_str(), sizeof(char), data.size(), fh);
          if (written_elements != data.size()) {  // GCOVR_EXCL_START
            DFTRACER_LOG_ERROR(
                "unable to finalize log write %s for O_WRONLY written only %ld "
                "of %ld",
                filename.c_str(), data.size(), written_elements);
          }  // GCOVR_EXCL_STOP
          status = fclose(fh);
          if (status != 0) {
            DFTRACER_LOG_ERROR("unable to close log file %s for O_WRONLY",
                               filename.c_str());  // GCOVR_EXCL_LINE
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