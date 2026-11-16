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

#import <array>
#import <sys/param.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

bool currentProcessIsSandboxed()
{
    return sandbox_check(getpid(), nullptr, SANDBOX_FILTER_NONE);
}

bool connectedProcessIsSandboxed(xpc_connection_t connectionToParent)
{
    audit_token_t token;
    xpc_connection_get_audit_token(connectionToParent, &token);
    return sandbox_check_by_audit_token(token, nullptr, SANDBOX_FILTER_NONE);
}

bool processHasContainer()
{
    static bool hasContainer = !pathForProcessContainer().isEmpty();
    return hasContainer;
}

String pathForProcessContainer()
{
    std::array<char, MAXPATHLEN> path;
    path[0] = 0;
    sandbox_container_path_for_pid(getpid(), path.data(), path.size());

    return String::fromUTF8(path.data());
}

}
