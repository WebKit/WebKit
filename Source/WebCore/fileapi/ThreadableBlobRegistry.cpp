/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "BlobPart.h"
#include "BlobRegistry.h"
#include "BlobURL.h"
#include "SecurityOrigin.h"
#include <mutex>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/text/StringHash.h>
#include <wtf/threads/BinarySemaphore.h>

using WTF::ThreadSpecific;

namespace WebCore {

struct BlobRegistryContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BlobRegistryContext(const URL& url, Vector<BlobPart> blobParts, const String& contentType)
        : url(url.copy())
        , contentType(contentType.isolatedCopy())
        , blobParts(std::move(blobParts))
    {
        for (BlobPart& part : blobParts)
            part.detachFromCurrentThread();
    }

    BlobRegistryContext(const URL& url, const URL& srcURL)
        : url(url.copy())
        , srcURL(srcURL.copy())
    {
    }

    BlobRegistryContext(const URL& url)
        : url(url.copy())
    {
    }

    BlobRegistryContext(const URL& url, const String& path, const String& contentType)
        : url(url.copy())
        , path(path.isolatedCopy())
        , contentType(contentType.isolatedCopy())
    {
    }

    URL url;
    URL srcURL;
    String path;
    String contentType;
    Vector<BlobPart> blobParts;
};

#if ENABLE(BLOB)

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

void ThreadableBlobRegistry::registerFileBlobURL(const URL& url, const String& path, const String& contentType)
{
    if (isMainThread())
        blobRegistry().registerFileBlobURL(url, path, contentType);
    else {
        // BlobRegistryContext performs an isolated copy of data.
        BlobRegistryContext* context = new BlobRegistryContext(url, path, contentType);
        BinarySemaphore semaphore;
        callOnMainThread([context] {
            std::unique_ptr<BlobRegistryContext> blobRegistryContext(context);
            blobRegistry().registerFileBlobURL(blobRegistryContext->url, blobRegistryContext->path, blobRegistryContext->contentType);
        });
    }
}

unsigned long long ThreadableBlobRegistry::registerBlobURL(const URL& url, Vector<BlobPart> blobParts, const String& contentType)
{
    unsigned long long resultSize;
    if (isMainThread())
        resultSize = blobRegistry().registerBlobURL(url, std::move(blobParts), contentType);
    else {
        // BlobRegistryContext performs an isolated copy of data.
        BlobRegistryContext* context = new BlobRegistryContext(url, std::move(blobParts), contentType);
        BinarySemaphore semaphore;
        callOnMainThread([context, &semaphore, &resultSize] {
            std::unique_ptr<BlobRegistryContext> blobRegistryContext(context);
            resultSize = blobRegistry().registerBlobURL(blobRegistryContext->url, std::move(blobRegistryContext->blobParts), blobRegistryContext->contentType);
            semaphore.signal();
        });
        semaphore.wait(std::numeric_limits<double>::max());
    }
    return resultSize;
}

void ThreadableBlobRegistry::registerBlobURL(SecurityOrigin* origin, const URL& url, const URL& srcURL)
{
    // If the blob URL contains null origin, as in the context with unique security origin or file URL, save the mapping between url and origin so that the origin can be retrived when doing security origin check.
    if (origin && BlobURL::getOrigin(url) == "null")
        originMap()->add(url.string(), origin);

    if (isMainThread())
        blobRegistry().registerBlobURL(url, srcURL);
    else {
        // BlobRegistryContext performs an isolated copy of data.
        BlobRegistryContext* context = new BlobRegistryContext(url, srcURL);
        callOnMainThread([context] {
            std::unique_ptr<BlobRegistryContext> blobRegistryContext(context);
            blobRegistry().registerBlobURL(blobRegistryContext->url, blobRegistryContext->srcURL);
        });
    }
}

unsigned long long ThreadableBlobRegistry::registerBlobURLForSlice(const URL& newURL, const URL& srcURL, long long start, long long end)
{
    unsigned long long resultSize;
    if (isMainThread())
        resultSize = blobRegistry().registerBlobURLForSlice(newURL, srcURL, start, end);
    else {
        // BlobRegistryContext performs an isolated copy of data.
        BlobRegistryContext* context = new BlobRegistryContext(newURL, srcURL);
        BinarySemaphore semaphore;
        callOnMainThread([context, start, end, &semaphore, &resultSize] {
            std::unique_ptr<BlobRegistryContext> blobRegistryContext(context);
            resultSize = blobRegistry().registerBlobURLForSlice(blobRegistryContext->url, blobRegistryContext->srcURL, start, end);
            semaphore.signal();
        });
        semaphore.wait(std::numeric_limits<double>::max());
    }
    return resultSize;
}

static void unregisterBlobURLTask(void* context)
{
    std::unique_ptr<BlobRegistryContext> blobRegistryContext(static_cast<BlobRegistryContext*>(context));
    blobRegistry().unregisterBlobURL(blobRegistryContext->url);
}

void ThreadableBlobRegistry::unregisterBlobURL(const URL& url)
{
    if (BlobURL::getOrigin(url) == "null")
        originMap()->remove(url.string());

    if (isMainThread())
        blobRegistry().unregisterBlobURL(url);
    else
        callOnMainThread(&unregisterBlobURLTask, new BlobRegistryContext(url));
}

PassRefPtr<SecurityOrigin> ThreadableBlobRegistry::getCachedOrigin(const URL& url)
{
    return originMap()->get(url.string());
}

#else

void ThreadableBlobRegistry::registerFileBlobURL(const URL&, const String&, const String&)
{
}

unsigned long long ThreadableBlobRegistry::registerBlobURL(const URL&, Vector<BlobPart>, const String&)
{
    return 0;
}

void ThreadableBlobRegistry::registerBlobURL(SecurityOrigin*, const URL&, const URL&)
{
}

unsigned long long ThreadableBlobRegistry::registerBlobURLForSlice(const URL&, const URL&, long long, long long)
{
    return 0;
}

void ThreadableBlobRegistry::unregisterBlobURL(const URL&)
{
}

PassRefPtr<SecurityOrigin> ThreadableBlobRegistry::getCachedOrigin(const URL&)
{
    return 0;
}

#endif // ENABL(BLOB)

} // namespace WebCore
