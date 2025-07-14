//
// Created for shared Chrome writer functionality
//

#include <dftracer/core/logging.h>
#include <dftracer/writer/perfetto_chrome_writer_base.h>
#include <fcntl.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <thread>

namespace dftracer {

void PerfettoChromeWriterBase::log(int index, ConstEventNameType event_name,
                                   ConstEventNameType category,
                                   TimeResolution start_time,
                                   TimeResolution duration,
                                   MetadataMap *metadata, ProcessID process_id,
                                   ThreadID thread_id) {
  DFTRACER_LOG_DEBUG("PerfettoChromeWriterBase.log", "");

  write_event_json_to_buffer(index, event_name, category, start_time, duration,
                             metadata, process_id, thread_id);
  flush_buffer_to_destination(false);
  is_first_write = false;
}

void PerfettoChromeWriterBase::log_metadata(int index, ConstEventNameType name,
                                            ConstEventNameType value,
                                            ConstEventNameType ph,
                                            ProcessID process_id, ThreadID tid,
                                            bool is_string) {
  DFTRACER_LOG_DEBUG("PerfettoChromeWriterBase.log_metadata", "");

  write_metadata_json_to_buffer(index, name, value, ph, process_id, tid,
                                is_string);
  flush_buffer_to_destination(false);
  is_first_write = false;
}

void PerfettoChromeWriterBase::write_event_json_to_buffer(
    int index, ConstEventNameType event_name, ConstEventNameType category,
    TimeResolution start_time, TimeResolution duration, MetadataMap *metadata,
    ProcessID process_id, ThreadID thread_id) {
  size_t previous_index = 0;
  (void)previous_index;

  char is_first_char[3] = "";
  if (!is_first_write) {
    strcpy(is_first_char, ",");
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

  DFTRACER_LOG_DEBUG(
      "PerfettoChromeWriterBase.write_event_json_to_buffer %s on %s",
      buffer.data() + previous_index, this->filename.c_str());
}

void PerfettoChromeWriterBase::write_metadata_json_to_buffer(
    int index, ConstEventNameType name, ConstEventNameType value,
    ConstEventNameType ph, ProcessID process_id, ThreadID thread_id,
    bool is_string) {
  size_t previous_index = 0;
  (void)previous_index;

  char is_first_char[3] = "";
  if (!is_first_write) {
    strcpy(is_first_char, ",");
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

  DFTRACER_LOG_DEBUG(
      "PerfettoChromeWriterBase.write_metadata_json_to_buffer %s on %s",
      buffer.data() + previous_index, this->filename.c_str());
}

std::string PerfettoChromeWriterBase::convert_metadata_to_json_string(
    MetadataMap *metadata) {
  std::stringstream metadata_stream;
  if (metadata != nullptr && !metadata->empty()) {
    metadata_stream << R"(,"args":{"hhash":")" << this->hostname_hash << "\"";
    for (const auto &item : *metadata) {
      metadata_stream << ",";
      metadata_stream << "\"" << item.first << "\":";
      if (item.second.type() == typeid(unsigned int)) {
        metadata_stream << std::any_cast<unsigned int>(item.second);
      } else if (item.second.type() == typeid(int)) {
        metadata_stream << std::any_cast<int>(item.second);
      } else if (item.second.type() == typeid(const char *)) {
        metadata_stream << "\"" << std::any_cast<const char *>(item.second)
                        << "\"";
      } else if (item.second.type() == typeid(std::string)) {
        metadata_stream << "\"" << std::any_cast<std::string>(item.second)
                        << "\"";
      } else if (item.second.type() == typeid(size_t)) {
        metadata_stream << std::any_cast<size_t>(item.second);
      } else if (item.second.type() == typeid(uint16_t)) {
        metadata_stream << std::any_cast<uint16_t>(item.second);
      } else if (item.second.type() == typeid(HashType)) {
        metadata_stream << "\"" << std::any_cast<HashType>(item.second) << "\"";
      } else if (item.second.type() == typeid(long)) {
        metadata_stream << std::any_cast<long>(item.second);
      } else if (item.second.type() == typeid(ssize_t)) {
        metadata_stream << std::any_cast<ssize_t>(item.second);
      } else if (item.second.type() == typeid(off_t)) {
        metadata_stream << std::any_cast<off_t>(item.second);
      } else if (item.second.type() == typeid(off64_t)) {
        metadata_stream << std::any_cast<off64_t>(item.second);
      } else {
        DFTRACER_LOG_INFO("No conversion for type %s", item.first.c_str());
      }
    }
    metadata_stream << "}";
  }
  return metadata_stream.str();
}

}  // namespace dftracer
