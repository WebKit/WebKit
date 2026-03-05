/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "BlobResourceHandle.h"
#include "FileStream.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include <wtf/CompletionHandler.h>
#include <wtf/MainThread.h>

namespace WebCore {

static const unsigned bufferSize = 512 * 1024;

static constexpr auto webKitBlobResourceDomain = "WebKitBlobResource"_s;

///////////////////////////////////////////////////////////////////////////////
// BlobResourceSynchronousLoader

namespace {

class BlobResourceSynchronousLoader : public ResourceHandleClient {
public:
    BlobResourceSynchronousLoader(ResourceError&, ResourceResponse&, Vector<uint8_t>&);

    void didReceiveResponseAsync(ResourceHandle*, ResourceResponse&&, CompletionHandler<void()>&&) final;
    void didFail(ResourceHandle*, const ResourceError&) final;
    void willSendRequestAsync(ResourceHandle*, ResourceRequest&&, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&&) final;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    void canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle*, const ProtectionSpace&, CompletionHandler<void(bool)>&&) final;
#endif

private:
    ResourceError& m_error;
    ResourceResponse& m_response;
    Vector<uint8_t>& m_data;
};

BlobResourceSynchronousLoader::BlobResourceSynchronousLoader(ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
    : m_error(error)
    , m_response(response)
    , m_data(data)
{
}

void BlobResourceSynchronousLoader::willSendRequestAsync(ResourceHandle*, ResourceRequest&& request, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(WTF::move(request));
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void BlobResourceSynchronousLoader::canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle*, const ProtectionSpace&, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(false);
}
#endif

void BlobResourceSynchronousLoader::didReceiveResponseAsync(ResourceHandle* handle, ResourceResponse&& response, CompletionHandler<void()>&& completionHandler)
{
    // We cannot handle the size that is more than maximum integer.
    if (response.expectedContentLength() > INT_MAX) {
        m_error = ResourceError(webKitBlobResourceDomain, static_cast<int>(BlobResourceHandle::Error::NotReadableError), response.url(), "File is too large"_s);
        completionHandler();
        return;
    }

    m_response = response;

    // Read all the data.
    m_data.resize(static_cast<uint64_t>(response.expectedContentLength()));
    downcast<BlobResourceHandle>(*handle).readSync(m_data.mutableSpan());
    completionHandler();
}

void BlobResourceSynchronousLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    m_error = error;
}

}

///////////////////////////////////////////////////////////////////////////////
// BlobResourceHandle

Ref<BlobResourceHandle> BlobResourceHandle::createAsync(BlobData* blobData, const ResourceRequest& request, ResourceHandleClient* client)
{
    return adoptRef(*new BlobResourceHandle(blobData, request, client, true));
}

void BlobResourceHandle::loadResourceSynchronously(BlobData* blobData, const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    if (!equalLettersIgnoringASCIICase(request.httpMethod(), "get"_s)) {
        error = ResourceError(webKitBlobResourceDomain, static_cast<int>(Error::MethodNotAllowed), response.url(), "Request method must be GET"_s);
        return;
    }

    BlobResourceSynchronousLoader loader(error, response, data);
    RefPtr<BlobResourceHandle> handle = adoptRef(new BlobResourceHandle(blobData, request, &loader, false));
    handle->start();
}

BlobResourceHandle::BlobResourceHandle(BlobData* blobData, const ResourceRequest& request, ResourceHandleClient* client, bool async)
    : BlobResourceHandleBase(async, blobData)
    , ResourceHandle { nullptr, request, client, false /* defersLoading */, false /* shouldContentSniff */, ContentEncodingSniffingPolicy::Default, nullptr /* sourceOrigin */, false /* isMainFrameNavigation */ }
{
}

BlobResourceHandle::~BlobResourceHandle() = default;

void BlobResourceHandle::cancel()
{
    clearAsyncStream();
    setIsFileOpen(false);

    m_aborted = true;

    ResourceHandle::cancel();
}

int BlobResourceHandle::readSync(std::span<uint8_t> buffer)
{
    ASSERT(isMainThread());

    ASSERT(!async());
    Ref<BlobResourceHandle> protectedThis(*this);

    int offset = 0;
    uint64_t remaining = buffer.size();
    while (remaining) {
        // Do not continue if the request is aborted or an error occurs.
        if (erroredOrAborted())
            break;

        // If there is no more remaining data to read, we are done.
        if (!totalRemainingSize() || readItemCount() >= blobData()->items().size())
            break;

        const BlobDataItem& item = blobData()->items().at(readItemCount());
        int bytesRead = 0;
        if (item.type() == BlobDataItem::Type::Data)
            bytesRead = readDataSync(item, buffer.subspan(offset));
        else if (item.type() == BlobDataItem::Type::File)
            bytesRead = readFileSync(item, buffer.subspan(offset));
        else
            ASSERT_NOT_REACHED();

        if (bytesRead > 0) {
            offset += bytesRead;
            remaining -= bytesRead;
        }
    }

    int result;
    if (erroredOrAborted())
        result = -1;
    else
        result = buffer.size() - remaining;

    if (result > 0)
        didReceiveData(buffer);

    if (!result)
        didFinish();

    return result;
}

int BlobResourceHandle::readDataSync(const BlobDataItem& item, std::span<uint8_t> buffer)
{
    ASSERT(isMainThread());

    ASSERT(!async());

    uint64_t remaining = item.length() - currentItemReadSize();
    uint64_t bytesToRead = std::min(std::min<uint64_t>(remaining, buffer.size()), totalRemainingSize());
    memcpySpan(buffer, protect(item.data())->span().subspan(item.offset() + currentItemReadSize()).first(bytesToRead));
    decrementTotalRemainingSizeBy(bytesToRead);

    setCurrentItemReadSize(currentItemReadSize() + bytesToRead);
    if (currentItemReadSize() == static_cast<uint64_t>(item.length())) {
        incrementReadItemCount();
        setCurrentItemReadSize(0);
    }

    return bytesToRead;
}

int BlobResourceHandle::readFileSync(const BlobDataItem& item, std::span<uint8_t> buffer)
{
    ASSERT(isMainThread());

    ASSERT(!async());

    if (!isFileOpen()) {
        auto bytesToRead = lengthOfItemBeingRead() - currentItemReadSize();
        if (bytesToRead > totalRemainingSize())
            bytesToRead = totalRemainingSize();
        bool success = syncStream()->openForRead(protect(item.file())->path(), item.offset() + currentItemReadSize(), bytesToRead);
        setCurrentItemReadSize(0);
        if (!success) {
            m_errorCode = Error::NotReadableError;
            return 0;
        }

        setIsFileOpen(true);
    }

    int bytesRead = syncStream()->read(buffer);
    if (bytesRead < 0) {
        m_errorCode = Error::NotReadableError;
        return 0;
    }
    if (!bytesRead) {
        syncStream()->close();
        setIsFileOpen(false);
        incrementReadItemCount();
    } else
        decrementTotalRemainingSizeBy(bytesRead);

    return bytesRead;
}

bool BlobResourceHandle::shouldAbortDispatchDidReceiveResponse()
{
    if (!client())
        return true;

    if (m_errorCode != Error::NoError) {
        didFail(m_errorCode);
        return true;
    }

    return false;
}

void BlobResourceHandle::didReceiveResponse(ResourceResponse&& response)
{
    client()->didReceiveResponseAsync(this, WTF::move(response), [this, protectedThis = Ref { *this }] {
        buffer().resize(bufferSize);
        readAsync();
    });
}

bool BlobResourceHandle::didReceiveData(std::span<const uint8_t> data)
{
    if (client())
        client()->didReceiveBuffer(this, SharedBuffer::create(data), data.size());
    return true;
}

void BlobResourceHandle::didFail(Error errorCode)
{
    if (client())
        client()->didFail(this, ResourceError(webKitBlobResourceDomain, static_cast<int>(errorCode), firstRequest().url(), String()));

    closeFileIfOpen();
}

static void doNotifyFinish(BlobResourceHandle& handle)
{
    if (handle.aborted())
        return;

    if (!handle.client())
        return;

    handle.client()->didFinishLoading(&handle, { });
}

void BlobResourceHandle::didFinish()
{
    if (!async()) {
        doNotifyFinish(*this);
        return;
    }

    // Schedule to notify the client from a standalone function because the client might dispose the handle immediately from the callback function
    // while we still have BlobResourceHandle calls in the stack.
    callOnMainThread([protectedThis = Ref { *this }]() mutable {
        doNotifyFinish(protectedThis);
    });

}

} // namespace WebCore
