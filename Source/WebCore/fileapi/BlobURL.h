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

#pragma once

#include <wtf/URL.h>

namespace WebCore {

class SecurityOrigin;

// Blob URLs are of the form
//     blob:%escaped_origin%/%UUID%
// For public urls, the origin of the host page is encoded in the URL value to
// allow easy lookup of the origin when security checks need to be performed.
// When loading blobs via ResourceHandle or when reading blobs via FileReader
// the loader conducts security checks that examine the origin of host page
// encoded in the public blob url. The origin baked into internal blob urls
// is a simple constant value, "blobinternal://", internal urls should not
// be used with ResourceHandle or FileReader.
class BlobURL {
public:
    static URL createPublicURL(SecurityOrigin*);
    static URL createInternalURL();

    static URL getOriginURL(const URL&);
    static bool isSecureBlobURL(const URL&);

private:
    static URL createBlobURL(const String& originString);
    BlobURL() { }
};

// Extends the lifetime of the Blob URL. This means that the blob URL will remain valid after
// revokeObjectURL() has been called, as long as BlobURLHandle objects refer to the blob URL.
class BlobURLHandle {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BlobURLHandle() = default;
    explicit BlobURLHandle(const URL&);
    ~BlobURLHandle();

    BlobURLHandle(const BlobURLHandle&);
    BlobURLHandle(BlobURLHandle&& other)
        : m_url(std::exchange(other.m_url, { }))
    { }

    BlobURLHandle& operator=(const BlobURLHandle&);
    BlobURLHandle& operator=(BlobURLHandle&&);
    BlobURLHandle& operator=(const URL&);

    URL url() const { return m_url.isolatedCopy(); }

    void clear();

private:
    void unregisterBlobURLHandleIfNecessary();
    void registerBlobURLHandleIfNecessary();

    URL m_url;
};

} // namespace WebCore
