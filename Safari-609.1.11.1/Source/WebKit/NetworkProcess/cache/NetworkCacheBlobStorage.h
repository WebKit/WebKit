/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef NetworkCacheBlobStorage_h
#define NetworkCacheBlobStorage_h

#include "NetworkCacheData.h"
#include "NetworkCacheKey.h"
#include <wtf/SHA1.h>

namespace WebKit {
namespace NetworkCache {

// BlobStorage deduplicates the data using SHA1 hash computed over the blob bytes.
class BlobStorage {
    WTF_MAKE_NONCOPYABLE(BlobStorage);
public:
    BlobStorage(const String& blobDirectoryPath, Salt);

    struct Blob {
        Data data;
        SHA1::Digest hash;
    };
    // These are all synchronous and should not be used from the main thread.
    Blob add(const String& path, const Data&);
    Blob get(const String& path);

    // Blob won't be removed until synchronization.
    void remove(const String& path);

    unsigned shareCount(const String& path);

    size_t approximateSize() const { return m_approximateSize; }

    void synchronize();

private:
    String blobDirectoryPathIsolatedCopy() const;
    String blobPathForHash(const SHA1::Digest&) const;

    const String m_blobDirectoryPath;
    const Salt m_salt;

    std::atomic<size_t> m_approximateSize { 0 };
};

}
}

#endif
