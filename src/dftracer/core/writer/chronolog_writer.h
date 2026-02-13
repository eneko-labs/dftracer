#ifndef DFTRACER_CHRONOLOG_WRITER_H
#define DFTRACER_CHRONOLOG_WRITER_H

#include <dftracer/core/writer/writer_interface.h>

#include <chronolog_client.h>
#include <string>

namespace dftracer {

class ChronologWriter : public WriterInterface {
 private:
  std::string protocol_;
  std::string host_;
  uint16_t port_;
  uint16_t provider_id_;
  std::string chronicle_name_;
  std::string story_name_;
  chronolog::Client* client_;
  chronolog::StoryHandle* story_handle_;
  pid_t init_pid_ = 0;

 public:
  ChronologWriter();
  ~ChronologWriter() override;

  void initialize(const char* filename) override;
  size_t write(const char* data, size_t len, bool force = false) override;
  void finalize(int index) override;
};

}  // namespace dftracer

#endif  // DFTRACER_CHRONOLOG_WRITER_H
