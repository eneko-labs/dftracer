#include <dftracer/core/logging.h>
#include <dftracer/writer/perfetto_chrome_zmq_writer.h>
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
std::shared_ptr<dftracer::PerfettoChromeZMQWriter>
    dftracer::Singleton<dftracer::PerfettoChromeZMQWriter>::instance = nullptr;
template <>
bool dftracer::Singleton<
    dftracer::PerfettoChromeZMQWriter>::stop_creating_instances = false;

namespace dftracer {

static PerfettoChromeZMQWriter* writer_instance = nullptr;

void on_fork_child() {
  if (writer_instance) {
    DFTRACER_LOG_DEBUG("Fork child: re-initializing ZMQ socket.", "");
    writer_instance->reconnect();
  }
}

size_t PerfettoChromeZMQWriter::flush_buffer_to_destination(bool force) {
  std::unique_lock lock(mtx);
  if (current_index == 0 || (!force && current_index < write_buffer_size))
    return 0;
  DFTRACER_LOG_DEBUG("PerfettoChromeZMQWriter.flush_buffer_to_stream %s",
                     this->filename.c_str());

  size_t bytes_to_send = current_index;
  if (socket && bytes_to_send > 0) {
    try {
      // Create ZMQ message from buffer content
      zmq::message_t message(buffer.data(), bytes_to_send);
      auto result = socket->send(message, zmq::send_flags::dontwait);
      if (result) {
        DFTRACER_LOG_DEBUG("PerfettoChromeZMQWriter sent %zu bytes",
                           bytes_to_send);
        current_index = 0;  // Reset buffer after successful send
        return bytes_to_send;
      } else {
        DFTRACER_LOG_ERROR("PerfettoChromeZMQWriter failed to send message",
                           "");
        return 0;
      }
    } catch (const zmq::error_t& e) {
      DFTRACER_LOG_ERROR("PerfettoChromeZMQWriter ZMQ error: %s", e.what());
      return 0;
    }
  }
  return 0;
}

void PerfettoChromeZMQWriter::initialize(char* filename, bool throw_error,
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

  writer_instance = this;

  pthread_atfork(nullptr, nullptr, on_fork_child);

  init = true;
  DFTRACER_LOG_DEBUG(
      "PerfettoChromeZMQWriter.initialize on %s with fork handling",
      this->filename.c_str());
}

void PerfettoChromeZMQWriter::finalize(bool has_entry) {
  if (init) {
    DFTRACER_LOG_DEBUG("PerfettoChromeZMQWriter.finalize", "");

    socket->close();
    context->close();

    if (writer_instance == this) {
      writer_instance = nullptr;
    }

    init = false;

    DFTRACER_LOG_DEBUG("Finished writer finalization", "");
  } else {
    DFTRACER_LOG_DEBUG("Already finalized writer", "");
  }
}

void PerfettoChromeZMQWriter::reconnect() {
  DFTRACER_LOG_DEBUG("Child process starting ZMQ reconnect", "");

  // Release ownership of the inherited pointers.
  // This prevents the destructor (and zmq_close) from being called.
  // The old state is invalid and must be abandoned.
  if (socket) socket.release();
  if (context) context.release();

  DFTRACER_LOG_DEBUG("Child process abandoned inherited ZMQ state", "");

  // Create a brand new context and socket for this child process.
  context = std::make_unique<zmq::context_t>(1);
  socket = std::make_unique<zmq::socket_t>(*context, zmq::socket_type::push);

  if (!socket) {
    DFTRACER_LOG_ERROR("Unable to create new ZeroMQ socket in child for %s",
                       this->filename.c_str());
    return;
  }

  // Connect the new socket to the original destination.
  socket->connect(this->filename.c_str());
  DFTRACER_LOG_DEBUG("Child process successfully reconnected ZMQ socket to %s",
                     this->filename.c_str());
}

}  // namespace dftracer