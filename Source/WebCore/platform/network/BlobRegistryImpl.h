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

#pragma once

#include "BlobData.h"
#include "BlobRegistry.h"
#include <wtf/HashMap.h>
#include <wtf/URLHash.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ResourceHandle;
class ResourceHandleClient;
class ResourceRequest;
class ThreadSafeDataBuffer;

// BlobRegistryImpl is not thread-safe. It should only be called from main thread.
class WEBCORE_EXPORT BlobRegistryImpl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~BlobRegistryImpl();

    BlobData* getBlobDataFromURL(const URL&) const;

    Ref<ResourceHandle> createResourceHandle(const ResourceRequest&, ResourceHandleClient*);
    void writeBlobToFilePath(const URL& blobURL, const String& path, Function<void(bool success)>&& completionHandler);

    void appendStorageItems(BlobData*, const BlobDataItemList&, long long offset, long long length);

    void registerFileBlobURL(const URL&, Ref<BlobDataFileReference>&&, const String& contentType);
    void registerBlobURL(const URL&, Vector<BlobPart>&&, const String& contentType);
    void registerBlobURL(const URL&, const URL& srcURL);
    void registerBlobURLOptionallyFileBacked(const URL&, const URL& srcURL, RefPtr<BlobDataFileReference>&&, const String& contentType);
    void registerBlobURLForSlice(const URL&, const URL& srcURL, long long start, long long end);
    void unregisterBlobURL(const URL&);

    unsigned long long blobSize(const URL&);

    void writeBlobsToTemporaryFiles(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&&);

    struct BlobForFileWriting {
        String blobURL;
        Vector<std::pair<String, ThreadSafeDataBuffer>> filePathsOrDataBuffers;
    };

    bool populateBlobsForFileWriting(const Vector<String>& blobURLs, Vector<BlobForFileWriting>&);
    Vector<RefPtr<BlobDataFileReference>> filesInBlob(const URL&) const;

private:
    HashMap<String, RefPtr<BlobData>> m_blobs;
};

} // namespace WebCore
