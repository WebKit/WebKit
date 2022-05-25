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

void WebPasteboardProxy::readText(const String& pasteboardName, CompletionHandler<void(String&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).readText(WTFMove(completionHandler));
}

void WebPasteboardProxy::readFilePaths(const String& pasteboardName, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).readFilePaths(WTFMove(completionHandler));
}

void WebPasteboardProxy::readBuffer(const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(IPC::SharedBufferReference&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).readBuffer(pasteboardType.utf8().data(), [completionHandler = WTFMove(completionHandler)](auto&& buffer) mutable {
        completionHandler(IPC::SharedBufferReference(WTFMove(buffer)));
    });
}

void WebPasteboardProxy::writeToClipboard(const String& pasteboardName, SelectionData&& selectionData)
{
    Clipboard::get(pasteboardName).write(WTFMove(selectionData));
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

void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(IPC::Connection&, const String& pasteboardName, const String& origin, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
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
    });
}

void WebPasteboardProxy::writeCustomData(IPC::Connection&, const Vector<PasteboardCustomData>& data, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(int64_t)>&& completionHandler)
{
    if (data.isEmpty() || data.size() > 1) {
        // We don't support more than one custom item in the clipboard.
        completionHandler(0);
        return;
    }

    SelectionData selectionData;
    const auto& customData = data[0];
    customData.forEachPlatformStringOrBuffer([&selectionData] (auto& type, auto& stringOrBuffer) {
        if (std::holds_alternative<String>(stringOrBuffer)) {
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

    Clipboard::get(pasteboardName).write(WTFMove(selectionData));
    completionHandler(0);
}

} // namespace WebKit
