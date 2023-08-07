/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "NetworkProcessPlatformStrategies.h"

#include <WebCore/BlobRegistry.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

void NetworkProcessPlatformStrategies::initialize()
{
    static NeverDestroyed<NetworkProcessPlatformStrategies> platformStrategies;
    setPlatformStrategies(&platformStrategies.get());
}

LoaderStrategy* NetworkProcessPlatformStrategies::createLoaderStrategy()
{
    return nullptr;
}

PasteboardStrategy* NetworkProcessPlatformStrategies::createPasteboardStrategy()
{
    return nullptr;
}

MediaStrategy* NetworkProcessPlatformStrategies::createMediaStrategy()
{
    return nullptr;
}

BlobRegistry* NetworkProcessPlatformStrategies::createBlobRegistry()
{
    using namespace WebCore;
    class EmptyBlobRegistry : public WebCore::BlobRegistry {
        void registerInternalFileBlobURL(const URL&, Ref<BlobDataFileReference>&&, const String& path, const String& contentType) final { ASSERT_NOT_REACHED(); }
        void registerInternalBlobURL(const URL&, Vector<BlobPart>&&, const String& contentType) final { ASSERT_NOT_REACHED(); }
        void registerBlobURL(const URL&, const URL& srcURL, const PolicyContainer&, const std::optional<SecurityOriginData>&) final { ASSERT_NOT_REACHED(); }
        void registerInternalBlobURLOptionallyFileBacked(const URL&, const URL& srcURL, RefPtr<BlobDataFileReference>&&, const String& contentType) final { ASSERT_NOT_REACHED(); }
        void registerInternalBlobURLForSlice(const URL&, const URL& srcURL, long long start, long long end, const String& contentType) final { ASSERT_NOT_REACHED(); }
        void unregisterBlobURL(const URL&, const std::optional<WebCore::SecurityOriginData>&) final { ASSERT_NOT_REACHED(); }
        unsigned long long blobSize(const URL&) final { ASSERT_NOT_REACHED(); return 0; }
        void writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&&) final { ASSERT_NOT_REACHED(); }
        void registerBlobURLHandle(const URL&, const std::optional<WebCore::SecurityOriginData>&) final { ASSERT_NOT_REACHED(); }
        void unregisterBlobURLHandle(const URL&, const std::optional<WebCore::SecurityOriginData>&) final { ASSERT_NOT_REACHED(); }
    };
    static NeverDestroyed<EmptyBlobRegistry> blobRegistry;
    return &blobRegistry.get();
}

}
