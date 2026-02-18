#include <dftracer/core/common/logging.h>
#include <dftracer/core/common/singleton.h>
#include <dftracer/core/writer/chronolog_writer.h>

#include <cstdlib>
#include <unistd.h>

namespace dftracer {
template <>
std::shared_ptr<ChronologWriter> Singleton<ChronologWriter>::instance = nullptr;
template <>
bool Singleton<ChronologWriter>::stop_creating_instances = false;

ChronologWriter::ChronologWriter()
    : client_(nullptr), story_handle_(nullptr), init_pid_(getpid()) {}

ChronologWriter::~ChronologWriter() { finalize(0); }

void ChronologWriter::initialize(const char* filename) {
  // Read configuration from environment variables with defaults
  const char* protocol_env = std::getenv("DFTRACER_CHRONOLOG_PROTOCOL");
  protocol_ = protocol_env ? protocol_env : "ofi+sockets";

  const char* host_env = std::getenv("DFTRACER_CHRONOLOG_HOST");
  host_ = host_env ? host_env : "127.0.0.1";

  const char* port_env = std::getenv("DFTRACER_CHRONOLOG_PORT");
  if (port_env) {
    try {
      int port_val = std::stoi(port_env);
      if (port_val > 0 && port_val <= 65535) {
        port_ = static_cast<uint16_t>(port_val);
      } else {
        DFTRACER_LOG_ERROR("Invalid DFTRACER_CHRONOLOG_PORT value: %s, using default 5555", port_env);
        port_ = 5555;
      }
    } catch (const std::exception& e) {
      DFTRACER_LOG_ERROR("Failed to parse DFTRACER_CHRONOLOG_PORT: %s, using default 5555", e.what());
      port_ = 5555;
    }
  } else {
    port_ = 5555;
  }

  const char* provider_id_env = std::getenv("DFTRACER_CHRONOLOG_PROVIDER_ID");
  if (provider_id_env) {
    try {
      int provider_id_val = std::stoi(provider_id_env);
      if (provider_id_val >= 0 && provider_id_val <= 65535) {
        provider_id_ = static_cast<uint16_t>(provider_id_val);
      } else {
        DFTRACER_LOG_ERROR("Invalid DFTRACER_CHRONOLOG_PROVIDER_ID value: %s, using default 55", provider_id_env);
        provider_id_ = 55;
      }
    } catch (const std::exception& e) {
      DFTRACER_LOG_ERROR("Failed to parse DFTRACER_CHRONOLOG_PROVIDER_ID: %s, using default 55", e.what());
      provider_id_ = 55;
    }
  } else {
    provider_id_ = 55;
  }

  const char* chronicle_name_env = std::getenv("DFTRACER_CHRONOLOG_CHRONICLE_NAME");
  chronicle_name_ = chronicle_name_env ? chronicle_name_env : "dftracer_chronicle";

  const char* story_name_env = std::getenv("DFTRACER_CHRONOLOG_STORY_NAME");
  story_name_ = story_name_env ? story_name_env : "dftracer_story";

  // Check if already initialized in this process
  if (client_ && init_pid_ == getpid()) {
    DFTRACER_LOG_INFO("ChronologWriter already initialized", "");
    return;
  }

  // Handle fork scenario: reset and do not connect in child. The child often
  // exec()s immediately; connecting here can block (Connect/CreateChronicle/
  // AcquireStory) and cause the test to time out. Child writes are no-ops
  // (write() returns 0 when story_handle_ is null).
  if (client_ && init_pid_ != getpid()) {
    DFTRACER_LOG_INFO(
        "ChronologWriter detected fork (Init PID: %d, Current PID: %d). Resetting for child process (no connect).",
        init_pid_, getpid());
    client_ = nullptr;
    story_handle_ = nullptr;
    init_pid_ = getpid();
    return;
  }

  try {
    // Create ClientPortalServiceConf
    chronolog::ClientPortalServiceConf conf;
    conf.PROTO_CONF = protocol_;
    conf.IP = host_;
    conf.PORT = port_;
    conf.PROVIDER_ID = provider_id_;

    // Create and connect client
    client_ = new chronolog::Client(conf);
    int ret = client_->Connect();
    if (ret != chronolog::CL_SUCCESS) {
      DFTRACER_LOG_ERROR("Failed to connect to ChronoLog: error code %d", ret);
      delete client_;
      client_ = nullptr;
      throw std::runtime_error("Failed to connect to ChronoLog");
    }
    DFTRACER_LOG_INFO("ChronoLog client connected", "");

    // Create chronicle
    // Empty attrs and flags are fine - ChronoLog API accepts these for default behavior.
    // Accept CL_SUCCESS, CL_ERR_ACQUIRED, and -6 (chronicle already exists per visor).
    std::map<std::string, std::string> chronicle_attrs;
    int chronicle_flags = 0;
    ret = client_->CreateChronicle(chronicle_name_, chronicle_attrs, chronicle_flags);
    const int CL_ERR_CHRONICLE_EXISTS = -6;  // ChronoVisor: "A Chronicle with the same name already exists"
    if (ret != chronolog::CL_SUCCESS && ret != chronolog::CL_ERR_ACQUIRED && ret != CL_ERR_CHRONICLE_EXISTS) {
      DFTRACER_LOG_ERROR("Failed to create chronicle '%s': error code %d", 
                        chronicle_name_.c_str(), ret);
      client_->Disconnect();
      delete client_;
      client_ = nullptr;
      throw std::runtime_error("Failed to create ChronoLog chronicle");
    }
    DFTRACER_LOG_INFO("ChronoLog chronicle '%s' created or already exists", 
                     chronicle_name_.c_str());

    // Acquire story
    // Empty attrs and flags are fine - ChronoLog API accepts these for default behavior
    std::map<std::string, std::string> story_attrs;
    int story_flags = 0;
    auto story_result = client_->AcquireStory(chronicle_name_, story_name_, 
                                               story_attrs, story_flags);
    if (story_result.first != chronolog::CL_SUCCESS) {
      DFTRACER_LOG_ERROR("Failed to acquire story '%s': error code %d", 
                        story_name_.c_str(), story_result.first);
      client_->DestroyChronicle(chronicle_name_);
      client_->Disconnect();
      delete client_;
      client_ = nullptr;
      throw std::runtime_error("Failed to acquire ChronoLog story");
    }
    story_handle_ = story_result.second;
    DFTRACER_LOG_INFO("ChronoLog story '%s' acquired", story_name_.c_str());

    init_pid_ = getpid();
    DFTRACER_LOG_INFO("ChronologWriter initialized with PID %d", init_pid_);
  } catch (const std::exception& e) {
    DFTRACER_LOG_ERROR("Failed to initialize ChronologWriter", e.what());
    throw;
  }
}

size_t ChronologWriter::write(const char* data, size_t len, bool force) {
  // Note: 'force' parameter is part of WriterInterface but unused here.
  // ChronoLog always writes immediately (no buffering), making force irrelevant.
  (void)force;  // Suppress unused parameter warning
  
  if (!story_handle_) {
    DFTRACER_LOG_ERROR("ChronoLog story not initialized", "");
    return 0;
  }
  try {
    // Log event as a string
    std::string event_data(data, len);
    story_handle_->log_event(event_data);
    return len;
  } catch (const std::exception& e) {
    DFTRACER_LOG_ERROR("ChronoLog write failed", e.what());
    return 0;
  }
}

void ChronologWriter::finalize(int index) {
  DFTRACER_LOG_INFO("ChronoLog finalizing", "");
  if (getpid() != init_pid_) {
    DFTRACER_LOG_INFO(
        "ChronologWriter::finalize called in child process (Init PID: %d, Current "
        "PID: %d). Skipping cleanup to avoid issues.",
        init_pid_, getpid());
    // In a child process, do not cleanup normally.
    // Just reset pointers without deletion to avoid double-free issues.
    story_handle_ = nullptr;
    client_ = nullptr;
    return;
  }
  
  // Writer must release the story so the reader can acquire and replay.
  story_handle_ = nullptr;
  if (client_ && !chronicle_name_.empty() && !story_name_.empty()) {
    int rel = client_->ReleaseStory(chronicle_name_, story_name_);
    if (rel == chronolog::CL_SUCCESS) {
      DFTRACER_LOG_INFO("ChronoLog story released", "");
    } else {
      DFTRACER_LOG_ERROR("Failed to release story '%s': error code %d",
                         story_name_.c_str(), rel);
    }
    // Always exit immediately: ChronoLog client teardown (Disconnect/delete) triggers
    // Thallium heap corruption. _exit(0) skips destructors safely.
    _exit(0);
  }
  // client_ is null → nothing to clean up (child process path already returned above)
}

}  // namespace dftracer
