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

#ifndef Blob_h
#define Blob_h

#include "BlobPart.h"
#include "ScriptWrappable.h"
#include "URLRegistry.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ScriptExecutionContext;

class Blob : public ScriptWrappable, public URLRegistrable, public RefCounted<Blob> {
public:
    static PassRefPtr<Blob> create()
    {
        return adoptRef(new Blob);
    }

    static PassRefPtr<Blob> create(Vector<char> data, const String& contentType)
    {
        return adoptRef(new Blob(WTF::move(data), contentType));
    }

    static PassRefPtr<Blob> create(Vector<BlobPart> blobParts, const String& contentType)
    {
        return adoptRef(new Blob(WTF::move(blobParts), contentType));
    }

    static PassRefPtr<Blob> deserialize(const URL& srcURL, const String& type, long long size)
    {
        ASSERT(Blob::isNormalizedContentType(type));
        return adoptRef(new Blob(deserializationContructor, srcURL, type, size));
    }

    virtual ~Blob();

    const URL& url() const { return m_internalURL; }
    const String& type() const { return m_type; }

    unsigned long long size() const;
    virtual bool isFile() const { return false; }

    // The checks described in the File API spec.
    static bool isValidContentType(const String&);
    // The normalization procedure described in the File API spec.
    static String normalizedContentType(const String&);
    // Intended for use in ASSERT statements.
    static bool isNormalizedContentType(const String&);
    static bool isNormalizedContentType(const CString&);

    // URLRegistrable
    virtual URLRegistry& registry() const override;

    PassRefPtr<Blob> slice(long long start = 0, long long end = std::numeric_limits<long long>::max(), const String& contentType = String()) const
    {
        return adoptRef(new Blob(m_internalURL, start, end, contentType));
    }

protected:
    Blob();
    Blob(Vector<char>, const String& contentType);
    Blob(Vector<BlobPart>, const String& contentType);

    enum UninitializedContructor { uninitializedContructor };
    Blob(UninitializedContructor);

    enum DeserializationContructor { deserializationContructor };
    Blob(DeserializationContructor, const URL& srcURL, const String& type, long long size);

    // For slicing.
    Blob(const URL& srcURL, long long start, long long end, const String& contentType);

    // This is an internal URL referring to the blob data associated with this object. It serves
    // as an identifier for this blob. The internal URL is never used to source the blob's content
    // into an HTML or for FileRead'ing, public blob URLs must be used for those purposes.
    URL m_internalURL;

    String m_type;
    mutable long long m_size;
};

} // namespace WebCore

#endif // Blob_h

