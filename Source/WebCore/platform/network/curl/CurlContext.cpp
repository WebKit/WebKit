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

#include "config.h"
#include "CurlContext.h"

#if USE(CURL)
#include "CertificateInfo.h"
#include "CurlRequestScheduler.h"
#include "CurlSSLHandle.h"
#include "CurlSSLVerifier.h"
#include "HTTPHeaderMap.h"
#include <NetworkLoadMetrics.h>
#include <mutex>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

#if OS(WINDOWS)
#include "WebCoreBundleWin.h"
#include <shlobj.h>
#include <shlwapi.h>
#endif

namespace WebCore {

class EnvironmentVariableReader {
public:
    const char* read(const char* name) { return ::getenv(name); }
    bool defined(const char* name) { return read(name) != nullptr; }

    template<typename T> std::optional<T> readAs(const char* name)
    {
        if (const char* valueStr = read(name)) {
            T value;
            if (sscanf(valueStr, sscanTemplate<T>(), &value) == 1)
                return value;
        }

        return std::nullopt;
    }

private:
    template<typename T> const char* sscanTemplate()
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
};

template<>
constexpr const char* EnvironmentVariableReader::sscanTemplate<signed>() { return "%d"; }

template<>
constexpr const char* EnvironmentVariableReader::sscanTemplate<unsigned>() { return "%u"; }

// ALPN Protocol ID (RFC7301) https://tools.ietf.org/html/rfc7301
static const ASCIILiteral httpVersion10 { "http/1.0"_s };
static const ASCIILiteral httpVersion11 { "http/1.1"_s };
static const ASCIILiteral httpVersion2 { "h2"_s };

// CurlContext -------------------------------------------------------------------

CurlContext& CurlContext::singleton()
{
    static NeverDestroyed<CurlContext> sharedInstance;
    return sharedInstance;
}

CurlContext::CurlContext()
{
    initShareHandle();

    EnvironmentVariableReader envVar;

    if (auto value = envVar.readAs<unsigned>("WEBKIT_CURL_DNS_CACHE_TIMEOUT"))
        m_dnsCacheTimeout = Seconds(*value);

    if (auto value = envVar.readAs<unsigned>("WEBKIT_CURL_CONNECT_TIMEOUT"))
        m_connectTimeout = Seconds(*value);

    long maxConnects { CurlDefaultMaxConnects };
    long maxTotalConnections { CurlDefaultMaxTotalConnections };
    long maxHostConnections { CurlDefaultMaxHostConnections };

    if (auto value = envVar.readAs<signed>("WEBKIT_CURL_MAXCONNECTS"))
        maxConnects = *value;

    if (auto value = envVar.readAs<signed>("WEBKIT_CURL_MAX_TOTAL_CONNECTIONS"))
        maxTotalConnections = *value;

    if (auto value = envVar.readAs<signed>("WEBKIT_CURL_MAX_HOST_CONNECTIONS"))
        maxHostConnections = *value;

    m_scheduler = std::make_unique<CurlRequestScheduler>(maxConnects, maxTotalConnections, maxHostConnections);

#ifndef NDEBUG
    m_verbose = envVar.defined("DEBUG_CURL");

    auto logFile = envVar.read("CURL_LOG_FILE");
    if (logFile)
        m_logFile = fopen(logFile, "a");
#endif
}

CurlContext::~CurlContext()
{
#ifndef NDEBUG
    if (m_logFile)
        fclose(m_logFile);
#endif
}

void CurlContext::initShareHandle()
{
    CURL* curl = curl_easy_init();

    if (!curl)
        return;

    curl_easy_setopt(curl, CURLOPT_SHARE, m_shareHandle.handle());

    curl_easy_cleanup(curl);
}

bool CurlContext::isHttp2Enabled() const
{
    curl_version_info_data* data = curl_version_info(CURLVERSION_NOW);
    return data->features & CURL_VERSION_HTTP2;
}

// CurlShareHandle --------------------------------------------

CurlShareHandle::CurlShareHandle()
{
    m_shareHandle = curl_share_init();
    curl_share_setopt(m_shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
    curl_share_setopt(m_shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(m_shareHandle, CURLSHOPT_LOCKFUNC, lockCallback);
    curl_share_setopt(m_shareHandle, CURLSHOPT_UNLOCKFUNC, unlockCallback);
}

CurlShareHandle::~CurlShareHandle()
{
    if (m_shareHandle)
        curl_share_cleanup(m_shareHandle);
}

void CurlShareHandle::lockCallback(CURL*, curl_lock_data data, curl_lock_access, void*)
{
    if (auto* mutex = mutexFor(data))
        mutex->lock();
}

void CurlShareHandle::unlockCallback(CURL*, curl_lock_data data, void*)
{
    if (auto* mutex = mutexFor(data))
        mutex->unlock();
}

Lock* CurlShareHandle::mutexFor(curl_lock_data data)
{
    static Lock cookieMutex;
    static Lock dnsMutex;
    static Lock shareMutex;

    switch (data) {
    case CURL_LOCK_DATA_COOKIE:
        return &cookieMutex;
    case CURL_LOCK_DATA_DNS:
        return &dnsMutex;
    case CURL_LOCK_DATA_SHARE:
        return &shareMutex;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

// CurlMultiHandle --------------------------------------------

CurlMultiHandle::CurlMultiHandle()
{
    m_multiHandle = curl_multi_init();

    if (CurlContext::singleton().isHttp2Enabled())
        curl_multi_setopt(m_multiHandle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
}

CurlMultiHandle::~CurlMultiHandle()
{
    if (m_multiHandle)
        curl_multi_cleanup(m_multiHandle);
}

void CurlMultiHandle::setMaxConnects(long maxConnects)
{
    if (maxConnects < 0)
        return;

    curl_multi_setopt(m_multiHandle, CURLMOPT_MAXCONNECTS, maxConnects);
}

void CurlMultiHandle::setMaxTotalConnections(long maxTotalConnections)
{
    curl_multi_setopt(m_multiHandle, CURLMOPT_MAX_TOTAL_CONNECTIONS, maxTotalConnections);
}

void CurlMultiHandle::setMaxHostConnections(long maxHostConnections)
{
    curl_multi_setopt(m_multiHandle, CURLMOPT_MAX_HOST_CONNECTIONS, maxHostConnections);
}

CURLMcode CurlMultiHandle::addHandle(CURL* handle)
{
    return curl_multi_add_handle(m_multiHandle, handle);
}

CURLMcode CurlMultiHandle::removeHandle(CURL* handle)
{
    return curl_multi_remove_handle(m_multiHandle, handle);
}

CURLMcode CurlMultiHandle::getFdSet(fd_set& readFdSet, fd_set& writeFdSet, fd_set& excFdSet, int& maxFd)
{
    FD_ZERO(&readFdSet);
    FD_ZERO(&writeFdSet);
    FD_ZERO(&excFdSet);
    maxFd = 0;

    return curl_multi_fdset(m_multiHandle, &readFdSet, &writeFdSet, &excFdSet, &maxFd);
}

CURLMcode CurlMultiHandle::perform(int& runningHandles)
{
    return curl_multi_perform(m_multiHandle, &runningHandles);
}

CURLMsg* CurlMultiHandle::readInfo(int& messagesInQueue)
{
    return curl_multi_info_read(m_multiHandle, &messagesInQueue);
}

// CurlHandle -------------------------------------------------

CurlHandle::CurlHandle()
{
    m_handle = curl_easy_init();
    curl_easy_setopt(m_handle, CURLOPT_ERRORBUFFER, m_errorBuffer);

    enableShareHandle();
    enableAllowedProtocols();
    enableAcceptEncoding();

    setDnsCacheTimeout(CurlContext::singleton().dnsCacheTimeout());
    setConnectTimeout(CurlContext::singleton().connectTimeout());

    enableProxyIfExists();

#ifndef NDEBUG
    enableVerboseIfUsed();
    enableStdErrIfUsed();
#endif
}

CurlHandle::~CurlHandle()
{
    if (m_handle)
        curl_easy_cleanup(m_handle);
}

const String CurlHandle::errorDescription(CURLcode errorCode)
{
    return String(curl_easy_strerror(errorCode));
}

void CurlHandle::enableSSLForHost(const String& host)
{
    auto& sslHandle = CurlContext::singleton().sslHandle();
    if (auto sslClientCertificate = sslHandle.getSSLClientCertificate(host)) {
        setSslCert(sslClientCertificate->first.utf8().data());
        setSslCertType("P12");
        setSslKeyPassword(sslClientCertificate->second.utf8().data());
    }

    if (sslHandle.canIgnoreAnyHTTPSCertificatesForHost(host) || sslHandle.shouldIgnoreSSLErrors()) {
        setSslVerifyPeer(CurlHandle::VerifyPeer::Disable);
        setSslVerifyHost(CurlHandle::VerifyHost::LooseNameCheck);
    } else {
        setSslVerifyPeer(CurlHandle::VerifyPeer::Enable);
        setSslVerifyHost(CurlHandle::VerifyHost::StrictNameCheck);
    }

    const auto& cipherList = sslHandle.getCipherList();
    if (!cipherList.isEmpty())
        setSslCipherList(cipherList.utf8().data());

    setSslCtxCallbackFunction(willSetupSslCtxCallback, this);

    if (auto* path = WTF::get_if<String>(sslHandle.getCACertInfo()))
        setCACertPath(path->utf8().data());
}

CURLcode CurlHandle::willSetupSslCtx(void* sslCtx)
{
    if (!sslCtx)
        return CURLE_ABORTED_BY_CALLBACK;

    if (!m_sslVerifier)
        m_sslVerifier = std::make_unique<CurlSSLVerifier>(*this, sslCtx);

    return CURLE_OK;
}

CURLcode CurlHandle::willSetupSslCtxCallback(CURL*, void* sslCtx, void* userData)
{
    return static_cast<CurlHandle*>(userData)->willSetupSslCtx(sslCtx);
}

int CurlHandle::sslErrors() const
{
    return m_sslVerifier ? m_sslVerifier->sslErrors() : 0;
}

CURLcode CurlHandle::perform()
{
    return curl_easy_perform(m_handle);
}

CURLcode CurlHandle::pause(int bitmask)
{
    return curl_easy_pause(m_handle, bitmask);
}

void CurlHandle::enableShareHandle()
{
    curl_easy_setopt(m_handle, CURLOPT_SHARE, CurlContext::singleton().shareHandle().handle());
}

void CurlHandle::setUrl(const URL& url)
{
    m_url = url.isolatedCopy();

    URL curlUrl = url;

    // Remove any fragment part, otherwise curl will send it as part of the request.
    curlUrl.removeFragmentIdentifier();

    // Remove any query part sent to a local file.
    if (curlUrl.isLocalFile()) {
        // By setting the query to a null string it'll be removed.
        if (!curlUrl.query().isEmpty())
            curlUrl.setQuery(String());
    }

    // url is in ASCII so latin1() will only convert it to char* without character translation.
    curl_easy_setopt(m_handle, CURLOPT_URL, curlUrl.string().latin1().data());

    if (url.protocolIs("https"))
        enableSSLForHost(m_url.host().toString());
}

void CurlHandle::appendRequestHeaders(const HTTPHeaderMap& headers)
{
    if (headers.size()) {
        for (auto& entry : headers) {
            auto& value = entry.value;
            appendRequestHeader(entry.key, entry.value);
        }
    }
}

void CurlHandle::appendRequestHeader(const String& name, const String& value)
{
    String header(name);

    if (value.isEmpty()) {
        // Insert the ; to tell curl that this header has an empty value.
        header.append(";");
    } else {
        header.append(": ");
        header.append(value);
    }

    appendRequestHeader(header);
}

void CurlHandle::removeRequestHeader(const String& name)
{
    // Add a header with no content, the internally used header will get disabled. 
    String header(name);
    header.append(":");

    appendRequestHeader(header);
}

void CurlHandle::appendRequestHeader(const String& header)
{
    bool needToEnable = m_requestHeaders.isEmpty();

    m_requestHeaders.append(header);

    if (needToEnable)
        enableRequestHeaders();
}

void CurlHandle::enableRequestHeaders()
{
    if (m_requestHeaders.isEmpty())
        return;

    const struct curl_slist* headers = m_requestHeaders.head();
    curl_easy_setopt(m_handle, CURLOPT_HTTPHEADER, headers);
}

void CurlHandle::enableHttp()
{
    if (m_url.protocolIs("https") && CurlContext::singleton().isHttp2Enabled()) {
        curl_easy_setopt(m_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
        curl_easy_setopt(m_handle, CURLOPT_PIPEWAIT, 1L);
        curl_easy_setopt(m_handle, CURLOPT_SSL_ENABLE_ALPN, 1L);
        curl_easy_setopt(m_handle, CURLOPT_SSL_ENABLE_NPN, 0L);
    } else
        curl_easy_setopt(m_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
}

void CurlHandle::enableHttpGetRequest()
{
    enableHttp();
    curl_easy_setopt(m_handle, CURLOPT_HTTPGET, 1L);
}

void CurlHandle::enableHttpHeadRequest()
{
    enableHttp();
    curl_easy_setopt(m_handle, CURLOPT_NOBODY, 1L);
}

void CurlHandle::enableHttpPostRequest()
{
    enableHttp();
    curl_easy_setopt(m_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(m_handle, CURLOPT_POSTFIELDSIZE, 0L);
}

void CurlHandle::setPostFields(const char* data, long size)
{
    curl_easy_setopt(m_handle, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(m_handle, CURLOPT_POSTFIELDSIZE, size);
}

void CurlHandle::setPostFieldLarge(curl_off_t size)
{
    if (expectedSizeOfCurlOffT() != sizeof(long long))
        size = static_cast<int>(size);

    curl_easy_setopt(m_handle, CURLOPT_POSTFIELDSIZE_LARGE, size);
}

void CurlHandle::enableHttpPutRequest()
{
    enableHttp();
    curl_easy_setopt(m_handle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(m_handle, CURLOPT_INFILESIZE, 0L);
}

void CurlHandle::setInFileSizeLarge(curl_off_t size)
{
    if (expectedSizeOfCurlOffT() != sizeof(long long))
        size = static_cast<int>(size);

    curl_easy_setopt(m_handle, CURLOPT_INFILESIZE_LARGE, size);
}

void CurlHandle::setHttpCustomRequest(const String& method)
{
    enableHttp();
    curl_easy_setopt(m_handle, CURLOPT_CUSTOMREQUEST, method.ascii().data());
}

void CurlHandle::enableAcceptEncoding()
{
    // enable all supported built-in compressions (gzip and deflate) through Accept-Encoding:
    curl_easy_setopt(m_handle, CURLOPT_ENCODING, "");
}

void CurlHandle::enableAllowedProtocols()
{
    static const long allowedProtocols = CURLPROTO_FILE | CURLPROTO_FTP | CURLPROTO_FTPS | CURLPROTO_HTTP | CURLPROTO_HTTPS;

    curl_easy_setopt(m_handle, CURLOPT_PROTOCOLS, allowedProtocols);
}

void CurlHandle::setHttpAuthUserPass(const String& user, const String& password, long authType)
{
    curl_easy_setopt(m_handle, CURLOPT_USERNAME, user.utf8().data());
    curl_easy_setopt(m_handle, CURLOPT_PASSWORD, password.utf8().data());
    curl_easy_setopt(m_handle, CURLOPT_HTTPAUTH, authType);
}

void CurlHandle::setCACertPath(const char* path)
{
    if (path)
        curl_easy_setopt(m_handle, CURLOPT_CAINFO, path);
}

void CurlHandle::setSslVerifyPeer(VerifyPeer verifyPeer)
{
    curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYPEER, static_cast<long>(verifyPeer));
}

void CurlHandle::setSslVerifyHost(VerifyHost verifyHost)
{
    curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYHOST, static_cast<long>(verifyHost));
}

void CurlHandle::setSslCert(const char* cert)
{
    curl_easy_setopt(m_handle, CURLOPT_SSLCERT, cert);
}

void CurlHandle::setSslCertType(const char* type)
{
    curl_easy_setopt(m_handle, CURLOPT_SSLCERTTYPE, type);
}

void CurlHandle::setSslKeyPassword(const char* password)
{
    curl_easy_setopt(m_handle, CURLOPT_KEYPASSWD, password);
}

void CurlHandle::setSslCipherList(const char* cipherList)
{
    curl_easy_setopt(m_handle, CURLOPT_SSL_CIPHER_LIST, cipherList);
}

void CurlHandle::enableProxyIfExists()
{
    auto& proxy = CurlContext::singleton().proxySettings();

    switch (proxy.mode()) {
    case CurlProxySettings::Mode::Default :
        // For the proxy set by environment variable
        if (!proxy.user().isEmpty())
            curl_easy_setopt(m_handle, CURLOPT_PROXYUSERNAME, proxy.user().utf8().data());
        if (!proxy.password().isEmpty())
            curl_easy_setopt(m_handle, CURLOPT_PROXYPASSWORD, proxy.password().utf8().data());
        curl_easy_setopt(m_handle, CURLOPT_PROXYAUTH, proxy.authMethod());
        break;
    case CurlProxySettings::Mode::NoProxy :
        // Disable the use of a proxy, even if there is an environment variable set for it.
        curl_easy_setopt(m_handle, CURLOPT_PROXY, "");
        break;
    case CurlProxySettings::Mode::Custom :
        curl_easy_setopt(m_handle, CURLOPT_PROXY, proxy.url().utf8().data());
        curl_easy_setopt(m_handle, CURLOPT_NOPROXY, proxy.ignoreHosts().utf8().data());
        curl_easy_setopt(m_handle, CURLOPT_PROXYAUTH, proxy.authMethod());
        break;
    }
}

static CURLoption safeTimeValue(double time)
{
    auto value = static_cast<unsigned>(time >= 0.0 ? time : 0);
    return static_cast<CURLoption>(value);
}

void CurlHandle::setDnsCacheTimeout(Seconds timeout)
{
    curl_easy_setopt(m_handle, CURLOPT_DNS_CACHE_TIMEOUT, safeTimeValue(timeout.seconds()));
}

void CurlHandle::setConnectTimeout(Seconds timeout)
{
    curl_easy_setopt(m_handle, CURLOPT_CONNECTTIMEOUT, safeTimeValue(timeout.seconds()));
}

void CurlHandle::setTimeout(Seconds timeout)
{
    // Originally CURLOPT_TIMEOUT_MS was used here, but that is not the
    // idle timeout, but entire duration time limit. It's not safe to specify
    // such a time limit for communications, such as downloading.
    // CURLOPT_LOW_SPEED_LIMIT is used instead. It enables the speed watcher
    // and if the speed is below specified limit and last for specified duration,
    // it invokes timeout error.
    curl_easy_setopt(m_handle, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(m_handle, CURLOPT_LOW_SPEED_TIME, safeTimeValue(timeout.seconds()));
}

void CurlHandle::setHeaderCallbackFunction(curl_write_callback callbackFunc, void* userData)
{
    curl_easy_setopt(m_handle, CURLOPT_HEADERFUNCTION, callbackFunc);
    curl_easy_setopt(m_handle, CURLOPT_HEADERDATA, userData);
}

void CurlHandle::setWriteCallbackFunction(curl_write_callback callbackFunc, void* userData)
{
    curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION, callbackFunc);
    curl_easy_setopt(m_handle, CURLOPT_WRITEDATA, userData);
}

void CurlHandle::setReadCallbackFunction(curl_read_callback callbackFunc, void* userData)
{
    curl_easy_setopt(m_handle, CURLOPT_READFUNCTION, callbackFunc);
    curl_easy_setopt(m_handle, CURLOPT_READDATA, userData);
}

void CurlHandle::setSslCtxCallbackFunction(curl_ssl_ctx_callback callbackFunc, void* userData)
{
    curl_easy_setopt(m_handle, CURLOPT_SSL_CTX_DATA, userData);
    curl_easy_setopt(m_handle, CURLOPT_SSL_CTX_FUNCTION, callbackFunc);
}

void CurlHandle::enableConnectionOnly()
{
    curl_easy_setopt(m_handle, CURLOPT_CONNECT_ONLY, 1L);
}

std::optional<String> CurlHandle::getProxyUrl()
{
    auto& proxy = CurlContext::singleton().proxySettings();
    if (proxy.mode() == CurlProxySettings::Mode::Default)
        return std::nullopt;

    return proxy.url();
}

std::optional<long> CurlHandle::getResponseCode()
{
    if (!m_handle)
        return std::nullopt;

    long responseCode;
    CURLcode errorCode = curl_easy_getinfo(m_handle, CURLINFO_RESPONSE_CODE, &responseCode);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    return responseCode;
}

std::optional<long> CurlHandle::getHttpConnectCode()
{
    if (!m_handle)
        return std::nullopt;

    long httpConnectCode;
    CURLcode errorCode = curl_easy_getinfo(m_handle, CURLINFO_HTTP_CONNECTCODE, &httpConnectCode);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    return httpConnectCode;
}

std::optional<long long> CurlHandle::getContentLength()
{
    if (!m_handle)
        return std::nullopt;

    double contentLength;

    CURLcode errorCode = curl_easy_getinfo(m_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    return static_cast<long long>(contentLength);
}

std::optional<long> CurlHandle::getHttpAuthAvail()
{
    if (!m_handle)
        return std::nullopt;

    long httpAuthAvailable;
    CURLcode errorCode = curl_easy_getinfo(m_handle, CURLINFO_HTTPAUTH_AVAIL, &httpAuthAvailable);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    return httpAuthAvailable;
}

std::optional<long> CurlHandle::getProxyAuthAvail()
{
    if (!m_handle)
        return std::nullopt;

    long proxyAuthAvailable;
    CURLcode errorCode = curl_easy_getinfo(m_handle, CURLINFO_PROXYAUTH_AVAIL, &proxyAuthAvailable);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    return proxyAuthAvailable;
}

std::optional<long> CurlHandle::getHttpVersion()
{
    if (!m_handle)
        return std::nullopt;

    long version;
    CURLcode errorCode = curl_easy_getinfo(m_handle, CURLINFO_HTTP_VERSION, &version);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    return version;
}

std::optional<NetworkLoadMetrics> CurlHandle::getNetworkLoadMetrics(const WTF::Seconds& domainLookupStart)
{
    double nameLookup = 0.0;
    double connect = 0.0;
    double appConnect = 0.0;
    double startTransfer = 0.0;
    long version = 0;

    if (!m_handle)
        return std::nullopt;

    CURLcode errorCode = curl_easy_getinfo(m_handle, CURLINFO_NAMELOOKUP_TIME, &nameLookup);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    errorCode = curl_easy_getinfo(m_handle, CURLINFO_CONNECT_TIME, &connect);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    errorCode = curl_easy_getinfo(m_handle, CURLINFO_APPCONNECT_TIME, &appConnect);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    errorCode = curl_easy_getinfo(m_handle, CURLINFO_STARTTRANSFER_TIME, &startTransfer);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    errorCode = curl_easy_getinfo(m_handle, CURLINFO_HTTP_VERSION, &version);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    NetworkLoadMetrics networkLoadMetrics;

    networkLoadMetrics.domainLookupStart = domainLookupStart;
    networkLoadMetrics.domainLookupEnd = domainLookupStart + Seconds(nameLookup);
    networkLoadMetrics.connectStart = domainLookupStart + Seconds(nameLookup);
    networkLoadMetrics.connectEnd = domainLookupStart + Seconds(connect);

    if (appConnect > 0.0) {
        networkLoadMetrics.secureConnectionStart = domainLookupStart + Seconds(connect);
        networkLoadMetrics.connectEnd = domainLookupStart + Seconds(appConnect);
    }

    networkLoadMetrics.requestStart = networkLoadMetrics.connectEnd;
    networkLoadMetrics.responseStart = domainLookupStart + Seconds(startTransfer);

    if (version == CURL_HTTP_VERSION_1_0)
        networkLoadMetrics.protocol = httpVersion10;
    else if (version == CURL_HTTP_VERSION_1_1)
        networkLoadMetrics.protocol = httpVersion11;
    else if (version == CURL_HTTP_VERSION_2)
        networkLoadMetrics.protocol = httpVersion2;

    return networkLoadMetrics;
}

void CurlHandle::addExtraNetworkLoadMetrics(NetworkLoadMetrics& networkLoadMetrics)
{
    long requestHeaderSize = 0;
    curl_off_t requestBodySize = 0;
    long responseHeaderSize = 0;
    curl_off_t responseBodySize = 0;
    char* ip = nullptr;
    long port = 0;

    // FIXME: Gets total request size not just headers https://bugs.webkit.org/show_bug.cgi?id=188363
    CURLcode errorCode = curl_easy_getinfo(m_handle, CURLINFO_REQUEST_SIZE, &requestHeaderSize);
    if (errorCode != CURLE_OK)
        return;

    errorCode = curl_easy_getinfo(m_handle, CURLINFO_SIZE_UPLOAD_T, &requestBodySize);
    if (errorCode != CURLE_OK)
        return;

    errorCode = curl_easy_getinfo(m_handle, CURLINFO_HEADER_SIZE, &responseHeaderSize);
    if (errorCode != CURLE_OK)
        return;

    errorCode = curl_easy_getinfo(m_handle, CURLINFO_SIZE_DOWNLOAD_T, &responseBodySize);
    if (errorCode != CURLE_OK)
        return;

    errorCode = curl_easy_getinfo(m_handle, CURLINFO_PRIMARY_IP, &ip);
    if (errorCode != CURLE_OK)
        return;

    errorCode = curl_easy_getinfo(m_handle, CURLINFO_PRIMARY_PORT, &port);
    if (errorCode != CURLE_OK)
        return;

    networkLoadMetrics.requestHeaderBytesSent = requestHeaderSize;
    networkLoadMetrics.requestBodyBytesSent = requestBodySize;
    networkLoadMetrics.responseHeaderBytesReceived = responseHeaderSize;
    networkLoadMetrics.responseBodyBytesReceived = responseBodySize;

    if (ip) {
        networkLoadMetrics.remoteAddress = String(ip);
        if (port)
            networkLoadMetrics.remoteAddress.append(":" + String::number(port));
    }
}

std::optional<CertificateInfo> CurlHandle::certificateInfo() const
{
    if (!m_sslVerifier)
        return std::nullopt;

    return m_sslVerifier->certificateInfo();
}

long long CurlHandle::maxCurlOffT()
{
    static const long long maxCurlOffT = (1LL << (expectedSizeOfCurlOffT() * 8 - 1)) - 1;

    return maxCurlOffT;
}

int CurlHandle::expectedSizeOfCurlOffT()
{
    // The size of a curl_off_t could be different in WebKit and in cURL depending on
    // compilation flags of both.
    static int expectedSizeOfCurlOffT = 0;
    if (!expectedSizeOfCurlOffT) {
        curl_version_info_data* infoData = curl_version_info(CURLVERSION_NOW);
        if (infoData->features & CURL_VERSION_LARGEFILE)
            expectedSizeOfCurlOffT = sizeof(long long);
        else
            expectedSizeOfCurlOffT = sizeof(int);
    }

    return expectedSizeOfCurlOffT;
}

#ifndef NDEBUG

void CurlHandle::enableVerboseIfUsed()
{
    if (CurlContext::singleton().isVerbose())
        curl_easy_setopt(m_handle, CURLOPT_VERBOSE, 1);
}

void CurlHandle::enableStdErrIfUsed()
{
    if (auto log = CurlContext::singleton().getLogFile())
        curl_easy_setopt(m_handle, CURLOPT_STDERR, log);
}

#endif

// CurlSocketHandle

CurlSocketHandle::CurlSocketHandle(const URL& url, Function<void(CURLcode)>&& errorHandler)
    : m_errorHandler(WTFMove(errorHandler))
{
    // Libcurl is not responsible for the protocol handling. It just handles connection.
    // Only scheme, host and port is required.
    URL urlForConnection;
    urlForConnection.setProtocol(url.protocolIs("wss") ? "https" : "http");
    urlForConnection.setHostAndPort(url.hostAndPort());
    setUrl(urlForConnection);

    enableConnectionOnly();
}

bool CurlSocketHandle::connect()
{
    CURLcode errorCode = perform();
    if (errorCode != CURLE_OK) {
        m_errorHandler(errorCode);
        return false;
    }

    return true;
}

size_t CurlSocketHandle::send(const uint8_t* buffer, size_t size)
{
    size_t totalBytesSent = 0;

    while (totalBytesSent < size) {
        size_t bytesSent = 0;
        CURLcode errorCode = curl_easy_send(handle(), buffer + totalBytesSent, size - totalBytesSent, &bytesSent);
        if (errorCode != CURLE_OK) {
            if (errorCode != CURLE_AGAIN)
                m_errorHandler(errorCode);
            break;
        }

        totalBytesSent += bytesSent;
    }

    return totalBytesSent;
}

std::optional<size_t> CurlSocketHandle::receive(uint8_t* buffer, size_t bufferSize)
{
    size_t bytesRead = 0;

    CURLcode errorCode = curl_easy_recv(handle(), buffer, bufferSize, &bytesRead);
    if (errorCode != CURLE_OK) {
        if (errorCode != CURLE_AGAIN)
            m_errorHandler(errorCode);

        return std::nullopt;
    }

    return bytesRead;
}

std::optional<CurlSocketHandle::WaitResult> CurlSocketHandle::wait(const Seconds& timeout, bool alsoWaitForWrite)
{
    curl_socket_t socket;
    CURLcode errorCode = curl_easy_getinfo(handle(), CURLINFO_ACTIVESOCKET, &socket);
    if (errorCode != CURLE_OK) {
        m_errorHandler(errorCode);
        return std::nullopt;
    }

    int64_t usec = timeout.microsecondsAs<int64_t>();

    struct timeval selectTimeout;
    if (usec <= 0) {
        selectTimeout.tv_sec = 0;
        selectTimeout.tv_usec = 0;
    } else {
        selectTimeout.tv_sec = usec / 1000000;
        selectTimeout.tv_usec = usec % 1000000;
    }

    int rc = 0;
    int maxfd = static_cast<int>(socket) + 1;
    fd_set fdread;
    fd_set fdwrite;
    fd_set fderr;

    // Retry 'select' if it was interrupted by a process signal.
    do {
        FD_ZERO(&fdread);
        FD_SET(socket, &fdread);

        FD_ZERO(&fdwrite);
        if (alsoWaitForWrite)
            FD_SET(socket, &fdwrite);

        FD_ZERO(&fderr);
        FD_SET(socket, &fderr);

        rc = ::select(maxfd, &fdread, &fdwrite, &fderr, &selectTimeout);
    } while (rc == -1 && errno == EINTR);

    if (rc <= 0)
        return std::nullopt;

    WaitResult result;
    result.readable = FD_ISSET(socket, &fdread) || FD_ISSET(socket, &fderr);
    result.writable = FD_ISSET(socket, &fdwrite);
    return result;
}

}

#endif
