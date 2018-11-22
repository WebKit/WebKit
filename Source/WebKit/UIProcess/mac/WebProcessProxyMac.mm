/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#import "WKFullKeyboardAccessWatcher.h"
#import <wtf/ProcessPrivilege.h>

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
#import <Kernel/kern/cs_blobs.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#endif

namespace WebKit {

bool WebProcessProxy::fullKeyboardAccessEnabled()
{
    return [WKFullKeyboardAccessWatcher fullKeyboardAccessEnabled];
}

bool WebProcessProxy::shouldAllowNonValidInjectedCode() const
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    static bool isSystemWebKit = [] {
#if WK_API_ENABLED
        NSBundle *webkit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];
#else
        NSBundle *webkit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKView")];
#endif
        return [webkit2Bundle.bundlePath hasPrefix:@"/System/"];
    }();

    if (!isSystemWebKit)
        return false;

    static bool isPlatformBinary = SecTaskGetCodeSignStatus(adoptCF(SecTaskCreateFromSelf(kCFAllocatorDefault)).get()) & CS_PLATFORM_BINARY;
    if (isPlatformBinary)
        return false;

    const String& path = m_processPool->configuration().injectedBundlePath();
    return !path.isEmpty() && !path.startsWith("/System/");
#else
    return false;
#endif
}

#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
void WebProcessProxy::startDisplayLink(unsigned observerID, uint32_t displayID)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    for (auto& displayLink : m_displayLinks) {
        if (displayLink->displayID() == displayID) {
            displayLink->addObserver(observerID);
            return;
        }
    }
    auto displayLink = std::make_unique<DisplayLink>(displayID, *this);
    displayLink->addObserver(observerID);
    m_displayLinks.append(WTFMove(displayLink));
}

void WebProcessProxy::stopDisplayLink(unsigned observerID, uint32_t displayID)
{
    size_t pos = 0;
    for (auto& displayLink : m_displayLinks) {
        if (displayLink->displayID() == displayID) {
            displayLink->removeObserver(observerID);
            if (!displayLink->hasObservers())
                m_displayLinks.remove(pos);
            return;
        }
        pos++;
    }
}
#endif

} // namespace WebKit

#endif // PLATFORM(MAC)
