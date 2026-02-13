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
    : client_(nullptr), story_handle_(nullptr) {}

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

  if (client_) {
    DFTRACER_LOG_INFO("ChronologWriter already initialized", "");
  } else {
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
      std::map<std::string, std::string> chronicle_attrs;
      int chronicle_flags = 0;
      ret = client_->CreateChronicle(chronicle_name_, chronicle_attrs, chronicle_flags);
      if (ret != chronolog::CL_SUCCESS && ret != chronolog::CL_ERR_ACQUIRED) {
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
}

size_t ChronologWriter::write(const char* data, size_t len, bool force) {
  if (!story_handle_) {
    DFTRACER_LOG_ERROR("ChronoLog story not initialized", "");
    return 0;
  }
  try {
    // Log event as a string
    std::string event_data(data, len);
    uint64_t event_id = story_handle_->log_event(event_data);
    // Log the event ID for debugging purposes
    DFTRACER_LOG_DEBUG("ChronoLog logged event with ID: %llu", (unsigned long long)event_id);
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
  
  if (story_handle_) {
    // Release story
    if (client_) {
      int ret = client_->ReleaseStory(chronicle_name_, story_name_);
      if (ret != chronolog::CL_SUCCESS) {
        DFTRACER_LOG_ERROR("Failed to release story: error code %d", ret);
      } else {
        DFTRACER_LOG_INFO("ChronoLog story released", "");
      }
      
      // Destroy story
      ret = client_->DestroyStory(chronicle_name_, story_name_);
      if (ret != chronolog::CL_SUCCESS) {
        DFTRACER_LOG_ERROR("Failed to destroy story: error code %d", ret);
      } else {
        DFTRACER_LOG_INFO("ChronoLog story destroyed", "");
      }
    }
    story_handle_ = nullptr;
  }
  
  if (client_) {
    // Destroy chronicle
    int ret = client_->DestroyChronicle(chronicle_name_);
    if (ret != chronolog::CL_SUCCESS) {
      DFTRACER_LOG_ERROR("Failed to destroy chronicle: error code %d", ret);
    } else {
      DFTRACER_LOG_INFO("ChronoLog chronicle destroyed", "");
    }
    
    // Disconnect
    ret = client_->Disconnect();
    if (ret != chronolog::CL_SUCCESS) {
      DFTRACER_LOG_ERROR("Failed to disconnect: error code %d", ret);
    } else {
      DFTRACER_LOG_INFO("ChronoLog client disconnected", "");
    }
    
    delete client_;
    client_ = nullptr;
  }
}

}  // namespace dftracer
