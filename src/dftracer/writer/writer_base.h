#ifndef DFTRACER_WRITER_BASE_H
#define DFTRACER_WRITER_BASE_H

#include <dftracer/core/typedef.h>

#include <any>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dftracer {
class WriterBase {
 protected:
  std::string filename;
  HashType hostname_hash;
  bool enable_core_affinity = false;
  bool include_metadata = false;
  bool init = false;
  bool throw_error = false;

 public:
  virtual ~WriterBase() = default;
  virtual void initialize(char *filename, bool throw_error,
                          HashType hostname_hash) = 0;
  virtual void log(int index, ConstEventNameType event_name,
                   ConstEventNameType category, TimeResolution start_time,
                   TimeResolution duration, MetadataMap *metadata,
                   ProcessID process_id, ThreadID tid) = 0;
  virtual void log_metadata(int index, ConstEventNameType name,
                            ConstEventNameType value, ConstEventNameType ph,
                            ProcessID process_id, ThreadID tid,
                            bool is_string = true) = 0;
  virtual void finalize(bool has_entry) = 0;
};
}  // namespace dftracer

#endif  // DFTRACER_WRITER_BASE_H