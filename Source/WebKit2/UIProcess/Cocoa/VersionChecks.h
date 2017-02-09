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

enum class SDKVersion : uint32_t {
#if PLATFORM(IOS)
    FirstWithNetworkCache = DYLD_IOS_VERSION_9_0,
    FirstWithMediaTypesRequiringUserActionForPlayback = DYLD_IOS_VERSION_10_0,
    FirstWithExceptionsForDuplicateCompletionHandlerCalls = DYLD_IOS_VERSION_10_3,
#elif PLATFORM(MAC)
    FirstWithNetworkCache = DYLD_MACOSX_VERSION_10_11,
    FirstWithMediaTypesRequiringUserActionForPlayback = DYLD_MACOSX_VERSION_10_12,
    FirstWithExceptionsForDuplicateCompletionHandlerCalls = DYLD_MACOSX_VERSION_10_12_4,
#endif
};

bool linkedOnOrAfter(SDKVersion);

/*
    During ToT WebKit development is is not always possible to use an SDK version, so we instead use 
    WebKit Library Version numbering.

    This technique has a big drawback; The check will fail if the application doesn't directly link
    against WebKit (e.g., It links to a different framework which links to WebKit).

    Once a new WebKit Library Version is released in an SDK, all linkedOnOrAfter checks should be moved
    from LibraryVersions to SDKVersions.

    Library version numbers are based on the 'current library version' specified in the WebKit build rules.
    All of these methods return or take version numbers with each part shifted to the left 2 bytes.
    For example the version 1.2.3 is returned as 0x00010203 and version 200.3.5 is returned as 0x00C80305
    A version of -1 is returned if the main executable did not link against WebKit.

    Use the current WebKit version number, available in WebKit2/Configurations/Version.xcconfig,
    when adding a new version constant.
*/

enum class LibraryVersion {
};

bool linkedOnOrAfter(LibraryVersion);

}
