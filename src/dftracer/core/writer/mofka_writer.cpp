#include <dftracer/core/common/logging.h>
#include <dftracer/core/common/singleton.h>
#include <dftracer/core/writer/mofka_writer.h>
#include <sys/prctl.h>

#include <cstdlib>
#include <diaspora/DataView.hpp>
#include <diaspora/Metadata.hpp>
#include <mofka/MofkaDriver.hpp>

namespace dftracer {
template <>
std::shared_ptr<MofkaWriter> Singleton<MofkaWriter>::instance = nullptr;
template <>
bool Singleton<MofkaWriter>::stop_creating_instances = false;

MofkaWriter::MofkaWriter() {}

MofkaWriter::~MofkaWriter() { finalize(0); }

void MofkaWriter::initialize(const char* filename) {
  const char* group_file_env = std::getenv("DFTRACER_MOFKA_GROUP_FILE");
  if (!group_file_env) {
    DFTRACER_LOG_ERROR("DFTRACER_MOFKA_GROUP_FILE not set", "");
    throw std::runtime_error("DFTRACER_MOFKA_GROUP_FILE not set");
  }
  group_file_ = group_file_env;
  const char* topic_name_env = std::getenv("DFTRACER_MOFKA_TOPIC_NAME");
  topic_name_ = topic_name_env ? topic_name_env : "dftracer_events";

  // Allow Mofka/Mercury to access this process's memory for shared memory
  // transport
  // prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);

  if (driver_) {
    DFTRACER_LOG_INFO("MofkaWriter already initialized", "");
  } else {
    try {
      diaspora::Metadata options;
      options.json()["group_file"] = group_file_;
      options.json()["margo"] = nlohmann::json::object();
      options.json()["margo"]["use_progress_thread"] = true;

      driver_ = std::make_unique<diaspora::Driver>(
          diaspora::Driver::New("mofka", options));
      DFTRACER_LOG_INFO("Mofka driver initialized", "");

      if (driver_->topicExists(topic_name_)) {
        DFTRACER_LOG_INFO("Mofka topic exists", "");
      } else {
        diaspora::Validator validator;
        diaspora::Serializer serializer;
        diaspora::PartitionSelector selector;
        driver_->createTopic(topic_name_, diaspora::Metadata{}, validator,
                             selector, serializer);
        driver_->as<mofka::MofkaDriver>().addMemoryPartition(topic_name_, 0);
        DFTRACER_LOG_INFO("Mofka topic created", "");
      }

      topic_ = std::make_unique<diaspora::TopicHandle>(
          driver_->openTopic(topic_name_));
      DFTRACER_LOG_INFO("Mofka topic opened", "");

      diaspora::BatchSize batchSize = diaspora::BatchSize::Adaptive();
      diaspora::ThreadCount threadCount = diaspora::ThreadCount{1};
      diaspora::Ordering ordering = diaspora::Ordering::Strict;
      producer_ = std::make_unique<diaspora::Producer>(
          topic_->producer("dftracer", batchSize, threadCount, ordering));
      DFTRACER_LOG_INFO("Mofka producer created", "");

      init_pid_ = getpid();
      DFTRACER_LOG_INFO("MofkaWriter initialized with PID %d", init_pid_);
    } catch (const std::exception& e) {
      DFTRACER_LOG_ERROR("Failed to initialize MofkaWriter", e.what());
      throw;
    }
  }
}

size_t MofkaWriter::write(const char* data, size_t len, bool force) {
  if (!topic_) {
    DFTRACER_LOG_ERROR("Mofka topic not initialized", "");
    return 0;
  }
  try {
    producer_->push(diaspora::Metadata{data}, diaspora::DataView{});
    return len;
  } catch (const std::exception& e) {
    DFTRACER_LOG_ERROR("Mofka write failed", e.what());
    return 0;
  }
}

void MofkaWriter::finalize(int index) {
  DFTRACER_LOG_INFO("Mofka finalizing", "");
  if (getpid() != init_pid_) {
    DFTRACER_LOG_INFO(
        "MofkaWriter::finalize called in child process (Init PID: %d, Current "
        "PID: %d). Skipping flush to avoid hang.",
        init_pid_, getpid());
    // In a child process, do not flush or destruct normally.
    // Just release ownership to avoid destructor calls that might wait on
    // futures.
    if (producer_) {
      producer_.release();
    }
    if (topic_) {
      topic_.release();
    }
    if (driver_) {
      driver_.release();
    }
    return;
  }
  if (producer_) {
    producer_->flush().wait(-1);
    DFTRACER_LOG_INFO("Mofka producer flushed", "");
    producer_.reset();
    DFTRACER_LOG_INFO("Mofka producer reset", "");
  }
  if (topic_) {
    topic_.reset();
    DFTRACER_LOG_INFO("Mofka topic reset", "");
  }
  if (driver_) {
    driver_.reset();
    DFTRACER_LOG_INFO("Mofka driver reset", "");
  }
}

}  // namespace dftracer