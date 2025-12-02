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
#include "ContextDestructionObserverInlines.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

Ref<NetworkSendQueue> NetworkSendQueue::create(ScriptExecutionContext& context, WriteString&& writeString, WriteRawData&& writeRawData, ProcessError&& processError)
{
    return adoptRef(*new NetworkSendQueue(context, WTFMove(writeString), WTFMove(writeRawData), WTFMove(processError)));
}

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
        m_writeRawData(binaryData.span().subspan(byteOffset, byteLength));
        return;
    }
    m_queue.append(Ref<FragmentedSharedBuffer> { SharedBuffer::create(binaryData.span().subspan(byteOffset, byteLength)) });
}

void NetworkSendQueue::enqueue(WebCore::Blob& blob)
{
    RefPtr context = scriptExecutionContext();
    if (!context)
        return;

    auto byteLength = blob.size();
    if (!byteLength) {
        // The cast looks weird, but is required for the overloading resolution to succeed.
        // Without it, there is an ambiguity where ArrayBuffer::create(const void* source, size_t byteLength) could be called instead.
        enqueue(JSC::ArrayBuffer::create(static_cast<size_t>(0U), 1), 0, 0);
        return;
    }
    Ref blobLoader = BlobLoader::create([weakThis = WeakPtr { *this }](BlobLoader&) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->processMessages();
    });
    m_queue.append(blobLoader.copyRef());
    blobLoader->start(blob, context.get(), FileReaderLoader::ReadAsArrayBuffer);
}

void NetworkSendQueue::clear()
{
    m_queue.clear();
}

void NetworkSendQueue::processMessages()
{
    while (!m_queue.isEmpty()) {
        bool shouldStopProcessing = false;
        switchOn(m_queue.first(), [this](const CString& utf8) {
            m_writeString(utf8);
        }, [this](Ref<FragmentedSharedBuffer>& data) {
            data->forEachSegment(m_writeRawData);
        }, [this, &shouldStopProcessing](Ref<BlobLoader>& loader) {
            auto errorCode = loader->errorCode();
            if (loader->isLoading() || (errorCode && errorCode.value() == ExceptionCode::AbortError)) {
                shouldStopProcessing = true;
                return;
            }

            if (const auto& result = loader->arrayBufferResult()) {
                m_writeRawData(result->span());
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
