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

extern "C" int __llvm_profile_write_file(void);
extern "C" void __llvm_profile_set_filename(const char*);
extern "C" void __llvm_profile_reset_counters(void);

#endif

namespace WTF {

template<typename OnceFlagDiversifier>
ALWAYS_INLINE void registerProfileGenerationCallback(const char* name)
{
#if ENABLE(LLVM_PROFILE_GENERATION)
    constexpr int WRITE_SUCCESS = 0;

    static int profileCount = 0;
    static NeverDestroyed<String> profileFileBase;
    static NeverDestroyed<String> profileFileName;
    static std::once_flag registerFlag;
    std::call_once(registerFlag, [name] {
        int pid = getpid();
        WTFLogAlways("<WEBKIT_LLVM_PROFILE><%s><%d>: Registering callback for profile data.", name, pid);
        WTFLogAlways("<WEBKIT_LLVM_PROFILE> To collect a profile: `notifyutil -p com.apple.WebKit.profiledata`");
        WTFLogAlways("<WEBKIT_LLVM_PROFILE> To copy the output: `"
            R"HERE(log stream --style json --color none | perl -mFile::Basename -mFile::Copy -nle 'if (m/<WEBKIT_LLVM_PROFILE>.*<BEGIN>(.*)<END>/) { (my $l = $1) =~ s/\\\//\//g; my $b = File::Basename::basename($l); my $d = "./profiles/$b"; print "Moving $l to $d"; File::Copy::move($l, $d); }')HERE"
            "`.");
        WTFLogAlways("<WEBKIT_LLVM_PROFILE> To sanity-check the output: `for f in ./profiles/*; do echo $f; xcrun -sdk macosx.internal llvm-profdata show $f; done;`.");

        WTFLogAlways("<WEBKIT_LLVM_PROFILE><%s><%d>: We will dump the resulting profile to %s.", name, pid, profileFileBase->utf8().data());

        int token;
        notify_register_dispatch("com.apple.WebKit.profiledata", &token, dispatch_get_main_queue(), ^(int) {
            
            {
                // Maybe we could use %t instead here, but this folder is permitted through the sandbox because of ANGLE.
                FileSystem::PlatformFileHandle fileHandle;
                auto filePath = FileSystem::openTemporaryFile(makeString(name, "-", getpid()), fileHandle, ".profraw"_s);
                profileFileBase.get() = String::fromUTF8(filePath.utf8().data());
                FileSystem::closeFile(fileHandle);
            }
            
            profileFileName.get() = makeString(profileFileBase.get(), ".", profileCount++, ".profraw");
            __llvm_profile_set_filename(profileFileName->utf8().data()); // Must stay alive while it is used by llvm.

            WTFLogAlways("<WEBKIT_LLVM_PROFILE><%s><%d><%lf>: About to write to %s.", 
                name, pid, MonotonicTime::now().secondsSinceEpoch().milliseconds(), profileFileName->utf8().data());

            int writeResult = __llvm_profile_write_file();
            if (writeResult == WRITE_SUCCESS) {
                WTFLogAlways("<WEBKIT_LLVM_PROFILE><%s><%d><%lf>: Wrote to file <BEGIN>%s<END>.", 
                    name, pid, MonotonicTime::now().secondsSinceEpoch().milliseconds(), profileFileName->utf8().data());
            } else {
                WTFLogAlways("<WEBKIT_LLVM_PROFILE><%s><%d><%lf>: Error writing profile file %s.", 
                    name, pid, MonotonicTime::now().secondsSinceEpoch().milliseconds(), profileFileName->utf8().data());
            }

            __llvm_profile_reset_counters();
        });
    });
#else
    UNUSED_PARAM(name);
#endif
}

} // namespace WTF
