/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "WebChromeClient.h"

#if PLATFORM(COCOA)

#import "WebIconUtilities.h"
#import "WebPage.h"
#import <WebCore/AXObjectCache.h>
#import <WebCore/Icon.h>

#if PLATFORM(MAC)

#import "ApplicationServicesSPI.h"

extern "C" AXError _AXUIElementNotifyProcessSuspendStatus(AXSuspendStatus);

#endif // PLATFORM(MAC)

namespace WebKit {
using namespace WebCore;

RefPtr<Icon> WebChromeClient::createIconForFiles(const Vector<String>& filenames)
{
    return Icon::create(iconForFiles(filenames).get());
}

void AXRelayProcessSuspendedNotification::sendProcessSuspendMessage(bool suspended)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;

#if PLATFORM(MAC)
    _AXUIElementNotifyProcessSuspendStatus(suspended ? AXSuspendStatusSuspended : AXSuspendStatusRunning);
#else
    NSDictionary *message = @{ @"pid" : @(getpid()), @"suspended" : @(suspended) };
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:message requiringSecureCoding:YES error:nil];
    protect(m_page)->relayAccessibilityNotification("AXProcessSuspended"_s, data);
#endif
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
