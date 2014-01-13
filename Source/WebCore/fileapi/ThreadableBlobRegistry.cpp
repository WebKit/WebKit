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

#include "BlobData.h"
#include "BlobRegistry.h"
#include "BlobURL.h"
#include "SecurityOrigin.h"
#include <mutex>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/text/StringHash.h>

using WTF::ThreadSpecific;

namespace WebCore {

struct BlobRegistryContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BlobRegistryContext(const URL& url, std::unique_ptr<BlobData> blobData)
        : url(url.copy())
        , blobData(std::move(blobData))
    {
        this->blobData->detachFromCurrentThread();
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

    URL url;
    URL srcURL;
    std::unique_ptr<BlobData> blobData;
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

static void registerBlobURLTask(void* context)
{
    std::unique_ptr<BlobRegistryContext> blobRegistryContext(static_cast<BlobRegistryContext*>(context));
    blobRegistry().registerBlobURL(blobRegistryContext->url, std::move(blobRegistryContext->blobData));
}

void ThreadableBlobRegistry::registerBlobURL(const URL& url, std::unique_ptr<BlobData> blobData)
{
    if (isMainThread())
        blobRegistry().registerBlobURL(url, std::move(blobData));
    else
        callOnMainThread(&registerBlobURLTask, new BlobRegistryContext(url, std::move(blobData)));
}

static void registerBlobURLFromTask(void* context)
{
    std::unique_ptr<BlobRegistryContext> blobRegistryContext(static_cast<BlobRegistryContext*>(context));
    blobRegistry().registerBlobURL(blobRegistryContext->url, blobRegistryContext->srcURL);
}

void ThreadableBlobRegistry::registerBlobURL(SecurityOrigin* origin, const URL& url, const URL& srcURL)
{
    // If the blob URL contains null origin, as in the context with unique security origin or file URL, save the mapping between url and origin so that the origin can be retrived when doing security origin check.
    if (origin && BlobURL::getOrigin(url) == "null")
        originMap()->add(url.string(), origin);

    if (isMainThread())
        blobRegistry().registerBlobURL(url, srcURL);
    else
        callOnMainThread(&registerBlobURLFromTask, new BlobRegistryContext(url, srcURL));
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

void ThreadableBlobRegistry::registerBlobURL(const URL&, std::unique_ptr<BlobData>)
{
}

void ThreadableBlobRegistry::registerBlobURL(SecurityOrigin*, const URL&, const URL&)
{
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
