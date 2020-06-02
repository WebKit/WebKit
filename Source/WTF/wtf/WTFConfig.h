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

#include <wtf/Assertions.h>
#include <wtf/Atomics.h>
#include <wtf/ExportMacros.h>
#include <wtf/PageBlock.h>
#include <wtf/PtrTag.h>
#include <wtf/StdLibExtras.h>
#include <wtf/threads/Signals.h>

namespace WTF {

constexpr size_t ConfigSizeToProtect = CeilingOnPageSize;

struct Config {
    WTF_EXPORT_PRIVATE static void permanentlyFreeze();

    struct AssertNotFrozenScope {
        AssertNotFrozenScope();
        ~AssertNotFrozenScope();
    };

    union {
        struct {
            // All the fields in this struct should be chosen such that their
            // initial value is 0 / null / falsy because Config is instantiated
            // as a global singleton.

            bool isPermanentlyFrozen;

#if USE(PTHREADS) && HAVE(MACHINE_CONTEXT)
            SignalHandlers signalHandlers;
#endif
            PtrTagLookup* ptrTagLookupHead;

            uint64_t spaceForExtensions[1];
        };
        char ensureSize[ConfigSizeToProtect];
    };
};

extern "C" alignas(ConfigSizeToProtect) WTF_EXPORT_PRIVATE Config g_wtfConfig;

static_assert(sizeof(Config) == ConfigSizeToProtect);

ALWAYS_INLINE Config::AssertNotFrozenScope::AssertNotFrozenScope()
{
    RELEASE_ASSERT(!g_wtfConfig.isPermanentlyFrozen);
    compilerFence();
};

ALWAYS_INLINE Config::AssertNotFrozenScope::~AssertNotFrozenScope()
{
    compilerFence();
    RELEASE_ASSERT(!g_wtfConfig.isPermanentlyFrozen);
};

} // namespace WTF
