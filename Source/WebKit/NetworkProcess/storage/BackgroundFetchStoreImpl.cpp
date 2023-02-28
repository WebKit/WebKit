/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "BackgroundFetchStoreImpl.h"

#if ENABLE(SERVICE_WORKER)

#include <WebCore/ResourceError.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {

using namespace WebCore;

// FIXME: Handle quota.

BackgroundFetchStoreImpl::BackgroundFetchStoreImpl()
{
}

BackgroundFetchStoreImpl::~BackgroundFetchStoreImpl()
{
}

void BackgroundFetchStoreImpl::initializeFetches(BackgroundFetchEngine& engine, const ServiceWorkerRegistrationKey& key, CompletionHandler<void()>&& callback)
{
    callback();
}

void BackgroundFetchStoreImpl::clearFetch(const ServiceWorkerRegistrationKey& key, const String& identifier, CompletionHandler<void()>&& callback)
{
    callback();
}

void BackgroundFetchStoreImpl::clearAllFetches(const ServiceWorkerRegistrationKey& key, CompletionHandler<void()>&& callback)
{
    callback();
}

void BackgroundFetchStoreImpl::storeFetch(const ServiceWorkerRegistrationKey& key, const String& identifier, Vector<uint8_t>&& fetch, CompletionHandler<void(StoreResult)>&& callback)
{
    callback(StoreResult::OK);
}

void BackgroundFetchStoreImpl::storeFetchResponseBodyChunk(const ServiceWorkerRegistrationKey& key, const String& identifier, size_t index, const SharedBuffer& data, CompletionHandler<void(StoreResult)>&& callback)
{
    callback(StoreResult::OK);
}

void BackgroundFetchStoreImpl::retrieveResponseBody(const ServiceWorkerRegistrationKey& key, const String& identifier, size_t index, RetrieveRecordResponseBodyCallback&& callback)
{
    callback(RefPtr<SharedBuffer> { });
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
