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
#include "ClipboardItemBindingsDataSource.h"

#include "BitmapImage.h"
#include "Blob.h"
#include "Clipboard.h"
#include "ClipboardItem.h"
#include "Document.h"
#include "FileReaderLoader.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "JSBlob.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "PasteboardCustomData.h"
#include "SharedBuffer.h"
#include "markup.h"
#include <wtf/Function.h>

namespace WebCore {

static RefPtr<Document> documentFromClipboard(const Clipboard* clipboard)
{
    if (!clipboard)
        return nullptr;

    auto* frame = clipboard->frame();
    if (!frame)
        return nullptr;

    return frame->document();
}

static FileReaderLoader::ReadType readTypeForMIMEType(const String& type)
{
    if (type == "text/uri-list"_s || type == "text/plain"_s || type == "text/html"_s)
        return FileReaderLoader::ReadAsText;
    return FileReaderLoader::ReadAsArrayBuffer;
}

ClipboardItemBindingsDataSource::ClipboardItemBindingsDataSource(ClipboardItem& item, Vector<KeyValuePair<String, RefPtr<DOMPromise>>>&& itemPromises)
    : ClipboardItemDataSource(item)
    , m_itemPromises(WTFMove(itemPromises))
{
}

ClipboardItemBindingsDataSource::~ClipboardItemBindingsDataSource() = default;

Vector<String> ClipboardItemBindingsDataSource::types() const
{
    return m_itemPromises.map([&] (auto& typeAndItem) {
        return typeAndItem.key;
    });
}

void ClipboardItemBindingsDataSource::getType(const String& type, Ref<DeferredPromise>&& promise)
{
    auto matchIndex = m_itemPromises.findMatching([&] (auto& item) {
        return type == item.key;
    });

    if (matchIndex == notFound) {
        promise->reject(NotFoundError);
        return;
    }

    auto itemPromise = m_itemPromises[matchIndex].value;
    itemPromise->whenSettled([itemPromise, promise = makeRefPtr(promise.get()), type] () mutable {
        if (itemPromise->status() != DOMPromise::Status::Fulfilled) {
            promise->reject(AbortError);
            return;
        }

        auto result = itemPromise->result();
        if (!result) {
            promise->reject(TypeError);
            return;
        }

        String string;
        result.getString(itemPromise->globalObject(), string);
        if (!string.isNull()) {
            promise->resolve<IDLInterface<Blob>>(ClipboardItem::blobFromString(string, type));
            return;
        }

        if (!result.isObject()) {
            promise->reject(TypeError);
            return;
        }

        if (auto blob = JSBlob::toWrapped(result.getObject()->vm(), result.getObject()))
            promise->resolve<IDLInterface<Blob>>(*blob);
        else
            promise->reject(TypeError);
    });
}

void ClipboardItemBindingsDataSource::collectDataForWriting(Clipboard& destination, CompletionHandler<void(Optional<PasteboardCustomData>)>&& completion)
{
    m_itemTypeLoaders.clear();
    ASSERT(!m_completionHandler);
    m_completionHandler = WTFMove(completion);
    m_writingDestination = makeWeakPtr(destination);
    m_numberOfPendingClipboardTypes = m_itemPromises.size();
    m_itemTypeLoaders = m_itemPromises.map([&] (auto& typeAndItem) {
        auto type = typeAndItem.key;
        auto itemTypeLoader = ClipboardItemTypeLoader::create(type, [this, protectedItem = makeRef(m_item)] {
            ASSERT(m_numberOfPendingClipboardTypes);
            if (!--m_numberOfPendingClipboardTypes)
                invokeCompletionHandler();
        });

        auto promise = typeAndItem.value;
        promise->whenSettled([this, protectedItem = makeRefPtr(m_item), destination = m_writingDestination, promise, type, weakItemTypeLoader = makeWeakPtr(itemTypeLoader.ptr())] () mutable {
            if (!weakItemTypeLoader)
                return;

            auto itemTypeLoader = makeRef(*weakItemTypeLoader);
            ASSERT_UNUSED(this, notFound != m_itemTypeLoaders.findMatching([&] (auto& loader) { return loader.ptr() == itemTypeLoader.ptr(); }));

            auto result = promise->result();
            if (!result) {
                itemTypeLoader->didFailToResolve();
                return;
            }

            auto clipboard = makeRefPtr(destination.get());
            if (!clipboard) {
                itemTypeLoader->didFailToResolve();
                return;
            }

            if (!clipboard->scriptExecutionContext()) {
                itemTypeLoader->didFailToResolve();
                return;
            }

            String text;
            result.getString(promise->globalObject(), text);
            if (!text.isNull()) {
                itemTypeLoader->didResolveToString(text);
                return;
            }

            if (!result.isObject()) {
                itemTypeLoader->didFailToResolve();
                return;
            }

            if (auto blob = makeRefPtr(JSBlob::toWrapped(result.getObject()->vm(), result.getObject())))
                itemTypeLoader->didResolveToBlob(*clipboard->scriptExecutionContext(), blob.releaseNonNull());
            else
                itemTypeLoader->didFailToResolve();
        });

        return itemTypeLoader;
    });

    if (!m_numberOfPendingClipboardTypes)
        invokeCompletionHandler();
}

void ClipboardItemBindingsDataSource::invokeCompletionHandler()
{
    if (!m_completionHandler) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto completionHandler = std::exchange(m_completionHandler, { });
    auto itemTypeLoaders = std::exchange(m_itemTypeLoaders, { });
    auto clipboard = makeRefPtr(m_writingDestination.get());
    m_writingDestination = nullptr;

    auto document = documentFromClipboard(clipboard.get());
    if (!document) {
        completionHandler(WTF::nullopt);
        return;
    }

    PasteboardCustomData customData;
    for (auto& itemTypeLoader : itemTypeLoaders) {
        auto type = itemTypeLoader->type();
        auto& data = itemTypeLoader->data();
        if (WTF::holds_alternative<String>(data) && !!WTF::get<String>(data))
            customData.writeString(type, WTF::get<String>(data));
        else if (WTF::holds_alternative<Ref<SharedBuffer>>(data))
            customData.writeData(type, WTF::get<Ref<SharedBuffer>>(data).copyRef());
        else {
            completionHandler(WTF::nullopt);
            return;
        }
    }

    customData.setOrigin(document->originIdentifierForPasteboard());
    completionHandler(WTFMove(customData));
}

ClipboardItemBindingsDataSource::ClipboardItemTypeLoader::ClipboardItemTypeLoader(const String& type, CompletionHandler<void()>&& completionHandler)
    : m_type(type)
    , m_completionHandler(WTFMove(completionHandler))
{
}

ClipboardItemBindingsDataSource::ClipboardItemTypeLoader::~ClipboardItemTypeLoader()
{
    if (m_blobLoader)
        m_blobLoader->cancel();

    invokeCompletionHandler();
}

void ClipboardItemBindingsDataSource::ClipboardItemTypeLoader::didFinishLoading()
{
    ASSERT(m_blobLoader);
    auto stringResult = readTypeForMIMEType(m_type) == FileReaderLoader::ReadAsText ? m_blobLoader->stringResult() : nullString();
    if (!stringResult.isNull())
        m_data = { stringResult };
    else if (auto arrayBuffer = m_blobLoader->arrayBufferResult())
        m_data = { SharedBuffer::create(static_cast<const char*>(arrayBuffer->data()), arrayBuffer->byteLength()) };
    m_blobLoader = nullptr;
    invokeCompletionHandler();
}

void ClipboardItemBindingsDataSource::ClipboardItemTypeLoader::didFail(int)
{
    ASSERT(m_blobLoader);
    m_blobLoader = nullptr;
    invokeCompletionHandler();
}

void ClipboardItemBindingsDataSource::ClipboardItemTypeLoader::sanitizeDataIfNeeded()
{
    if (m_type == "text/html"_s) {
        String markupToSanitize;
        if (WTF::holds_alternative<Ref<SharedBuffer>>(m_data)) {
            auto& buffer = WTF::get<Ref<SharedBuffer>>(m_data);
            markupToSanitize = String::fromUTF8(buffer->data(), buffer->size());
        } else if (WTF::holds_alternative<String>(m_data))
            markupToSanitize = WTF::get<String>(m_data);

        if (markupToSanitize.isEmpty())
            return;

        m_data = { sanitizeMarkup(markupToSanitize) };
    }

    if (m_type == "image/png"_s) {
        RefPtr<SharedBuffer> bufferToSanitize;
        if (WTF::holds_alternative<Ref<SharedBuffer>>(m_data))
            bufferToSanitize = WTF::get<Ref<SharedBuffer>>(m_data).ptr();
        else if (WTF::holds_alternative<String>(m_data))
            bufferToSanitize = utf8Buffer(WTF::get<String>(m_data));

        if (!bufferToSanitize || bufferToSanitize->isEmpty())
            return;

        auto bitmapImage = BitmapImage::create();
        bitmapImage->setData(WTFMove(bufferToSanitize), true);
        auto imageBuffer = ImageBuffer::create(bitmapImage->size(), RenderingMode::Unaccelerated);
        if (!imageBuffer) {
            m_data = { nullString() };
            return;
        }

        imageBuffer->context().drawImage(bitmapImage.get(), FloatPoint::zero());
        m_data = { SharedBuffer::create(imageBuffer->toData("image/png"_s)) };
    }
}

void ClipboardItemBindingsDataSource::ClipboardItemTypeLoader::invokeCompletionHandler()
{
    if (auto completion = WTFMove(m_completionHandler)) {
        sanitizeDataIfNeeded();
        completion();
    }
}

void ClipboardItemBindingsDataSource::ClipboardItemTypeLoader::didResolveToBlob(ScriptExecutionContext& context, Ref<Blob>&& blob)
{
    ASSERT(!m_blobLoader);
    m_blobLoader = makeUnique<FileReaderLoader>(readTypeForMIMEType(m_type), this);
    m_blobLoader->start(&context, WTFMove(blob));
}

void ClipboardItemBindingsDataSource::ClipboardItemTypeLoader::didFailToResolve()
{
    ASSERT(!m_blobLoader);
    invokeCompletionHandler();
}

void ClipboardItemBindingsDataSource::ClipboardItemTypeLoader::didResolveToString(const String& text)
{
    ASSERT(!m_blobLoader);
    m_data = { text };
    invokeCompletionHandler();
}

} // namespace WebCore
