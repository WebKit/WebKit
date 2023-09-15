/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/BlobRegistry.h>

namespace WebKit {

class BlobRegistryProxy final : public WebCore::BlobRegistry {
public:
    void registerInternalFileBlobURL(const URL&, Ref<WebCore::BlobDataFileReference>&&, const String& path, const String& contentType) final;
    void registerInternalBlobURL(const URL&, Vector<WebCore::BlobPart>&&, const String& contentType) final;
    void registerBlobURL(const URL&, const URL& srcURL, const WebCore::PolicyContainer&, const std::optional<WebCore::SecurityOriginData>& topOrigin) final;
    void registerInternalBlobURLOptionallyFileBacked(const URL&, const URL& srcURL, RefPtr<WebCore::BlobDataFileReference>&&, const String& contentType) final;
    void unregisterBlobURL(const URL&, const std::optional<WebCore::SecurityOriginData>& topOrigin) final;
    void registerInternalBlobURLForSlice(const URL&, const URL& srcURL, long long start, long long end, const String& contentType) final;
    unsigned long long blobSize(const URL&) final;
    void writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&&) final;
    void registerBlobURLHandle(const URL&, const std::optional<WebCore::SecurityOriginData>& topOrigin) final;
    void unregisterBlobURLHandle(const URL&, const std::optional<WebCore::SecurityOriginData>& topOrigin) final;
};

}
