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

  try {
    diaspora::Metadata options;
    options.json()["group_file"] = group_file_;
    options.json()["margo"] = nlohmann::json::object();
    options.json()["margo"]["use_progress_thread"] = true;

    driver_ = std::make_unique<diaspora::Driver>(
        diaspora::Driver::New("mofka", options));

    if (!driver_->topicExists(topic_name_)) {
      diaspora::Validator validator;
      diaspora::Serializer serializer;
      diaspora::PartitionSelector selector;
      driver_->createTopic(topic_name_, diaspora::Metadata{}, validator,
                           selector, serializer);
      driver_->as<mofka::MofkaDriver>().addMemoryPartition(topic_name_, 0);
    }

    topic_ = std::make_unique<diaspora::TopicHandle>(
        driver_->openTopic(topic_name_));

    diaspora::BatchSize batchSize = diaspora::BatchSize::Adaptive();
    diaspora::ThreadCount threadCount = diaspora::ThreadCount{1};
    diaspora::Ordering ordering = diaspora::Ordering::Strict;
    producer_ = std::make_unique<diaspora::Producer>(
        topic_->producer("dftracer", batchSize, threadCount, ordering));
  } catch (const std::exception& e) {
    DFTRACER_LOG_ERROR("Failed to initialize MofkaWriter", e.what());
    throw;
  }
}

size_t MofkaWriter::write(const char* data, size_t len, bool force) {
  if (!topic_) return 0;
  try {
    producer_->push(diaspora::Metadata{data}, diaspora::DataView{});
    return len;
  } catch (const std::exception& e) {
    DFTRACER_LOG_ERROR("Mofka write failed", e.what());
    return 0;
  }
}

void MofkaWriter::finalize(int index) {
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