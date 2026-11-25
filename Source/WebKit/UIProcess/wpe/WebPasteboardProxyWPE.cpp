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
#include <wtf/ListHashSet.h>
#include <wtf/glib/GLibExtras.h>
#include <wtf/glib/GSpanExtras.h>

#if ENABLE(WPE_PLATFORM)
#include "WPEUtilities.h"
#include <wpe/GRefPtrWPE.h>
#include <wpe/wpe-platform.h>
#endif

namespace WebKit {
using namespace WebCore;

#if ENABLE(WPE_PLATFORM)
static Vector<RefPtr<StringImpl>> clipboardFormats(WPEClipboard* clipboard)
{
    const auto formats = span(wpe_clipboard_get_formats(clipboard));
    return Vector<RefPtr<StringImpl>>(formats.size(), [&formats](size_t index) {
        return StringImpl::fromUTF8(formats[index]);
    });
}
#endif

void WebPasteboardProxy::getTypes(StringImpl*, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        completionHandler(clipboardFormats(clipboard));
        return;
    }
#endif

#if USE(LIBWPE)
    Vector<RefPtr<StringImpl>> pasteboardTypes;
    PlatformPasteboard().getTypes(pasteboardTypes);
    completionHandler(WTF::move(pasteboardTypes));
#endif
}

void WebPasteboardProxy::readText(IPC::Connection&, StringImpl*, StringImpl* pasteboardType, CompletionHandler<void(RefPtr<StringImpl>&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        gsize textLength;
        GUniquePtr<char> text(wpe_clipboard_read_text(clipboard, utf8(pasteboardType).legacyCStringPointer(), &textLength));
        completionHandler(StringImpl::fromUTF8(unsafeMakeSpan(text.get(), textLength)));
        return;
    }
#endif

#if USE(LIBWPE)
    completionHandler(PlatformPasteboard().readString(0, startsWith(pasteboardType, "text/plain"_s) ? "text/plain;charset=utf-8"_si.get() : pasteboardType));
#endif
}

void WebPasteboardProxy::readFilePaths(IPC::Connection&, StringImpl*, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&& completionHandler)
{
    completionHandler({ });
}

void WebPasteboardProxy::readBuffer(IPC::Connection&, StringImpl*, StringImpl* pasteboardType, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        if (GRefPtr<GBytes> bytes = adoptGRef(wpe_clipboard_read_bytes(clipboard, utf8(pasteboardType).legacyCStringPointer()))) {
            completionHandler(SharedBuffer::create(bytes.get()));
            return;
        }
    }
#endif
    completionHandler({ });
}

#if ENABLE(WPE_PLATFORM)
static void setClipboardContentFromSpan(WPEClipboardContent* content, const char* type, std::span<const char8_t> text)
{
    GRefPtr bytes = gBytesNew(text);
    wpe_clipboard_content_set_bytes(content, type, bytes.get());
}
#endif

void WebPasteboardProxy::writeToClipboard(StringImpl*, SelectionData&& selectionData)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        GRefPtr<WPEClipboardContent> content = adoptGRef(wpe_clipboard_content_new());
        if (selectionData.hasURIList())
            setClipboardContentFromSpan(content.get(), "text/uri-list", utf8(selectionData.uriList()).span());
        if (selectionData.hasMarkup())
            setClipboardContentFromSpan(content.get(), "text/html", utf8(selectionData.markup()).span());
        if (selectionData.hasText())
            wpe_clipboard_content_set_text(content.get(), utf8(selectionData.text()).legacyCStringPointer());
        if (selectionData.canSmartReplace()) {
            GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(nullptr, 0));
            wpe_clipboard_content_set_bytes(content.get(), "application/vnd.wpewebkit.smartpaste", bytes.get());
        }
        if (selectionData.hasCustomData())
            wpe_clipboard_content_set_bytes(content.get(), PasteboardCustomData::wpeType().characters(), selectionData.customData()->createGBytes().get());

        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        wpe_clipboard_set_content(clipboard, content.get());
        return;
    }
#endif

#if USE(LIBWPE)
    PasteboardWebContent contents;
    if (selectionData.hasText())
        contents.text = selectionData.text();
    if (selectionData.hasMarkup())
        contents.markup = selectionData.markup();
    PlatformPasteboard().write(contents);
#endif
}

void WebPasteboardProxy::clearClipboard(StringImpl*)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        wpe_clipboard_set_content(clipboard, nullptr);
    }
#endif
}

void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(IPC::Connection&, StringImpl*, StringImpl* origin, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        ListHashSet<RefPtr<StringImpl>> domTypes;
        if (GRefPtr<GBytes> bytes = adoptGRef(wpe_clipboard_read_bytes(clipboard, PasteboardCustomData::wpeType().characters()))) {
            Ref buffer = SharedBuffer::create(bytes.get());
            auto customData = PasteboardCustomData::fromSharedBuffer(buffer.get());
            if (equal(customData.origin(), origin)) {
                for (auto& type : customData.orderedTypes())
                    domTypes.add(type);
            }
        }

        for (const char* format : span(wpe_clipboard_get_formats(clipboard))) {
            RefPtr<StringImpl> formatString = StringImpl::fromUTF8(format);
            if (equal(formatString, PasteboardCustomData::wpeType()))
                continue;

            if (Pasteboard::isSafeTypeForDOMToReadAndWrite(formatString))
                domTypes.add(formatString);
        }
        completionHandler(copyToVector(domTypes));
        return;
    }
#endif
    completionHandler({ });
}

void WebPasteboardProxy::writeCustomData(IPC::Connection&, const Vector<PasteboardCustomData>& data, StringImpl*, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&& completionHandler)
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
        customData.forEachPlatformStringOrBuffer([&content](auto* type, auto& stringOrBuffer) {
            if (std::holds_alternative<Ref<SharedBuffer>>(stringOrBuffer)) {
                auto buffer = std::get<Ref<SharedBuffer>>(stringOrBuffer);
                auto bytes = buffer->createGBytes();
                wpe_clipboard_content_set_bytes(content.get(), utf8(type).legacyCStringPointer(), bytes.get());
            } else if (std::holds_alternative<RefPtr<StringImpl>>(stringOrBuffer)) {
                if (equal(type, "text/plain"_s))
                    wpe_clipboard_content_set_text(content.get(), utf8(std::get<RefPtr<StringImpl>>(stringOrBuffer)).legacyCStringPointer());
                else if (equal(type, "text/html"_s))
                    setClipboardContentFromSpan(content.get(), "text/html", utf8(std::get<RefPtr<StringImpl>>(stringOrBuffer)).span());
                else if (equal(type, "text/uri-list"_s))
                    setClipboardContentFromSpan(content.get(), "text/uri-list", utf8(std::get<RefPtr<StringImpl>>(stringOrBuffer)).span());
                else if (equal(type, "image/svg+xml"_s))
                    setClipboardContentFromSpan(content.get(), "image/svg+xml", utf8(std::get<RefPtr<StringImpl>>(stringOrBuffer)).span());
            }
        });

        if (customData.hasSameOriginCustomData() || !isEmpty(customData.origin())) {
            auto buffer = customData.createSharedBuffer();
            auto bytes = buffer->createGBytes();
            wpe_clipboard_content_set_bytes(content.get(), PasteboardCustomData::wpeType(), bytes.get());
        }

        wpe_clipboard_set_content(clipboard, content.get());
        completionHandler(wpe_clipboard_get_change_count(clipboard));
        return;
    }
#endif

#if USE(LIBWPE)
    completionHandler(PlatformPasteboard().write(data));
#endif
}

#if ENABLE(WPE_PLATFORM)
static PasteboardItemInfo pasteboardItemInfoFromFormats(Vector<RefPtr<StringImpl>>&& formats)
{
    PasteboardItemInfo info;
    auto contains = [&](ASCIILiteral type) {
        return formats.containsIf([&](auto& format) { return equal(format, type); });
    };
    if (contains("text/plain"_s) || contains("text/plain;charset=utf-8"_s))
        info.webSafeTypesByFidelity.append("text/plain"_s);
    if (contains("text/html"_s))
        info.webSafeTypesByFidelity.append("text/html"_s);
    if (contains("text/uri-list"_s))
        info.webSafeTypesByFidelity.append("text/uri-list"_s);
    if (contains("image/svg+xml"_s))
        info.webSafeTypesByFidelity.append("image/svg+xml"_s);
    info.platformTypesByFidelity = WTF::move(formats);
    return info;
}
#endif

void WebPasteboardProxy::allPasteboardItemInfo(IPC::Connection&, StringImpl*, int64_t changeCount, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(std::optional<Vector<PasteboardItemInfo>>&&)>&& completionHandler)
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

void WebPasteboardProxy::informationForItemAtIndex(IPC::Connection&, uint64_t index, StringImpl*, int64_t changeCount, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(std::optional<PasteboardItemInfo>&&)>&& completionHandler)
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

void WebPasteboardProxy::getPasteboardItemsCount(IPC::Connection&, StringImpl*, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(uint64_t)>&& completionHandler)
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

void WebPasteboardProxy::readURLFromPasteboard(IPC::Connection& connection, uint64_t index, StringImpl*, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(RefPtr<StringImpl>&& url, RefPtr<StringImpl>&& title)>&& completionHandler)
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
            auto buffer = SharedBuffer::create(bytes.get());
            completionHandler(StringImpl::createOrNull(byteCast<Latin1Character>(buffer->span())), { });
            return;
        }
    }
#endif
    completionHandler({ }, { });
}

void WebPasteboardProxy::readBufferFromPasteboard(IPC::Connection& connection, std::optional<uint64_t> index, StringImpl* pasteboardType, StringImpl*, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        if (index && index.value()) {
            // We don't support more than one item in the clipboard.
            completionHandler({ });
            return;
        }

        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        if (GRefPtr<GBytes> bytes = adoptGRef(wpe_clipboard_read_bytes(clipboard, utf8(pasteboardType).legacyCStringPointer()))) {
            completionHandler(SharedBuffer::create(bytes.get()));
            return;
        }
    }
#endif
    completionHandler({ });
}

void WebPasteboardProxy::getPasteboardChangeCount(IPC::Connection&, StringImpl*, CompletionHandler<void(int64_t)>&& completionHandler)
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        completionHandler(wpe_clipboard_get_change_count(clipboard));
        return;
    }
#endif

#if USE(LIBWPE)
    completionHandler(PlatformPasteboard().changeCount());
#endif
}

} // namespace WebKit

#endif // PLATFORM(WPE)
