/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include <stdint.h>
#include <wtf/Platform.h>

#if OS(DARWIN) && CPU(ARM64)

#include "JSCConfig.h"

#include <wtf/Platform.h>

#if USE(INLINE_JIT_PERMISSIONS_API)
#include <BrowserEngineCore/BEMemory.h>

static ALWAYS_INLINE bool threadSelfRestrictSupported()
{
    return (&be_memory_inline_jit_restrict_with_witness_supported != nullptr
            && !!be_memory_inline_jit_restrict_with_witness_supported());
}

static ALWAYS_INLINE void threadSelfRestrictRWXToRW()
{
    ASSERT(g_jscConfig.useFastJITPermissions);
    be_memory_inline_jit_restrict_rwx_to_rw_with_witness();
}

static ALWAYS_INLINE void threadSelfRestrictRWXToRX()
{
    ASSERT(g_jscConfig.useFastJITPermissions);
    be_memory_inline_jit_restrict_rwx_to_rx_with_witness();
}

#elif USE(PTHREAD_JIT_PERMISSIONS_API)
#include <pthread.h>

static ALWAYS_INLINE bool threadSelfRestrictSupported()
{
    return !!pthread_jit_write_protect_supported_np();;
}

static ALWAYS_INLINE void threadSelfRestrictRWXToRW()
{
    ASSERT(g_jscConfig.useFastJITPermissions);
    pthread_jit_write_protect_np(false);
}

static ALWAYS_INLINE void threadSelfRestrictRWXToRX()
{
    ASSERT(g_jscConfig.useFastJITPermissions);
    pthread_jit_write_protect_np(true);
}

#elif USE(APPLE_INTERNAL_SDK)
#include <os/thread_self_restrict.h>

static ALWAYS_INLINE bool threadSelfRestrictSupported()
{
    return !!os_thread_self_restrict_rwx_is_supported();
}

static ALWAYS_INLINE void threadSelfRestrictRWXToRW()
{
    ASSERT(g_jscConfig.useFastJITPermissions);
    os_thread_self_restrict_rwx_to_rw();
}

static ALWAYS_INLINE void threadSelfRestrictRWXToRX()
{
    ASSERT(g_jscConfig.useFastJITPermissions);
    os_thread_self_restrict_rwx_to_rx();
}

#else

static ALWAYS_INLINE bool threadSelfRestrictSupported()
{
    return false;
}

static ALWAYS_INLINE void threadSelfRestrictRWXToRW()
{
    bool tautologyToIgnoreWarning = true;
    if (tautologyToIgnoreWarning)
        RELEASE_ASSERT_NOT_REACHED();
}

static ALWAYS_INLINE void threadSelfRestrictRWXToRX()
{
    bool tautologyToIgnoreWarning = true;
    if (tautologyToIgnoreWarning)
        RELEASE_ASSERT_NOT_REACHED();
}

#endif

#else // Not OS(DARWIN) && CPU(ARM64)

static ALWAYS_INLINE bool threadSelfRestrictSupported()
{
    return false;
}

NO_RETURN_DUE_TO_CRASH ALWAYS_INLINE void threadSelfRestrictRWXToRW()
{
    CRASH();
}

NO_RETURN_DUE_TO_CRASH ALWAYS_INLINE void threadSelfRestrictRWXToRX()
{
    CRASH();
}

#endif // OS(DARWIN) && CPU(ARM64)
