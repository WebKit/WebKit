/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebURL_h
#define WebURL_h

#include "WebCString.h"
#include <googleurl/src/url_parse.h>

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class KURL; }
#else
#include <googleurl/src/gurl.h>
#endif

namespace WebKit {

class WebURL {
public:
    ~WebURL()
    {
    }

    WebURL() : m_isValid(false)
    {
    }

    WebURL(const WebCString& spec, const url_parse::Parsed& parsed, bool isValid)
        : m_spec(spec)
        , m_parsed(parsed)
        , m_isValid(isValid)
    {
    }

    WebURL(const WebURL& s)
        : m_spec(s.m_spec)
        , m_parsed(s.m_parsed)
        , m_isValid(s.m_isValid)
    {
    }

    WebURL& operator=(const WebURL& s)
    {
        m_spec = s.m_spec;
        m_parsed = s.m_parsed;
        m_isValid = s.m_isValid;
        return *this;
    }

    void assign(const WebCString& spec, const url_parse::Parsed& parsed, bool isValid)
    {
        m_spec = spec;
        m_parsed = parsed;
        m_isValid = isValid;
    }

    const WebCString& spec() const
    {
        return m_spec;
    }

    const url_parse::Parsed& parsed() const
    {
        return m_parsed;
    }

    bool isValid() const
    {
        return m_isValid;
    }

    bool isEmpty() const
    {
        return m_spec.isEmpty();
    }

    bool isNull() const
    {
        return m_spec.isEmpty();
    }

#if WEBKIT_IMPLEMENTATION
    WebURL(const WebCore::KURL&);
    WebURL& operator=(const WebCore::KURL&);
    operator WebCore::KURL() const;
#else
    WebURL(const GURL& g)
        : m_spec(g.possibly_invalid_spec())
        , m_parsed(g.parsed_for_possibly_invalid_spec())
        , m_isValid(g.is_valid())
    {
    }

    WebURL& operator=(const GURL& g)
    {
        m_spec = g.possibly_invalid_spec();
        m_parsed = g.parsed_for_possibly_invalid_spec();
        m_isValid = g.is_valid();
        return *this;
    }

    operator GURL() const
    {
        return isNull() ? GURL() : GURL(m_spec.data(), m_spec.length(), m_parsed, m_isValid);
    }
#endif

private:
    WebCString m_spec;  // UTF-8 encoded
    url_parse::Parsed m_parsed;
    bool m_isValid;
};

} // namespace WebKit

#endif
