#include <dftracer/core/logging.h>
#include <dftracer/writer/zeromq_writer.h>
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
std::shared_ptr<dftracer::ZeroMQWriter>
    dftracer::Singleton<dftracer::ZeroMQWriter>::instance = nullptr;
template <>
bool dftracer::Singleton<dftracer::ZeroMQWriter>::stop_creating_instances =
    false;

namespace dftracer {
void ZeroMQWriter::initialize(char *filename, bool throw_error,
                              HashType hostname_hash) {
  this->hostname_hash = hostname_hash;
  this->throw_error = throw_error;
  this->filename = filename;
  context = std::make_unique<zmq::context_t>(1);
  socket = std::make_unique<zmq::socket_t>(*context, zmq::socket_type::push);
  if (!socket) {
    DFTRACER_LOG_ERROR("unable to create ZeroMQ socket %s", filename);
  }
  socket->connect(filename);
  this->init = true;
  DFTRACER_LOG_INFO("created ZeroMQ socket and connected to %s", filename);
}

void ZeroMQWriter::log(int index, ConstEventNameType event_name,
                       ConstEventNameType category, TimeResolution start_time,
                       TimeResolution duration, MetadataMap *metadata,
                       ProcessID process_id, ThreadID thread_id) {
  DFTRACER_LOG_DEBUG("ZeroMQWriter.log", "");

  if (init) {
    auto event_json =
        convert_event_to_json_string(index, event_name, category, start_time,
                                     duration, metadata, process_id, thread_id);
    zmq::message_t message(event_json.data(), event_json.size());
    socket->send(message, zmq::send_flags::dontwait);
  } else {
    DFTRACER_LOG_ERROR("ZeroMQWriter.log invalid", "");
  }
}

void ZeroMQWriter::log_metadata(int index, ConstEventNameType name,
                                ConstEventNameType value, ConstEventNameType ph,
                                ProcessID process_id, ThreadID thread_id,
                                bool is_string) {
  DFTRACER_LOG_DEBUG("ZeroMQWriter.log_metadata", "");
  if (init) {
    auto event_json = convert_metadata_event_to_json_string(
        index, name, value, ph, process_id, thread_id);
    zmq::message_t message(event_json.data(), event_json.size());
    socket->send(message, zmq::send_flags::dontwait);
  } else {
    DFTRACER_LOG_ERROR("ZeroMQWriter.log_metadata invalid", "");
  }
}

void ZeroMQWriter::finalize(bool has_entry) {
  DFTRACER_LOG_DEBUG("ZeroMQWriter.finalize", "");
  if (this->init) {
    socket->close();
    context->close();
    this->init = false;
  } else {
    DFTRACER_LOG_ERROR("ZeroMQWriter.finalize invalid", "");
  }
}

}  // namespace dftracer