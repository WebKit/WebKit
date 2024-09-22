/*
 * Copyright 2024 Igalia S.L.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/private/base/SkMalloc.h"

#include <bmalloc/bmalloc.h>
#include <cstdlib>

void sk_abort_no_print() {
    abort();
}

void sk_free(void* p) {
    bmalloc::api::free(p);
}

void* sk_realloc_throw(void *addr, size_t size) {
    return bmalloc::api::realloc(addr, size, bmalloc::CompactAllocationMode::NonCompact);
}

void* sk_malloc_flags(size_t size, unsigned flags) {
    static constexpr auto allocMode = bmalloc::CompactAllocationMode::NonCompact;

    if (flags & SK_MALLOC_ZERO_INITIALIZE) {
        if (flags & SK_MALLOC_THROW)
            return bmalloc::api::zeroedMalloc(size, allocMode);
        return bmalloc::api::tryZeroedMalloc(size, allocMode);
    }

    if (flags & SK_MALLOC_THROW)
        return bmalloc::api::malloc(size, allocMode);
    return bmalloc::api::tryMalloc(size, allocMode);
}

size_t sk_malloc_size(void *addr [[maybe_unused]], size_t size) {
#if BENABLE(MALLOC_SIZE)
    return bmalloc::api::mallocSize(addr);
#else
    return size;
#endif
}
