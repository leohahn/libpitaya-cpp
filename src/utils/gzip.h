#ifndef PITAYA_UTILS_GZIP_H
#define PITAYA_UTILS_GZIP_H

#include <cstdlib>

namespace pitaya {
namespace utils {

int IsCompressed(const uint8_t* data, size_t size);

int Decompress(uint8_t** output, size_t* output_size, uint8_t* data, size_t size);

int Compress(uint8_t** output, size_t* output_size, uint8_t* data, size_t size);

} // namespace utils
} // namespace pitaya

#endif // PITAYA_UTILS_GZIP_H