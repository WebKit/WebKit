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

#if USE(CURL)
#include "CurlContext.h"

#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

#if OS(WINDOWS)
#include "WebCoreBundleWin.h"
#include <shlobj.h>
#include <shlwapi.h>
#endif

#if USE(CF)
#include <wtf/RetainPtr.h>
#endif

using namespace WebCore;

namespace WebCore {

static CString certificatePath()
{
    char* envPath = getenv("CURL_CA_BUNDLE_PATH");
    if (envPath)
        return envPath;

#if USE(CF)
    CFBundleRef webKitBundleRef = webKitBundle();
    if (webKitBundleRef) {
        RetainPtr<CFURLRef> certURLRef = adoptCF(CFBundleCopyResourceURL(webKitBundleRef, CFSTR("cacert"), CFSTR("pem"), CFSTR("certificates")));
        if (certURLRef) {
            char path[MAX_PATH];
            CFURLGetFileSystemRepresentation(certURLRef.get(), false, reinterpret_cast<UInt8*>(path), MAX_PATH);
            return path;
        }
    }
#endif

    return CString();
}

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

CurlContext::CurlContext()
    : m_cookieJarFileName(cookieJarPath())
    , m_certificatePath(certificatePath())
{
    curl_global_init(CURL_GLOBAL_ALL);
    m_curlShareHandle = curl_share_init();
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_LOCKFUNC, lock);
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_UNLOCKFUNC, unlock);

    initCookieSession();

    m_ignoreSSLErrors = getenv("WEBKIT_IGNORE_SSL_ERRORS");

#ifndef NDEBUG
    m_verbose = getenv("DEBUG_CURL");

    char* logFile = getenv("CURL_LOG_FILE");
    if (logFile)
        m_logFile = fopen(logFile, "a");
#endif
}

CurlContext::~CurlContext()
{
    curl_share_cleanup(m_curlShareHandle);
    curl_global_cleanup();

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

    curl_easy_setopt(curl, CURLOPT_SHARE, curlShareHandle());

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

// Curl Utilities =======================

URL CurlContext::getEffectiveURL(CURL* handle)
{
    const char* url;
    CURLcode err = curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &url);
    if (CURLE_OK != err)
        return URL();
    return URL(URL(), url);
}

CURLM* CurlContext::createMultiHandle()
{
    return curl_multi_init();
}

// Shared Resource management =======================

Lock* CurlContext::mutexFor(curl_lock_data data)
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Lock, cookieMutex, ());
    DEPRECATED_DEFINE_STATIC_LOCAL(Lock, dnsMutex, ());
    DEPRECATED_DEFINE_STATIC_LOCAL(Lock, shareMutex, ());

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

// libcurl does not implement its own thread synchronization primitives.
// these two functions provide mutexes for cookies, and for the global DNS
// cache.
void CurlContext::lock(CURL* /* handle */, curl_lock_data data, curl_lock_access /* access */, void* /* userPtr */)
{
    if (Lock* mutex = mutexFor(data))
        mutex->lock();
}

void CurlContext::unlock(CURL* /* handle */, curl_lock_data data, void* /* userPtr */)
{
    if (Lock* mutex = mutexFor(data))
        mutex->unlock();
}

}

#endif
