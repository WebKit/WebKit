/*
 * Copyright (C) 2004, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Appcelerator Inc.
 * Copyright (C) 2009 Brent Fulgham <bfulgham@webkit.org>
 * Copyright (C) 2013 Peter Gal <galpeter@inf.u-szeged.hu>, University of Szeged
 * Copyright (C) 2013 Alex Christensen <achristensen@webkit.org>
 * Copyright (C) 2013 University of Szeged
 * All rights reserved.
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
#include "ResourceHandleManager.h"

#if USE(CURL)

#include "CredentialStorage.h"
#include "CurlCacheManager.h"
#include "DataURL.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "MultipartHandle.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"
#include "SSLHandle.h"

#if OS(WINDOWS)
#include "WebCoreBundleWin.h"
#include <shlobj.h>
#include <shlwapi.h>
#else
#include <sys/param.h>
#define MAX_PATH MAXPATHLEN
#endif

#include <errno.h>
#include <stdio.h>
#if ENABLE(WEB_TIMING)
#include <wtf/CurrentTime.h>
#endif
#if USE(CF)
#include <wtf/RetainPtr.h>
#endif
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>


namespace WebCore {

const int selectTimeoutMS = 5;
const double pollTimeSeconds = 0.05;
const int maxRunningJobs = 5;

static const bool ignoreSSLErrors = getenv("WEBKIT_IGNORE_SSL_ERRORS");

static CString certificatePath()
{
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
    char* envPath = getenv("CURL_CA_BUNDLE_PATH");
    if (envPath)
       return envPath;

    return CString();
}

static char* cookieJarPath()
{
    char* cookieJarPath = getenv("CURL_COOKIE_JAR_PATH");
    if (cookieJarPath)
        return fastStrDup(cookieJarPath);

#if OS(WINDOWS)
    char executablePath[MAX_PATH];
    char appDataDirectory[MAX_PATH];
    char cookieJarFullPath[MAX_PATH];
    char cookieJarDirectory[MAX_PATH];

    if (FAILED(::SHGetFolderPathA(0, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, 0, 0, appDataDirectory))
        || FAILED(::GetModuleFileNameA(0, executablePath, MAX_PATH)))
        return fastStrDup("cookies.dat");

    ::PathRemoveExtensionA(executablePath);
    LPSTR executableName = ::PathFindFileNameA(executablePath);
    sprintf_s(cookieJarDirectory, MAX_PATH, "%s/%s", appDataDirectory, executableName);
    sprintf_s(cookieJarFullPath, MAX_PATH, "%s/cookies.dat", cookieJarDirectory);

    if (::SHCreateDirectoryExA(0, cookieJarDirectory, 0) != ERROR_SUCCESS
        && ::GetLastError() != ERROR_FILE_EXISTS
        && ::GetLastError() != ERROR_ALREADY_EXISTS)
        return fastStrDup("cookies.dat");

    return fastStrDup(cookieJarFullPath);
#else
    return fastStrDup("cookies.dat");
#endif
}

static Mutex* sharedResourceMutex(curl_lock_data data) {
    DEPRECATED_DEFINE_STATIC_LOCAL(Mutex, cookieMutex, ());
    DEPRECATED_DEFINE_STATIC_LOCAL(Mutex, dnsMutex, ());
    DEPRECATED_DEFINE_STATIC_LOCAL(Mutex, shareMutex, ());

    switch (data) {
        case CURL_LOCK_DATA_COOKIE:
            return &cookieMutex;
        case CURL_LOCK_DATA_DNS:
            return &dnsMutex;
        case CURL_LOCK_DATA_SHARE:
            return &shareMutex;
        default:
            ASSERT_NOT_REACHED();
            return NULL;
    }
}

#if ENABLE(WEB_TIMING)
static int milisecondsSinceRequest(double requestTime)
{
    return static_cast<int>((monotonicallyIncreasingTime() - requestTime) * 1000.0);
}

static void calculateWebTimingInformations(ResourceHandleInternal* d)
{
    double startTransfertTime = 0;
    double preTransferTime = 0;
    double dnslookupTime = 0;
    double connectTime = 0;
    double appConnectTime = 0;

    curl_easy_getinfo(d->m_handle, CURLINFO_NAMELOOKUP_TIME, &dnslookupTime);
    curl_easy_getinfo(d->m_handle, CURLINFO_CONNECT_TIME, &connectTime);
    curl_easy_getinfo(d->m_handle, CURLINFO_APPCONNECT_TIME, &appConnectTime);
    curl_easy_getinfo(d->m_handle, CURLINFO_STARTTRANSFER_TIME, &startTransfertTime);
    curl_easy_getinfo(d->m_handle, CURLINFO_PRETRANSFER_TIME, &preTransferTime);

    d->m_response.resourceLoadTiming()->dnsStart = 0;
    d->m_response.resourceLoadTiming()->dnsEnd = static_cast<int>(dnslookupTime * 1000);

    d->m_response.resourceLoadTiming()->connectStart = static_cast<int>(dnslookupTime * 1000);
    d->m_response.resourceLoadTiming()->connectEnd = static_cast<int>(connectTime * 1000);

    d->m_response.resourceLoadTiming()->sendStart = static_cast<int>(connectTime *1000);
    d->m_response.resourceLoadTiming()->sendEnd =static_cast<int>(preTransferTime * 1000);

    if (appConnectTime) {
        d->m_response.resourceLoadTiming()->sslStart = static_cast<int>(connectTime * 1000);
        d->m_response.resourceLoadTiming()->sslEnd = static_cast<int>(appConnectTime *1000);
    }
}

static int sockoptfunction(void* data, curl_socket_t /*curlfd*/, curlsocktype /*purpose*/)
{
    ResourceHandle* job = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = job->getInternal();

    if (d->m_response.resourceLoadTiming())
        d->m_response.resourceLoadTiming()->requestTime = monotonicallyIncreasingTime();

    return 0;
}
#endif

// libcurl does not implement its own thread synchronization primitives.
// these two functions provide mutexes for cookies, and for the global DNS
// cache.
static void curl_lock_callback(CURL* /* handle */, curl_lock_data data, curl_lock_access /* access */, void* /* userPtr */)
{
    if (Mutex* mutex = sharedResourceMutex(data))
        mutex->lock();
}

static void curl_unlock_callback(CURL* /* handle */, curl_lock_data data, void* /* userPtr */)
{
    if (Mutex* mutex = sharedResourceMutex(data))
        mutex->unlock();
}

inline static bool isHttpInfo(int statusCode)
{
    return 100 <= statusCode && statusCode < 200;
}

inline static bool isHttpRedirect(int statusCode)
{
    return 300 <= statusCode && statusCode < 400 && statusCode != 304;
}

inline static bool isHttpAuthentication(int statusCode)
{
    return statusCode == 401;
}

ResourceHandleManager::ResourceHandleManager()
    : m_downloadTimer(this, &ResourceHandleManager::downloadTimerCallback)
    , m_cookieJarFileName(cookieJarPath())
    , m_certificatePath (certificatePath())
    , m_runningJobs(0)
{
    curl_global_init(CURL_GLOBAL_ALL);
    m_curlMultiHandle = curl_multi_init();
    m_curlShareHandle = curl_share_init();
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_LOCKFUNC, curl_lock_callback);
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_UNLOCKFUNC, curl_unlock_callback);

    initCookieSession();
}

ResourceHandleManager::~ResourceHandleManager()
{
    curl_multi_cleanup(m_curlMultiHandle);
    curl_share_cleanup(m_curlShareHandle);
    if (m_cookieJarFileName)
        fastFree(m_cookieJarFileName);
    curl_global_cleanup();
}

CURLSH* ResourceHandleManager::getCurlShareHandle() const
{
    return m_curlShareHandle;
}

void ResourceHandleManager::setCookieJarFileName(const char* cookieJarFileName)
{
    m_cookieJarFileName = fastStrDup(cookieJarFileName);
}

const char* ResourceHandleManager::getCookieJarFileName() const
{
    return m_cookieJarFileName;
}

ResourceHandleManager* ResourceHandleManager::sharedInstance()
{
    static ResourceHandleManager* sharedInstance = 0;
    if (!sharedInstance)
        sharedInstance = new ResourceHandleManager();
    return sharedInstance;
}

static void handleLocalReceiveResponse (CURL* handle, ResourceHandle* job, ResourceHandleInternal* d)
{
    // since the code in headerCallback will not have run for local files
    // the code to set the URL and fire didReceiveResponse is never run,
    // which means the ResourceLoader's response does not contain the URL.
    // Run the code here for local files to resolve the issue.
    // TODO: See if there is a better approach for handling this.
     const char* hdr;
     CURLcode err = curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &hdr);
     ASSERT_UNUSED(err, CURLE_OK == err);
     d->m_response.setURL(URL(ParsedURLString, hdr));
     if (d->client())
         d->client()->didReceiveResponse(job, d->m_response);
     d->m_response.setResponseFired(true);
}


// called with data after all headers have been processed via headerCallback
static size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* data)
{
    ResourceHandle* job = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = job->getInternal();
    if (d->m_cancelled)
        return 0;

    // We should never be called when deferred loading is activated.
    ASSERT(!d->m_defersLoading);

    size_t totalSize = size * nmemb;

    // this shouldn't be necessary but apparently is. CURL writes the data
    // of html page even if it is a redirect that was handled internally
    // can be observed e.g. on gmail.com
    CURL* h = d->m_handle;
    long httpCode = 0;
    CURLcode err = curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &httpCode);
    if (CURLE_OK == err && httpCode >= 300 && httpCode < 400)
        return totalSize;

    if (!d->m_response.responseFired()) {
        handleLocalReceiveResponse(h, job, d);
        if (d->m_cancelled)
            return 0;
    }

    if (d->m_multipartHandle)
        d->m_multipartHandle->contentReceived(static_cast<const char*>(ptr), totalSize);
    else if (d->client()) {
        d->client()->didReceiveData(job, static_cast<char*>(ptr), totalSize, 0);
        CurlCacheManager::getInstance().didReceiveData(*job, static_cast<char*>(ptr), totalSize);
    }

    return totalSize;
}

static bool isAppendableHeader(const String &key)
{
    static const char* appendableHeaders[] = {
        "access-control-allow-headers",
        "access-control-allow-methods",
        "access-control-allow-origin",
        "access-control-expose-headers",
        "allow",
        "cache-control",
        "connection",
        "content-encoding",
        "content-language",
        "if-match",
        "if-none-match",
        "keep-alive",
        "pragma",
        "proxy-authenticate",
        "public",
        "server",
        "set-cookie",
        "te",
        "trailer",
        "transfer-encoding",
        "upgrade",
        "user-agent",
        "vary",
        "via",
        "warning",
        "www-authenticate",
        0
    };

    // Custom headers start with 'X-', and need no further checking.
    if (key.startsWith("x-", /* caseSensitive */ false))
        return true;

    for (unsigned i = 0; appendableHeaders[i]; ++i)
        if (equalIgnoringCase(key, appendableHeaders[i]))
            return true;

    return false;
}

static void removeLeadingAndTrailingQuotes(String& value)
{
    unsigned length = value.length();
    if (value.startsWith('"') && value.endsWith('"') && length > 1)
        value = value.substring(1, length-2);
}

static bool getProtectionSpace(CURL* h, const ResourceResponse& response, ProtectionSpace& protectionSpace)
{
    CURLcode err;

    long port = 0;
    err = curl_easy_getinfo(h, CURLINFO_PRIMARY_PORT, &port);
    if (err != CURLE_OK)
        return false;

    long availableAuth = CURLAUTH_NONE;
    err = curl_easy_getinfo(h, CURLINFO_HTTPAUTH_AVAIL, &availableAuth);
    if (err != CURLE_OK)
        return false;

    const char* effectiveUrl = 0;
    err = curl_easy_getinfo(h, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
    if (err != CURLE_OK)
        return false;

    URL url(ParsedURLString, effectiveUrl);

    String host = url.host();
    String protocol = url.protocol();

    String realm;

    const String authHeader = response.httpHeaderField(HTTPHeaderName::Authorization);
    const String realmString = "realm=";
    int realmPos = authHeader.find(realmString);
    if (realmPos > 0) {
        realm = authHeader.substring(realmPos + realmString.length());
        realm = realm.left(realm.find(','));
        removeLeadingAndTrailingQuotes(realm);
    }

    ProtectionSpaceServerType serverType = ProtectionSpaceServerHTTP;
    if (protocol == "https")
        serverType = ProtectionSpaceServerHTTPS;

    ProtectionSpaceAuthenticationScheme authScheme = ProtectionSpaceAuthenticationSchemeUnknown;

    if (availableAuth & CURLAUTH_BASIC)
        authScheme = ProtectionSpaceAuthenticationSchemeHTTPBasic;
    if (availableAuth & CURLAUTH_DIGEST)
        authScheme = ProtectionSpaceAuthenticationSchemeHTTPDigest;
    if (availableAuth & CURLAUTH_GSSNEGOTIATE)
        authScheme = ProtectionSpaceAuthenticationSchemeNegotiate;
    if (availableAuth & CURLAUTH_NTLM)
        authScheme = ProtectionSpaceAuthenticationSchemeNTLM;

    protectionSpace = ProtectionSpace(host, port, serverType, realm, authScheme);

    return true;
}

/*
 * This is being called for each HTTP header in the response. This includes '\r\n'
 * for the last line of the header.
 *
 * We will add each HTTP Header to the ResourceResponse and on the termination
 * of the header (\r\n) we will parse Content-Type and Content-Disposition and
 * update the ResourceResponse and then send it away.
 *
 */
static size_t headerCallback(char* ptr, size_t size, size_t nmemb, void* data)
{
    ResourceHandle* job = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = job->getInternal();
    if (d->m_cancelled)
        return 0;

    // We should never be called when deferred loading is activated.
    ASSERT(!d->m_defersLoading);

    size_t totalSize = size * nmemb;
    ResourceHandleClient* client = d->client();

    String header = String::fromUTF8WithLatin1Fallback(static_cast<const char*>(ptr), totalSize);

    /*
     * a) We can finish and send the ResourceResponse
     * b) We will add the current header to the HTTPHeaderMap of the ResourceResponse
     *
     * The HTTP standard requires to use \r\n but for compatibility it recommends to
     * accept also \n.
     */
    if (header == String("\r\n") || header == String("\n")) {
        CURL* h = d->m_handle;

        long httpCode = 0;
        curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &httpCode);

        if (isHttpInfo(httpCode)) {
            // Just return when receiving http info, e.g. HTTP/1.1 100 Continue.
            // If not, the request might be cancelled, because the MIME type will be empty for this response.
            return totalSize;
        }

        double contentLength = 0;
        curl_easy_getinfo(h, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
        d->m_response.setExpectedContentLength(static_cast<long long int>(contentLength));

        const char* hdr;
        curl_easy_getinfo(h, CURLINFO_EFFECTIVE_URL, &hdr);
        d->m_response.setURL(URL(ParsedURLString, hdr));

        d->m_response.setHTTPStatusCode(httpCode);
        d->m_response.setMimeType(extractMIMETypeFromMediaType(d->m_response.httpHeaderField(HTTPHeaderName::ContentType)).lower());
        d->m_response.setTextEncodingName(extractCharsetFromMediaType(d->m_response.httpHeaderField(HTTPHeaderName::ContentType)));
        d->m_response.setSuggestedFilename(filenameFromHTTPContentDisposition(d->m_response.httpHeaderField(HTTPHeaderName::ContentDisposition)));

        if (d->m_response.isMultipart()) {
            String boundary;
            bool parsed = MultipartHandle::extractBoundary(d->m_response.httpHeaderField(HTTPHeaderName::ContentType), boundary);
            if (parsed)
                d->m_multipartHandle = MultipartHandle::create(job, boundary);
        }

#if ENABLE(WEB_TIMING)
        if (d->m_response.resourceLoadTiming() && d->m_response.resourceLoadTiming()->requestTime)
            d->m_response.resourceLoadTiming()->receiveHeadersEnd = milisecondsSinceRequest(d->m_response.resourceLoadTiming()->requestTime);
#endif

        // HTTP redirection
        if (isHttpRedirect(httpCode)) {
            String location = d->m_response.httpHeaderField(HTTPHeaderName::Location);
            if (!location.isEmpty()) {
                URL newURL = URL(job->firstRequest().url(), location);

                ResourceRequest redirectedRequest = job->firstRequest();
                redirectedRequest.setURL(newURL);
                if (client)
                    client->willSendRequest(job, redirectedRequest, d->m_response);

                d->m_firstRequest.setURL(newURL);

                return totalSize;
            }
        } else if (isHttpAuthentication(httpCode)) {
            ProtectionSpace protectionSpace;
            if (getProtectionSpace(d->m_handle, d->m_response, protectionSpace)) {
                Credential credential;
                AuthenticationChallenge challenge(protectionSpace, credential, d->m_authFailureCount, d->m_response, ResourceError());
                challenge.setAuthenticationClient(job);
                job->didReceiveAuthenticationChallenge(challenge);
                d->m_authFailureCount++;
                return totalSize;
            }
        }

        if (client) {
            if (httpCode == 304) {
                const String& url = job->firstRequest().url().string();
                CurlCacheManager::getInstance().getCachedResponse(url, d->m_response);
            }
            client->didReceiveResponse(job, d->m_response);
            CurlCacheManager::getInstance().didReceiveResponse(*job, d->m_response);
        }
        d->m_response.setResponseFired(true);

    } else {
        int splitPos = header.find(":");
        if (splitPos != -1) {
            String key = header.left(splitPos).stripWhiteSpace();
            String value = header.substring(splitPos + 1).stripWhiteSpace();

            if (isAppendableHeader(key))
                d->m_response.addHTTPHeaderField(key, value);
            else
                d->m_response.setHTTPHeaderField(key, value);
        } else if (header.startsWith("HTTP", false)) {
            // This is the first line of the response.
            // Extract the http status text from this.
            //
            // If the FOLLOWLOCATION option is enabled for the curl handle then
            // curl will follow the redirections internally. Thus this header callback
            // will be called more than one time with the line starting "HTTP" for one job.
            long httpCode = 0;
            curl_easy_getinfo(d->m_handle, CURLINFO_RESPONSE_CODE, &httpCode);

            String httpCodeString = String::number(httpCode);
            int statusCodePos = header.find(httpCodeString);

            if (statusCodePos != -1) {
                // The status text is after the status code.
                String status = header.substring(statusCodePos + httpCodeString.length());
                d->m_response.setHTTPStatusText(status.stripWhiteSpace());
            }

        }
    }

    return totalSize;
}

/* This is called to obtain HTTP POST or PUT data.
   Iterate through FormData elements and upload files.
   Carefully respect the given buffer size and fill the rest of the data at the next calls.
*/
size_t readCallback(void* ptr, size_t size, size_t nmemb, void* data)
{
    ResourceHandle* job = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = job->getInternal();

    if (d->m_cancelled)
        return 0;

    // We should never be called when deferred loading is activated.
    ASSERT(!d->m_defersLoading);

    if (!size || !nmemb)
        return 0;

    if (!d->m_formDataStream.hasMoreElements())
        return 0;

    size_t sent = d->m_formDataStream.read(ptr, size, nmemb);

    // Something went wrong so cancel the job.
    if (!sent)
        job->cancel();

    return sent;
}

void ResourceHandleManager::downloadTimerCallback(Timer* /* timer */)
{
    startScheduledJobs();

    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
    int maxfd = 0;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = selectTimeoutMS * 1000;       // select waits microseconds

    // Retry 'select' if it was interrupted by a process signal.
    int rc = 0;
    do {
        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);
        curl_multi_fdset(m_curlMultiHandle, &fdread, &fdwrite, &fdexcep, &maxfd);
        // When the 3 file descriptors are empty, winsock will return -1
        // and bail out, stopping the file download. So make sure we
        // have valid file descriptors before calling select.
        if (maxfd >= 0)
            rc = ::select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
    } while (rc == -1 && errno == EINTR);

    if (-1 == rc) {
#ifndef NDEBUG
        perror("bad: select() returned -1: ");
#endif
        return;
    }

    int runningHandles = 0;
    while (curl_multi_perform(m_curlMultiHandle, &runningHandles) == CURLM_CALL_MULTI_PERFORM) { }

    // check the curl messages indicating completed transfers
    // and free their resources
    while (true) {
        int messagesInQueue;
        CURLMsg* msg = curl_multi_info_read(m_curlMultiHandle, &messagesInQueue);
        if (!msg)
            break;

        // find the node which has same d->m_handle as completed transfer
        CURL* handle = msg->easy_handle;
        ASSERT(handle);
        ResourceHandle* job = 0;
        CURLcode err = curl_easy_getinfo(handle, CURLINFO_PRIVATE, &job);
        ASSERT_UNUSED(err, CURLE_OK == err);
        ASSERT(job);
        if (!job)
            continue;
        ResourceHandleInternal* d = job->getInternal();
        ASSERT(d->m_handle == handle);

        if (d->m_cancelled) {
            removeFromCurl(job);
            continue;
        }

        if (CURLMSG_DONE != msg->msg)
            continue;


        if (CURLE_OK == msg->data.result) {
#if ENABLE(WEB_TIMING)
            calculateWebTimingInformations(d);
#endif
            if (!d->m_response.responseFired()) {
                handleLocalReceiveResponse(d->m_handle, job, d);
                if (d->m_cancelled) {
                    removeFromCurl(job);
                    continue;
                }
            }

            if (d->m_multipartHandle)
                d->m_multipartHandle->contentEnded();

            if (d->client()) {
                d->client()->didFinishLoading(job, 0);
                CurlCacheManager::getInstance().didFinishLoading(*job);
            }
        } else {
            char* url = 0;
            curl_easy_getinfo(d->m_handle, CURLINFO_EFFECTIVE_URL, &url);
#ifndef NDEBUG
            fprintf(stderr, "Curl ERROR for url='%s', error: '%s'\n", url, curl_easy_strerror(msg->data.result));
#endif
            if (d->client()) {
                ResourceError resourceError(String(), msg->data.result, String(url), String(curl_easy_strerror(msg->data.result)));
                resourceError.setSSLErrors(d->m_sslErrors);
                d->client()->didFail(job, resourceError);
                CurlCacheManager::getInstance().didFail(*job);
            }
        }

        removeFromCurl(job);
    }

    bool started = startScheduledJobs(); // new jobs might have been added in the meantime

    if (!m_downloadTimer.isActive() && (started || (runningHandles > 0)))
        m_downloadTimer.startOneShot(pollTimeSeconds);
}

void ResourceHandleManager::setProxyInfo(const String& host,
                                         unsigned long port,
                                         ProxyType type,
                                         const String& username,
                                         const String& password)
{
    m_proxyType = type;

    if (!host.length()) {
        m_proxy = emptyString();
    } else {
        String userPass;
        if (username.length() || password.length())
            userPass = username + ":" + password + "@";

        m_proxy = String("http://") + userPass + host + ":" + String::number(port);
    }
}

void ResourceHandleManager::removeFromCurl(ResourceHandle* job)
{
    ResourceHandleInternal* d = job->getInternal();
    ASSERT(d->m_handle);
    if (!d->m_handle)
        return;
    m_runningJobs--;
    curl_multi_remove_handle(m_curlMultiHandle, d->m_handle);
    curl_easy_cleanup(d->m_handle);
    d->m_handle = 0;
    job->deref();
}

static inline size_t getFormElementsCount(ResourceHandle* job)
{
    RefPtr<FormData> formData = job->firstRequest().httpBody();

    if (!formData)
        return 0;

    // Resolve the blob elements so the formData can correctly report it's size.
    formData = formData->resolveBlobReferences();
    job->firstRequest().setHTTPBody(formData);

    return formData->elements().size();
}

static void setupFormData(ResourceHandle* job, CURLoption sizeOption, struct curl_slist** headers)
{
    ResourceHandleInternal* d = job->getInternal();
    Vector<FormDataElement> elements = job->firstRequest().httpBody()->elements();
    size_t numElements = elements.size();

    // The size of a curl_off_t could be different in WebKit and in cURL depending on
    // compilation flags of both.
    static int expectedSizeOfCurlOffT = 0;
    if (!expectedSizeOfCurlOffT) {
        curl_version_info_data *infoData = curl_version_info(CURLVERSION_NOW);
        if (infoData->features & CURL_VERSION_LARGEFILE)
            expectedSizeOfCurlOffT = sizeof(long long);
        else
            expectedSizeOfCurlOffT = sizeof(int);
    }

    static const long long maxCurlOffT = (1LL << (expectedSizeOfCurlOffT * 8 - 1)) - 1;
    // Obtain the total size of the form data
    curl_off_t size = 0;
    bool chunkedTransfer = false;
    for (size_t i = 0; i < numElements; i++) {
        FormDataElement element = elements[i];
        if (element.m_type == FormDataElement::Type::EncodedFile) {
            long long fileSizeResult;
            if (getFileSize(element.m_filename, fileSizeResult)) {
                if (fileSizeResult > maxCurlOffT) {
                    // File size is too big for specifying it to cURL
                    chunkedTransfer = true;
                    break;
                }
                size += fileSizeResult;
            } else {
                chunkedTransfer = true;
                break;
            }
        } else
            size += elements[i].m_data.size();
    }

    // cURL guesses that we want chunked encoding as long as we specify the header
    if (chunkedTransfer)
        *headers = curl_slist_append(*headers, "Transfer-Encoding: chunked");
    else {
        if (sizeof(long long) == expectedSizeOfCurlOffT)
            curl_easy_setopt(d->m_handle, sizeOption, (long long)size);
        else
            curl_easy_setopt(d->m_handle, sizeOption, (int)size);
    }

    curl_easy_setopt(d->m_handle, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(d->m_handle, CURLOPT_READDATA, job);
}

void ResourceHandleManager::setupPUT(ResourceHandle* job, struct curl_slist** headers)
{
    ResourceHandleInternal* d = job->getInternal();
    curl_easy_setopt(d->m_handle, CURLOPT_UPLOAD, TRUE);
    curl_easy_setopt(d->m_handle, CURLOPT_INFILESIZE, 0);

    // Disable the Expect: 100 continue header
    *headers = curl_slist_append(*headers, "Expect:");

    size_t numElements = getFormElementsCount(job);
    if (!numElements)
        return;

    setupFormData(job, CURLOPT_INFILESIZE_LARGE, headers);
}

void ResourceHandleManager::setupPOST(ResourceHandle* job, struct curl_slist** headers)
{
    ResourceHandleInternal* d = job->getInternal();
    curl_easy_setopt(d->m_handle, CURLOPT_POST, TRUE);
    curl_easy_setopt(d->m_handle, CURLOPT_POSTFIELDSIZE, 0);

    size_t numElements = getFormElementsCount(job);
    if (!numElements)
        return;

    // Do not stream for simple POST data
    if (numElements == 1) {
        job->firstRequest().httpBody()->flatten(d->m_postBytes);
        if (d->m_postBytes.size()) {
            curl_easy_setopt(d->m_handle, CURLOPT_POSTFIELDSIZE, d->m_postBytes.size());
            curl_easy_setopt(d->m_handle, CURLOPT_POSTFIELDS, d->m_postBytes.data());
        }
        return;
    }

    setupFormData(job, CURLOPT_POSTFIELDSIZE_LARGE, headers);
}

void ResourceHandleManager::add(ResourceHandle* job)
{
    // we can be called from within curl, so to avoid re-entrancy issues
    // schedule this job to be added the next time we enter curl download loop
    job->ref();
    m_resourceHandleList.append(job);
    if (!m_downloadTimer.isActive())
        m_downloadTimer.startOneShot(pollTimeSeconds);
}

bool ResourceHandleManager::removeScheduledJob(ResourceHandle* job)
{
    int size = m_resourceHandleList.size();
    for (int i = 0; i < size; i++) {
        if (job == m_resourceHandleList[i]) {
            m_resourceHandleList.remove(i);
            job->deref();
            return true;
        }
    }
    return false;
}

bool ResourceHandleManager::startScheduledJobs()
{
    // TODO: Create a separate stack of jobs for each domain.

    bool started = false;
    while (!m_resourceHandleList.isEmpty() && m_runningJobs < maxRunningJobs) {
        ResourceHandle* job = m_resourceHandleList[0];
        m_resourceHandleList.remove(0);
        startJob(job);
        started = true;
    }
    return started;
}

void ResourceHandleManager::dispatchSynchronousJob(ResourceHandle* job)
{
    URL kurl = job->firstRequest().url();

    if (kurl.protocolIsData()) {
        handleDataURL(job);
        return;
    }

    ResourceHandleInternal* handle = job->getInternal();

    // If defersLoading is true and we call curl_easy_perform
    // on a paused handle, libcURL would do the transfert anyway
    // and we would assert so force defersLoading to be false.
    handle->m_defersLoading = false;

    initializeHandle(job);

    // curl_easy_perform blocks until the transfert is finished.
    CURLcode ret =  curl_easy_perform(handle->m_handle);

    if (ret != CURLE_OK) {
        ResourceError error(String(handle->m_url), ret, String(handle->m_url), String(curl_easy_strerror(ret)));
        error.setSSLErrors(handle->m_sslErrors);
        handle->client()->didFail(job, error);
    } else {
        if (handle->client())
            handle->client()->didReceiveResponse(job, handle->m_response);
    }

#if ENABLE(WEB_TIMING)
    calculateWebTimingInformations(handle);
#endif

    curl_easy_cleanup(handle->m_handle);
}

void ResourceHandleManager::startJob(ResourceHandle* job)
{
    URL kurl = job->firstRequest().url();

    if (kurl.protocolIsData()) {
        handleDataURL(job);
        return;
    }

    initializeHandle(job);

    m_runningJobs++;
    CURLMcode ret = curl_multi_add_handle(m_curlMultiHandle, job->getInternal()->m_handle);
    // don't call perform, because events must be async
    // timeout will occur and do curl_multi_perform
    if (ret && ret != CURLM_CALL_MULTI_PERFORM) {
#ifndef NDEBUG
        fprintf(stderr, "Error %d starting job %s\n", ret, encodeWithURLEscapeSequences(job->firstRequest().url().string()).latin1().data());
#endif
        job->cancel();
        return;
    }
}

void ResourceHandleManager::applyAuthenticationToRequest(ResourceHandle* handle, ResourceRequest& request)
{
    // m_user/m_pass are credentials given manually, for instance, by the arguments passed to XMLHttpRequest.open().
    ResourceHandleInternal* d = handle->getInternal();

    if (handle->shouldUseCredentialStorage()) {
        if (d->m_user.isEmpty() && d->m_pass.isEmpty()) {
            // <rdar://problem/7174050> - For URLs that match the paths of those previously challenged for HTTP Basic authentication, 
            // try and reuse the credential preemptively, as allowed by RFC 2617.
            d->m_initialCredential = CredentialStorage::get(request.url());
        } else {
            // If there is already a protection space known for the URL, update stored credentials
            // before sending a request. This makes it possible to implement logout by sending an
            // XMLHttpRequest with known incorrect credentials, and aborting it immediately (so that
            // an authentication dialog doesn't pop up).
            CredentialStorage::set(Credential(d->m_user, d->m_pass, CredentialPersistenceNone), request.url());
        }
    }

    String user = d->m_user;
    String password = d->m_pass;

    if (!d->m_initialCredential.isEmpty()) {
        user = d->m_initialCredential.user();
        password = d->m_initialCredential.password();
        curl_easy_setopt(d->m_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    }

    // It seems we need to set CURLOPT_USERPWD even if username and password is empty.
    // Otherwise cURL will not automatically continue with a new request after a 401 response.

    // curl CURLOPT_USERPWD expects username:password
    String userpass = user + ":" + password;
    curl_easy_setopt(d->m_handle, CURLOPT_USERPWD, userpass.utf8().data());
}

void ResourceHandleManager::initializeHandle(ResourceHandle* job)
{
    static const int allowedProtocols = CURLPROTO_FILE | CURLPROTO_FTP | CURLPROTO_FTPS | CURLPROTO_HTTP | CURLPROTO_HTTPS;
    URL url = job->firstRequest().url();

    // Remove any fragment part, otherwise curl will send it as part of the request.
    url.removeFragmentIdentifier();

    ResourceHandleInternal* d = job->getInternal();
    String urlString = url.string();

    if (url.isLocalFile()) {
        // Remove any query part sent to a local file.
        if (!url.query().isEmpty()) {
            // By setting the query to a null string it'll be removed.
            url.setQuery(String());
            urlString = url.string();
        }
        // Determine the MIME type based on the path.
        d->m_response.setMimeType(MIMETypeRegistry::getMIMETypeForPath(url));
    }

    d->m_handle = curl_easy_init();

    if (d->m_defersLoading) {
        CURLcode error = curl_easy_pause(d->m_handle, CURLPAUSE_ALL);
        // If we did not pause the handle, we would ASSERT in the
        // header callback. So just assert here.
        ASSERT_UNUSED(error, error == CURLE_OK);
    }
#ifndef NDEBUG
    if (getenv("DEBUG_CURL"))
        curl_easy_setopt(d->m_handle, CURLOPT_VERBOSE, 1);
#endif
    curl_easy_setopt(d->m_handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(d->m_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(d->m_handle, CURLOPT_PRIVATE, job);
    curl_easy_setopt(d->m_handle, CURLOPT_ERRORBUFFER, m_curlErrorBuffer);
    curl_easy_setopt(d->m_handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(d->m_handle, CURLOPT_WRITEDATA, job);
    curl_easy_setopt(d->m_handle, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(d->m_handle, CURLOPT_WRITEHEADER, job);
    curl_easy_setopt(d->m_handle, CURLOPT_AUTOREFERER, 1);
    curl_easy_setopt(d->m_handle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(d->m_handle, CURLOPT_MAXREDIRS, 10);
    curl_easy_setopt(d->m_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
    curl_easy_setopt(d->m_handle, CURLOPT_SHARE, m_curlShareHandle);
    curl_easy_setopt(d->m_handle, CURLOPT_DNS_CACHE_TIMEOUT, 60 * 5); // 5 minutes
    curl_easy_setopt(d->m_handle, CURLOPT_PROTOCOLS, allowedProtocols);
    curl_easy_setopt(d->m_handle, CURLOPT_REDIR_PROTOCOLS, allowedProtocols);
    setSSLClientCertificate(job);

    if (ignoreSSLErrors)
        curl_easy_setopt(d->m_handle, CURLOPT_SSL_VERIFYPEER, false);
    else
        setSSLVerifyOptions(job);

    if (!m_certificatePath.isNull())
       curl_easy_setopt(d->m_handle, CURLOPT_CAINFO, m_certificatePath.data());

    // enable gzip and deflate through Accept-Encoding:
    curl_easy_setopt(d->m_handle, CURLOPT_ENCODING, "");

    // url must remain valid through the request
    ASSERT(!d->m_url);

    // url is in ASCII so latin1() will only convert it to char* without character translation.
    d->m_url = fastStrDup(urlString.latin1().data());
    curl_easy_setopt(d->m_handle, CURLOPT_URL, d->m_url);

    if (m_cookieJarFileName)
        curl_easy_setopt(d->m_handle, CURLOPT_COOKIEJAR, m_cookieJarFileName);

    struct curl_slist* headers = 0;
    if (job->firstRequest().httpHeaderFields().size() > 0) {
        HTTPHeaderMap customHeaders = job->firstRequest().httpHeaderFields();

        if (CurlCacheManager::getInstance().isCached(url)) {
            HTTPHeaderMap& requestHeaders = CurlCacheManager::getInstance().requestHeaders(url);

            // append additional cache information
            HTTPHeaderMap::const_iterator it = requestHeaders.begin();
            HTTPHeaderMap::const_iterator end = requestHeaders.end();
            while (it != end) {
                customHeaders.set(it->key, it->value);
                ++it;
            }
        }

        HTTPHeaderMap::const_iterator end = customHeaders.end();
        for (HTTPHeaderMap::const_iterator it = customHeaders.begin(); it != end; ++it) {
            String key = it->key;
            String value = it->value;
            String headerString(key);
            if (value.isEmpty())
                // Insert the ; to tell curl that this header has an empty value.
                headerString.append(";");
            else {
                headerString.append(": ");
                headerString.append(value);
            }
            CString headerLatin1 = headerString.latin1();
            headers = curl_slist_append(headers, headerLatin1.data());
        }
    }

    String method = job->firstRequest().httpMethod();
    if ("GET" == method)
        curl_easy_setopt(d->m_handle, CURLOPT_HTTPGET, TRUE);
    else if ("POST" == method)
        setupPOST(job, &headers);
    else if ("PUT" == method)
        setupPUT(job, &headers);
    else if ("HEAD" == method)
        curl_easy_setopt(d->m_handle, CURLOPT_NOBODY, TRUE);
    else {
        curl_easy_setopt(d->m_handle, CURLOPT_CUSTOMREQUEST, method.latin1().data());
        setupPUT(job, &headers);
    }

    if (headers) {
        curl_easy_setopt(d->m_handle, CURLOPT_HTTPHEADER, headers);
        d->m_customHeaders = headers;
    }

    applyAuthenticationToRequest(job, job->firstRequest());

    // Set proxy options if we have them.
    if (m_proxy.length()) {
        curl_easy_setopt(d->m_handle, CURLOPT_PROXY, m_proxy.utf8().data());
        curl_easy_setopt(d->m_handle, CURLOPT_PROXYTYPE, m_proxyType);
    }
#if ENABLE(WEB_TIMING)
    curl_easy_setopt(d->m_handle, CURLOPT_SOCKOPTFUNCTION, sockoptfunction);
    curl_easy_setopt(d->m_handle, CURLOPT_SOCKOPTDATA, job);
    d->m_response.setResourceLoadTiming(ResourceLoadTiming::create());
#endif
}

void ResourceHandleManager::initCookieSession()
{
    // Curl saves both persistent cookies, and session cookies to the cookie file.
    // The session cookies should be deleted before starting a new session.

    CURL* curl = curl_easy_init();

    if (!curl)
        return;

    curl_easy_setopt(curl, CURLOPT_SHARE, m_curlShareHandle);

    if (m_cookieJarFileName) {
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, m_cookieJarFileName);
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, m_cookieJarFileName);
    }

    curl_easy_setopt(curl, CURLOPT_COOKIESESSION, 1);

    curl_easy_cleanup(curl);
}

void ResourceHandleManager::cancel(ResourceHandle* job)
{
    if (removeScheduledJob(job))
        return;

    ResourceHandleInternal* d = job->getInternal();
    d->m_cancelled = true;
    if (!m_downloadTimer.isActive())
        m_downloadTimer.startOneShot(pollTimeSeconds);
}

} // namespace WebCore

#endif
