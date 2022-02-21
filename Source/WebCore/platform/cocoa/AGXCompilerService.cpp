/*
* Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "AGXCompilerService.h"

#if PLATFORM(IOS_FAMILY)

#include <sys/utsname.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

static std::optional<bool> hasAGXCompilerService;

void setDeviceHasAGXCompilerServiceForTesting()
{
    hasAGXCompilerService = true;
}

bool deviceHasAGXCompilerService()
{
    if (!hasAGXCompilerService) {
        struct utsname systemInfo;
        if (uname(&systemInfo)) {
            hasAGXCompilerService = false;
            return *hasAGXCompilerService;
        }
        const char* machine = systemInfo.machine;
        if (!strcmp(machine, "iPad5,1") || !strcmp(machine, "iPad5,2") || !strcmp(machine, "iPad5,3") || !strcmp(machine, "iPad5,4"))
            hasAGXCompilerService = true;
        else
            hasAGXCompilerService = false;
    }
    return *hasAGXCompilerService;
}

Span<const ASCIILiteral> agxCompilerServices()
{
    static constexpr std::array services {
        "com.apple.AGXCompilerService"_s,
        "com.apple.AGXCompilerService-S2A8"_s
    };
    return services;
}

Span<const ASCIILiteral> agxCompilerClasses()
{
    static constexpr std::array classes {
        "AGXCommandQueue"_s,
        "AGXDevice"_s,
        "AGXSharedUserClient"_s,
        "IOAccelContext"_s,
        "IOAccelContext2"_s,
        "IOAccelDevice"_s,
        "IOAccelDevice2"_s,
        "IOAccelSharedUserClient"_s,
        "IOAccelSharedUserClient2"_s,
        "IOAccelSubmitter2"_s,
    };
    return classes;
}

}

#endif
