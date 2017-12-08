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

#include "CookieJarCurl.h"
#include "CurlSSLHandle.h"
#include "URL.h"

#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
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

// CurlGlobal --------------------------------------------
// to make the initialization of libcurl happen before other initialization of CurlContext

class CurlGlobal {
protected:
    CurlGlobal()
    {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    
    virtual ~CurlGlobal()
    {
        curl_global_cleanup();
    }
};

// CurlShareHandle --------------------------------------------

class CurlShareHandle {
    WTF_MAKE_NONCOPYABLE(CurlShareHandle);

public:
    CurlShareHandle();
    ~CurlShareHandle();

    CURLSH* handle() const { return m_shareHandle; }

private:
    static void lockCallback(CURL*, curl_lock_data, curl_lock_access, void*);
    static void unlockCallback(CURL*, curl_lock_data, void*);
    static StaticLock* mutexFor(curl_lock_data);

    CURLSH* m_shareHandle { nullptr };
};

// CurlContext --------------------------------------------

class CurlContext : public CurlGlobal {
    WTF_MAKE_NONCOPYABLE(CurlContext);
    friend NeverDestroyed<CurlContext>;
public:
    struct ProxyInfo {
        String host;
        unsigned long port;
        CurlProxyType type { CurlProxyType::Invalid };
        String username;
        String password;

        const String url() const;
    };

    static CurlContext& singleton();

    virtual ~CurlContext();

    const CurlShareHandle& shareHandle() { return m_shareHandle; }

    // Cookie
    const char* getCookieJarFileName() const { return m_cookieJarFileName.data(); }
    void setCookieJarFileName(const char* cookieJarFileName) { m_cookieJarFileName = CString(cookieJarFileName); }
    CookieJarCurl& cookieJar() { return *m_cookieJar; }

    // Proxy
    const ProxyInfo& proxyInfo() const { return m_proxy; }
    void setProxyInfo(const ProxyInfo& info) { m_proxy = info;  }
    void setProxyInfo(const String& host = emptyString(), unsigned long port = 0, CurlProxyType = CurlProxyType::HTTP, const String& username = emptyString(), const String& password = emptyString());

    // SSL
    CurlSSLHandle& sslHandle() { return m_sslHandle; }

#ifndef NDEBUG
    FILE* getLogFile() const { return m_logFile; }
    bool isVerbose() const { return m_verbose; }
#endif

private:
    ProxyInfo m_proxy;
    CString m_cookieJarFileName;
    CurlShareHandle m_shareHandle;
    std::unique_ptr<CookieJarCurl> m_cookieJar;
    CurlSSLHandle m_sslHandle;

    CurlContext();
    void initCookieSession();

#ifndef NDEBUG
    FILE* m_logFile { nullptr };
    bool m_verbose { false };
#endif
};

// CurlMultiHandle --------------------------------------------

class CurlMultiHandle {
    WTF_MAKE_NONCOPYABLE(CurlMultiHandle);

public:
    CurlMultiHandle();
    ~CurlMultiHandle();

    CURLMcode addHandle(CURL*);
    CURLMcode removeHandle(CURL*);

    CURLMcode getFdSet(fd_set&, fd_set&, fd_set&, int&);
    CURLMcode perform(int&);
    CURLMsg* readInfo(int&);

private:
    CURLM* m_multiHandle { nullptr };
};

// CurlSList -------------------------------------------------

class CurlSList {
public:
    CurlSList() { }
    ~CurlSList() { clear(); }

    operator struct curl_slist** () { return &m_list; }

    const struct curl_slist* head() const { return m_list; }
    bool isEmpty() const { return !m_list; }
    void clear()
    {
        if (m_list) {
            curl_slist_free_all(m_list);
            m_list = nullptr;
        }
    }

    void append(const char* str) { m_list = curl_slist_append(m_list, str); }
    void append(const String& str) { append(str.latin1().data()); }

private:
    struct curl_slist* m_list { nullptr };
};

// CurlHandle -------------------------------------------------

class HTTPHeaderMap;
class NetworkLoadMetrics;

class CurlHandle {
    WTF_MAKE_NONCOPYABLE(CurlHandle);

public:
    enum class VerifyPeer {
        Disable = 0L,
        Enable = 1L
    };

    enum class VerifyHost {
        LooseNameCheck = 0,
        StrictNameCheck = 2
    };

    CurlHandle();
    virtual ~CurlHandle();

    CURL* handle() const { return m_handle; }

    void initialize();

    CURLcode perform();
    CURLcode pause(int);

    static const String errorDescription(CURLcode);

    void enableShareHandle();

    void setUrl(const URL&);

    void appendRequestHeaders(const HTTPHeaderMap&);
    void appendRequestHeader(const String& name, const String& value);
    void appendRequestHeader(const String& name);
    void removeRequestHeader(const String& name);

    void enableHttpGetRequest();
    void enableHttpHeadRequest();
    void enableHttpPostRequest();
    void setPostFields(const char*, long);
    void setPostFieldLarge(curl_off_t);
    void enableHttpPutRequest();
    void setInFileSizeLarge(curl_off_t);
    void setHttpCustomRequest(const String&);

    void enableAcceptEncoding();
    void enableAllowedProtocols();

    void enableHttpAuthentication(long);
    void setHttpAuthUserPass(const String&, const String&);

    void setCACertPath(const char*);
    void setSslVerifyPeer(VerifyPeer);
    void setSslVerifyHost(VerifyHost);
    void setSslCert(const char*);
    void setSslCertType(const char*);
    void setSslKeyPassword(const char*);

    void enableCookieJarIfExists();
    void setCookieList(const char*);
    void fetchCookieList(CurlSList &cookies) const;

    void enableProxyIfExists();

    void enableTimeout();

    // Callback function
    void setHeaderCallbackFunction(curl_write_callback, void*);
    void setWriteCallbackFunction(curl_write_callback, void*);
    void setReadCallbackFunction(curl_read_callback, void*);
    void setSslCtxCallbackFunction(curl_ssl_ctx_callback, void*);

    // Status
    std::optional<uint16_t> getPrimaryPort();
    std::optional<long> getResponseCode();
    std::optional<long> getHttpConnectCode();
    std::optional<long long> getContentLength();
    std::optional<long> getHttpAuthAvail();
    std::optional<NetworkLoadMetrics> getNetworkLoadMetrics();

    static long long maxCurlOffT();

#ifndef NDEBUG
    void enableVerboseIfUsed();
    void enableStdErrIfUsed();
#endif

private:
    void enableRequestHeaders();
    static int expectedSizeOfCurlOffT();

    CURL* m_handle { nullptr };
    char m_errorBuffer[CURL_ERROR_SIZE] { };

    CurlSList m_requestHeaders;
};

} // namespace WebCore
