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
#include "Blob.h"

#include "BlobURL.h"
#include "File.h"
#include "ScriptExecutionContext.h"
#include "ThreadableBlobRegistry.h"
#include <wtf/text/CString.h>

namespace WebCore {

class BlobURLRegistry final : public URLRegistry {
public:
    virtual void registerURL(SecurityOrigin*, const URL&, URLRegistrable*) override;
    virtual void unregisterURL(const URL&) override;

    static URLRegistry& registry();
};


void BlobURLRegistry::registerURL(SecurityOrigin* origin, const URL& publicURL, URLRegistrable* blob)
{
    ASSERT(&blob->registry() == this);
    ThreadableBlobRegistry::registerBlobURL(origin, publicURL, static_cast<Blob*>(blob)->url());
}

void BlobURLRegistry::unregisterURL(const URL& url)
{
    ThreadableBlobRegistry::unregisterBlobURL(url);
}

URLRegistry& BlobURLRegistry::registry()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(BlobURLRegistry, instance, ());
    return instance;
}


Blob::Blob(UninitializedContructor)
{
}

Blob::Blob()
    : m_size(0)
{
    m_internalURL = BlobURL::createInternalURL();
    ThreadableBlobRegistry::registerBlobURL(m_internalURL, Vector<BlobPart>(), String());
}

Blob::Blob(Vector<char> data, const String& contentType)
    : m_type(contentType)
    , m_size(data.size())
{
    Vector<BlobPart> blobParts;
    blobParts.append(BlobPart(WTF::move(data)));
    m_internalURL = BlobURL::createInternalURL();
    ThreadableBlobRegistry::registerBlobURL(m_internalURL, WTF::move(blobParts), contentType);
}

Blob::Blob(Vector<BlobPart> blobParts, const String& contentType)
    : m_type(contentType)
    , m_size(-1)
{
    m_internalURL = BlobURL::createInternalURL();
    ThreadableBlobRegistry::registerBlobURL(m_internalURL, WTF::move(blobParts), contentType);
}

Blob::Blob(DeserializationContructor, const URL& srcURL, const String& type, long long size)
    : m_type(Blob::normalizedContentType(type))
    , m_size(size)
{
    m_internalURL = BlobURL::createInternalURL();
    ThreadableBlobRegistry::registerBlobURL(0, m_internalURL, srcURL);
}

Blob::Blob(const URL& srcURL, long long start, long long end, const String& type)
    : m_type(Blob::normalizedContentType(type))
    , m_size(-1) // size is not necessarily equal to end - start.
{
    m_internalURL = BlobURL::createInternalURL();
    ThreadableBlobRegistry::registerBlobURLForSlice(m_internalURL, srcURL, start, end);
}

Blob::~Blob()
{
    ThreadableBlobRegistry::unregisterBlobURL(m_internalURL);
}

unsigned long long Blob::size() const
{
    if (m_size < 0) {
        // FIXME: JavaScript cannot represent sizes as large as unsigned long long, we need to
        // come up with an exception to throw if file size is not representable.
        unsigned long long actualSize = ThreadableBlobRegistry::blobSize(m_internalURL);
        m_size = (WTF::isInBounds<long long>(actualSize)) ? static_cast<long long>(actualSize) : 0;
    }

    return static_cast<unsigned long long>(m_size);
}

bool Blob::isValidContentType(const String& contentType)
{
    if (contentType.isNull())
        return true;

    size_t length = contentType.length();
    if (contentType.is8Bit()) {
        const LChar* characters = contentType.characters8();
        for (size_t i = 0; i < length; ++i) {
            if (characters[i] < 0x20 || characters[i] > 0x7e)
                return false;
        }
    } else {
        const UChar* characters = contentType.characters16();
        for (size_t i = 0; i < length; ++i) {
            if (characters[i] < 0x20 || characters[i] > 0x7e)
                return false;
        }
    }
    return true;
}

String Blob::normalizedContentType(const String& contentType)
{
    if (Blob::isValidContentType(contentType))
        return contentType.lower();
    return emptyString();
}

bool Blob::isNormalizedContentType(const String& contentType)
{
    if (contentType.isNull())
        return true;

    size_t length = contentType.length();
    if (contentType.is8Bit()) {
        const LChar* characters = contentType.characters8();
        for (size_t i = 0; i < length; ++i) {
            if (characters[i] < 0x20 || characters[i] > 0x7e)
                return false;
            if (characters[i] >= 'A' && characters[i] <= 'Z')
                return false;
        }
    } else {
        const UChar* characters = contentType.characters16();
        for (size_t i = 0; i < length; ++i) {
            if (characters[i] < 0x20 || characters[i] > 0x7e)
                return false;
            if (characters[i] >= 'A' && characters[i] <= 'Z')
                return false;
        }
    }
    return true;
}

bool Blob::isNormalizedContentType(const CString& contentType)
{
    size_t length = contentType.length();
    const char* characters = contentType.data();
    for (size_t i = 0; i < length; ++i) {
        if (characters[i] < 0x20 || characters[i] > 0x7e)
            return false;
        if (characters[i] >= 'A' && characters[i] <= 'Z')
            return false;
    }
    return true;
}

URLRegistry& Blob::registry() const
{
    return BlobURLRegistry::registry();
}


} // namespace WebCore
