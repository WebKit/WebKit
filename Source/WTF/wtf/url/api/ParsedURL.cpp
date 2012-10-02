/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ParsedURL.h"

#if USE(WTFURL)

#include <wtf/DataLog.h>
#include <wtf/RawURLBuffer.h>
#include <wtf/URLComponent.h>
#include <wtf/URLUtil.h>
#include <wtf/text/CString.h>

namespace WTF {

ParsedURL::ParsedURL(const String& urlString, ParsedURLStringTag)
{
    unsigned urlStringLength = urlString.length();
    if (!urlStringLength)
        return; // FIXME: we should ASSERT on this, but people use KURL incorrectly with ParsedURLStringTag :(.

    RawURLBuffer<char> outputBuffer;
    String base;
    const CString& baseStr = base.utf8();
    bool isValid = false;
    URLSegments baseSegments;

    // FIXME: we should take shortcuts here! We do not have to resolve the relative part.
    if (urlString.is8Bit())
        isValid = URLUtilities::resolveRelative(baseStr.data(), baseSegments,
                                                reinterpret_cast<const char*>(urlString.characters8()), urlStringLength,
                                                /* charsetConverter */ 0,
                                                outputBuffer, &m_segments);
    else
        isValid = URLUtilities::resolveRelative(baseStr.data(), baseSegments,
                                                urlString.characters16(), urlStringLength,
                                                /* charsetConverter */ 0,
                                                outputBuffer, &m_segments);

    // FIXME: we should ASSERT on isValid, but people use KURL incorrectly with ParsedURLStringTag :(.
    if (isValid)
        m_spec = URLString(String(outputBuffer.data(), outputBuffer.length()));
}

ParsedURL::ParsedURL(const String& urlString, URLQueryCharsetConverter* queryCharsetConverter)
{
    unsigned urlStringLength = urlString.length();
    if (!urlStringLength)
        return;

    RawURLBuffer<char> outputBuffer;
    String base;
    const CString& baseStr = base.utf8();
    bool isValid = false;
    URLSegments baseSegments;

    // FIXME: we should take shortcuts here! We do not have to resolve the relative part.
    if (urlString.is8Bit())
        isValid = URLUtilities::resolveRelative(baseStr.data(), baseSegments,
                                                reinterpret_cast<const char*>(urlString.characters8()), urlStringLength,
                                                queryCharsetConverter,
                                                outputBuffer, &m_segments);
    else
        isValid = URLUtilities::resolveRelative(baseStr.data(), baseSegments,
                                                urlString.characters16(), urlStringLength,
                                                queryCharsetConverter,
                                                outputBuffer, &m_segments);

    if (isValid)
        m_spec = URLString(String(outputBuffer.data(), outputBuffer.length()));
}

ParsedURL::ParsedURL(const ParsedURL& base, const String& relative, URLQueryCharsetConverter* queryCharsetConverter)
{
    if (!base.isValid())
        return;

    unsigned relativeLength = relative.length();
    if (!relativeLength) {
        *this = base.withoutFragment();
        return;
    }

    RawURLBuffer<char> outputBuffer;
    const CString& baseStr = base.m_spec.m_string.utf8();
    bool isValid = false;

    if (relative.is8Bit())
        isValid = URLUtilities::resolveRelative(baseStr.data(), base.m_segments,
                                                reinterpret_cast<const char*>(relative.characters8()), relativeLength,
                                                queryCharsetConverter,
                                                outputBuffer, &m_segments);
    else
        isValid = URLUtilities::resolveRelative(baseStr.data(), base.m_segments,
                                                relative.characters16(), relativeLength,
                                                queryCharsetConverter,
                                                outputBuffer, &m_segments);

    if (isValid)
        m_spec = URLString(String(outputBuffer.data(), outputBuffer.length()));
}

ParsedURL ParsedURL::isolatedCopy() const
{
    ParsedURL copy;
    copy.m_segments = this->m_segments;
    copy.m_spec = URLString(this->m_spec.string().isolatedCopy());
    return copy;
}

String ParsedURL::scheme() const
{
    return segment(m_segments.scheme);
}

String ParsedURL::username() const
{
    return segment(m_segments.username);
}

String ParsedURL::password() const
{
    return segment(m_segments.password);
}

String ParsedURL::host() const
{
    return segment(m_segments.host);
}

String ParsedURL::port() const
{
    return segment(m_segments.port);
}

String ParsedURL::path() const
{
    return segment(m_segments.path);
}

String ParsedURL::query() const
{
    return segment(m_segments.query);
}

bool ParsedURL::hasFragment() const
{
    return m_segments.fragment.isValid();
}

String ParsedURL::fragment() const
{
    return segment(m_segments.fragment);
}

ParsedURL ParsedURL::withoutFragment() const
{
    if (!hasFragment())
        return *this;

    ParsedURL newURL;

    int charactersBeforeFragemnt = m_segments.charactersBefore(URLSegments::Fragment, URLSegments::DelimiterExcluded);
    newURL.m_spec = URLString(m_spec.string().substringSharingImpl(0, charactersBeforeFragemnt));

    newURL.m_segments = m_segments;
    newURL.m_segments.fragment = URLComponent();
    return newURL;
}

String ParsedURL::baseAsString() const
{
    // FIXME: Add WTFURL Implementation.
    return String();
}

String ParsedURL::segment(const URLComponent& component) const
{
    ASSERT(isValid());

    if (!component.isValid())
        return String();

    String segment = m_spec.string().substring(component.begin(), component.length());

    // FIXME: GoogleURL create empty segments. This happen for the fragment for the test fast/url/segments.html
    // ASSERT_WITH_MESSAGE(!segment.isEmpty(), "A valid URL component should not be empty.");
    return segment;
}

#ifndef NDEBUG

#define SHOW_COMPONENT(parsedURL, componentName) \
    if (!parsedURL->componentName().isNull()) { \
        dataLog("    " #componentName " = "); \
        parsedURL->componentName().show(); \
    }

void ParsedURL::print() const
{
    if (!isValid()) {
        dataLog("Invalid ParsedURL.\n");
        return;
    }

    dataLog("Valid ParsedURL with:\n");
    dataLog("    m_spec = ");
    m_spec.print();

    SHOW_COMPONENT(this, scheme);
    SHOW_COMPONENT(this, username);
    SHOW_COMPONENT(this, password);
    SHOW_COMPONENT(this, host);
    SHOW_COMPONENT(this, port);
    SHOW_COMPONENT(this, path);
    SHOW_COMPONENT(this, query);
    SHOW_COMPONENT(this, fragment);
}
#endif

}

#endif // USE(WTFURL)
