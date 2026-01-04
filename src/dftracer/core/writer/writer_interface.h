#ifndef DFTRACER_WRITER_INTERFACE_H
#define DFTRACER_WRITER_INTERFACE_H

#include <cstddef>

namespace dftracer {
class WriterInterface {
 public:
  virtual void initialize(const char* filename) = 0;
  virtual size_t write(const char* data, size_t len, bool force = false) = 0;
  virtual void finalize(int index) = 0;
  virtual ~WriterInterface() = default;
};
}  // namespace dftracer

#endif  // DFTRACER_WRITER_INTERFACE_H
