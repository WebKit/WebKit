/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
#import "WebProcessPool.h"
#import "WebProcessProxy.h"

#if PLATFORM(MAC)

#import "CodeSigning.h"
#import "WKFullKeyboardAccessWatcher.h"
#import <signal.h>
#import <wtf/ProcessPrivilege.h>

namespace WebKit {

bool WebProcessProxy::fullKeyboardAccessEnabled()
{
    return [WKFullKeyboardAccessWatcher fullKeyboardAccessEnabled];
}

bool WebProcessProxy::shouldAllowNonValidInjectedCode() const
{
    static bool isSystemWebKit = [] {
        NSBundle *webkit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];
        return [webkit2Bundle.bundlePath hasPrefix:@"/System/"];
    }();

    if (!isSystemWebKit)
        return false;

    static bool isPlatformBinary = currentProcessIsPlatformBinary();
    if (isPlatformBinary)
        return false;

    const String& path = m_processPool->configuration().injectedBundlePath();
    return !path.isEmpty() && !path.startsWith("/System/"_s);
}

void WebProcessProxy::platformSuspendProcess()
{
    m_platformSuspendDidReleaseNearSuspendedAssertion = throttler().isHoldingNearSuspendedAssertion();
    throttler().setShouldTakeNearSuspendedAssertion(false);
}

void WebProcessProxy::platformResumeProcess()
{
    if (m_platformSuspendDidReleaseNearSuspendedAssertion) {
        m_platformSuspendDidReleaseNearSuspendedAssertion = false;
        throttler().setShouldTakeNearSuspendedAssertion(true);
    }
}

} // namespace WebKit

#endif // PLATFORM(MAC)
