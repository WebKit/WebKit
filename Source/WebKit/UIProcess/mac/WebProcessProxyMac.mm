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

void WebProcessProxy::startDisplayLink(DisplayLinkObserverID observerID, WebCore::PlatformDisplayID displayID, WebCore::FramesPerSecond preferredFramesPerSecond)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    ASSERT(connection());
    processPool().startDisplayLink(*connection(), observerID, displayID, preferredFramesPerSecond);
}

void WebProcessProxy::stopDisplayLink(DisplayLinkObserverID observerID, WebCore::PlatformDisplayID displayID)
{
    ASSERT(connection());
    processPool().stopDisplayLink(*connection(), observerID, displayID);
}

void WebProcessProxy::setDisplayLinkPreferredFramesPerSecond(DisplayLinkObserverID observerID, WebCore::PlatformDisplayID displayID, WebCore::FramesPerSecond preferredFramesPerSecond)
{
    ASSERT(connection());
    processPool().setDisplayLinkPreferredFramesPerSecond(*connection(), observerID, displayID, preferredFramesPerSecond);
}

void WebProcessProxy::platformSuspendProcess()
{
    // FIXME: Adopt RunningBoard on macOS to support process suspension.
}

void WebProcessProxy::platformResumeProcess()
{
    // FIXME: Adopt RunningBoard on macOS to support process suspension.
}

} // namespace WebKit

#endif // PLATFORM(MAC)
