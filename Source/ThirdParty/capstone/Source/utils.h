/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

#ifndef CS_UTILS_H
#define CS_UTILS_H

#if defined(CAPSTONE_HAS_OSXKERNEL)
#include <libkern/libkern.h>
#else
#include <stddef.h>
#include "include/capstone/capstone.h"
#endif
#include "cs_priv.h"
#include "Mapping.h"

// threshold number, so above this number will be printed in hexa mode
#define HEX_THRESHOLD 9

// count number of positive members in a list.
// NOTE: list must be guaranteed to end in 0
unsigned int count_positive(const uint16_t *list);
unsigned int count_positive8(const unsigned char *list);

#define ARR_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define MATRIX_SIZE(a) (sizeof(a[0])/sizeof(a[0][0]))

char *cs_strdup(const char *str);

#define MIN(x, y) ((x) < (y) ? (x) : (y))

// we need this since Windows doesn't have snprintf()
int cs_snprintf(char *buffer, size_t size, const char *fmt, ...);

#define CS_AC_IGNORE (1 << 7)

// check if an id is existent in an array
bool arr_exist8(unsigned char *arr, unsigned char max, unsigned int id);

bool arr_exist(uint16_t *arr, unsigned char max, unsigned int id);

struct IndexType {
	uint16_t encoding;
	unsigned index;
};

// binary search for encoding in IndexType array
// return -1 if not found, or index if found
unsigned int binsearch_IndexTypeEncoding(const struct IndexType *index, size_t size, uint16_t encoding);

uint16_t readBytes16(MCInst *MI, const uint8_t *Bytes);
uint32_t readBytes32(MCInst *MI, const uint8_t *Bytes);

#endif

