/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#import "VersionChecks.h"

#import <mach-o/dyld.h>

#if PLATFORM(IOS)
#import <WebCore/RuntimeApplicationChecks.h>
#endif

namespace WebKit {

bool linkedOnOrAfter(int libraryVersion, uint32_t sdkVersion)
{
#if PLATFORM(IOS)
    // Always make new features available for Safari.
    if (WebCore::IOSApplication::isMobileSafari())
        return true;
#endif

    // If the app was build against a new enough SDK, it definitely passes the linked-on-or-after check.
    if (dyld_get_program_sdk_version() >= sdkVersion)
        return true;

    // If the app was built against an older SDK, we might still consider it linked-on-or-after
    // by checking the linked WebKit library version number, if one exists.

    int linkedVersion = NSVersionOfLinkTimeLibrary("WebKit");
    return linkedVersion == -1 ? false : linkedVersion >= libraryVersion;
}

}
