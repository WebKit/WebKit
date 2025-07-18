/*
 * Copyright (C) 2025 Igalia S.L.
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

#if PLATFORM(WPE)
#include "Connection.h"
#include <WebCore/Pasteboard.h>
#include <WebCore/PasteboardCustomData.h>
#include <WebCore/PasteboardItemInfo.h>
#include <WebCore/PlatformPasteboard.h>
#include <WebCore/SelectionData.h>
#include <wtf/StdLibExtras.h>
#include <wtf/glib/GUniquePtr.h>

#if ENABLE(WPE_PLATFORM)
#include "GRefPtrWPE.h"
#include "WPEUtilities.h"
#include <wpe/wpe-platform.h>
#endif

namespace WebKit {
using namespace WebCore;

#if ENABLE(WPE_PLATFORM)
static Vector<String> clipboardFormats(WPEClipboard* clipboard)
{
    Vector<String> types;
    if (const auto* formats = wpe_clipboard_get_formats(clipboard)) {
        for (unsigned i = 0; formats[i]; ++i)
            types.append(String::fromUTF8(formats[i]));
    }
    return types;
}
#endif

void WebPasteboardProxy::getTypes(const String&, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        completionHandler(clipboardFormats(clipboard));
        return;
    }
#endif

    Vector<String> pasteboardTypes;
    PlatformPasteboard().getTypes(pasteboardTypes);
    completionHandler(WTFMove(pasteboardTypes));
}

void WebPasteboardProxy::readText(IPC::Connection&, const String&, const String& pasteboardType, CompletionHandler<void(String&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        gsize textLength;
        GUniquePtr<char> text(wpe_clipboard_read_text(clipboard, pasteboardType.utf8().data(), &textLength));
        completionHandler(String::fromUTF8(unsafeMakeSpan(text.get(), textLength)));
        return;
    }
#endif

    completionHandler(PlatformPasteboard().readString(0, pasteboardType.startsWith("text/plain"_s) ? "text/plain;charset=utf-8"_s : pasteboardType));
}

void WebPasteboardProxy::readFilePaths(IPC::Connection&, const String&, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler({ });
}

void WebPasteboardProxy::readBuffer(IPC::Connection&, const String&, const String& pasteboardType, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        if (GRefPtr<GBytes> bytes = adoptGRef(wpe_clipboard_read_bytes(clipboard, pasteboardType.utf8().data()))) {
            completionHandler(FragmentedSharedBuffer::create(bytes.get())->makeContiguous());
            return;
        }
    }
#endif
    completionHandler({ });
}

#if ENABLE(WPE_PLATFORM)
static void setClipboardContentFromSpan(WPEClipboardContent* content, const char* type, const std::span<const char>& text)
{
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(text.data(), text.size()));
    wpe_clipboard_content_set_bytes(content, type, bytes.get());
}
#endif

void WebPasteboardProxy::writeToClipboard(const String&, SelectionData&& selectionData)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        GRefPtr<WPEClipboardContent> content = adoptGRef(wpe_clipboard_content_new());
        if (selectionData.hasText())
            wpe_clipboard_content_set_text(content.get(), selectionData.text().utf8().data());
        if (selectionData.hasMarkup())
            setClipboardContentFromSpan(content.get(), "text/html", selectionData.markup().utf8().span());
        if (selectionData.hasURIList())
            setClipboardContentFromSpan(content.get(), "text/uri-list", selectionData.uriList().utf8().span());

        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        wpe_clipboard_set_content(clipboard, content.get());
        return;
    }
#endif

    PasteboardWebContent contents;
    if (selectionData.hasText())
        contents.text = selectionData.text();
    if (selectionData.hasMarkup())
        contents.markup = selectionData.markup();
    PlatformPasteboard().write(contents);
}

void WebPasteboardProxy::clearClipboard(const String&)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        wpe_clipboard_set_content(clipboard, nullptr);
    }
#endif
}

void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(IPC::Connection&, const String&, const String& origin, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        if (GRefPtr<GBytes> bytes = adoptGRef(wpe_clipboard_read_bytes(clipboard, PasteboardCustomData::wpeType().characters()))) {
            ListHashSet<String> domTypes;
            auto buffer = FragmentedSharedBuffer::create(bytes.get())->makeContiguous();
            auto customData = PasteboardCustomData::fromSharedBuffer(buffer.get());
            if (customData.origin() == origin) {
                for (auto& type : customData.orderedTypes())
                    domTypes.add(type);
            }

            if (const auto* formats = wpe_clipboard_get_formats(clipboard)) {
                for (unsigned i = 0; formats[i]; ++i) {
                    String format = String::fromUTF8(formats[i]);
                    if (format == PasteboardCustomData::wpeType())
                        continue;

                    if (Pasteboard::isSafeTypeForDOMToReadAndWrite(format))
                        domTypes.add(format);
                }
            }
            completionHandler(copyToVector(domTypes));
            return;
        }
    }
#endif
    completionHandler({ });
}

void WebPasteboardProxy::writeCustomData(IPC::Connection&, const Vector<PasteboardCustomData>& data, const String&, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        if (data.isEmpty() || data.size() > 1) {
            // We don't support more than one custom item in the clipboard.
            completionHandler(wpe_clipboard_get_change_count(clipboard));
            return;
        }

        GRefPtr<WPEClipboardContent> content = adoptGRef(wpe_clipboard_content_new());
        const auto& customData = data[0];
        customData.forEachPlatformStringOrBuffer([&content](auto& type, auto& stringOrBuffer) {
            if (std::holds_alternative<Ref<SharedBuffer>>(stringOrBuffer)) {
                auto buffer = std::get<Ref<SharedBuffer>>(stringOrBuffer);
                auto bytes = buffer->createGBytes();
                wpe_clipboard_content_set_bytes(content.get(), type.utf8().data(), bytes.get());
            } else if (std::holds_alternative<String>(stringOrBuffer)) {
                if (type == "text/plain"_s)
                    wpe_clipboard_content_set_text(content.get(), std::get<String>(stringOrBuffer).utf8().data());
                else if (type == "text/html"_s)
                    setClipboardContentFromSpan(content.get(), "text/html", std::get<String>(stringOrBuffer).utf8().span());
                else if (type == "text/uri-list"_s)
                    setClipboardContentFromSpan(content.get(), "text/uri-list", std::get<String>(stringOrBuffer).utf8().span());
            }
        });

        if (customData.hasSameOriginCustomData() || !customData.origin().isEmpty()) {
            auto buffer = customData.createSharedBuffer();
            auto bytes = buffer->createGBytes();
            wpe_clipboard_content_set_bytes(content.get(), PasteboardCustomData::wpeType(), bytes.get());
        }

        wpe_clipboard_set_content(clipboard, content.get());
        completionHandler(wpe_clipboard_get_change_count(clipboard));
        return;
    }
#endif

    completionHandler(PlatformPasteboard().write(data));
}

#if ENABLE(WPE_PLATFORM)
static PasteboardItemInfo pasteboardItemInfoFromFormats(Vector<String>&& formats)
{
    PasteboardItemInfo info;
    if (formats.contains("text/plain"_s) || formats.contains("text/plain;charset=utf-8"_s))
        info.webSafeTypesByFidelity.append("text/plain"_s);
    if (formats.contains("text/html"_s))
        info.webSafeTypesByFidelity.append("text/html"_s);
    if (formats.contains("text/uri-list"_s))
        info.webSafeTypesByFidelity.append("text/uri-list"_s);
    info.platformTypesByFidelity = WTFMove(formats);
    return info;
}
#endif

void WebPasteboardProxy::allPasteboardItemInfo(IPC::Connection&, const String&, int64_t changeCount, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(std::optional<Vector<PasteboardItemInfo>>&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        if (wpe_clipboard_get_change_count(clipboard) != changeCount) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(Vector<PasteboardItemInfo> { pasteboardItemInfoFromFormats(clipboardFormats(clipboard)) });
        return;
    }
#endif
    completionHandler(std::nullopt);
}

void WebPasteboardProxy::informationForItemAtIndex(IPC::Connection&, uint64_t index, const String&, int64_t changeCount, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(std::optional<PasteboardItemInfo>&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI() && index) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        if (wpe_clipboard_get_change_count(clipboard) != changeCount) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(pasteboardItemInfoFromFormats(clipboardFormats(clipboard)));
        return;
    }
#endif
    completionHandler(std::nullopt);
}

void WebPasteboardProxy::getPasteboardItemsCount(IPC::Connection&, const String&, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(uint64_t)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        const auto* formats = wpe_clipboard_get_formats(clipboard);
        completionHandler(formats && formats[0] ? 1 : 0);
        return;
    }
#endif
    completionHandler(0);
}

void WebPasteboardProxy::readURLFromPasteboard(IPC::Connection& connection, uint64_t index, const String&, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(String&& url, String&& title)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        if (index) {
            // We don't support more than one item in the clipboard.
            completionHandler({ }, { });
            return;
        }

        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        if (GRefPtr<GBytes> bytes = adoptGRef(wpe_clipboard_read_bytes(clipboard, "text/uri-list"))) {
            auto buffer = FragmentedSharedBuffer::create(bytes.get())->makeContiguous();
            completionHandler(String(buffer->span()), { });
            return;
        }
    }
#endif
    completionHandler({ }, { });
}

void WebPasteboardProxy::readBufferFromPasteboard(IPC::Connection& connection, std::optional<uint64_t> index, const String& pasteboardType, const String&, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        if (index && index.value()) {
            // We don't support more than one item in the clipboard.
            completionHandler({ });
            return;
        }

        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        if (GRefPtr<GBytes> bytes = adoptGRef(wpe_clipboard_read_bytes(clipboard, pasteboardType.utf8().data()))) {
            completionHandler(FragmentedSharedBuffer::create(bytes.get())->makeContiguous());
            return;
        }
    }
#endif
    completionHandler({ });
}

void WebPasteboardProxy::getPasteboardChangeCount(IPC::Connection&, const String&, CompletionHandler<void(int64_t)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        completionHandler(wpe_clipboard_get_change_count(clipboard));
        return;
    }
#endif
    completionHandler(PlatformPasteboard().changeCount());
}

} // namespace WebKit

#endif // PLATFORM(WPE)
