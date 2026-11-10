/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "APIObject.h"
#include <wtf/Forward.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace API {

class URL : public ObjectImpl<Object::Type::URL> {
public:
    static Ref<API::URL> create(StringImpl* string)
    {
        return adoptRef(*new URL(string));
    }

    static Ref<API::URL> create(const API::URL* baseURL, StringImpl* relativeURL)
    {
        ASSERT(baseURL);
        baseURL->parseURLIfNecessary();
        auto absoluteURL = makeUnique<WTF::URL>(*baseURL->m_parsedURL.get(), relativeURL);
        const RefPtr<StringImpl>& absoluteURLString = absoluteURL->string();

        return adoptRef(*new API::URL(WTF::move(absoluteURL), absoluteURLString));
    }

    bool isNull() const { return !m_string; }
    bool isEmpty() const { return WTF::isEmpty(m_string); }

    const RefPtr<StringImpl>& string() const LIFETIME_BOUND { return m_string; }

    static bool equals(const API::URL& a, const API::URL& b)
    {
        return a.url() == b.url();
    }

    RefPtr<StringImpl> host() const
    {
        parseURLIfNecessary();
        return m_parsedURL->host().toString();
    }

    RefPtr<StringImpl> protocol() const
    {
        parseURLIfNecessary();
        return m_parsedURL->protocol().toString();
    }

    RefPtr<StringImpl> path() const
    {
        parseURLIfNecessary();
        return m_parsedURL->path().toString();
    }

    RefPtr<StringImpl> lastPathComponent() const
    {
        parseURLIfNecessary();
        return m_parsedURL->lastPathComponent().toString();
    }

private:
    URL(StringImpl* string)
        : m_string(string)
    {
    }

    URL(std::unique_ptr<WTF::URL> parsedURL, StringImpl* string)
        : m_string(string)
        , m_parsedURL(WTF::move(parsedURL))
    {
    }

    const WTF::URL& url() const LIFETIME_BOUND
    {
        parseURLIfNecessary();
        return *m_parsedURL;
    }

    void parseURLIfNecessary() const
    {
        if (m_parsedURL)
            return;
        m_parsedURL = makeUnique<WTF::URL>(m_string);
    }

    RefPtr<StringImpl> m_string;
    mutable std::unique_ptr<WTF::URL> m_parsedURL;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_API_OBJECT(URL);
