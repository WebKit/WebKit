/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if ENABLE(LLVM_PROFILE_GENERATION) && PLATFORM(IOS_FAMILY)

#include <unistd.h>
#include <wtf/Assertions.h>

extern "C" int __llvm_profile_runtime = 0;
extern "C" void __llvm_profile_initialize_file(void);
extern "C" const char *__llvm_profile_get_filename(void);

namespace WTF {

ALWAYS_INLINE void initializeLLVMProfiling()
{
    static std::once_flag registerFlag;
    std::call_once(registerFlag, [] {
        __llvm_profile_initialize_file();
        const char *profilePath = __llvm_profile_get_filename();
        int pid = getpid();
        if (access(profilePath, F_OK))
            WTFLogAlways("Process(%d) failed to create LLVM profile at %s.", pid, profilePath);
        else
            WTFLogAlways("Process(%d) created LLVM profile at %s.", pid, profilePath);
    });
}

} // namespace WTF

#endif // ENABLE(LLVM_PROFILE_GENERATION) && PLATFORM(IOS_FAMILY)
