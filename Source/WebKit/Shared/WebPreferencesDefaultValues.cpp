/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WebPreferencesDefaultValues.h"
#include <WebCore/RuntimeApplicationChecks.h>

#if PLATFORM(COCOA)
#include <wtf/spi/darwin/dyldSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include "VersionChecks.h"
#endif

bool defaultPassiveTouchListenersAsDefaultOnDocument()
{
#if PLATFORM(IOS_FAMILY)
    return linkedOnOrAfter(WebKit::SDKVersion::FirstThatDefaultsToPassiveTouchListenersOnDocument);
#else
    return true;
#endif
}

bool defaultCustomPasteboardDataEnabled()
{
#if PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110300
    return WebCore::IOSApplication::isMobileSafari() || dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_11_3;
#elif PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300
    return WebCore::MacApplication::isSafari() || dyld_get_program_sdk_version() > DYLD_MACOSX_VERSION_10_13;
#elif PLATFORM(MAC)
    return WebCore::MacApplication::isSafari();
#else
    return false;
#endif
}

