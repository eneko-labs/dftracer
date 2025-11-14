#include <dftracer/core/common/singleton.h>
#include <dftracer/core/writer/stdio_writer.h>
namespace dftracer {
template <>
std::shared_ptr<STDIOWriter> Singleton<STDIOWriter>::instance = nullptr;
template <>
bool Singleton<STDIOWriter>::stop_creating_instances = false;
}  // namespace dftracer