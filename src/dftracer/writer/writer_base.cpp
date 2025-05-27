#include <dftracer/core/logging.h>
#include <dftracer/writer/writer_base.h>

#include <memory>
#include <sstream>
#include <typeinfo>

namespace dftracer {

std::string WriterBase::convert_event_to_json_string(
    int index, ConstEventNameType event_name, ConstEventNameType category,
    TimeResolution start_time, TimeResolution duration, MetadataMap *metadata,
    ProcessID process_id, ThreadID thread_id) {
  std::string metadata_json_string;
  if (include_metadata) {
    metadata_json_string = convert_metadata_to_json_string(metadata);
  }
  std::stringstream json_stream;
  json_stream << "{";
  json_stream << R"("id":)" << index;
  json_stream << R"(,"name":")" << event_name;
  json_stream << R"(","cat":")" << category;
  json_stream << R"(","pid":)" << process_id;
  json_stream << R"(,"tid":)" << thread_id;
  json_stream << R"(,"ts":)" << start_time;
  json_stream << R"(,"dur":)" << duration;
  json_stream << R"(,"ph":"X")" << metadata_json_string;
  json_stream << "}";
  DFTRACER_LOG_DEBUG("WriterBase.convert_event_to_json_string %s on %s",
                     json_stream.str().c_str(), this->filename.c_str());
  return json_stream.str();
}

std::string WriterBase::convert_metadata_event_to_json_string(
    int index, ConstEventNameType name, ConstEventNameType value,
    ConstEventNameType ph, ProcessID process_id, ThreadID thread_id) {
  MetadataMap metadata;
  metadata["name"] = name;
  metadata["value"] = value;
  std::string metadata_json_string = convert_metadata_to_json_string(&metadata);
  std::stringstream json_stream;
  json_stream << "{";
  json_stream << R"("id":)" << index;
  json_stream << R"(,"name":")" << ph;
  json_stream << R"(","cat":")" << "dftracer";
  json_stream << R"(","pid":)" << process_id;
  json_stream << R"(,"tid":)" << thread_id;
  json_stream << R"(,"ph":"M")" << metadata_json_string;
  json_stream << "}";
  DFTRACER_LOG_DEBUG(
      "WriterBase.convert_metadata_event_to_json_string %s on %s",
      json_stream.str().c_str(), this->filename.c_str());
  return json_stream.str();
}

std::string WriterBase::convert_metadata_to_json_string(MetadataMap *metadata) {
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
