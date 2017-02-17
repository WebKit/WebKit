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

#pragma once

#import <wtf/spi/darwin/dyldSPI.h>

namespace WebKit {

/*
    Version numbers are based on the 'current library version' specified in the WebKit build rules.
    All of these methods return or take version numbers with each part shifted to the left 2 bytes.
    For example the version 1.2.3 is returned as 0x00010203 and version 200.3.5 is returned as 0x00C80305
    A version of -1 is returned if the main executable did not link against WebKit.

    Use the current WebKit version number, available in WebKit2/Configurations/Version.xcconfig,
    when adding a new version constant.
*/

struct FirstWebKitWithNetworkCache {
    static const int LibraryVersion { 0x02590116 }; // 601.1.22
#if PLATFORM(IOS)
    static const uint32_t SDKVersion { DYLD_IOS_VERSION_9_0 };
#elif PLATFORM(MAC)
    static const uint32_t SDKVersion { DYLD_MACOSX_VERSION_10_11 };
#endif
};

struct FirstWebKitWithMediaTypesRequiringUserActionForPlayback {
    static const int LibraryVersion { 0x025A0121 }; // 602.1.33
#if PLATFORM(IOS)
    static const uint32_t SDKVersion { DYLD_IOS_VERSION_10_0 };
#elif PLATFORM(MAC)
    static const uint32_t SDKVersion { DYLD_MACOSX_VERSION_10_12 };
#endif
};

struct FirstWebKitWithExceptionsForDuplicateCompletionHandlerCalls {
    static const int LibraryVersion { 0x025B0111 }; // 603.1.17
#if PLATFORM(IOS)
    static const uint32_t SDKVersion { DYLD_IOS_VERSION_10_3 };
#elif PLATFORM(MAC)
    static const uint32_t SDKVersion { DYLD_MACOSX_VERSION_10_12_4 };
#endif
};

bool linkedOnOrAfter(int libraryVersion, uint32_t sdkVersion);
template <typename VersionInfo> bool linkedOnOrAfter()
{
    return linkedOnOrAfter(VersionInfo::LibraryVersion, VersionInfo::SDKVersion);
}

}
