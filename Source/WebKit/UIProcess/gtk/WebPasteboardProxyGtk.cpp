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

void WebPasteboardProxy::getTypes(const String& pasteboardName, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).formats(WTFMove(completionHandler));
}

void WebPasteboardProxy::readText(IPC::Connection& connection, const String& pasteboardName, CompletionHandler<void(String&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).readText(WTFMove(completionHandler), connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::readFilePaths(IPC::Connection& connection, const String& pasteboardName, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).readFilePaths(WTFMove(completionHandler), connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::readBuffer(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).readBuffer(pasteboardType.utf8().data(), [completionHandler = WTFMove(completionHandler)](auto&& buffer) mutable {
        completionHandler(WTFMove(buffer));
    }, connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::writeToClipboard(const String& pasteboardName, SelectionData&& selectionData)
{
    Clipboard::get(pasteboardName).write(WTFMove(selectionData), [](int64_t) { });
}

void WebPasteboardProxy::clearClipboard(const String& pasteboardName)
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

void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(IPC::Connection& connection, const String& pasteboardName, const String& origin, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    auto& clipboard = Clipboard::get(pasteboardName);
    clipboard.readBuffer(PasteboardCustomData::gtkType(), [&clipboard, origin, completionHandler = WTFMove(completionHandler)](auto&& buffer) mutable {
        ListHashSet<String> domTypes;
        auto customData = PasteboardCustomData::fromSharedBuffer(WTFMove(buffer));
        if (customData.origin() == origin) {
            for (auto& type : customData.orderedTypes())
                domTypes.add(type);
        }

        clipboard.formats([domTypes = WTFMove(domTypes), completionHandler = WTFMove(completionHandler)](Vector<String>&& formats) mutable {
            for (const auto& format : formats) {
                if (format == PasteboardCustomData::gtkType())
                    continue;

                if (Pasteboard::isSafeTypeForDOMToReadAndWrite(format))
                    domTypes.add(format);
            }

            completionHandler(copyToVector(domTypes));
        });
    }, connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::writeCustomData(IPC::Connection&, const Vector<PasteboardCustomData>& data, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(int64_t)>&& completionHandler)
{
    auto& clipboard = Clipboard::get(pasteboardName);
    if (data.isEmpty() || data.size() > 1) {
        // We don't support more than one custom item in the clipboard.
        completionHandler(clipboard.changeCount());
        return;
    }

    SelectionData selectionData;
    const auto& customData = data[0];
    customData.forEachPlatformStringOrBuffer([&selectionData] (auto& type, auto& stringOrBuffer) {
        if (std::holds_alternative<Ref<SharedBuffer>>(stringOrBuffer)) {
            selectionData.addBuffer(type, std::get<Ref<SharedBuffer>>(stringOrBuffer));
        } else if (std::holds_alternative<String>(stringOrBuffer)) {
            if (type == "text/plain"_s)
                selectionData.setText(std::get<String>(stringOrBuffer));
            else if (type == "text/html"_s)
                selectionData.setMarkup(std::get<String>(stringOrBuffer));
            else if (type == "text/uri-list"_s)
                selectionData.setURIList(std::get<String>(stringOrBuffer));
        }
    });

    if (customData.hasSameOriginCustomData() || !customData.origin().isEmpty())
        selectionData.setCustomData(customData.createSharedBuffer());

    clipboard.write(WTFMove(selectionData), WTFMove(completionHandler));
}

static WebCore::PasteboardItemInfo pasteboardIemInfoFromFormats(Vector<String>&& formats)
{
    WebCore::PasteboardItemInfo info;
    if (formats.contains("text/plain"_s) || formats.contains("text/plain;charset=utf-8"_s))
        info.webSafeTypesByFidelity.append("text/plain"_s);
    if (formats.contains("text/html"_s))
        info.webSafeTypesByFidelity.append("text/html"_s);
    if (formats.contains("text/uri-list"_s))
        info.webSafeTypesByFidelity.append("text/uri-list"_s);
    if (formats.contains("image/png"_s))
        info.webSafeTypesByFidelity.append("image/png"_s);
    info.platformTypesByFidelity = WTFMove(formats);
    return info;
}

void WebPasteboardProxy::allPasteboardItemInfo(IPC::Connection&, const String& pasteboardName, int64_t changeCount, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(std::optional<Vector<WebCore::PasteboardItemInfo>>&&)>&& completionHandler)
{
    auto& clipboard = Clipboard::get(pasteboardName);
    if (changeCount != clipboard.changeCount()) {
        completionHandler(std::nullopt);
        return;
    }

    clipboard.formats([completionHandler = WTFMove(completionHandler)](Vector<String>&& formats) mutable {
        completionHandler(Vector<WebCore::PasteboardItemInfo> { pasteboardIemInfoFromFormats(WTFMove(formats)) });
    });
}

void WebPasteboardProxy::informationForItemAtIndex(IPC::Connection&, size_t index, const String& pasteboardName, int64_t changeCount, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(std::optional<WebCore::PasteboardItemInfo>&&)>&& completionHandler)
{
    auto& clipboard = Clipboard::get(pasteboardName);
    if (changeCount != clipboard.changeCount() || index) {
        completionHandler(std::nullopt);
        return;
    }

    clipboard.formats([completionHandler = WTFMove(completionHandler)](Vector<String>&& formats) mutable {
        completionHandler(pasteboardIemInfoFromFormats(WTFMove(formats)));
    });
}

void WebPasteboardProxy::getPasteboardItemsCount(IPC::Connection&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    Clipboard::get(pasteboardName).formats([completionHandler = WTFMove(completionHandler)](Vector<String>&& formats) mutable {
        completionHandler(formats.isEmpty() ? 0 : 1);
    });
}

void WebPasteboardProxy::readURLFromPasteboard(IPC::Connection& connection, size_t index, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(String&& url, String&& title)>&& completionHandler)
{
    if (index) {
        completionHandler({ }, { });
        return;
    }

    Clipboard::get(pasteboardName).readURL(WTFMove(completionHandler), connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::readBufferFromPasteboard(IPC::Connection& connection, std::optional<size_t> index, const String& pasteboardType, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    if (index && index.value()) {
        completionHandler({ });
        return;
    }

    Clipboard::get(pasteboardName).readBuffer(pasteboardType.utf8().data(), [completionHandler = WTFMove(completionHandler)](auto&& buffer) mutable {
        completionHandler(WTFMove(buffer));
    }, connection.inDispatchSyncMessageCount() > 1 ? Clipboard::ReadMode::Synchronous : Clipboard::ReadMode::Asynchronous);
}

void WebPasteboardProxy::getPasteboardChangeCount(IPC::Connection&, const String& pasteboardName, CompletionHandler<void(int64_t)>&& completionHandler)
{
    completionHandler(Clipboard::get(pasteboardName).changeCount());
}

} // namespace WebKit
