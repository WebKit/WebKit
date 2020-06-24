/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "Clipboard.h"

#include "ClipboardImageReader.h"
#include "ClipboardItem.h"
#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "JSBlob.h"
#include "JSClipboardItem.h"
#include "JSDOMPromiseDeferred.h"
#include "Navigator.h"
#include "Pasteboard.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "UserGestureIndicator.h"
#include "WebContentReader.h"
#include <wtf/CompletionHandler.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Clipboard);

static bool shouldProceedWithClipboardWrite(const Frame& frame)
{
    auto& settings = frame.settings();
    if (settings.javaScriptCanAccessClipboard() || frame.editor().isCopyingFromMenuOrKeyBinding())
        return true;

    switch (settings.clipboardAccessPolicy()) {
    case ClipboardAccessPolicy::Allow:
        return true;
    case ClipboardAccessPolicy::RequiresUserGesture:
        return UserGestureIndicator::processingUserGesture();
    case ClipboardAccessPolicy::Deny:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

Ref<Clipboard> Clipboard::create(Navigator& navigator)
{
    return adoptRef(*new Clipboard(navigator));
}

Clipboard::Clipboard(Navigator& navigator)
    : m_navigator(makeWeakPtr(navigator))
{
}

Clipboard::~Clipboard()
{
    if (auto writer = WTFMove(m_activeItemWriter))
        writer->invalidate();
}

Navigator* Clipboard::navigator()
{
    return m_navigator.get();
}

EventTargetInterface Clipboard::eventTargetInterface() const
{
    return ClipboardEventTargetInterfaceType;
}

ScriptExecutionContext* Clipboard::scriptExecutionContext() const
{
    return m_navigator ? m_navigator->scriptExecutionContext() : nullptr;
}

void Clipboard::readText(Ref<DeferredPromise>&& promise)
{
    auto frame = makeRefPtr(this->frame());
    if (!frame) {
        promise->reject(NotAllowedError);
        return;
    }

    auto pasteboard = Pasteboard::createForCopyAndPaste();
    auto changeCountAtStart = pasteboard->changeCount();
    if (!frame->requestDOMPasteAccess()) {
        promise->reject(NotAllowedError);
        return;
    }

    auto allInfo = pasteboard->allPasteboardItemInfo();
    if (!allInfo) {
        promise->reject(NotAllowedError);
        return;
    }

    String text;
    for (size_t index = 0; index < allInfo->size(); ++index) {
        if (allInfo->at(index).webSafeTypesByFidelity.contains("text/plain"_s)) {
            PasteboardPlainText plainTextReader;
            pasteboard->read(plainTextReader, PlainTextURLReadingPolicy::IgnoreURL, index);
            text = WTFMove(plainTextReader.text);
            break;
        }
    }

    if (changeCountAtStart == pasteboard->changeCount())
        promise->resolve<IDLDOMString>(WTFMove(text));
    else
        promise->reject(NotAllowedError);
}

void Clipboard::writeText(const String& data, Ref<DeferredPromise>&& promise)
{
    auto frame = makeRefPtr(this->frame());
    auto document = makeRefPtr(frame ? frame->document() : nullptr);
    if (!document || !shouldProceedWithClipboardWrite(*frame)) {
        promise->reject(NotAllowedError);
        return;
    }

    PasteboardCustomData customData;
    customData.writeString("text/plain"_s, data);
    customData.setOrigin(document->originIdentifierForPasteboard());
    Pasteboard::createForCopyAndPaste()->writeCustomData({ WTFMove(customData) });
    promise->resolve();
}

void Clipboard::read(Ref<DeferredPromise>&& promise)
{
    auto rejectPromiseAndClearActiveSession = [&] {
        m_activeSession = WTF::nullopt;
        promise->reject(NotAllowedError);
    };

    auto frame = makeRefPtr(this->frame());
    if (!frame) {
        rejectPromiseAndClearActiveSession();
        return;
    }

    auto pasteboard = Pasteboard::createForCopyAndPaste();
    auto changeCountAtStart = pasteboard->changeCount();

    if (!frame->requestDOMPasteAccess()) {
        rejectPromiseAndClearActiveSession();
        return;
    }

    if (!m_activeSession || m_activeSession->changeCount != changeCountAtStart) {
        auto allInfo = pasteboard->allPasteboardItemInfo();
        if (!allInfo) {
            rejectPromiseAndClearActiveSession();
            return;
        }

        Vector<Ref<ClipboardItem>> clipboardItems;
        clipboardItems.reserveInitialCapacity(allInfo->size());
        for (auto& itemInfo : *allInfo)
            clipboardItems.uncheckedAppend(ClipboardItem::create(*this, itemInfo));
        m_activeSession = {{ WTFMove(pasteboard), WTFMove(clipboardItems), changeCountAtStart }};
    }

    promise->resolve<IDLSequence<IDLInterface<ClipboardItem>>>(m_activeSession->items);
}

void Clipboard::getType(ClipboardItem& item, const String& type, Ref<DeferredPromise>&& promise)
{
    if (!m_activeSession) {
        promise->reject(NotAllowedError);
        return;
    }

    auto frame = makeRefPtr(this->frame());
    if (!frame) {
        m_activeSession = WTF::nullopt;
        promise->reject(NotAllowedError);
        return;
    }

    auto itemIndex = m_activeSession->items.findMatching([&] (auto& activeItem) {
        return activeItem.ptr() == &item;
    });

    if (itemIndex == notFound) {
        promise->reject(NotAllowedError);
        return;
    }

    if (!item.types().contains(type)) {
        promise->reject(NotAllowedError);
        return;
    }

    if (type == "image/png"_s) {
        ClipboardImageReader imageReader { type };
        activePasteboard().read(imageReader, itemIndex);
        auto imageBlob = imageReader.takeResult();
        if (updateSessionValidity() == SessionIsValid::Yes && imageBlob)
            promise->resolve<IDLInterface<Blob>>(imageBlob.releaseNonNull());
        else
            promise->reject(NotAllowedError);
        return;
    }

    String resultAsString;

    if (type == "text/uri-list"_s) {
        String title;
        resultAsString = activePasteboard().readURL(itemIndex, title).string();
    }

    if (type == "text/plain"_s) {
        PasteboardPlainText plainTextReader;
        activePasteboard().read(plainTextReader, PlainTextURLReadingPolicy::IgnoreURL, itemIndex);
        resultAsString = WTFMove(plainTextReader.text);
    }

    if (type == "text/html"_s) {
        WebContentMarkupReader markupReader { *frame };
        activePasteboard().read(markupReader, WebContentReadingPolicy::OnlyRichTextTypes, itemIndex);
        resultAsString = WTFMove(markupReader.markup);
    }

    // FIXME: Support reading custom data.
    if (updateSessionValidity() == SessionIsValid::No || resultAsString.isNull()) {
        promise->reject(NotAllowedError);
        return;
    }

    promise->resolve<IDLInterface<Blob>>(ClipboardItem::blobFromString(resultAsString, type));
}

Clipboard::SessionIsValid Clipboard::updateSessionValidity()
{
    if (!m_activeSession)
        return SessionIsValid::No;

    if (m_activeSession->changeCount != activePasteboard().changeCount()) {
        m_activeSession = WTF::nullopt;
        return SessionIsValid::No;
    }

    return SessionIsValid::Yes;
}

void Clipboard::write(const Vector<RefPtr<ClipboardItem>>& items, Ref<DeferredPromise>&& promise)
{
    auto frame = makeRefPtr(this->frame());
    if (!frame || !shouldProceedWithClipboardWrite(*frame)) {
        promise->reject(NotAllowedError);
        return;
    }

    if (auto existingWriter = std::exchange(m_activeItemWriter, ItemWriter::create(*this, WTFMove(promise))))
        existingWriter->invalidate();

    m_activeItemWriter->write(items);
}

void Clipboard::didResolveOrReject(Clipboard::ItemWriter& writer)
{
    if (m_activeItemWriter == &writer)
        m_activeItemWriter = nullptr;
}

Frame* Clipboard::frame() const
{
    return m_navigator ? m_navigator->frame() : nullptr;
}

Pasteboard& Clipboard::activePasteboard()
{
    ASSERT(m_activeSession);
    ASSERT(m_activeSession->pasteboard);
    return *m_activeSession->pasteboard;
}

Clipboard::ItemWriter::ItemWriter(Clipboard& clipboard, Ref<DeferredPromise>&& promise)
    : m_clipboard(makeWeakPtr(clipboard))
    , m_promise(WTFMove(promise))
    , m_pasteboard(Pasteboard::createForCopyAndPaste())
{
}

Clipboard::ItemWriter::~ItemWriter() = default;

void Clipboard::ItemWriter::write(const Vector<RefPtr<ClipboardItem>>& items)
{
    ASSERT(m_promise);
    ASSERT(m_clipboard);
#if PLATFORM(COCOA)
    m_changeCountAtStart = m_pasteboard->changeCount();
#endif
    m_dataToWrite.fill(WTF::nullopt, items.size());
    m_pendingItemCount = items.size();
    for (size_t index = 0; index < items.size(); ++index) {
        items[index]->collectDataForWriting(*m_clipboard, [this, protectedThis = makeRef(*this), index] (auto data) {
            protectedThis->setData(WTFMove(data), index);
            if (!--m_pendingItemCount)
                didSetAllData();
        });
    }
    if (items.isEmpty())
        didSetAllData();
}

void Clipboard::ItemWriter::invalidate()
{
    if (m_promise)
        reject();
}

void Clipboard::ItemWriter::setData(Optional<PasteboardCustomData>&& data, size_t index)
{
    if (index >= m_dataToWrite.size()) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_dataToWrite[index] = WTFMove(data);
}

void Clipboard::ItemWriter::didSetAllData()
{
    if (!m_promise)
        return;

#if PLATFORM(COCOA)
    auto newChangeCount = m_pasteboard->changeCount();
    if (m_changeCountAtStart != newChangeCount) {
        // FIXME: Instead of checking the changeCount here, send it over to the client (e.g. the UI process
        // in WebKit2) and perform it there.
        reject();
        return;
    }
#endif // PLATFORM(COCOA)
    auto dataToWrite = std::exchange(m_dataToWrite, { });
    Vector<PasteboardCustomData> customData;
    customData.reserveInitialCapacity(dataToWrite.size());
    for (auto data : dataToWrite) {
        if (!data) {
            reject();
            return;
        }
        customData.append(*data);
    }

    m_pasteboard->writeCustomData(WTFMove(customData));
    m_promise->resolve();
    m_promise = nullptr;

    if (auto clipboard = std::exchange(m_clipboard, nullptr))
        clipboard->didResolveOrReject(*this);
}

void Clipboard::ItemWriter::reject()
{
    if (auto promise = std::exchange(m_promise, nullptr))
        promise->reject(NotAllowedError);

    if (auto clipboard = std::exchange(m_clipboard, nullptr))
        clipboard->didResolveOrReject(*this);
}

}
