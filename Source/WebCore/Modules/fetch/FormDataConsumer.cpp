/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "FormDataConsumer.h"

#include "BlobLoader.h"
#include "FormData.h"
#include <wtf/WorkQueue.h>

namespace WebCore {

FormDataConsumer::FormDataConsumer(const FormData& formData, ScriptExecutionContext& context, Callback&& callback)
    : m_formData(formData.copy())
    , m_context(&context)
    , m_callback(WTFMove(callback))
    , m_fileQueue(WorkQueue::create("FormDataConsumer file queue"))
{
    read();
}

FormDataConsumer::~FormDataConsumer()
{
}

void FormDataConsumer::read()
{
    if (isCancelled())
        return;

    ASSERT(m_callback);
    ASSERT(!m_blobLoader);

    if (m_currentElementIndex >= m_formData->elements().size()) {
        m_callback(std::span<const uint8_t> { });
        return;
    }

    switchOn(m_formData->elements()[m_currentElementIndex++].data, [this](const Vector<uint8_t>& content) {
        consumeData(content);
    }, [this](const FormDataElement::EncodedFileData& fileData) {
        consumeFile(fileData.filename);
    }, [this](const FormDataElement::EncodedBlobData& blobData) {
        consumeBlob(blobData.url);
    });
}

void FormDataConsumer::consumeData(const Vector<uint8_t>& content)
{
    consume(content.span());
}

void FormDataConsumer::consumeFile(const String& filename)
{
    m_fileQueue->dispatch([weakThis = WeakPtr { *this }, identifier = m_context->identifier(), path = filename.isolatedCopy()]() mutable {
        ScriptExecutionContext::postTaskTo(identifier, [weakThis = WTFMove(weakThis), content = FileSystem::readEntireFile(path)](auto&) {
            if (!weakThis)
                return;

            if (!content) {
                weakThis->didFail(Exception { ExceptionCode::InvalidStateError, "Unable to read form data file"_s });
                return;
            }

            weakThis->consume(*content);
        });
    });
}

void FormDataConsumer::consumeBlob(const URL& blobURL)
{
    m_blobLoader = makeUnique<BlobLoader>([weakThis = WeakPtr { *this }](BlobLoader&) mutable {
        if (!weakThis)
            return;

        auto loader = std::exchange(weakThis->m_blobLoader, { });
        if (!loader)
            return;

        if (auto optionalErrorCode = loader->errorCode()) {
            weakThis->didFail(Exception { ExceptionCode::InvalidStateError, "Failed to read form data blob"_s });
            return;
        }

        if (auto data = loader->arrayBufferResult())
            weakThis->consume(std::span<const uint8_t> { static_cast<const uint8_t*>(data->data()), data->byteLength() });
    });

    m_blobLoader->start(blobURL, m_context.get(), FileReaderLoader::ReadAsArrayBuffer);

    if (!m_blobLoader || !m_blobLoader->isLoading())
        didFail(Exception { ExceptionCode::InvalidStateError, "Unable to read form data blob"_s });
}

void FormDataConsumer::consume(std::span<const uint8_t> content)
{
    if (!m_callback)
        return;

    m_callback(WTFMove(content));
    if (!m_callback)
        return;

    read();
}

void FormDataConsumer::didFail(Exception&& exception)
{
    auto callback = std::exchange(m_callback, nullptr);
    cancel();
    if (callback)
        callback(WTFMove(exception));
}

void FormDataConsumer::cancel()
{
    m_callback = nullptr;
    if (auto loader = std::exchange(m_blobLoader, { }))
        loader->cancel();
    m_context = nullptr;
}

} // namespace WebCore
