#ifndef DFTRACER_MOFKA_WRITER_H
#define DFTRACER_MOFKA_WRITER_H

#include <dftracer/core/writer/writer_interface.h>

#include <diaspora/Driver.hpp>
#include <diaspora/TopicHandle.hpp>
#include <memory>
#include <string>

namespace dftracer {

class MofkaWriter : public WriterInterface {
 private:
  std::string group_file_;
  std::string topic_name_;
  std::unique_ptr<diaspora::Driver> driver_;
  std::unique_ptr<diaspora::Producer> producer_;
  std::unique_ptr<diaspora::TopicHandle> topic_;
  pid_t init_pid_;

 public:
  MofkaWriter();
  ~MofkaWriter() override;

  void initialize(const char* filename) override;
  size_t write(const char* data, size_t len, bool force = false) override;
  void finalize(int index) override;
};

}  // namespace dftracer

#endif  // DFTRACER_MOFKA_WRITER_H
