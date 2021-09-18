#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct jsstring_iterator;

typedef void (*JStringIteratorAppendCallback)(struct jsstring_iterator* ctx, void* ptr, uint32_t length);
typedef void (*JStringIteratorWriteCallback)(struct jsstring_iterator* ctx, void* ptr, uint32_t length, uint32_t offset);

typedef struct jsstring_iterator {
    void* data;
    char stop;
    JStringIteratorAppendCallback append8;
    JStringIteratorAppendCallback append16;
    JStringIteratorWriteCallback write8;
    JStringIteratorWriteCallback write16;
} jsstring_iterator;

#ifdef __cplusplus
}
#endif
