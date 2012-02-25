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

namespace WebCore {

KURL::KURL(ParsedURLStringTag, const String&)
{
    // FIXME: Add WTFURL Implementation.
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
    // FIXME: Add WTFURL Implementation.
    return KURL();
}

bool KURL::isNull() const
{
    return !m_urlImpl;
}

bool KURL::isEmpty() const
{
    // FIXME: Add WTFURL Implementation.
    return true;
}

bool KURL::isValid() const
{
    // FIXME: Add WTFURL Implementation.
    return false;
}

bool KURL::hasPath() const
{
    // FIXME: Add WTFURL Implementation.
    return false;
}

const String &KURL::string() const
{
    // FIXME: Add WTFURL Implementation.
    ASSERT(m_urlImpl);
    return m_urlImpl->m_invalidUrlString;
}

String KURL::protocol() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

String KURL::host() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

unsigned short KURL::port() const
{
    // FIXME: Add WTFURL Implementation.
    return 0;
}

bool KURL::hasPort() const
{
    // FIXME: Add WTFURL Implementation.
    return false;
}

String KURL::user() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

String KURL::pass() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

String KURL::path() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

String KURL::lastPathComponent() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

String KURL::query() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

String KURL::fragmentIdentifier() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

bool KURL::hasFragmentIdentifier() const
{
    // FIXME: Add WTFURL Implementation.
    return false;
}

void KURL::copyParsedQueryTo(ParsedURLParameters&) const
{
    // FIXME: Add WTFURL Implementation.
}

String KURL::baseAsString() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

String KURL::deprecatedString() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

String KURL::fileSystemPath() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

bool KURL::protocolIs(const char*) const
{
    // FIXME: Add WTFURL Implementation.
    return false;
}

bool KURL::protocolIsInHTTPFamily() const
{
    // FIXME: Add WTFURL Implementation.
    return false;
}

bool KURL::setProtocol(const String&)
{
    // FIXME: Add WTFURL Implementation.
    return false;
}

void KURL::setHost(const String&)
{
    // FIXME: Add WTFURL Implementation.
}

void KURL::removePort()
{
    // FIXME: Add WTFURL Implementation.
}

void KURL::setPort(unsigned short)
{
    // FIXME: Add WTFURL Implementation.
}

void KURL::setHostAndPort(const String&)
{
    // FIXME: Add WTFURL Implementation.
}

void KURL::setUser(const String&)
{
    // FIXME: Add WTFURL Implementation.
}

void KURL::setPass(const String&)
{
    // FIXME: Add WTFURL Implementation.
}

void KURL::setPath(const String&)
{
    // FIXME: Add WTFURL Implementation.
}

void KURL::setQuery(const String&)
{
    // FIXME: Add WTFURL Implementation.
}

void KURL::setFragmentIdentifier(const String&)
{
    // FIXME: Add WTFURL Implementation.
}

void KURL::removeFragmentIdentifier()
{
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

bool protocolHostAndPortAreEqual(const KURL&, const KURL&)
{
    // FIXME: Add WTFURL Implementation.
    return false;
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
