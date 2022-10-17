/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)
#import "WebScreenOrientationManagerProxy.h"

#import "WKWebView.h"
#import "WebPageProxy.h"
#import <WebCore/Exception.h>

namespace WebKit {

void WebScreenOrientationManagerProxy::platformInitialize()
{
    m_page.addDidMoveToWindowObserver(*this);
    setWindow([m_page.cocoaView() window]);
}

void WebScreenOrientationManagerProxy::platformDestroy()
{
    m_page.removeDidMoveToWindowObserver(*this);
}

void WebScreenOrientationManagerProxy::setWindow(UIWindow *window)
{
    m_provider->setWindow(window);
}

void WebScreenOrientationManagerProxy::webViewDidMoveToWindow()
{
    setWindow([m_page.cocoaView() window]);
}

std::optional<WebCore::Exception> WebScreenOrientationManagerProxy::platformShouldRejectLockRequest() const
{
    if (UIApplication.sharedApplication.supportsMultipleScenes)
        return WebCore::Exception { WebCore::NotSupportedError, "Apps supporting multiple scenes (multitask) cannot lock their orientation"_s };
    return std::nullopt;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
