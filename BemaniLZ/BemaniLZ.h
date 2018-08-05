
#ifndef BEMANILZ_H
#define BEMANILZ_H

#include <stdint.h>

// Decompress data compressed with KonamiLZ
// Return value: final decompressed size
int decompress(uint8_t *src, int srcSize, uint8_t *dst, int dstSize);

#endif // BEMANILZ_H
