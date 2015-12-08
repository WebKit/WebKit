/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "SandboxUtilities.h"

#import <WebCore/SecuritySPI.h>
#import <array>
#import <sys/param.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

bool processIsSandboxed(pid_t pid)
{
    return sandbox_check(pid, nullptr, SANDBOX_FILTER_NONE);
}

static bool processHasContainer(pid_t pid)
{
    std::array<char, MAXPATHLEN> path;

    if (sandbox_container_path_for_pid(pid, path.data(), path.size()))
        return false;

    if (!path[0])
        return false;

    return true;
}

bool processHasContainer()
{
    static bool hasContainer = processHasContainer(getpid());

    return hasContainer;
}

String pathForProcessContainer()
{
    std::array<char, MAXPATHLEN> path;
    path[0] = 0;
    sandbox_container_path_for_pid(getpid(), path.data(), path.size());

    return String::fromUTF8(path.data());
}

bool processHasEntitlement(NSString *entitlement)
{
    auto task = adoptCF(SecTaskCreateFromSelf(CFAllocatorGetDefault()));
    if (!task)
        return false;

    auto value = adoptCF(SecTaskCopyValueForEntitlement(task.get(), (__bridge CFStringRef)entitlement, nullptr));
    if (!value)
        return false;

    if (CFGetTypeID(value.get()) != CFBooleanGetTypeID())
        return false;

    return CFBooleanGetValue(static_cast<CFBooleanRef>(value.get()));
}

}
