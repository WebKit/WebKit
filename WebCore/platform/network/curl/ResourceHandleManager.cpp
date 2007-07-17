/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
 * Copyright (C) 2007 Holger Hans Peter Freyther
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "CString.h"
#include "NotImplemented.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"
#include "HTTPParsers.h"

#include <wtf/Vector.h>

namespace WebCore {

const int selectTimeoutMS = 5;
const double pollTimeSeconds = 0.05;

ResourceHandleManager::ResourceHandleManager()
    : m_downloadTimer(this, &ResourceHandleManager::downloadTimerCallback)
    , m_cookieJarFileName(0)
    , m_resourceHandleListHead(0)
{
    curl_global_init(CURL_GLOBAL_ALL);
    m_curlMultiHandle = curl_multi_init();
    m_curlShareHandle = curl_share_init();
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
}

ResourceHandleManager::~ResourceHandleManager()
{
    curl_multi_cleanup(m_curlMultiHandle);
    curl_share_cleanup(m_curlShareHandle);
    if (m_cookieJarFileName)
        free(m_cookieJarFileName);
}

void ResourceHandleManager::setCookieJarFileName(const char* cookieJarFileName)
{ 
    m_cookieJarFileName = strdup(cookieJarFileName);
}

ResourceHandleManager* ResourceHandleManager::sharedInstance()
{
    static ResourceHandleManager* sharedInstance = 0;
    if (!sharedInstance)
        sharedInstance = new ResourceHandleManager();
    return sharedInstance;
}

// called with data after all headers have been processed via headerCallback
static size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* data)
{
    ResourceHandle* job = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = job->getInternal();
    int totalSize = size * nmemb;

    // this shouldn't be necessary but apparently is. CURL writes the data
    // of html page even if it is a redirect that was handled internally
    // can be observed e.g. on gmail.com
    CURL* h = d->m_handle;
    long httpCode = 0;
    CURLcode err = curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &httpCode);
    if (CURLE_OK == err && httpCode >= 300 && httpCode < 400)
        return totalSize;

    if (d->client())
        d->client()->didReceiveData(job, static_cast<char*>(ptr), totalSize, 0);
    return totalSize;
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

    unsigned int totalSize = size * nmemb;
    ResourceHandleClient* client = d->client();
    if (!client) {
        return totalSize;
    }

    
    String header(static_cast<const char*>(ptr), totalSize);

    /*
     * a) We can finish and send the ResourceResponse
     * b) We will add the current header to the HTTPHeaderMap of the ResourceResponse 
     */
    if (header == String("\r\n")) {
        CURL* h = d->m_handle;
        CURLcode err;

        double contentLength = 0;
        err = curl_easy_getinfo(h, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
        d->m_response.setExpectedContentLength(static_cast<long long int>(contentLength)); 

        const char* hdr;
        err = curl_easy_getinfo(h, CURLINFO_EFFECTIVE_URL, &hdr);
        d->m_response.setUrl(KURL(hdr));

        long httpCode = 0;
        err = curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &httpCode);
        d->m_response.setHTTPStatusCode(httpCode);
    
        d->m_response.setMimeType(extractMIMETypeFromMediaType(d->m_response.httpHeaderField("Content-Type")));
        d->m_response.setTextEncodingName(extractCharsetFromMediaType(d->m_response.httpHeaderField("Content-Type")));
        d->m_response.setSuggestedFilename(filenameFromHTTPContentDisposition(d->m_response.httpHeaderField("Content-Disposition")));

        client->didReceiveResponse(job, d->m_response);
    } else {
        int splitPos = header.find(":");
        if (splitPos != -1)
            d->m_response.setHTTPHeaderField(header.left(splitPos), header.substring(splitPos+1).stripWhiteSpace());
    }

    return totalSize;
}

void ResourceHandleManager::downloadTimerCallback(Timer<ResourceHandleManager>* timer)
{
    startScheduledJobs();

    fd_set fdread;
    FD_ZERO(&fdread);
    fd_set fdwrite;
    FD_ZERO(&fdwrite);
    fd_set fdexcep;
    FD_ZERO(&fdexcep);
    int maxfd = 0;
    curl_multi_fdset(m_curlMultiHandle, &fdread, &fdwrite, &fdexcep, &maxfd);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = selectTimeoutMS * 1000;       // select waits microseconds

    int rc = ::select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);

    if (-1 == rc) {
#ifndef NDEBUG
        printf("bad: select() returned -1\n");
#endif
        return;
    }

    int runningHandles = 0;
    CURLMcode curlCode = CURLM_CALL_MULTI_PERFORM;
    while (CURLM_CALL_MULTI_PERFORM == curlCode) {
        curlCode = curl_multi_perform(m_curlMultiHandle, &runningHandles);
    }

    // check the curl messages indicating completed transfers
    // and free their resources
    while (true) {
        int messagesInQueue;
        CURLMsg* msg = curl_multi_info_read(m_curlMultiHandle, &messagesInQueue);
        if (!msg)
            break;

        if (CURLMSG_DONE != msg->msg)
            continue;

        // find the node which has same d->m_handle as completed transfer
        CURL* handle = msg->easy_handle;
        ASSERT(handle);
        ResourceHandle* job = 0;
        CURLcode err = curl_easy_getinfo(handle, CURLINFO_PRIVATE, &job);
        ASSERT(CURLE_OK == err);
        ASSERT(job);
        if (!job)
            continue;
        ResourceHandleInternal* d = job->getInternal();
        ASSERT(d->m_handle == handle);
        if (CURLE_OK == msg->data.result) {
            if (d->client())
                d->client()->didFinishLoading(job);
        } else {
#ifndef NDEBUG
            char* url = 0;
            curl_easy_getinfo(d->m_handle, CURLINFO_EFFECTIVE_URL, &url);
            printf("Curl ERROR for url='%s', error: '%s'\n", url, curl_easy_strerror(msg->data.result));
#endif
            if (d->client())
                d->client()->didFail(job, ResourceError());
        }

        removeFromCurl(job);
    }

    bool started = startScheduledJobs(); // new jobs might have been added in the meantime

    if (!m_downloadTimer.isActive() && (started || (runningHandles > 0)))
        m_downloadTimer.startOneShot(pollTimeSeconds);
}

void ResourceHandleManager::removeFromCurl(ResourceHandle* job)
{
    ResourceHandleInternal* d = job->getInternal();
    ASSERT(d->m_handle);
    if (!d->m_handle)
        return;
    curl_multi_remove_handle(m_curlMultiHandle, d->m_handle);
    curl_easy_cleanup(d->m_handle);
    d->m_handle = 0;
}

void ResourceHandleManager::setupPUT(ResourceHandle*)
{
    notImplemented();
}

void ResourceHandleManager::setupPOST(ResourceHandle* job)
{
    ResourceHandleInternal* d = job->getInternal();

    curl_easy_setopt(d->m_handle, CURLOPT_POST, TRUE);

    job->request().httpBody()->flatten(d->m_postBytes);
    if (d->m_postBytes.size() != 0) {
        curl_easy_setopt(d->m_handle, CURLOPT_POSTFIELDSIZE, d->m_postBytes.size());
        curl_easy_setopt(d->m_handle, CURLOPT_POSTFIELDS, d->m_postBytes.data());
    }

    Vector<FormDataElement> elements = job->request().httpBody()->elements();
    size_t size = elements.size();
    struct curl_httppost* lastItem = 0;
    struct curl_httppost* post = 0;
    for (size_t i = 0; i < size; i++) {
        if (elements[i].m_type != FormDataElement::encodedFile)
            continue;
        CString cstring = elements[i].m_filename.utf8();
        ASSERT(!d->m_fileName);
        d->m_fileName = strdup(cstring.data());

        // Fill in the file upload field
        curl_formadd(&post, &lastItem, CURLFORM_COPYNAME, "sendfile", CURLFORM_FILE, d->m_fileName, CURLFORM_END);

        // Fill in the filename field
        curl_formadd(&post, &lastItem, CURLFORM_COPYNAME, "filename", CURLFORM_COPYCONTENTS, d->m_fileName, CURLFORM_END);

        // FIXME: We should not add a "submit" field for each file uploaded. Review this code.
        // Fill in the submit field too, even if this is rarely needed
        curl_formadd(&post, &lastItem, CURLFORM_COPYNAME, "submit", CURLFORM_COPYCONTENTS, "send", CURLFORM_END);

        // FIXME: should we support more than one file?
        break;
    }

    if (post)
        curl_easy_setopt(d->m_handle, CURLOPT_HTTPPOST, post);
}

void ResourceHandleManager::add(ResourceHandle* job)
{
    // we can be called from within curl, so to avoid re-entrancy issues
    // schedule this job to be added the next time we enter curl download loop
    m_resourceHandleListHead = new ResourceHandleList(job, m_resourceHandleListHead);
    if (!m_downloadTimer.isActive())
        m_downloadTimer.startOneShot(pollTimeSeconds);
}

bool ResourceHandleManager::removeScheduledJob(ResourceHandle* job)
{
    ResourceHandleList* node = m_resourceHandleListHead;
    while (node) {
        ResourceHandleList* next = node->next();
        if (job == node->job()) {
            node->setRemoved(true);
            return true;
        }
        node = next;
    }
    return false;
}

bool ResourceHandleManager::startScheduledJobs()
{
    bool started = false;
    ResourceHandleList* node = m_resourceHandleListHead;
    while (node) {
        ResourceHandleList* next = node->next();
        if (!node->removed()) {
            startJob(node->job());
            started = true;
        }
        delete node;
        node = next;
    }
    m_resourceHandleListHead = 0;
    return started;
}

void ResourceHandleManager::startJob(ResourceHandle* job)
{
    ResourceHandleInternal* d = job->getInternal();
    DeprecatedString url = job->request().url().url();
    d->m_handle = curl_easy_init();
#ifndef NDEBUG
    if (getenv("DEBUG_CURL"))
        curl_easy_setopt(d->m_handle, CURLOPT_VERBOSE, 1);
#endif
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
    // enable gzip and deflate through Accept-Encoding:
    curl_easy_setopt(d->m_handle, CURLOPT_ENCODING, "");

    // url must remain valid through the request
    ASSERT(!d->m_url);
    d->m_url = strdup(url.ascii());
    curl_easy_setopt(d->m_handle, CURLOPT_URL, d->m_url);

    if (m_cookieJarFileName) {
        curl_easy_setopt(d->m_handle, CURLOPT_COOKIEFILE, m_cookieJarFileName);
        curl_easy_setopt(d->m_handle, CURLOPT_COOKIEJAR, m_cookieJarFileName);
    }

    if (job->request().httpHeaderFields().size() > 0) {
        struct curl_slist* headers = 0;
        HTTPHeaderMap customHeaders = job->request().httpHeaderFields();
        HTTPHeaderMap::const_iterator end = customHeaders.end();
        for (HTTPHeaderMap::const_iterator it = customHeaders.begin(); it != end; ++it) {
            String key = it->first;
            String value = it->second;
            String headerString(key);
            headerString.append(": ");
            headerString.append(value);
            CString headerLatin1 = headerString.latin1();
            headers = curl_slist_append(headers, headerLatin1.data());
        }
        curl_easy_setopt(d->m_handle, CURLOPT_HTTPHEADER, headers);
        d->m_customHeaders = headers;
    }

    if ("GET" == job->request().httpMethod())
        curl_easy_setopt(d->m_handle, CURLOPT_HTTPGET, TRUE);
    else if ("POST" == job->request().httpMethod())
        setupPOST(job);
    else if ("PUT" == job->request().httpMethod())
        setupPUT(job);
    else if ("HEAD" == job->request().httpMethod())
        curl_easy_setopt(d->m_handle, CURLOPT_NOBODY, TRUE);

    CURLMcode ret = curl_multi_add_handle(m_curlMultiHandle, d->m_handle);
    // don't call perform, because events must be async
    // timeout will occur and do curl_multi_perform
    if (ret && ret != CURLM_CALL_MULTI_PERFORM) {
#ifndef NDEBUG
        printf("Error %d starting job %s\n", ret, job->request().url().url().ascii());
#endif
        job->cancel();
        return;
    }
}

void ResourceHandleManager::cancel(ResourceHandle* job)
{
    if (removeScheduledJob(job))
        return;
    removeFromCurl(job);
    // FIXME: report an error?
}

} // namespace WebCore
