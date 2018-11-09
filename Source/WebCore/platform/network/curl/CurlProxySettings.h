/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include "URL.h"
#include <wtf/text/WTFString.h>

#define kCURLAUTH_DIGEST_IE    (((unsigned long) 1) << 4)
#define kCURLAUTH_ANY          (~kCURLAUTH_DIGEST_IE)

namespace WebCore {

class CurlProxySettings {
public:
    enum class Mode : uint8_t {
        Default,
        NoProxy,
        Custom
    };

    CurlProxySettings() = default;
    WEBCORE_EXPORT explicit CurlProxySettings(Mode mode) : m_mode(mode) { }
    WEBCORE_EXPORT CurlProxySettings(URL&&, String&& ignoreHosts);

    WEBCORE_EXPORT CurlProxySettings(const CurlProxySettings&) = default;
    WEBCORE_EXPORT CurlProxySettings& operator=(const CurlProxySettings&) = default;

    bool isEmpty() const { return m_mode == Mode::Custom && m_urlSerializedWithPort.isEmpty() && m_ignoreHosts.isEmpty(); }

    Mode mode() const { return m_mode; }
    const String& url() const { return m_urlSerializedWithPort; }
    const String& ignoreHosts() const { return m_ignoreHosts; }

    WEBCORE_EXPORT void setUserPass(const String&, const String&);
    const String user() const { return m_url.user(); }
    const String password() const { return m_url.pass(); }

    void setDefaultAuthMethod() { m_authMethod = kCURLAUTH_ANY; }
    void setAuthMethod(long);
    long authMethod() const { return m_authMethod; }

private:
    Mode m_mode { Mode::Default };
    URL m_url;
    String m_ignoreHosts;
    long m_authMethod { static_cast<long>(kCURLAUTH_ANY) };

    // We can't simply use m_url.string() because we need to explicitly indicate the port number
    // to libcurl. URLParser omit the default port while parsing, but libcurl assume 1080 as a
    // default HTTP Proxy, not 80, if port number is not in the url.
    String m_urlSerializedWithPort;

    void rebuildUrl();
};

bool protocolIsInSocksFamily(const URL&);

} // namespace WebCore
