/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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

#if ENABLE(LLVM_PROFILE_GENERATION)
#include "FileSystem.h"
#include "MonotonicTime.h"
#include "text/WTFString.h"
#include <notify.h>
#include <unistd.h>

extern "C" uint64_t __llvm_profile_get_size_for_buffer();
extern "C" int __llvm_profile_write_buffer(char *);
extern "C" void __llvm_profile_reset_counters(void);

#endif

namespace WTF {

template<typename OnceFlagDiversifier>
ALWAYS_INLINE void registerProfileGenerationCallback(const char* name)
{
#if ENABLE(LLVM_PROFILE_GENERATION)
    static std::once_flag registerFlag;
    std::call_once(registerFlag, [name] {
        WTFLogAlways("<WEBKIT_LLVM_PROFILE><%s><%d><0>: Registering callback for profile data.", name, getpid());
        WTFLogAlways("<WEBKIT_LLVM_PROFILE> To collect a profile: `notifyutil -p com.apple.WebKit.profiledata`");
        WTFLogAlways("<WEBKIT_LLVM_PROFILE> To copy the output: `"
            R"HERE(log stream --style json --color none | perl -mFile::Basename -mFile::Copy -nle 'if (m/<WEBKIT_LLVM_PROFILE>.*<BEGIN>(.*)<END>/) { (my $l = $1) =~ s/\\\//\//g; my $b = File::Basename::basename($l); my $d = "./profiles/$b"; print "Moving $l to $d"; File::Copy::move($l, $d); }')HERE"
            "`.");
        WTFLogAlways("<WEBKIT_LLVM_PROFILE> To sanity-check the output: `for f in ./profiles/*; do echo $f; xcrun -sdk macosx.internal llvm-profdata show $f; done;`.");
        int token;
        notify_register_dispatch("com.apple.WebKit.profiledata", &token, dispatch_get_main_queue(), ^(int) {
            int pid = getpid();
            int64_t time = MonotonicTime::now().secondsSinceEpoch().milliseconds();

            auto bufferSize = __llvm_profile_get_size_for_buffer();
            WTFLogAlways("<WEBKIT_LLVM_PROFILE><%s><%d><%lld>: LLVM collected %llu bytes of profile data.", 
                name, pid, time, bufferSize);

            auto* buffer = static_cast<char*>(calloc(sizeof(char), bufferSize));
            __llvm_profile_write_buffer(buffer);

            String fileName(String::fromUTF8(name) + "-" + pid + "-" + time + "-");

            FileSystem::PlatformFileHandle fileHandle;
            auto filePath = FileSystem::openTemporaryFile(fileName, fileHandle, ".profraw");
            size_t bytesWritten = FileSystem::writeToFile(fileHandle, reinterpret_cast<const void*>(buffer), bufferSize);

            WTFLogAlways("<WEBKIT_LLVM_PROFILE><%s><%d><%lld>: Wrote %zu bytes to file <BEGIN>%s<END>.", 
                name, pid, time, bytesWritten, filePath.utf8().data());

            FileSystem::closeFile(fileHandle);
            free(buffer);
            __llvm_profile_reset_counters();
        });
    });
#else
    UNUSED_PARAM(name);
#endif
}

} // namespace WTF
