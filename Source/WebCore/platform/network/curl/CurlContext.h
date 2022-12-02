/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
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

#include "CertificateInfo.h"
#include "CurlProxySettings.h"
#include "CurlSSLHandle.h"

#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/Seconds.h>
#include <wtf/Threading.h>
#include <wtf/URL.h>

#if OS(WINDOWS)
#include <windows.h>
#include <winsock2.h>
#endif

#include <curl/curl.h>

namespace WebCore {

// Values taken from http://www.browserscope.org/ following
// the rule "Do What Every Other Modern Browser Is Doing".
const long CurlDefaultMaxConnects { -1 }; // -1 : Does not set CURLMOPT_MAXCONNECTS
const long CurlDefaultMaxTotalConnections { 17 };
const long CurlDefaultMaxHostConnections { 6 };

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
    static Lock* mutexFor(curl_lock_data);

    CURLSH* m_shareHandle { nullptr };
};

// CurlContext --------------------------------------------

class CurlRequestScheduler;
class CurlStreamScheduler;

class CurlContext : public CurlGlobal {
    WTF_MAKE_NONCOPYABLE(CurlContext);
    friend NeverDestroyed<CurlContext>;
public:
    WEBCORE_EXPORT static CurlContext& singleton();

    virtual ~CurlContext();

    const CurlShareHandle& shareHandle() { return m_shareHandle; }

    CurlRequestScheduler& scheduler() { return *m_scheduler; }
    WEBCORE_EXPORT CurlStreamScheduler& streamScheduler();

    // Proxy
    const CurlProxySettings& proxySettings() const { return m_proxySettings; }
    void setProxySettings(const CurlProxySettings& settings) { m_proxySettings = settings; }
    void setProxyUserPass(const String& user, const String& password) { m_proxySettings.setUserPass(user, password); }
    void setDefaultProxyAuthMethod() { m_proxySettings.setDefaultAuthMethod(); }
    void setProxyAuthMethod(long authMethod) { m_proxySettings.setAuthMethod(authMethod); }

    // SSL
    CurlSSLHandle& sslHandle() { return m_sslHandle; }

    // HTTP/2
    bool isHttp2Enabled() const;

    // Timeout
    Seconds dnsCacheTimeout() const { return m_dnsCacheTimeout; }
    Seconds connectTimeout() const { return m_connectTimeout; }
    Seconds defaultTimeoutInterval() const { return m_defaultTimeoutInterval; }

#ifndef NDEBUG
    FILE* getLogFile() const { return m_logFile; }
    bool isVerbose() const { return m_verbose; }
#endif

private:
    CurlContext();
    void initShareHandle();

    CurlProxySettings m_proxySettings;
    CurlShareHandle m_shareHandle;
    CurlSSLHandle m_sslHandle;
    std::unique_ptr<CurlRequestScheduler> m_scheduler;

    Seconds m_dnsCacheTimeout { Seconds::fromMinutes(5) };
    Seconds m_connectTimeout { 30.0 };
    Seconds m_defaultTimeoutInterval { 60.0 };

#ifndef NDEBUG
    FILE* m_logFile { nullptr };
    bool m_verbose { false };
#endif
};

// CurlMultiHandle --------------------------------------------

class CurlMultiHandle {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CurlMultiHandle);

public:
    CurlMultiHandle();
    ~CurlMultiHandle();

    void setMaxConnects(long);
    void setMaxTotalConnections(long);
    void setMaxHostConnections(long);

    CURLMcode addHandle(CURL*);
    CURLMcode removeHandle(CURL*);

    CURLMcode getFdSet(fd_set&, fd_set&, fd_set&, int&);
    CURLMcode poll(const Vector<curl_waitfd>&, int);
    CURLMcode wakeUp();
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

class CertificateInfo;
class CurlSSLVerifier;
class HTTPHeaderMap;
class NetworkLoadMetrics;

class CurlHandle {
    WTF_MAKE_FAST_ALLOCATED;
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
    const URL& url() const { return m_url; }

    CURLcode perform();
    CURLcode pause(int);

    static const String errorDescription(CURLcode);

    void enableShareHandle();

    void setUrl(const URL&);
    void enableSSLForHost(const String&);

    void appendRequestHeaders(const HTTPHeaderMap&);
    void appendRequestHeader(const String& name, const String& value);
    void appendRequestHeader(const String& name);
    void removeRequestHeader(const String& name);

    void enableHttp();
    void enableHttpGetRequest();
    void enableHttpHeadRequest();
    void enableHttpPostRequest();
    void setPostFields(const uint8_t*, long);
    void setPostFieldLarge(curl_off_t);
    void enableHttpPutRequest();
    void setInFileSizeLarge(curl_off_t);
    void setHttpCustomRequest(const String&);

    void enableConnectionOnly();

    void enableAcceptEncoding();
    void enableAllowedProtocols();

    void setHttpAuthUserPass(const String&, const String&, long authType = CURLAUTH_ANY);

    void disableServerTrustEvaluation();
    void setCACertPath(const char*);
    void setCACertBlob(void*, size_t);
    void setSslVerifyPeer(VerifyPeer);
    void setSslVerifyHost(VerifyHost);
    void setSslCert(const char*);
    void setSslCertType(const char*);
    void setSslKeyPassword(const char*);
    void setSslCipherList(const char*);
    void setSslECCurves(const char*);

    void enableProxyIfExists();

    void setDnsCacheTimeout(Seconds);
    void setConnectTimeout(Seconds);
    void setTimeout(Seconds);

    // Callback function
    void setHeaderCallbackFunction(curl_write_callback, void*);
    void setWriteCallbackFunction(curl_write_callback, void*);
    void setReadCallbackFunction(curl_read_callback, void*);
    void setSslCtxCallbackFunction(curl_ssl_ctx_callback, void*);
    void setDebugCallbackFunction(curl_debug_callback, void*);

    // Status
    std::optional<String> getProxyUrl();
    std::optional<long> getResponseCode();
    std::optional<long> getHttpConnectCode();
    std::optional<long long> getContentLength();
    std::optional<long> getHttpAuthAvail();
    std::optional<long> getProxyAuthAvail();
    std::optional<long> getHttpVersion();
    std::optional<long> getSSLVerifyResult() const;
    std::optional<NetworkLoadMetrics> getNetworkLoadMetrics(MonotonicTime startTime);
    void addExtraNetworkLoadMetrics(NetworkLoadMetrics&);

    std::optional<CertificateInfo> certificateInfo() const;

    static long long maxCurlOffT();

    // socket
    Expected<curl_socket_t, CURLcode> getActiveSocket();
    CURLcode send(const uint8_t*, size_t, size_t&);
    CURLcode receive(uint8_t*, size_t, size_t&);

#ifndef NDEBUG
    void enableVerboseIfUsed();
    void enableStdErrIfUsed();
#endif

private:
    struct TLSConnectionInfo {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        String protocol;
        String cipher;
    };

    void enableRequestHeaders();
    static int expectedSizeOfCurlOffT();

    static CURLcode willSetupSslCtxCallback(CURL*, void* sslCtx, void* userData);
    CURLcode willSetupSslCtx(void* sslCtx);

    std::optional<SSL*> sslConnection() const;

    CURL* m_handle { nullptr };
    char m_errorBuffer[CURL_ERROR_SIZE] { };

    URL m_url;
    CurlSList m_requestHeaders;

    std::unique_ptr<CurlSSLVerifier> m_sslVerifier;
    std::unique_ptr<TLSConnectionInfo> m_tlsConnectionInfo;
    mutable std::unique_ptr<CertificateInfo> m_certificateInfo;
};

} // namespace WebCore
