/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013, 2014, 2016 Apple Inc. All rights reserved.
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
#include "ThreadableBlobRegistry.h"

#include "BlobDataFileReference.h"
#include "BlobPart.h"
#include "BlobRegistry.h"
#include "BlobURL.h"
#include "SecurityOrigin.h"
#include <mutex>
#include <wtf/CrossThreadQueue.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/text/StringHash.h>
#include <wtf/threads/BinarySemaphore.h>

using WTF::ThreadSpecific;

namespace WebCore {

typedef HashMap<String, RefPtr<SecurityOrigin>> BlobUrlOriginMap;

static ThreadSpecific<BlobUrlOriginMap>& originMap()
{
    static std::once_flag onceFlag;
    static ThreadSpecific<BlobUrlOriginMap>* map;
    std::call_once(onceFlag, []{
        map = new ThreadSpecific<BlobUrlOriginMap>;
    });

    return *map;
}

void ThreadableBlobRegistry::registerFileBlobURL(PAL::SessionID sessionID, const URL& url, const String& path, const String& contentType)
{
    if (isMainThread()) {
        blobRegistry().registerFileBlobURL(sessionID, url, BlobDataFileReference::create(path), contentType);
        return;
    }

    callOnMainThread([sessionID, url = url.isolatedCopy(), path = path.isolatedCopy(), contentType = contentType.isolatedCopy()] {
        blobRegistry().registerFileBlobURL(sessionID, url, BlobDataFileReference::create(path), contentType);
    });
}

void ThreadableBlobRegistry::registerBlobURL(PAL::SessionID sessionID, const URL& url, Vector<BlobPart>&& blobParts, const String& contentType)
{
    if (isMainThread()) {
        blobRegistry().registerBlobURL(sessionID, url, WTFMove(blobParts), contentType);
        return;
    }
    for (auto& part : blobParts)
        part.detachFromCurrentThread();
    callOnMainThread([sessionID, url = url.isolatedCopy(), blobParts = WTFMove(blobParts), contentType = contentType.isolatedCopy()]() mutable {
        blobRegistry().registerBlobURL(sessionID, url, WTFMove(blobParts), contentType);
    });
}

static inline bool isBlobURLContainsNullOrigin(const URL& url)
{
    ASSERT(url.protocolIsBlob());
    return BlobURL::getOrigin(url) == "null";
}

void ThreadableBlobRegistry::registerBlobURL(PAL::SessionID sessionID, SecurityOrigin* origin, const URL& url, const URL& srcURL)
{
    // If the blob URL contains null origin, as in the context with unique security origin or file URL, save the mapping between url and origin so that the origin can be retrived when doing security origin check.
    if (origin && isBlobURLContainsNullOrigin(url))
        originMap()->add(url.string(), origin);

    if (isMainThread()) {
        blobRegistry().registerBlobURL(sessionID, url, srcURL);
        return;
    }

    callOnMainThread([sessionID, url = url.isolatedCopy(), srcURL = srcURL.isolatedCopy()] {
        blobRegistry().registerBlobURL(sessionID, url, srcURL);
    });
}

void ThreadableBlobRegistry::registerBlobURLOptionallyFileBacked(PAL::SessionID sessionID, const URL& url, const URL& srcURL, const String& fileBackedPath, const String& contentType)
{
    if (isMainThread()) {
        blobRegistry().registerBlobURLOptionallyFileBacked(sessionID, url, srcURL, BlobDataFileReference::create(fileBackedPath), contentType);
        return;
    }
    callOnMainThread([sessionID, url = url.isolatedCopy(), srcURL = srcURL.isolatedCopy(), fileBackedPath = fileBackedPath.isolatedCopy(), contentType = contentType.isolatedCopy()] {
        blobRegistry().registerBlobURLOptionallyFileBacked(sessionID, url, srcURL, BlobDataFileReference::create(fileBackedPath), contentType);
    });
}

void ThreadableBlobRegistry::registerBlobURLForSlice(PAL::SessionID sessionID, const URL& newURL, const URL& srcURL, long long start, long long end)
{
    if (isMainThread()) {
        blobRegistry().registerBlobURLForSlice(sessionID, newURL, srcURL, start, end);
        return;
    }

    callOnMainThread([sessionID, newURL = newURL.isolatedCopy(), srcURL = srcURL.isolatedCopy(), start, end] {
        blobRegistry().registerBlobURLForSlice(sessionID, newURL, srcURL, start, end);
    });
}

unsigned long long ThreadableBlobRegistry::blobSize(PAL::SessionID sessionID, const URL& url)
{
    if (isMainThread())
        return blobRegistry().blobSize(sessionID, url);

    unsigned long long resultSize;
    BinarySemaphore semaphore;
    callOnMainThread([sessionID, url = url.isolatedCopy(), &semaphore, &resultSize] {
        resultSize = blobRegistry().blobSize(sessionID, url);
        semaphore.signal();
    });
    semaphore.wait();
    return resultSize;
}

void ThreadableBlobRegistry::unregisterBlobURL(PAL::SessionID sessionID, const URL& url)
{
    if (isBlobURLContainsNullOrigin(url))
        originMap()->remove(url.string());

    if (isMainThread()) {
        blobRegistry().unregisterBlobURL(sessionID, url);
        return;
    }
    callOnMainThread([sessionID, url = url.isolatedCopy()] {
        blobRegistry().unregisterBlobURL(sessionID, url);
    });
}

RefPtr<SecurityOrigin> ThreadableBlobRegistry::getCachedOrigin(const URL& url)
{
    if (auto cachedOrigin = originMap()->get(url.string()))
        return cachedOrigin;

    if (!url.protocolIsBlob() || !isBlobURLContainsNullOrigin(url))
        return nullptr;

    // If we do not have a cached origin for null blob URLs, we use a unique origin.
    return SecurityOrigin::createUnique();
}

} // namespace WebCore
