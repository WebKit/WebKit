/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include <wtf/MemoryFootprint.h>

#if OS(LINUX)
#include <array>
#include <fcntl.h>
#include <mutex>
#include <unistd.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringToIntegerConversion.h>
#elif OS(FREEBSD)
#include <array>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <wtf/PageBlock.h>
#endif

namespace WTF {

#if OS(LINUX)
struct LinuxMemory {
    static const LinuxMemory& singleton()
    {
        static LinuxMemory s_singleton;
        static std::once_flag s_onceFlag;
        std::call_once(s_onceFlag,
            [] {
                s_singleton.pageSize = sysconf(_SC_PAGE_SIZE);
                s_singleton.statmFd = open("/proc/self/statm", O_RDONLY | O_CLOEXEC);
            });
        return s_singleton;
    }

    size_t footprint() const
    {
        if (statmFd == -1)
            return 0;

        std::array<char, 256> statmBuffer;
        ssize_t numBytes = pread(statmFd, statmBuffer.data(), statmBuffer.size(), 0);
        if (numBytes <= 0)
            return 0;

        auto parsingBuffer = spanReinterpretCast<const Latin1Character>(unsafeMakeSpan(statmBuffer.data(), numBytes));
        skipUntil<isASCIIWhitespace>(parsingBuffer);
        if (parsingBuffer.size() && isASCIIWhitespace(parsingBuffer[0])) {
            auto result = checkedProduct<size_t>(pageSize, parseInteger<size_t>(parsingBuffer).value_or(0));
            if (!result.hasOverflowed()) [[likely]]
                return result.value();
        }
        return 0;
    }

    long pageSize { 0 };
    int statmFd { -1 };
};
#endif

size_t memoryFootprint()
{
#if OS(LINUX)
    return LinuxMemory::singleton().footprint();
#elif OS(FREEBSD)
    struct kinfo_proc info;
    size_t infolen = sizeof(info);

    std::array<int, 4> mib { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };

    if (!sysctl(mib.data(), mib.size(), &info, &infolen, nullptr, 0))
        return static_cast<size_t>(info.ki_rssize) * pageSize();
    return 0;
#else
    return 0;
#endif
}

} // namespace WTF
