/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_CONFIG_H
#define PAS_CONFIG_H

#include "pas_config_prefix.h"

#define LIBPAS_ENABLED 1

#if defined(PAS_BMALLOC)
#include "BPlatform.h"
#if !BENABLE(LIBPAS)
#undef LIBPAS_ENABLED
#define LIBPAS_ENABLED 0
#endif
#endif

#if ((PAS_OS(DARWIN) && __PAS_ARM64 && !__PAS_ARM64E) || PAS_PLATFORM(PLAYSTATION)) && defined(NDEBUG)
#define PAS_ENABLE_ASSERT 0
#else
#define PAS_ENABLE_ASSERT 1
#endif
#define PAS_ENABLE_TESTING __PAS_ENABLE_TESTING

#define PAS_ARM64 __PAS_ARM64
#define PAS_ARM32 __PAS_ARM32

#define PAS_ARM __PAS_ARM

#define PAS_RISCV __PAS_RISCV

#define PAS_ADDRESS_BITS                 48

#if PAS_ARM || PAS_PLATFORM(PLAYSTATION)
#define PAS_MAX_GRANULES                 256
#else
#define PAS_MAX_GRANULES                 1024
#endif

#define PAS_INTERNAL_MIN_ALIGN_SHIFT     3
#define PAS_INTERNAL_MIN_ALIGN           ((size_t)1 << PAS_INTERNAL_MIN_ALIGN_SHIFT)

#if defined(PAS_BMALLOC)
#define PAS_ENABLE_THINGY                0
#define PAS_ENABLE_ISO                   0
#define PAS_ENABLE_ISO_TEST              0
#define PAS_ENABLE_MINALIGN32            0
#define PAS_ENABLE_PAGESIZE64K           0
#define PAS_ENABLE_BMALLOC               1
#define PAS_ENABLE_HOTBIT                0
#define PAS_ENABLE_JIT                   1
#elif defined(PAS_LIBMALLOC)
#define PAS_ENABLE_THINGY                0
#define PAS_ENABLE_ISO                   1
#define PAS_ENABLE_ISO_TEST              0
#define PAS_ENABLE_MINALIGN32            0
#define PAS_ENABLE_PAGESIZE64K           0
#define PAS_ENABLE_BMALLOC               0
#define PAS_ENABLE_HOTBIT                0
#define PAS_ENABLE_JIT                   0
#else /* PAS_LIBMALLOC -> so !defined(PAS_BMALLOC) && !defined(PAS_LIBMALLOC) */
#define PAS_ENABLE_THINGY                1
#define PAS_ENABLE_ISO                   1
#define PAS_ENABLE_ISO_TEST              1
#define PAS_ENABLE_MINALIGN32            1
#define PAS_ENABLE_PAGESIZE64K           1
#define PAS_ENABLE_BMALLOC               1
#define PAS_ENABLE_HOTBIT                1
#define PAS_ENABLE_JIT                   1
#endif /* PAS_LIBMALLOC -> so end of !defined(PAS_BMALLOC) && !defined(PAS_LIBMALLOC) */

#define PAS_COMPACT_PTR_SIZE             3
#define PAS_COMPACT_PTR_BITS             (PAS_COMPACT_PTR_SIZE << 3)
#define PAS_COMPACT_PTR_MASK             ((uintptr_t)(((uint64_t)1 \
                                                       << (PAS_COMPACT_PTR_BITS & 63)) - 1))

#define PAS_ALLOCATOR_INDEX_BYTES        4

#if PAS_OS(DARWIN) || PAS_PLATFORM(PLAYSTATION)
#define PAS_USE_SPINLOCKS                0
#else
#define PAS_USE_SPINLOCKS                1
#endif

#endif /* PAS_CONFIG_H */

