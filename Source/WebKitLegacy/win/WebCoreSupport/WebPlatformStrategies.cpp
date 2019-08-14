/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#include "WebPlatformStrategies.h"

#include "WebFrameNetworkingContext.h"
#include "WebResourceLoadScheduler.h"
#include <WebCore/BlobRegistry.h>
#include <WebCore/BlobRegistryImpl.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>

#if USE(CFURLCONNECTION)
#include <pal/spi/cf/CFNetworkSPI.h>
#endif

using namespace WebCore;

void WebPlatformStrategies::initialize()
{
    static NeverDestroyed<WebPlatformStrategies> platformStrategies;
}

WebPlatformStrategies::WebPlatformStrategies()
{
    setPlatformStrategies(this);
}

LoaderStrategy* WebPlatformStrategies::createLoaderStrategy()
{
    return new WebResourceLoadScheduler;
}

PasteboardStrategy* WebPlatformStrategies::createPasteboardStrategy()
{
    return nullptr;
}

class WebBlobRegistry final : public BlobRegistry {
private:
    void registerFileBlobURL(PAL::SessionID, const URL& url, Ref<BlobDataFileReference>&& reference, const String& contentType) final { m_blobRegistry.registerFileBlobURL(url, WTFMove(reference), contentType); }
    void registerBlobURL(PAL::SessionID, const URL& url, Vector<BlobPart>&& parts, const String& contentType) final { m_blobRegistry.registerBlobURL(url, WTFMove(parts), contentType); }
    void registerBlobURL(PAL::SessionID, const URL& url, const URL& srcURL) final { m_blobRegistry.registerBlobURL(url, srcURL); }
    void registerBlobURLOptionallyFileBacked(PAL::SessionID, const URL& url, const URL& srcURL, RefPtr<BlobDataFileReference>&& reference, const String& contentType) final { m_blobRegistry.registerBlobURLOptionallyFileBacked(url, srcURL, WTFMove(reference), contentType); }
    void registerBlobURLForSlice(PAL::SessionID, const URL& url, const URL& srcURL, long long start, long long end) final { m_blobRegistry.registerBlobURLForSlice(url, srcURL, start, end); }
    void unregisterBlobURL(PAL::SessionID, const URL& url) final { m_blobRegistry.unregisterBlobURL(url); }
    unsigned long long blobSize(PAL::SessionID, const URL& url) final { return m_blobRegistry.blobSize(url); }
    void writeBlobsToTemporaryFiles(PAL::SessionID, const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&& completionHandler) final { m_blobRegistry.writeBlobsToTemporaryFiles(blobURLs, WTFMove(completionHandler)); }

    BlobRegistryImpl* blobRegistryImpl() final { return &m_blobRegistry; }

    BlobRegistryImpl m_blobRegistry;
};

BlobRegistry* WebPlatformStrategies::createBlobRegistry()
{
    return new WebBlobRegistry;
}
