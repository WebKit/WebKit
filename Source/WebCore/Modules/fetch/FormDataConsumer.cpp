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
#include "ExceptionOr.h"
#include "FormData.h"
#include "JSDOMPromiseDeferred.h"
#include "PendingStreamState.h"
#include "SWContextManager.h"
#include "SharedBuffer.h"
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FormDataConsumer);

FormDataConsumer::FormDataConsumer(const FormData& formData, ScriptExecutionContext& context, Callback&& callback, Mode mode)
    : m_formData(formData.copy())
    , m_context(&context)
    , m_callback(WTF::move(callback))
    , m_fileQueue(WorkQueue::create("FormDataConsumer file queue"_s))
    , m_mode(mode)
{
    // We explicitly copy pendingStreamState as FormData::copy does not, to limit the risk of trying to consume mulitple times the same pendingStreamState.
    if (RefPtr state = formData.pendingStreamState())
        m_formData->setPendingStreamState(state.releaseNonNull());
}

FormDataConsumer::~FormDataConsumer()
{
    ASSERT(!m_pendingStreamState);
}

void FormDataConsumer::read()
{
    if (isCancelled())
        return;

    ASSERT(m_callback);
    ASSERT(!m_blobLoader);

    if (m_currentElementIndex >= m_formData->elements().size()) {
        auto callback = std::exchange(m_callback, nullptr);
        callback(std::span<const uint8_t> { });
        return;
    }

    switchOn(m_formData->elements()[m_currentElementIndex++].data, [this](const Vector<uint8_t>& content) {
        consumeData(content);
    }, [this](const FormDataElement::EncodedFileData& fileData) {
        consumeFile(fileData.filename);
    }, [this](const FormDataElement::EncodedBlobData& blobData) {
        consumeBlob(blobData.url);
    }, [this](const FormDataElement::PendingStreamData&) {
        if (RefPtr state = m_formData->pendingStreamState()) {
            consumePendingStream(*state);
            return;
        }
        didFail(Exception { ExceptionCode::InvalidStateError, "Stream upload is missing state"_s });
    });
}

void FormDataConsumer::consumeData(const Vector<uint8_t>& content)
{
    consume(content.span());
}

void FormDataConsumer::consumeFile(const String& filename)
{
    m_isReadingFile = true;
    m_fileQueue->dispatch([weakThis = WeakPtr { *this }, identifier = m_context->identifier(), path = filename.isolatedCopy()]() mutable {
        ScriptExecutionContext::postTaskTo(identifier, [weakThis = WTF::move(weakThis), content = FileSystem::readEntireFile(path)](auto&) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !protectedThis->m_isReadingFile)
                return;

            protectedThis->m_isReadingFile = false;
            if (!content) {
                protectedThis->didFail(Exception { ExceptionCode::InvalidStateError, "Unable to read form data file"_s });
                return;
            }

            protectedThis->consume(*content);
        });
    });
}

void FormDataConsumer::consumeBlob(const URL& blobURL)
{
    m_blobLoader = BlobLoader::create([weakThis = WeakPtr { *this }](BlobLoader&) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto loader = std::exchange(protectedThis->m_blobLoader, { });
        if (!loader)
            return;

        if (auto optionalErrorCode = loader->errorCode()) {
            protectedThis->didFail(Exception { ExceptionCode::InvalidStateError, "Failed to read form data blob"_s });
            return;
        }

        if (auto data = loader->arrayBufferResult())
            protectedThis->consume(data->span());
    });
    if (RefPtr blobLoader = m_blobLoader.get()) {
        blobLoader->start(blobURL, m_context.get(), FileReaderLoader::ReadAsArrayBuffer);
        if (blobLoader->isLoading())
            return;
    }
    didFail(Exception { ExceptionCode::InvalidStateError, "Unable to read form data blob"_s });
}

void FormDataConsumer::consumePendingStream(PendingStreamState& state)
{
    RefPtr context = m_context;
    if (!context) {
        didFail(Exception { ExceptionCode::InvalidStateError, "Context is gone"_s });
        return;
    }

    m_pendingStreamState = &state;
    state.setDataAvailableHandler([weakThis = WeakPtr { *this }, contextIdentifier = context->identifier()] {
        ScriptExecutionContext::postTaskTo(contextIdentifier, [weakThis](auto&) {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->drainPendingStream();
        });
    });

    if (!m_hasRequestedPendingStream) {
        ASSERT(state.serviceWorkerFetchIdentifier());
        m_hasRequestedPendingStream = true;
        ensureOnMainThread([state = Ref { state }] {
            if (RefPtr connection = SWContextManager::singleton().connection())
                connection->startPendingStreamUploadForwarding(state.get());
        });
    }
}

void FormDataConsumer::drainPendingStream()
{
    if (isCancelled())
        return;

    RefPtr state = m_pendingStreamState;
    if (!state)
        return;

    if (m_mode == Mode::Pull && !m_pendingPullPromise)
        return;

    auto result = state->takeAvailableChunks();

    if (!result) {
        didFail(Exception { ExceptionCode::NetworkError, "Stream upload failed"_s });
        return;
    }

    if (result->first.isEmpty() && !result->second) {
        // We do not have data, let's wait for data availability notification.
        return;
    }

    auto scope = makeScopeExit([promise = std::exchange(m_pendingPullPromise, { })] {
        if (promise)
            promise->resolve();
    });

    for (auto chunk : result->first) {
        if (m_callback) {
            if (!m_callback(chunk->span())) {
                cancel();
                return;
            }
        }
    }

    if (!result->second)
        return;

    state->clearDataAvailableHandler();
    m_pendingStreamState = nullptr;

    if (auto callback = std::exchange(m_callback, nullptr))
        callback(std::span<const uint8_t> { });
}

void FormDataConsumer::resume(RefPtr<DeferredPromise>&& promise)
{
    ASSERT(m_mode == Mode::Pull);
    m_pendingPullPromise = WTF::move(promise);
    drainPendingStream();
}

void FormDataConsumer::consume(std::span<const uint8_t> content)
{
    if (!m_callback)
        return;

    if (!content.empty()) {
        bool result = m_callback(WTF::move(content));
        if (!result) {
            cancel();
            return;
        }

        if (!m_callback)
            return;
    }

    read();
}

void FormDataConsumer::didFail(Exception&& exception)
{
    auto callback = std::exchange(m_callback, nullptr);
    cancel();
    if (callback)
        callback(WTF::move(exception));
}

void FormDataConsumer::cancel()
{
    m_callback = nullptr;
    if (auto loader = std::exchange(m_blobLoader, { }))
        loader->cancel();
    if (RefPtr state = std::exchange(m_pendingStreamState, { })) {
        state->clearDataAvailableHandler();
        if (m_hasRequestedPendingStream) {
            ensureOnMainThread([state = state.releaseNonNull()] {
                if (RefPtr connection = SWContextManager::singleton().connection())
                    connection->cancelPendingStreamUploadForwarding(state.get());
            });
        }
    }
    m_isReadingFile = false;
    m_context = nullptr;
}

} // namespace WebCore
