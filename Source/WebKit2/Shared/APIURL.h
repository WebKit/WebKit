/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebURL_h
#define WebURL_h

#include "APIObject.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/URL.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace API {

class URL : public ObjectImpl<Object::Type::URL> {
public:
    static PassRefPtr<URL> create(const WTF::String& string)
    {
        return adoptRef(new URL(string));
    }

    static PassRefPtr<URL> create(const URL* baseURL, const WTF::String& relativeURL)
    {
        ASSERT(baseURL);
        baseURL->parseURLIfNecessary();
        auto absoluteURL = std::make_unique<WebCore::URL>(*baseURL->m_parsedURL.get(), relativeURL);
        const WTF::String& absoluteURLString = absoluteURL->string();

        return adoptRef(new URL(std::move(absoluteURL), absoluteURLString));
    }

    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }

    const WTF::String& string() const { return m_string; }

    static bool equals(const URL& a, const URL& b)
    {
        return a.url() == b.url();
    }

    WTF::String host() const
    {
        parseURLIfNecessary();
        return m_parsedURL->isValid() ? m_parsedURL->host() : WTF::String();
    }

    WTF::String protocol() const
    {
        parseURLIfNecessary();
        return m_parsedURL->isValid() ? m_parsedURL->protocol() : WTF::String();
    }

    WTF::String path() const
    {
        parseURLIfNecessary();
        return m_parsedURL->isValid() ? m_parsedURL->path() : WTF::String();
    }

    WTF::String lastPathComponent() const
    {
        parseURLIfNecessary();
        return m_parsedURL->isValid() ? m_parsedURL->lastPathComponent() : WTF::String();
    }

    void encode(IPC::ArgumentEncoder& encoder) const
    {
        encoder << m_string;
    }

    static bool decode(IPC::ArgumentDecoder& decoder, RefPtr<Object>& result)
    {
        WTF::String string;
        if (!decoder.decode(string))
            return false;
        
        result = create(string);
        return true;
    }

private:
    URL(const WTF::String& string)
        : m_string(string)
    {
    }

    URL(std::unique_ptr<WebCore::URL> parsedURL, const WTF::String& string)
        : m_string(string)
        , m_parsedURL(std::move(parsedURL))
    {
    }

    const WebCore::URL& url() const
    {
        parseURLIfNecessary();
        return *m_parsedURL;
    }

    void parseURLIfNecessary() const
    {
        if (m_parsedURL)
            return;
        m_parsedURL = std::make_unique<WebCore::URL>(WebCore::URL(), m_string);
    }

    WTF::String m_string;
    mutable std::unique_ptr<WebCore::URL> m_parsedURL;
};

} // namespace WebKit

#endif // URL_h
