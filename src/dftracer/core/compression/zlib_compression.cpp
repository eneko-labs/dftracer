#include <dftracer/core/common/singleton.h>
#include <dftracer/core/compression/zlib_compression.h>
namespace dftracer {
template <>
std::shared_ptr<ZlibCompression> Singleton<ZlibCompression>::instance = nullptr;
template <>
bool Singleton<ZlibCompression>::stop_creating_instances = false;
}  // namespace dftracer