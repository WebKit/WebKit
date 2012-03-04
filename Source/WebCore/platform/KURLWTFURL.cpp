/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "KURL.h"

#if USE(WTFURL)

using namespace WTF;

namespace WebCore {

static const unsigned maximumValidPortNumber = 0xFFFE;
static const unsigned invalidPortNumber = 0xFFFF;

static inline void detach(RefPtr<KURLWTFURLImpl>& urlImpl)
{
    if (!urlImpl)
        return;

    if (urlImpl->hasOneRef())
        return;

    urlImpl = urlImpl->copy();
}

KURL::KURL(ParsedURLStringTag, const String& urlString)
    : m_urlImpl(adoptRef(new KURLWTFURLImpl()))
{
    m_urlImpl->m_parsedURL = ParsedURL(urlString);
    ASSERT(m_urlImpl->m_parsedURL.isValid());
}

KURL::KURL(const KURL&, const String&)
{
    // FIXME: Add WTFURL Implementation.
}

KURL::KURL(const KURL&, const String&, const TextEncoding&)
{
    // FIXME: Add WTFURL Implementation.
}

KURL KURL::copy() const
{
    KURL other;
    if (!isNull())
        other.m_urlImpl = m_urlImpl->copy();
    return other;
}

bool KURL::isNull() const
{
    return !m_urlImpl;
}

// FIXME: Can we get rid of the concept of EmptyURL? Can an null URL be enough?
// If we cannot get rid of the concept, we should make a shared empty URL.
bool KURL::isEmpty() const
{
    return !m_urlImpl
           || (!m_urlImpl->m_parsedURL.isValid() && m_urlImpl->m_invalidUrlString.isEmpty());
}

bool KURL::isValid() const
{
    if (!m_urlImpl)
        return false;

    bool isParsedURLValid = m_urlImpl->m_parsedURL.isValid();
#ifndef NDEBUG
    if (isParsedURLValid)
        ASSERT_WITH_MESSAGE(m_urlImpl->m_invalidUrlString.isNull(), "A valid URL should never have a valid invalidUrlString.");
#endif
    return isParsedURLValid;
}

const String &KURL::string() const
{
    if (isNull()) {
        DEFINE_STATIC_LOCAL(const String, nullString, ());
        return nullString;
    }

    if (!m_urlImpl->m_invalidUrlString.isNull()) {
        ASSERT(!isValid());
        return m_urlImpl->m_invalidUrlString;
    }

    ASSERT(isValid());
    return m_urlImpl->m_parsedURL.spec().string();
}

String KURL::protocol() const
{
    ASSERT(isValid());
    return m_urlImpl->m_parsedURL.scheme();
}

String KURL::host() const
{
    ASSERT(isValid());
    return m_urlImpl->m_parsedURL.host();
}

bool KURL::hasPort() const
{
    ASSERT(isValid());
    return !m_urlImpl->m_parsedURL.port().isNull();
}

unsigned short KURL::port() const
{
    ASSERT(isValid());

    String portString = m_urlImpl->m_parsedURL.port();
    bool ok = false;
    unsigned portValue = portString.toUIntStrict(&ok);

    if (!ok || portValue > maximumValidPortNumber)
        return invalidPortNumber;

    return static_cast<unsigned short>(portValue);
}

String KURL::user() const
{
    ASSERT(isValid());
    return m_urlImpl->m_parsedURL.username();
}

String KURL::pass() const
{
    ASSERT(isValid());
    return m_urlImpl->m_parsedURL.password();
}

bool KURL::hasPath() const
{
    ASSERT(isValid());
    return !path().isEmpty();
}

String KURL::path() const
{
    ASSERT(isValid());
    return m_urlImpl->m_parsedURL.path();
}

String KURL::lastPathComponent() const
{
    ASSERT(isValid());

    String pathString = path();
    size_t index = pathString.reverseFind('/');

    if (index == notFound)
        return pathString;

    return pathString.substring(index + 1);
}

String KURL::query() const
{
    ASSERT(isValid());
    return m_urlImpl->m_parsedURL.query();
}

bool KURL::hasFragmentIdentifier() const
{
    ASSERT(isValid());
    return !fragmentIdentifier().isNull();
}

String KURL::fragmentIdentifier() const
{
    ASSERT(isValid());
    return m_urlImpl->m_parsedURL.fragment();
}

// FIXME: track an fix the bad use of this method.
String KURL::baseAsString() const
{
    ASSERT(isValid());
    return m_urlImpl->m_parsedURL.baseAsString();
}

// FIXME: Get rid of this function from KURL.
String KURL::fileSystemPath() const
{
    return String();
}

bool KURL::protocolIs(const char* testProtocol) const
{
    ASSERT(isValid());
    return WebCore::protocolIs(protocol(), testProtocol);
}

bool KURL::protocolIsInHTTPFamily() const
{
    return protocolIs("http") || protocolIs("https");
}

bool KURL::setProtocol(const String&)
{
    detach(m_urlImpl);
    // FIXME: Add WTFURL Implementation.
    return false;
}

void KURL::setHost(const String&)
{
    detach(m_urlImpl);
    // FIXME: Add WTFURL Implementation.
}

void KURL::removePort()
{
    detach(m_urlImpl);
    // FIXME: Add WTFURL Implementation.
}

void KURL::setPort(unsigned short)
{
    detach(m_urlImpl);
    // FIXME: Add WTFURL Implementation.
}

void KURL::setHostAndPort(const String&)
{
    detach(m_urlImpl);
    // FIXME: Add WTFURL Implementation.
}

void KURL::setUser(const String&)
{
    detach(m_urlImpl);
    // FIXME: Add WTFURL Implementation.
}

void KURL::setPass(const String&)
{
    detach(m_urlImpl);
    // FIXME: Add WTFURL Implementation.
}

void KURL::setPath(const String&)
{
    detach(m_urlImpl);
    // FIXME: Add WTFURL Implementation.
}

void KURL::setQuery(const String&)
{
    detach(m_urlImpl);
    // FIXME: Add WTFURL Implementation.
}

void KURL::setFragmentIdentifier(const String&)
{
    detach(m_urlImpl);
    // FIXME: Add WTFURL Implementation.
}

void KURL::removeFragmentIdentifier()
{
    detach(m_urlImpl);
    // FIXME: Add WTFURL Implementation.
}

unsigned KURL::hostStart() const
{
    // FIXME: Add WTFURL Implementation.
    return 0;
}

unsigned KURL::hostEnd() const
{
    // FIXME: Add WTFURL Implementation.
    return 0;
}

unsigned KURL::pathStart() const
{
    // FIXME: Add WTFURL Implementation.
    return 0;
}

unsigned KURL::pathEnd() const
{
    // FIXME: Add WTFURL Implementation.
    return 0;
}

unsigned KURL::pathAfterLastSlash() const
{
    // FIXME: Add WTFURL Implementation.
    return 0;
}

void KURL::invalidate()
{
    m_urlImpl = nullptr;
}

bool KURL::isHierarchical() const
{
    // FIXME: Add WTFURL Implementation.
    return false;
}

bool protocolIs(const String&, const char*)
{
    // FIXME: Add WTFURL Implementation.
    return false;
}

bool equalIgnoringFragmentIdentifier(const KURL&, const KURL&)
{
    // FIXME: Add WTFURL Implementation.
    return false;
}

bool protocolHostAndPortAreEqual(const KURL& a, const KURL& b)
{
    if (!a.isValid() || !b.isValid())
        return false;

    return a.protocol() == b.protocol()
           && a.host() == b.host()
           && a.port() == b.port();
}

String encodeWithURLEscapeSequences(const String&)
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

String decodeURLEscapeSequences(const String&)
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

String decodeURLEscapeSequences(const String&, const TextEncoding&)
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

}

#endif // USE(WTFURL)
