/*
 * Copyright (c) 2018-2025 Apple Inc. All rights reserved.
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

#ifndef PAS_ZERO_MEMORY_H
#define PAS_ZERO_MEMORY_H

#include "pas_mte_config.h"
#include "pas_utils.h"
#include <stdint.h>

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE void pas_zero_memory(void* memory, size_t size)
{
    PAS_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    /*
     * MTE systems perform poorly when zeroing large chunks
     * of memory, so we set TCO when applicable to avoid that overhead.
     */
    if (PAS_USE_MTE)
        PAS_MTE_CHECK_TAG_AND_SET_TCO(memory);
    PAS_PROFILE(ZERO_MEMORY, memory, size);
    memset(memory, 0, size);
    if (PAS_USE_MTE)
        PAS_MTE_CLEAR_TCO;
    PAS_ALLOW_UNSAFE_BUFFER_USAGE_END
}

PAS_END_EXTERN_C;

#endif // PAS_ZERO_MEMORY_H
