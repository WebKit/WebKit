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

#include "BlobURL.h"
#include "Document.h"
#include "SecurityOrigin.h"
#include "ThreadableBlobRegistry.h"

#include <wtf/URL.h>
#include <wtf/UUID.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

const char* kBlobProtocol = "blob";

URL BlobURL::createPublicURL(SecurityOrigin* securityOrigin)
{
    ASSERT(securityOrigin);
    return createBlobURL(securityOrigin->toString());
}

URL BlobURL::createInternalURL()
{
    return createBlobURL("blobinternal://");
}

static const Document* blobOwner(const SecurityOrigin& blobOrigin)
{
    if (!isMainThread())
        return nullptr;

    for (const auto* document : Document::allDocuments()) {
        if (&document->securityOrigin() == &blobOrigin)
            return document;
    }
    return nullptr;
}

URL BlobURL::getOriginURL(const URL& url)
{
    ASSERT(url.protocolIs(kBlobProtocol));

    if (auto blobOrigin = ThreadableBlobRegistry::getCachedOrigin(url)) {
        if (auto* document = blobOwner(*blobOrigin))
            return document->url();
    }
    return SecurityOrigin::extractInnerURL(url);
}

bool BlobURL::isSecureBlobURL(const URL& url)
{
    ASSERT(url.protocolIs(kBlobProtocol));

    // As per https://github.com/w3c/webappsec-mixed-content/issues/41, Blob URL is secure if the document that created it is secure.
    if (auto origin = ThreadableBlobRegistry::getCachedOrigin(url)) {
        if (auto* document = blobOwner(*origin))
            return document->isSecureContext();
    }
    return SecurityOrigin::isSecure(url);
}

URL BlobURL::createBlobURL(const String& originString)
{
    ASSERT(!originString.isEmpty());
    String urlString = "blob:" + originString + '/' + createCanonicalUUIDString();
    return URL({ }, urlString);
}

BlobURLHandle::BlobURLHandle(const BlobURLHandle& other)
    : m_url(other.m_url.isolatedCopy())
{
    registerBlobURLHandleIfNecessary();
}

BlobURLHandle::BlobURLHandle(const URL& url)
    : m_url(url.isolatedCopy())
{
    ASSERT(m_url.protocolIsBlob());
    registerBlobURLHandleIfNecessary();
}

BlobURLHandle::~BlobURLHandle()
{
    unregisterBlobURLHandleIfNecessary();
}

void BlobURLHandle::registerBlobURLHandleIfNecessary()
{
    if (m_url.protocolIsBlob())
        ThreadableBlobRegistry::registerBlobURLHandle(m_url);
}

void BlobURLHandle::unregisterBlobURLHandleIfNecessary()
{
    if (m_url.protocolIsBlob())
        ThreadableBlobRegistry::unregisterBlobURLHandle(m_url);
}

BlobURLHandle& BlobURLHandle::operator=(const BlobURLHandle& other)
{
    if (this == &other)
        return *this;

    unregisterBlobURLHandleIfNecessary();
    m_url = other.m_url.isolatedCopy();
    registerBlobURLHandleIfNecessary();

    return *this;
}

void BlobURLHandle::clear()
{
    unregisterBlobURLHandleIfNecessary();
    m_url = { };
}

BlobURLHandle& BlobURLHandle::operator=(BlobURLHandle&& other)
{
    unregisterBlobURLHandleIfNecessary();
    m_url = std::exchange(other.m_url, { });
    return *this;
}

BlobURLHandle& BlobURLHandle::operator=(const URL& url)
{
    ASSERT(url.protocolIsBlob());
    unregisterBlobURLHandleIfNecessary();
    m_url = url.isolatedCopy();
    registerBlobURLHandleIfNecessary();
    return *this;
}

} // namespace WebCore
