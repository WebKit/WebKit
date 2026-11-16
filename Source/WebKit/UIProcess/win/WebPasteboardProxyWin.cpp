/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(WIN)

#include "Connection.h"
#include <WebCore/Pasteboard.h>
#include <WebCore/PasteboardCustomData.h>
#include <WebCore/PasteboardItemInfo.h>
#include <WebCore/PlatformPasteboard.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {
using namespace WebCore;

// The pasteboard name is unused because Windows has a single system clipboard.

void WebPasteboardProxy::getPasteboardChangeCount(IPC::Connection&, const String& pasteboardName, CompletionHandler<void(int64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).changeCount());
}

void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(IPC::Connection&, const String& pasteboardName, const String& origin, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).typesSafeForDOMToReadAndWrite(origin));
}

void WebPasteboardProxy::writeCustomData(IPC::Connection&, const Vector<PasteboardCustomData>& data, const String& pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).write(data));
}

void WebPasteboardProxy::allPasteboardItemInfo(IPC::Connection&, const String& pasteboardName, int64_t changeCount, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(std::optional<Vector<PasteboardItemInfo>>&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).allPasteboardItemInfo(changeCount));
}

void WebPasteboardProxy::informationForItemAtIndex(IPC::Connection&, uint64_t index, const String& pasteboardName, int64_t changeCount, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(std::optional<PasteboardItemInfo>&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).informationForItemAtIndex(index, changeCount));
}

void WebPasteboardProxy::getPasteboardItemsCount(IPC::Connection&, const String& pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).count());
}

void WebPasteboardProxy::readStringFromPasteboard(IPC::Connection&, uint64_t index, const String& pasteboardType, const String& pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(String&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).readString(index, pasteboardType));
}

void WebPasteboardProxy::readURLFromPasteboard(IPC::Connection&, uint64_t, const String&, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(String&& url, String&& title)>&& completionHandler)
{
    completionHandler({ }, { });
}

void WebPasteboardProxy::readBufferFromPasteboard(IPC::Connection&, std::optional<uint64_t>, const String&, const String&, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
    completionHandler({ });
}

} // namespace WebKit

#endif // PLATFORM(WIN)
