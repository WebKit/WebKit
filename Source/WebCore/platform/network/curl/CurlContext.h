/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#pragma once

#include "URL.h"

#include <WTF/Noncopyable.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>

#if OS(WINDOWS)
#include <windows.h>
#include <winsock2.h>
#endif

#include <curl/curl.h>

namespace WebCore {

enum class CurlProxyType {
    Invalid = -1,
    HTTP = CURLPROXY_HTTP,
    Socks4 = CURLPROXY_SOCKS4,
    Socks4A = CURLPROXY_SOCKS4A,
    Socks5 = CURLPROXY_SOCKS5,
    Socks5Hostname = CURLPROXY_SOCKS5_HOSTNAME
};

class CurlContext {
    WTF_MAKE_NONCOPYABLE(CurlContext);

public:
    struct ProxyInfo {
        String host;
        unsigned long port;
        CurlProxyType type { CurlProxyType::Invalid };
        String username;
        String password;

        const String url() const;
    };

    static CurlContext& singleton()
    {
        static CurlContext shared;
        return shared;
    }

    ~CurlContext();

    CURLSH* curlShareHandle() const { return m_curlShareHandle; }

    CURLM* createMultiHandle();

    // Cookie
    const char* getCookieJarFileName() const { return m_cookieJarFileName.data(); }
    void setCookieJarFileName(const char* cookieJarFileName) { m_cookieJarFileName = CString(cookieJarFileName); }

    // Certificate
    const char* getCertificatePath() const { return m_certificatePath.data(); }
    bool shouldIgnoreSSLErrors() const { return m_ignoreSSLErrors; }

    // Proxy
    const ProxyInfo& proxyInfo() const { return m_proxy; }
    void setProxyInfo(const ProxyInfo& info) { m_proxy = info;  }
    void setProxyInfo(const String& host = emptyString(), unsigned long port = 0, CurlProxyType = CurlProxyType::HTTP, const String& username = emptyString(), const String& password = emptyString());

    // Utilities
    URL getEffectiveURL(CURL* handle);

#ifndef NDEBUG
    FILE* getLogFile() const { return m_logFile; }
    bool isVerbose() const { return m_verbose; }
#endif

private:

    CURLSH* m_curlShareHandle { nullptr };

    CString m_cookieJarFileName;

    CString m_certificatePath;
    bool m_ignoreSSLErrors { false };

    ProxyInfo m_proxy;

    CurlContext();
    void initCookieSession();

    static void lock(CURL*, curl_lock_data, curl_lock_access, void*);
    static void unlock(CURL*, curl_lock_data, void*);
    static Lock* mutexFor(curl_lock_data);

#ifndef NDEBUG
    FILE* m_logFile { nullptr };
    bool m_verbose { false };
#endif
};

}
