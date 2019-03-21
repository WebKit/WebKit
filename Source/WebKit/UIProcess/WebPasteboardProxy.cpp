/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebPasteboardProxy.h"

#include "WebPasteboardProxyMessages.h"
#include "WebProcessProxy.h"
#include <mutex>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

WebPasteboardProxy& WebPasteboardProxy::singleton()
{
    static std::once_flag onceFlag;
    static LazyNeverDestroyed<WebPasteboardProxy> proxy;

    std::call_once(onceFlag, [] {
        proxy.construct();
    });

    return proxy;
}

WebPasteboardProxy::WebPasteboardProxy()
{
}

void WebPasteboardProxy::addWebProcessProxy(WebProcessProxy& webProcessProxy)
{
    // FIXME: Can we handle all of these on a background queue?
    webProcessProxy.addMessageReceiver(Messages::WebPasteboardProxy::messageReceiverName(), *this);
    m_webProcessProxyList.add(&webProcessProxy);
}
    
void WebPasteboardProxy::removeWebProcessProxy(WebProcessProxy& webProcessProxy)
{
    m_webProcessProxyList.remove(&webProcessProxy);
}

#if !PLATFORM(COCOA)

void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(const String&, const String&, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler({ });
}

void WebPasteboardProxy::writeCustomData(const WebCore::PasteboardCustomData&, const String&, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(0);
}

#endif

} // namespace WebKit
