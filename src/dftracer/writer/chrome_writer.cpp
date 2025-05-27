//
// Created by haridev on 3/28/23.
//

#include <dftracer/core/logging.h>
#include <dftracer/writer/chrome_writer.h>
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
std::shared_ptr<dftracer::ChromeWriter>
    dftracer::Singleton<dftracer::ChromeWriter>::instance = nullptr;
template <>
bool dftracer::Singleton<dftracer::ChromeWriter>::stop_creating_instances =
    false;

namespace dftracer {
void ChromeWriter::initialize(char *filename, bool throw_error,
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
  DFTRACER_LOG_DEBUG("ChromeWriter.initialize %s", this->filename.c_str());
}

void ChromeWriter::log(int index, ConstEventNameType event_name,
                       ConstEventNameType category, TimeResolution start_time,
                       TimeResolution duration, MetadataMap *metadata,
                       ProcessID process_id, ThreadID thread_id) {
  DFTRACER_LOG_DEBUG("ChromeWriter.log", "");

  if (fh != nullptr) {
    write_event_json_to_buffer(index, event_name, category, start_time,
                               duration, metadata, process_id, thread_id);
    flush_buffer_to_file(false);
  } else {
    DFTRACER_LOG_ERROR("ChromeWriter.log invalid", "");
  }
  is_first_write = false;
}

void ChromeWriter::log_metadata(int index, ConstEventNameType name,
                                ConstEventNameType value, ConstEventNameType ph,
                                ProcessID process_id, ThreadID tid,
                                bool is_string) {
  DFTRACER_LOG_DEBUG("ChromeWriter.log_metadata", "");

  if (fh != nullptr) {
    write_metadata_json_to_buffer(index, name, value, ph, process_id, tid,
                                  is_string);
    flush_buffer_to_file(false);
  } else {
    DFTRACER_LOG_ERROR("ChromeWriter.log_metadata invalid", "");
  }
  is_first_write = false;
}

void ChromeWriter::finalize(bool has_entry) {
  if (this->init) {
    DFTRACER_LOG_DEBUG("ChromeWriter.finalize", "");
    if (fh != nullptr) {
      DFTRACER_LOG_INFO("Profiler finalizing writer %s", filename.c_str());
      flush_buffer_to_file(true);
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
              DFTRACER_LOG_ERROR("Unable to compress file %s",
                                 filename.c_str());
            }
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

void ChromeWriter::write_event_json_to_buffer(
    int index, ConstEventNameType event_name, ConstEventNameType category,
    TimeResolution start_time, TimeResolution duration, MetadataMap *metadata,
    ProcessID process_id, ThreadID thread_id) {
  size_t previous_index = 0;
  (void)previous_index;

  char is_first_char[3] = "  ";
  if (!is_first_write) {
    is_first_char[0] = '\0';
  }

  std::string metadata_json_string;
  if (include_metadata) {
    metadata_json_string = convert_metadata_to_json_string(metadata);
  }

  {
    std::unique_lock lock(mtx);
    previous_index = current_index;
    auto written_size = sprintf(
        buffer.data() + current_index,
        R"(%s{"id":%d,"name":"%s","cat":"%s","pid":%lu,"tid":%lu,"ts":%llu,"dur":%llu,"ph":"X"%s})",
        is_first_char, index, event_name, category, process_id, thread_id,
        start_time, duration, metadata_json_string.c_str());
    current_index += written_size;
    buffer[current_index] = '\n';
    current_index++;
  }

  DFTRACER_LOG_DEBUG("ChromeWriter.write_event_json_to_buffer %s on %s",
                     buffer.data() + previous_index, this->filename.c_str());
}

void ChromeWriter::write_metadata_json_to_buffer(
    int index, ConstEventNameType name, ConstEventNameType value,
    ConstEventNameType ph, ProcessID process_id, ThreadID thread_id,
    bool is_string) {
  size_t previous_index = 0;
  (void)previous_index;

  char is_first_char[3] = "  ";
  if (!is_first_write) {
    is_first_char[0] = '\0';
  }

  {
    std::unique_lock lock(mtx);
    previous_index = current_index;
    auto written_size = 0;
    if (is_string) {
      written_size = sprintf(
          buffer.data() + current_index,
          R"(%s{"id":%d,"name":"%s","cat":"dftracer","pid":%lu,"tid":%lu,"ph":"M","args":{"hhash":"%s","name":"%s","value":"%s"}})",
          is_first_char, index, ph, process_id, thread_id, this->hostname_hash,
          name, value);
    } else {
      written_size = sprintf(
          buffer.data() + current_index,
          R"(%s{"id":%d,"name":"%s","cat":"dftracer","pid":%lu,"tid":%lu,"ph":"M","args":{"hhash":"%s","name":"%s","value":%s}})",
          is_first_char, index, ph, process_id, thread_id, this->hostname_hash,
          name, value);
    }
    current_index += written_size;
    buffer[current_index] = '\n';
    current_index++;
  }

  DFTRACER_LOG_DEBUG("ChromeWriter.write_metadata_json_to_buffer %s on %s",
                     buffer.data() + previous_index, this->filename.c_str());
}

}  // namespace dftracer