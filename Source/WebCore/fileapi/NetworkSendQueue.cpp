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
#include "NetworkSendQueue.h"

#include "BlobLoader.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

NetworkSendQueue::NetworkSendQueue(ScriptExecutionContext& context, WriteString&& writeString, WriteRawData&& writeRawData, ProcessError&& processError)
    : ContextDestructionObserver(&context)
    , m_writeString(WTFMove(writeString))
    , m_writeRawData(WTFMove(writeRawData))
    , m_processError(WTFMove(processError))
{
}

NetworkSendQueue::~NetworkSendQueue() = default;

void NetworkSendQueue::enqueue(CString&& utf8)
{
    if (m_queue.isEmpty()) {
        m_writeString(utf8);
        return;
    }
    m_queue.append(WTFMove(utf8));
}

void NetworkSendQueue::enqueue(const JSC::ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    if (m_queue.isEmpty()) {
        auto* data = static_cast<const uint8_t*>(binaryData.data());
        m_writeRawData(Span { data + byteOffset, byteLength });
        return;
    }
    m_queue.append(SharedBuffer::create(static_cast<const uint8_t*>(binaryData.data()) + byteOffset, byteLength));
}

void NetworkSendQueue::enqueue(WebCore::Blob& blob)
{
    auto* context = scriptExecutionContext();
    if (!context)
        return;

    auto byteLength = blob.size();
    if (!byteLength) {
        enqueue(JSC::ArrayBuffer::create(0U, 1), 0, 0);
        return;
    }
    auto blobLoader = makeUniqueRef<BlobLoader>([this](BlobLoader&) {
        processMessages();
    });
    auto* blobLoaderPtr = &blobLoader.get();
    m_queue.append(WTFMove(blobLoader));
    blobLoaderPtr->start(blob, context, FileReaderLoader::ReadAsArrayBuffer);
}

void NetworkSendQueue::clear()
{
    // Do not call m_queue.clear() here since destroying a BlobLoader will cause its completion
    // handler to get called, which will call processMessages() to iterate over m_queue.
    std::exchange(m_queue, { });
}

void NetworkSendQueue::processMessages()
{
    while (!m_queue.isEmpty()) {
        bool shouldStopProcessing = false;
        switchOn(m_queue.first(), [this](const CString& utf8) {
            m_writeString(utf8);
        }, [this](Ref<SharedBuffer>& data) {
            data->forEachSegment(m_writeRawData);
        }, [this, &shouldStopProcessing](UniqueRef<BlobLoader>& loader) {
            auto errorCode = loader->errorCode();
            if (loader->isLoading() || (errorCode && errorCode.value() == AbortError)) {
                shouldStopProcessing = true;
                return;
            }

            if (const auto& result = loader->arrayBufferResult()) {
                m_writeRawData(Span { static_cast<const uint8_t*>(result->data()), result->byteLength() });
                return;
            }
            ASSERT(errorCode);
            shouldStopProcessing = m_processError(errorCode.value()) == Continue::No;
        });
        if (shouldStopProcessing)
            return;
        m_queue.removeFirst();
    }

}

} // namespace WebCore
