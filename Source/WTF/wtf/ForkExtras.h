#pragma once

#include <stddef.h>
#include <stdint.h>
#include <wtf/Platform.h>

#ifdef __cplusplus
extern "C" {
#endif

struct jsstring_iterator;

typedef void (*JStringIteratorAppendCallback)(struct jsstring_iterator* ctx, const void* ptr, uint32_t length);
typedef void (*JStringIteratorWriteCallback)(struct jsstring_iterator* ctx, const void* ptr, uint32_t length, uint32_t offset);

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

#if CPU(X86_64)

#ifndef __extendhfsf2
#if defined(__FLT16_MANT_DIG__)
extern "C" float __extendhfsf2(_Float16 a);
#else
extern "C" float __extendhfsf2(uint16_t a);
#endif
#endif

#ifndef __truncdfhf2
#if defined(__FLT16_MANT_DIG__)
extern "C" _Float16 __truncdfhf2(double a);
#else
extern "C" uint16_t __truncdfhf2(double a);
#endif
#endif

#ifndef __truncsfhf2
#if defined(__FLT16_MANT_DIG__)
extern "C" _Float16 __truncsfhf2(float a);
#else
extern "C" uint16_t __truncsfhf2(float a);
#endif
#endif

#endif
