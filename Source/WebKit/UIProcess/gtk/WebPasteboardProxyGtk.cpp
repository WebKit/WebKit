/*
 * Copyright (C) 2016 Red Hat Inc.
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

#include "Clipboard.h"
#include "Connection.h"
#include "SharedBufferReference.h"
#include "WebFrameProxy.h"
#include <WebCore/Pasteboard.h>
#include <WebCore/PasteboardCustomData.h>
#include <WebCore/PasteboardItemInfo.h>
#include <WebCore/PlatformPasteboard.h>
#include <WebCore/SelectionData.h>
#include <wtf/ListHashSet.h>
#include <wtf/SetForScope.h>

namespace WebKit {
using namespace WebCore;

void WebPasteboardProxy::getTypes(StringImpl* pasteboardName, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).formats(WTF::move(completionHandler));
}

void WebPasteboardProxy::readText(IPC::Connection& connection, StringImpl* pasteboardName, StringImpl* pasteboardType, CompletionHandler<void(RefPtr<StringImpl>&&)>&& completionHandler)
{
    if (startsWith(pasteboardType, "text/plain"_s)) {
        Clipboard::get(pasteboardName).readText(WTF::move(completionHandler), connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
        return;
    }

    Clipboard::get(pasteboardName).readBuffer(utf8(pasteboardType), [completionHandler = WTF::move(completionHandler)](auto&& buffer) mutable {
        completionHandler(StringImpl::fromUTF8(buffer->span()));
    }, connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::readFilePaths(IPC::Connection& connection, StringImpl* pasteboardName, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).readFilePaths(WTF::move(completionHandler), connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::readBuffer(IPC::Connection& connection, StringImpl* pasteboardName, StringImpl* pasteboardType, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).readBuffer(utf8(pasteboardType), [completionHandler = WTF::move(completionHandler)](auto&& buffer) mutable {
        completionHandler(WTF::move(buffer));
    }, connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::writeToClipboard(StringImpl* pasteboardName, SelectionData&& selectionData)
{
    Clipboard::get(pasteboardName).write(WTF::move(selectionData), [](int64_t) { });
}

void WebPasteboardProxy::clearClipboard(StringImpl* pasteboardName)
{
    Clipboard::get(pasteboardName).clear();
}

void WebPasteboardProxy::setPrimarySelectionOwner(WebFrameProxy* frame)
{
    if (m_primarySelectionOwner == frame)
        return;

    if (m_primarySelectionOwner)
        m_primarySelectionOwner->collapseSelection();

    m_primarySelectionOwner = frame;
}

void WebPasteboardProxy::didDestroyFrame(WebFrameProxy* frame)
{
    if (frame == m_primarySelectionOwner)
        m_primarySelectionOwner = nullptr;
}

void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(IPC::Connection& connection, StringImpl* pasteboardName, StringImpl* origin, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&& completionHandler)
{
    auto& clipboard = Clipboard::get(pasteboardName);
    clipboard.readBuffer(PasteboardCustomData::gtkType(), [&clipboard, origin = RefPtr { origin }, completionHandler = WTF::move(completionHandler)](auto&& buffer) mutable {
        ListHashSet<RefPtr<StringImpl>> domTypes;
        auto customData = PasteboardCustomData::fromSharedBuffer(WTF::move(buffer));
        if (equal(customData.origin(), origin)) {
            for (auto& type : customData.orderedTypes())
                domTypes.add(type);
        }

        clipboard.formats([domTypes = WTF::move(domTypes), completionHandler = WTF::move(completionHandler)](Vector<RefPtr<StringImpl>>&& formats) mutable {
            for (const auto& format : formats) {
                if (equal(format, PasteboardCustomData::gtkType()))
                    continue;

                if (Pasteboard::isSafeTypeForDOMToReadAndWrite(format))
                    domTypes.add(format);
            }

            completionHandler(copyToVector(domTypes));
        });
    }, connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::writeCustomData(IPC::Connection&, const Vector<PasteboardCustomData>& data, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&& completionHandler)
{
    auto& clipboard = Clipboard::get(pasteboardName);
    if (data.isEmpty() || data.size() > 1) {
        // We don't support more than one custom item in the clipboard.
        completionHandler(clipboard.changeCount());
        return;
    }

    SelectionData selectionData;
    const auto& customData = data[0];
    customData.forEachPlatformStringOrBuffer([&selectionData] (auto* type, auto& stringOrBuffer) {
        if (std::holds_alternative<Ref<SharedBuffer>>(stringOrBuffer)) {
            selectionData.addBuffer(type, std::get<Ref<SharedBuffer>>(stringOrBuffer));
        } else if (std::holds_alternative<RefPtr<StringImpl>>(stringOrBuffer)) {
            if (equal(type, "text/plain"_s))
                selectionData.setText(std::get<RefPtr<StringImpl>>(stringOrBuffer));
            else if (equal(type, "text/html"_s))
                selectionData.setMarkup(std::get<RefPtr<StringImpl>>(stringOrBuffer));
            else if (equal(type, "text/uri-list"_s))
                selectionData.setURIList(std::get<RefPtr<StringImpl>>(stringOrBuffer));
            else if (equal(type, "image/svg+xml"_s))
                selectionData.addBuffer(type, SharedBuffer::create(utf8(std::get<RefPtr<StringImpl>>(stringOrBuffer)).span()));
        }
    });

    if (customData.hasSameOriginCustomData() || !isEmpty(customData.origin()))
        selectionData.setCustomData(customData.createSharedBuffer());

    clipboard.write(WTF::move(selectionData), WTF::move(completionHandler));
}

static WebCore::PasteboardItemInfo pasteboardItemInfoFromFormats(Vector<RefPtr<StringImpl>>&& formats)
{
    WebCore::PasteboardItemInfo info;
    auto contains = [&](ASCIILiteral type) {
        return formats.containsIf([&](auto& format) { return equal(format, type); });
    };
    if (contains("text/plain"_s) || contains("text/plain;charset=utf-8"_s))
        info.webSafeTypesByFidelity.append("text/plain"_s);
    if (contains("text/html"_s))
        info.webSafeTypesByFidelity.append("text/html"_s);
    if (contains("text/uri-list"_s))
        info.webSafeTypesByFidelity.append("text/uri-list"_s);
    if (contains("image/png"_s))
        info.webSafeTypesByFidelity.append("image/png"_s);
    if (contains("image/svg+xml"_s))
        info.webSafeTypesByFidelity.append("image/svg+xml"_s);
    info.platformTypesByFidelity = WTF::move(formats);
    return info;
}

void WebPasteboardProxy::allPasteboardItemInfo(IPC::Connection&, StringImpl* pasteboardName, int64_t changeCount, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(std::optional<Vector<WebCore::PasteboardItemInfo>>&&)>&& completionHandler)
{
    auto& clipboard = Clipboard::get(pasteboardName);
    if (changeCount != clipboard.changeCount()) {
        completionHandler(std::nullopt);
        return;
    }

    clipboard.formats([completionHandler = WTF::move(completionHandler)](Vector<RefPtr<StringImpl>>&& formats) mutable {
        completionHandler(Vector<WebCore::PasteboardItemInfo> { pasteboardItemInfoFromFormats(WTF::move(formats)) });
    });
}

void WebPasteboardProxy::informationForItemAtIndex(IPC::Connection&, uint64_t index, StringImpl* pasteboardName, int64_t changeCount, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(std::optional<WebCore::PasteboardItemInfo>&&)>&& completionHandler)
{
    auto& clipboard = Clipboard::get(pasteboardName);
    if (changeCount != clipboard.changeCount() || index) {
        completionHandler(std::nullopt);
        return;
    }

    clipboard.formats([completionHandler = WTF::move(completionHandler)](Vector<RefPtr<StringImpl>>&& formats) mutable {
        completionHandler(pasteboardItemInfoFromFormats(WTF::move(formats)));
    });
}

void WebPasteboardProxy::getPasteboardItemsCount(IPC::Connection&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    Clipboard::get(pasteboardName).formats([completionHandler = WTF::move(completionHandler)](Vector<RefPtr<StringImpl>>&& formats) mutable {
        completionHandler(formats.isEmpty() ? 0 : 1);
    });
}

void WebPasteboardProxy::readURLFromPasteboard(IPC::Connection& connection, uint64_t index, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(RefPtr<StringImpl>&& url, RefPtr<StringImpl>&& title)>&& completionHandler)
{
    if (index) {
        completionHandler({ }, { });
        return;
    }

    Clipboard::get(pasteboardName).readURL(WTF::move(completionHandler), connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::readBufferFromPasteboard(IPC::Connection& connection, std::optional<uint64_t> index, StringImpl* pasteboardType, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    if (index && index.value()) {
        completionHandler({ });
        return;
    }

    Clipboard::get(pasteboardName).readBuffer(utf8(pasteboardType), [completionHandler = WTF::move(completionHandler)](auto&& buffer) mutable {
        completionHandler(WTF::move(buffer));
    }, connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::getPasteboardChangeCount(IPC::Connection&, StringImpl* pasteboardName, CompletionHandler<void(int64_t)>&& completionHandler)
{
    completionHandler(Clipboard::get(pasteboardName).changeCount());
}

} // namespace WebKit
