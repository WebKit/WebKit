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

#include "config.h"
#include "CurlContext.h"

#if USE(CURL)
#include "HTTPHeaderMap.h"
#include <NetworkLoadMetrics.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

#if OS(WINDOWS)
#include "WebCoreBundleWin.h"
#include <shlobj.h>
#include <shlwapi.h>
#endif

namespace WebCore {

static CString cookieJarPath()
{
    char* cookieJarPath = getenv("CURL_COOKIE_JAR_PATH");
    if (cookieJarPath)
        return cookieJarPath;

#if OS(WINDOWS)
    char executablePath[MAX_PATH];
    char appDataDirectory[MAX_PATH];
    char cookieJarFullPath[MAX_PATH];
    char cookieJarDirectory[MAX_PATH];

    if (FAILED(::SHGetFolderPathA(0, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, 0, 0, appDataDirectory))
        || FAILED(::GetModuleFileNameA(0, executablePath, MAX_PATH)))
        return "cookies.dat";

    ::PathRemoveExtensionA(executablePath);
    LPSTR executableName = ::PathFindFileNameA(executablePath);
    sprintf_s(cookieJarDirectory, MAX_PATH, "%s/%s", appDataDirectory, executableName);
    sprintf_s(cookieJarFullPath, MAX_PATH, "%s/cookies.dat", cookieJarDirectory);

    if (::SHCreateDirectoryExA(0, cookieJarDirectory, 0) != ERROR_SUCCESS
        && ::GetLastError() != ERROR_FILE_EXISTS
        && ::GetLastError() != ERROR_ALREADY_EXISTS)
        return "cookies.dat";

    return cookieJarFullPath;
#else
    return "cookies.dat";
#endif
}

// CurlContext -------------------------------------------------------------------

CurlContext& CurlContext::singleton()
{
    static NeverDestroyed<CurlContext> sharedInstance;
    return sharedInstance;
}

CurlContext::CurlContext()
: m_cookieJarFileName { cookieJarPath() }
, m_cookieJar { std::make_unique<CookieJarCurlFileSystem>() }
{
    initCookieSession();

#ifndef NDEBUG
    m_verbose = getenv("DEBUG_CURL");

    char* logFile = getenv("CURL_LOG_FILE");
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

// Cookie =======================

void CurlContext::initCookieSession()
{
    // Curl saves both persistent cookies, and session cookies to the cookie file.
    // The session cookies should be deleted before starting a new session.

    CURL* curl = curl_easy_init();

    if (!curl)
        return;

    curl_easy_setopt(curl, CURLOPT_SHARE, m_shareHandle.handle());

    if (!m_cookieJarFileName.isNull()) {
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, m_cookieJarFileName.data());
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, m_cookieJarFileName.data());
    }

    curl_easy_setopt(curl, CURLOPT_COOKIESESSION, 1);

    curl_easy_cleanup(curl);
}

// Proxy =======================

const String CurlContext::ProxyInfo::url() const
{
    String userPass;
    if (username.length() || password.length())
        userPass = username + ":" + password + "@";

    return String("http://") + userPass + host + ":" + String::number(port);
}

void CurlContext::setProxyInfo(const String& host,
    unsigned long port,
    CurlProxyType type,
    const String& username,
    const String& password)
{
    ProxyInfo info;

    info.host = host;
    info.port = port;
    info.type = type;
    info.username = username;
    info.password = password;

    setProxyInfo(info);
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

StaticLock* CurlShareHandle::mutexFor(curl_lock_data data)
{
    static StaticLock cookieMutex;
    static StaticLock dnsMutex;
    static StaticLock shareMutex;

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
}

CurlMultiHandle::~CurlMultiHandle()
{
    if (m_multiHandle)
        curl_multi_cleanup(m_multiHandle);
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

void CurlHandle::initialize()
{
    curl_easy_setopt(m_handle, CURLOPT_ERRORBUFFER, m_errorBuffer);
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

void CurlHandle::enableHttpGetRequest()
{
    curl_easy_setopt(m_handle, CURLOPT_HTTPGET, 1L);
}

void CurlHandle::enableHttpHeadRequest()
{
    curl_easy_setopt(m_handle, CURLOPT_NOBODY, 1L);
}

void CurlHandle::enableHttpPostRequest()
{
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

void CurlHandle::enableHttpAuthentication(long option)
{
    curl_easy_setopt(m_handle, CURLOPT_HTTPAUTH, option);
}

void CurlHandle::setHttpAuthUserPass(const String& user, const String& password)
{
    curl_easy_setopt(m_handle, CURLOPT_USERNAME, user.utf8().data());
    curl_easy_setopt(m_handle, CURLOPT_PASSWORD, password.utf8().data());
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

void CurlHandle::enableCookieJarIfExists()
{
    const char* cookieJar = CurlContext::singleton().getCookieJarFileName();
    if (cookieJar)
        curl_easy_setopt(m_handle, CURLOPT_COOKIEJAR, cookieJar);
}

void CurlHandle::setCookieList(const char* cookieList)
{
    if (!cookieList)
        return;

    curl_easy_setopt(m_handle, CURLOPT_COOKIELIST, cookieList);
}

void CurlHandle::fetchCookieList(CurlSList &cookies) const
{
    curl_easy_getinfo(m_handle, CURLINFO_COOKIELIST, static_cast<struct curl_slist**>(cookies));
}

void CurlHandle::enableProxyIfExists()
{
    auto& proxy = CurlContext::singleton().proxyInfo();

    if (proxy.type != CurlProxyType::Invalid) {
        curl_easy_setopt(m_handle, CURLOPT_PROXY, proxy.url().utf8().data());
        curl_easy_setopt(m_handle, CURLOPT_PROXYTYPE, proxy.type);
    }
}

void CurlHandle::enableTimeout()
{
    static const long dnsCacheTimeout = 5 * 60; // [sec.]

    curl_easy_setopt(m_handle, CURLOPT_DNS_CACHE_TIMEOUT, dnsCacheTimeout);
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

std::optional<uint16_t> CurlHandle::getPrimaryPort()
{
    if (!m_handle)
        return std::nullopt;

    long port;
    CURLcode errorCode = curl_easy_getinfo(m_handle, CURLINFO_PRIMARY_PORT, &port);
    if (errorCode != CURLE_OK)
        return std::nullopt;

    /*
     * https://github.com/curl/curl/blob/master/lib/connect.c#L612-L660
     * confirmed that `port` is originally unsigned short.
     */
    return static_cast<uint16_t>(port);
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

std::optional<NetworkLoadMetrics> CurlHandle::getNetworkLoadMetrics()
{
    double nameLookup = 0.0;
    double connect = 0.0;
    double appConnect = 0.0;
    double startTransfer = 0.0;

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

    NetworkLoadMetrics networkLoadMetrics;

    networkLoadMetrics.domainLookupStart = Seconds(0);
    networkLoadMetrics.domainLookupEnd = Seconds(nameLookup);
    networkLoadMetrics.connectStart = Seconds(nameLookup);
    networkLoadMetrics.connectEnd = Seconds(connect);

    if (appConnect > 0.0) {
        networkLoadMetrics.secureConnectionStart = Seconds(connect);
        networkLoadMetrics.connectEnd = Seconds(appConnect);
    }

    networkLoadMetrics.requestStart = networkLoadMetrics.connectEnd;
    networkLoadMetrics.responseStart = Seconds(startTransfer);

    return networkLoadMetrics;
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
    if (CurlContext::singleton().getLogFile())
        curl_easy_setopt(m_handle, CURLOPT_VERBOSE, 1);
}

#endif

}

#endif
