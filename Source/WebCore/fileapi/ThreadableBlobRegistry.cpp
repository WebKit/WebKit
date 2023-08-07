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
#include "CrossOriginOpenerPolicy.h"
#include "PolicyContainer.h"
#include "SecurityOrigin.h"
#include "URLKeepingBlobAlive.h"
#include <mutex>
#include <wtf/CrossThreadQueue.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/text/StringHash.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {

using BlobURLOriginMap = HashMap<String, RefPtr<SecurityOrigin>>;

static BlobURLOriginMap& originMap()
{
    static MainThreadNeverDestroyed<BlobURLOriginMap> map;
    return map;
}

static HashCountedSet<String>& blobURLReferencesMap()
{
    static MainThreadNeverDestroyed<HashCountedSet<String>> map;
    return map;
}

static inline bool isBlobURLContainingNullOrigin(const URL& url)
{
    ASSERT(url.protocolIsBlob());
    unsigned startIndex = url.pathStart();
    unsigned endIndex = url.pathAfterLastSlash();
    return StringView(url.string()).substring(startIndex, endIndex - startIndex - 1) == "null"_s;
}

// If the blob URL contains null origin, as in the context with unique security origin or file URL, save the mapping between url and origin so that the origin can be retrived when doing security origin check.
static void addToOriginMapIfNecessary(const URL& url, RefPtr<SecurityOrigin>&& origin)
{
    if (!origin || !isBlobURLContainingNullOrigin(url))
        return;

    auto urlWithoutFragment = url.stringWithoutFragmentIdentifier();
    originMap().add(urlWithoutFragment, WTFMove(origin));
    blobURLReferencesMap().add(urlWithoutFragment);
};

void ThreadableBlobRegistry::registerInternalFileBlobURL(const URL& url, const String& path, const String& replacementPath, const String& contentType)
{
    ASSERT(BlobURL::isInternalURL(url));
    String effectivePath = !replacementPath.isNull() ? replacementPath : path;

    if (isMainThread()) {
        blobRegistry().registerInternalFileBlobURL(url, BlobDataFileReference::create(effectivePath), path, contentType);
        return;
    }

    callOnMainThread([url = url.isolatedCopy(), effectivePath = effectivePath.isolatedCopy(), path = path.isolatedCopy(), contentType = contentType.isolatedCopy()] {
        blobRegistry().registerInternalFileBlobURL(url, BlobDataFileReference::create(effectivePath), path, contentType);
    });
}

void ThreadableBlobRegistry::registerInternalBlobURL(const URL& url, Vector<BlobPart>&& blobParts, const String& contentType)
{
    ASSERT(BlobURL::isInternalURL(url));
    if (isMainThread()) {
        blobRegistry().registerInternalBlobURL(url, WTFMove(blobParts), contentType);
        return;
    }
    for (auto& part : blobParts)
        part.detachFromCurrentThread();
    callOnMainThread([url = url.isolatedCopy(), blobParts = WTFMove(blobParts), contentType = contentType.isolatedCopy()]() mutable {
        blobRegistry().registerInternalBlobURL(url, WTFMove(blobParts), contentType);
    });
}

static void unregisterBlobURLOriginIfNecessaryOnMainThread(const URL& url)
{
    ASSERT(isMainThread());
    if (!isBlobURLContainingNullOrigin(url))
        return;

    auto urlWithoutFragment = url.stringWithoutFragmentIdentifier();
    if (blobURLReferencesMap().remove(urlWithoutFragment))
        originMap().remove(urlWithoutFragment);
}

void ThreadableBlobRegistry::registerBlobURL(SecurityOrigin* origin, PolicyContainer&& policyContainer, const URL& url, const URL& srcURL, const std::optional<SecurityOriginData>& topOrigin)
{
    if (isMainThread()) {
        addToOriginMapIfNecessary(url, origin);
        blobRegistry().registerBlobURL(url, srcURL, policyContainer, topOrigin);
        return;
    }

    RefPtr<SecurityOrigin> strongOrigin;
    if (origin)
        strongOrigin = origin->isolatedCopy();

    callOnMainThread([url = url.isolatedCopy(), srcURL = srcURL.isolatedCopy(), policyContainer = crossThreadCopy(WTFMove(policyContainer)), strongOrigin = WTFMove(strongOrigin), topOrigin = crossThreadCopy(topOrigin)]() mutable {
        addToOriginMapIfNecessary(url, WTFMove(strongOrigin));
        blobRegistry().registerBlobURL(url, srcURL, policyContainer, topOrigin);
    });
}

void ThreadableBlobRegistry::registerBlobURL(SecurityOrigin* origin, PolicyContainer&& policyContainer, const URLKeepingBlobAlive& url, const URL& srcURL)
{
    registerBlobURL(origin, std::forward<PolicyContainer>(policyContainer), url, srcURL, url.topOrigin());
}

void ThreadableBlobRegistry::registerInternalBlobURLOptionallyFileBacked(const URL& url, const URL& srcURL, const String& fileBackedPath, const String& contentType)
{
    ASSERT(BlobURL::isInternalURL(url));
    if (isMainThread()) {
        blobRegistry().registerInternalBlobURLOptionallyFileBacked(url, srcURL, BlobDataFileReference::create(fileBackedPath), contentType);
        return;
    }
    callOnMainThread([url = url.isolatedCopy(), srcURL = srcURL.isolatedCopy(), fileBackedPath = fileBackedPath.isolatedCopy(), contentType = contentType.isolatedCopy()] {
        blobRegistry().registerInternalBlobURLOptionallyFileBacked(url, srcURL, BlobDataFileReference::create(fileBackedPath), contentType);
    });
}

void ThreadableBlobRegistry::registerInternalBlobURLForSlice(const URL& newURL, const URL& srcURL, long long start, long long end, const String& contentType)
{
    ASSERT(BlobURL::isInternalURL(newURL));
    if (isMainThread()) {
        blobRegistry().registerInternalBlobURLForSlice(newURL, srcURL, start, end, contentType);
        return;
    }

    callOnMainThread([newURL = newURL.isolatedCopy(), srcURL = srcURL.isolatedCopy(), start, end, contentType = contentType.isolatedCopy()] {
        blobRegistry().registerInternalBlobURLForSlice(newURL, srcURL, start, end, contentType);
    });
}

unsigned long long ThreadableBlobRegistry::blobSize(const URL& url)
{
    if (isMainThread())
        return blobRegistry().blobSize(url);

    unsigned long long resultSize;
    callOnMainThreadAndWait([url = url.isolatedCopy(), &resultSize] {
        resultSize = blobRegistry().blobSize(url);
    });
    return resultSize;
}

void ThreadableBlobRegistry::unregisterBlobURL(const URL& url, const std::optional<SecurityOriginData>& topOrigin)
{
    ensureOnMainThread([url = url.isolatedCopy(), topOrigin = crossThreadCopy(topOrigin)] {
        unregisterBlobURLOriginIfNecessaryOnMainThread(url);
        blobRegistry().unregisterBlobURL(url, topOrigin);
    });
}

void ThreadableBlobRegistry::unregisterBlobURL(const URLKeepingBlobAlive& url)
{
    unregisterBlobURL(url, url.topOrigin());
}

void ThreadableBlobRegistry::registerBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& topOrigin)
{
    ensureOnMainThread([url = url.isolatedCopy(), topOrigin = crossThreadCopy(topOrigin)] {
        if (isBlobURLContainingNullOrigin(url))
            blobURLReferencesMap().add(url.stringWithoutFragmentIdentifier());

        blobRegistry().registerBlobURLHandle(url, topOrigin);
    });
}

void ThreadableBlobRegistry::unregisterBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& topOrigin)
{
    ensureOnMainThread([url = url.isolatedCopy(), topOrigin = crossThreadCopy(topOrigin)] {
        unregisterBlobURLOriginIfNecessaryOnMainThread(url);
        blobRegistry().unregisterBlobURLHandle(url, topOrigin);
    });
}

RefPtr<SecurityOrigin> ThreadableBlobRegistry::getCachedOrigin(const URL& url)
{
    ASSERT(url.protocolIsBlob());
    RefPtr<SecurityOrigin> cachedOrigin;

    bool wasOnMainThread = isMainThread();
    callOnMainThreadAndWait([url = url.isolatedCopy(), wasOnMainThread, &cachedOrigin] {
        if (auto* origin = originMap().get<StringViewHashTranslator>(url.viewWithoutFragmentIdentifier()))
            cachedOrigin = wasOnMainThread ? Ref { *origin } : origin->isolatedCopy();
    });
    if (cachedOrigin)
        return cachedOrigin;

    if (!isBlobURLContainingNullOrigin(url))
        return nullptr;

    // If we do not have a cached origin for null blob URLs, we use an opaque origin.
    return SecurityOrigin::createOpaque();
}

} // namespace WebCore
