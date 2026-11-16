/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Compiler.h>
#include <wtf/Noncopyable.h>
#include <wtf/Platform.h>

#if (OS(LINUX) || OS(FREEBSD) || OS(HURD)) && (COMPILER(GCC) || COMPILER(CLANG))
#define HAVE_WEAK_LSAN 1
#endif

#if HAVE(WEAK_LSAN)
extern "C" {
__attribute__((weak)) void __lsan_disable(void);
__attribute__((weak)) void __lsan_enable(void);
}
#endif

namespace WTF {

class ScopedLeakSanitizerDisabler {
    WTF_MAKE_NONCOPYABLE(ScopedLeakSanitizerDisabler);
public:
    ScopedLeakSanitizerDisabler()
    {
#if HAVE(WEAK_LSAN)
        if (__lsan_disable)
            __lsan_disable();
#endif
    }

    ~ScopedLeakSanitizerDisabler()
    {
#if HAVE(WEAK_LSAN)
        if (__lsan_enable)
            __lsan_enable();
#endif
    }
};

} // namespace WTF

using WTF::ScopedLeakSanitizerDisabler;
