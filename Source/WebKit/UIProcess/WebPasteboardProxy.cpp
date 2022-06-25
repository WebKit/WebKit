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

#include "SharedMemory.h"
#include "WebPasteboardProxyMessages.h"
#include "WebProcessProxy.h"
#include <mutex>
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>

#if !PLATFORM(COCOA)
#include <WebCore/PasteboardCustomData.h>
#include <WebCore/PasteboardItemInfo.h>
#endif

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
    m_webProcessProxySet.add(webProcessProxy);
}
    
void WebPasteboardProxy::removeWebProcessProxy(WebProcessProxy& webProcessProxy)
{
    m_webProcessProxySet.remove(webProcessProxy);
}

WebProcessProxy* WebPasteboardProxy::webProcessProxyForConnection(IPC::Connection& connection) const
{
    for (auto& webProcessProxy : m_webProcessProxySet) {
        if (webProcessProxy.hasConnection(connection))
            return &webProcessProxy;
    }
    return nullptr;
}

#if !PLATFORM(COCOA)

#if !PLATFORM(GTK)
void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(IPC::Connection&, const String&, const String&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler({ });
}

void WebPasteboardProxy::writeCustomData(IPC::Connection&, const Vector<WebCore::PasteboardCustomData>&, const String&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(int64_t)>&& completionHandler)
{
    completionHandler(0);
}
#endif

void WebPasteboardProxy::allPasteboardItemInfo(IPC::Connection&, const String&, int64_t, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(std::optional<Vector<WebCore::PasteboardItemInfo>>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void WebPasteboardProxy::informationForItemAtIndex(IPC::Connection&, size_t, const String&, int64_t, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(std::optional<WebCore::PasteboardItemInfo>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void WebPasteboardProxy::getPasteboardItemsCount(IPC::Connection&, const String&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(0);
}

void WebPasteboardProxy::readURLFromPasteboard(IPC::Connection&, size_t, const String&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(String&& url, String&& title)>&& completionHandler)
{
    completionHandler({ }, { });
}

void WebPasteboardProxy::readBufferFromPasteboard(IPC::Connection&, std::optional<size_t>, const String&, const String&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    completionHandler({ });
}

#if !USE(LIBWPE)

void WebPasteboardProxy::readStringFromPasteboard(IPC::Connection&, size_t, const String&, const String&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(String&&)>&& completionHandler)
{
    completionHandler({ });
}

#endif // !USE(LIBWPE)

void WebPasteboardProxy::containsStringSafeForDOMToReadForType(IPC::Connection&, const String&, const String&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(false);
}

void WebPasteboardProxy::containsURLStringSuitableForLoading(IPC::Connection&, const String&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(false);
}

void WebPasteboardProxy::urlStringSuitableForLoading(IPC::Connection&, const String&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(String&& url, String&& title)>&& completionHandler)
{
    completionHandler({ }, { });
}

#endif // !PLATFORM(COCOA)

} // namespace WebKit
