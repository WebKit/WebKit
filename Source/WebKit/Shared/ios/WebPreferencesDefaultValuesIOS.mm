/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#import "WebPreferencesDefaultValuesIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "UserInterfaceIdiom.h"
#import <WebCore/Device.h>
#import <pal/ios/ManagedConfigurationSoftLink.h>
#import <pal/spi/ios/ManagedConfigurationSPI.h>

namespace WebKit {

#if ENABLE(TEXT_AUTOSIZING)

bool defaultTextAutosizingUsesIdempotentMode()
{
    return !currentUserInterfaceIdiomIsSmallScreen();
}

#endif

#if !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
static std::optional<bool>& cachedAllowsRequest()
{
    static NeverDestroyed<std::optional<bool>> allowsRequest;
    return allowsRequest;
}

bool allowsDeprecatedSynchronousXMLHttpRequestDuringUnload()
{
    if (!cachedAllowsRequest())
        cachedAllowsRequest() = [[PAL::getMCProfileConnectionClass() sharedConnection] effectiveBoolValueForSetting:@"allowDeprecatedWebKitSynchronousXHRLoads"] == MCRestrictedBoolExplicitYes;
    return *cachedAllowsRequest();
}

void setAllowsDeprecatedSynchronousXMLHttpRequestDuringUnload(bool allowsRequest)
{
    cachedAllowsRequest() = allowsRequest;
}
#endif

#if ENABLE(MEDIA_SOURCE)

bool defaultMediaSourceEnabled()
{
    return !WebCore::deviceClassIsSmallScreen();
}

#endif

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
