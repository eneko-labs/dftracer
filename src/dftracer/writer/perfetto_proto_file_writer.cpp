#include <dftracer/core/logging.h>
#include <dftracer/writer/perfetto_proto_file_writer.h>
#include <fcntl.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <thread>

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("dftracer").SetDescription("DFTracer Events"));

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

template <>
std::shared_ptr<dftracer::PerfettoProtoFileWriter>
    dftracer::Singleton<dftracer::PerfettoProtoFileWriter>::instance = nullptr;
template <>
bool dftracer::Singleton<
    dftracer::PerfettoProtoFileWriter>::stop_creating_instances = false;

namespace dftracer {
void PerfettoProtoFileWriter::initialize(char *filename, bool throw_error,
                                         HashType hostname_hash) {
  this->hostname_hash = hostname_hash;
  this->throw_error = throw_error;
  this->filename = filename;

  perfetto::TracingInitArgs tracing_args;
  tracing_args.backends = perfetto::kInProcessBackend;
  perfetto::Tracing::Initialize(tracing_args);
  perfetto::TrackEvent::Register();

  perfetto::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024 * 16);  // 16MB buffer
  auto *data_source_config = trace_config.add_data_sources()->mutable_config();
  data_source_config->set_name("track_event");

  int trace_fd = open(this->filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (trace_fd == -1) {
    DFTRACER_LOG_ERROR("unable to create log file %s", filename);
    if (this->throw_error) {
      throw std::runtime_error("unable to create log file");
    }
    return;
  }

  tracing_session = perfetto::Tracing::NewTrace();
  tracing_session->Setup(trace_config, trace_fd);
  tracing_session->StartBlocking();

  init = true;

  DFTRACER_LOG_DEBUG("PerfettoProtoFileWriter.initialize %s",
                     this->filename.c_str());
}

void PerfettoProtoFileWriter::log(int index, ConstEventNameType event_name,
                                  ConstEventNameType category,
                                  TimeResolution start_time,
                                  TimeResolution duration,
                                  MetadataMap *metadata, ProcessID process_id,
                                  ThreadID thread_id) {
  DFTRACER_LOG_DEBUG("PerfettoProtoFileWriter.log", "");
  if (!init) {
    DFTRACER_LOG_ERROR("PerfettoProtoFileWriter not initialized", "");
    return;
  }
  std::lock_guard<std::mutex> lock(mtx);

  perfetto::TrackEvent::Trace([&](perfetto::TrackEvent::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(start_time * 1000);
    packet->set_timestamp_clock_id(
        perfetto::protos::pbzero::BUILTIN_CLOCK_MONOTONIC);

    auto event = packet->set_track_event();
    event->add_categories(category);
    event->set_name(event_name);

    auto *legacy_event = event->set_legacy_event();
    legacy_event->set_phase('X');
    legacy_event->set_duration_us(duration);
    legacy_event->set_thread_duration_us(duration);
    legacy_event->set_pid_override(process_id);
    legacy_event->set_tid_override(thread_id);

    auto *hostname_annot = event->add_debug_annotations();
    hostname_annot->set_name("hhash");
    hostname_annot->set_string_value(this->hostname_hash);
    if (metadata) {
      for (auto const &[key, val] : *metadata) {
        auto *debug_annot = event->add_debug_annotations();
        debug_annot->set_name(key.c_str());
        if (val.type() == typeid(int)) {
          debug_annot->set_int_value(std::any_cast<int>(val));
        } else if (val.type() == typeid(unsigned int)) {
          debug_annot->set_uint_value(std::any_cast<unsigned int>(val));
        } else if (val.type() == typeid(long)) {
          debug_annot->set_int_value(std::any_cast<long>(val));
        } else if (val.type() == typeid(unsigned long)) {
          debug_annot->set_uint_value(std::any_cast<unsigned long>(val));
        } else if (val.type() == typeid(long long)) {
          debug_annot->set_int_value(std::any_cast<long long>(val));
        } else if (val.type() == typeid(unsigned long long)) {
          debug_annot->set_uint_value(std::any_cast<unsigned long long>(val));
        } else if (val.type() == typeid(const char *)) {
          debug_annot->set_string_value(std::any_cast<const char *>(val));
        } else if (val.type() == typeid(std::string)) {
          debug_annot->set_string_value(std::any_cast<std::string>(val));
        } else if (val.type() == typeid(HashType)) {
          debug_annot->set_string_value(std::any_cast<HashType>(val));
        } else if (val.type() == typeid(size_t)) {
          debug_annot->set_uint_value(std::any_cast<size_t>(val));
        } else if (val.type() == typeid(uint16_t)) {
          debug_annot->set_uint_value(std::any_cast<uint16_t>(val));
        } else if (val.type() == typeid(ssize_t)) {
          debug_annot->set_int_value(std::any_cast<ssize_t>(val));
        } else if (val.type() == typeid(off_t)) {
          debug_annot->set_int_value(std::any_cast<off_t>(val));
        } else if (val.type() == typeid(off64_t)) {
          debug_annot->set_int_value(std::any_cast<off64_t>(val));
        } else if (val.type() == typeid(float)) {
          debug_annot->set_double_value(std::any_cast<float>(val));
        } else if (val.type() == typeid(double)) {
          debug_annot->set_double_value(std::any_cast<double>(val));
        } else {
          DFTRACER_LOG_WARN("No conversion for type %s", key.c_str());
        }
      }
    }
  });
}

void PerfettoProtoFileWriter::log_metadata(int index, ConstEventNameType name,
                                           ConstEventNameType value,
                                           ConstEventNameType ph,
                                           ProcessID process_id,
                                           ThreadID thread_id, bool is_string) {
  DFTRACER_LOG_DEBUG("PerfettoProtoFileWriter.log_metadata", "");
}

void PerfettoProtoFileWriter::finalize(bool has_entry) {
  DFTRACER_LOG_DEBUG("PerfettoProtoFileWriter.finalize", "");
  if (init) {
    tracing_session->StopBlocking();

    init = false;
  } else {
    DFTRACER_LOG_ERROR("PerfettoProtoFileWriter.finalize invalid", "");
  }
}

}  // namespace dftracer