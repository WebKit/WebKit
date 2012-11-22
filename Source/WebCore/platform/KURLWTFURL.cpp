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

#include <TextEncoding.h>
#include <wtf/DataLog.h>
#include <wtf/text/CString.h>
#include <wtf/url/api/URLBuffer.h>
#include <wtf/url/api/URLQueryCharsetConverter.h>

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
    m_urlImpl->m_parsedURL = ParsedURL(urlString, ParsedURL::ParsedURLString);

    // FIXME: Frame::init() actually create empty URL, investigate why not just null URL.
    // ASSERT(m_urlImpl->m_parsedURL.isValid());
}

class CharsetConverter : public URLQueryCharsetConverter {
public:
    CharsetConverter(const TextEncoding& encoding)
        : m_encoding(encoding)
    {
    }

    virtual void convertFromUTF16(const UChar* input, unsigned inputLength, URLBuffer<char>& output) OVERRIDE
    {
        CString encoded = m_encoding.encode(input, inputLength, URLEncodedEntitiesForUnencodables);
        output.append(encoded.data(), static_cast<int>(encoded.length()));
    }

private:
    const TextEncoding& m_encoding;
};

KURL::KURL(const KURL& baseURL, const String& relative)
    : m_urlImpl(adoptRef(new KURLWTFURLImpl()))
{
    // FIXME: the case with a null baseURL is common. We should have a separate constructor in KURL.
    // FIXME: the case of an empty Base is useless, we should get rid of empty URLs.
    CharsetConverter charsetConverter(UTF8Encoding());
    if (baseURL.isEmpty())
        m_urlImpl->m_parsedURL = ParsedURL(relative, &charsetConverter);
    else
        m_urlImpl->m_parsedURL = ParsedURL(baseURL.m_urlImpl->m_parsedURL, relative, &charsetConverter);

    if (!m_urlImpl->m_parsedURL.isValid())
        m_urlImpl->m_invalidUrlString = relative;
}

KURL::KURL(const KURL& baseURL, const String& relative, const TextEncoding& encoding)
    : m_urlImpl(adoptRef(new KURLWTFURLImpl()))
{
    CharsetConverter charsetConverter(encoding.encodingForFormSubmission());
    if (baseURL.isEmpty())
        m_urlImpl->m_parsedURL = ParsedURL(relative, &charsetConverter);
    else
        m_urlImpl->m_parsedURL = ParsedURL(baseURL.m_urlImpl->m_parsedURL, relative, &charsetConverter);

    if (!m_urlImpl->m_parsedURL.isValid())
        m_urlImpl->m_invalidUrlString = relative;
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
        ASSERT_WITH_MESSAGE(m_urlImpl->m_invalidUrlString.isNull(), "A valid URL must have a null invalidUrlString.");
#endif
    return isParsedURLValid;
}

const String& KURL::string() const
{
    if (isNull())
        return emptyString();

    if (isValid())
        return m_urlImpl->m_parsedURL.spec().string();

    return m_urlImpl->m_invalidUrlString;
}

String KURL::protocol() const
{
    // Skip the ASSERT for now, SubframeLoader::requestFrame() does not check the validity of URLs.
    // ASSERT(isValid());
    if (!isValid())
        return String();
    return m_urlImpl->m_parsedURL.scheme();
}

String KURL::host() const
{
    // Skip the ASSERT for now, HTMLAnchorElement::parseAttribute() does not check the validity of URLs.
    // ASSERT(isValid());
    if (!isValid())
        return String();
    return m_urlImpl->m_parsedURL.host();
}

bool KURL::hasPort() const
{
    // This should be an ASSERT. HTMLAnchorElement::port() does not check the validity of the URL.
    if (!isValid())
        return false;

    return m_urlImpl->m_parsedURL.hasPort();
}

unsigned short KURL::port() const
{
    ASSERT(isValid());

    String portString = m_urlImpl->m_parsedURL.port();
    if (portString.isNull())
        return 0;

    bool ok = false;
    unsigned portValue = portString.toUIntStrict(&ok);

    if (!ok || portValue > maximumValidPortNumber)
        return invalidPortNumber;

    return static_cast<unsigned short>(portValue);
}

String KURL::user() const
{
    // FIXME: this should be an ASSERT(), call site should not use invalid URLs.
    if (!isValid())
        return String();

    return m_urlImpl->m_parsedURL.username();
}

String KURL::pass() const
{
    // FIXME: this should be an ASSERT(), call site should not use invalid URLs.
    if (!isValid())
        return String();

    return m_urlImpl->m_parsedURL.password();
}

bool KURL::hasPath() const
{
    // FIXME: this should be an ASSERTION, call site should not use invalid URLs.
    if (!isValid())
        return false;

    return !path().isEmpty();
}

String KURL::path() const
{
    // FIXME: this should be an ASSERTION, HTMLAnchorElement should not use invalid URLs.
    if (!isValid())
        return String();

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
    // FIXME: this should be an ASSERTION, HTMLAnchorElement should not use invalid URLs.
    if (!isValid())
        return String();

    return m_urlImpl->m_parsedURL.query();
}

bool KURL::hasFragmentIdentifier() const
{
    // Skip the ASSERT for now, ScriptElement::requestScript() create requests for invalid URLs.
    // ASSERT(isValid());
    if (!isValid())
        return false;

    return m_urlImpl->m_parsedURL.hasFragment();
}

String KURL::fragmentIdentifier() const
{
    // Skip the ASSERT for now, ScriptElement::requestScript() create requests for invalid URLs.
    // ASSERT(isValid());
    if (!isValid())
        return String();

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
    return string();
}

bool KURL::protocolIs(const char* testProtocol) const
{
    if (!isValid())
        return false;
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
    if (!hasPort())
        return;

    detach(m_urlImpl);
    m_urlImpl->m_parsedURL.removePort();
}

void KURL::setPort(unsigned short newPort)
{
    detach(m_urlImpl);
    m_urlImpl->m_parsedURL.replacePort(newPort);
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
    if (!hasFragmentIdentifier())
        return;

    detach(m_urlImpl);
    m_urlImpl->m_parsedURL = m_urlImpl->m_parsedURL.withoutFragment();
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

#ifndef NDEBUG
void KURL::print() const
{
    if (isValid())
        m_urlImpl->m_parsedURL.print();
    else {
        if (isNull())
            dataLogF("Null KURL");
        else if (isEmpty())
            dataLogF("Empty KURL");
        else {
            dataLogF("Invalid KURL from string =");
            m_urlImpl->m_invalidUrlString.show();
        }
    }
}
#endif

void KURL::invalidate()
{
    m_urlImpl = nullptr;
}

bool KURL::isHierarchical() const
{
    if (!isValid())
        return false;
    return m_urlImpl->m_parsedURL.hasStandardScheme();
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
