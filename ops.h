#ifndef BITMAP_OPS_H
#define BITMAP_OPS_H

#include <cstdint>

void bit_or(uint8_t *dst, const uint8_t *src, uint64_t len);

void bit_and(uint8_t *dst, const uint8_t *src, uint64_t len);

void bit_xor(uint8_t *dst, const uint8_t *src, uint64_t len);

#endif //BITMAP_OPS_H
