/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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
#include "ResourceLoaderManager.h"

#include "ResourceLoader.h"
#include "ResourceLoaderInternal.h"

namespace WebCore {

const int selectTimeoutMS = 5;
const double pollTimeSeconds = 0.05;

ResourceLoaderManager::ResourceLoaderManager()
    : m_useSimple(false)
    , jobs(new HashSet<ResourceLoader*>)
    , m_downloadTimer(this, &ResourceLoaderManager::downloadTimerCallback)
{
    curl_global_init(CURL_GLOBAL_ALL);
    curlMultiHandle = curl_multi_init();
}

ResourceLoaderManager* ResourceLoaderManager::get()
{
    static ResourceLoaderManager* s_singleton;

    if (!s_singleton)
        s_singleton = new ResourceLoaderManager;

    return s_singleton;
}

void ResourceLoaderManager::useSimpleTransfer(bool useSimple)
{
    m_useSimple = useSimple;
}

static size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* obj)
{
    ResourceLoader* job = static_cast<ResourceLoader*>(obj);
    ResourceLoaderInternal* d = job->getInternal();
    int totalSize = size * nmemb;
    d->client->receivedData(job, static_cast<char*>(ptr), totalSize);
    return totalSize;
}

static size_t headerCallback(char* ptr, size_t size, size_t nmemb, void* obj)
{
    ResourceLoader* job = static_cast<ResourceLoader*>(obj);
 
    if (job->method() == "POST")
        job->receivedResponse(ptr);
    
    int totalSize = size * nmemb;
    return totalSize;
}

void ResourceLoaderManager::downloadTimerCallback(Timer<ResourceLoaderManager>* timer)
{
    if (jobs->isEmpty()) {
        m_downloadTimer.stop();
        return;
    }

    if (m_useSimple) {
        for (HashSet<ResourceLoader*>::iterator it = jobs->begin(); it != jobs->end(); ++it) {
            ResourceLoader* job = *it;
            ResourceLoaderInternal* d = job->getInternal();
            CURLcode res = curl_easy_perform(d->m_handle);
            if (res != CURLE_OK)
                printf("Error WITH JOB %d\n", res);
            d->client->receivedAllData(job, 0);
            d->client->receivedAllData(job);
            curl_easy_cleanup(d->m_handle);
            d->m_handle = 0;
        }

        jobs->clear();
        m_downloadTimer.stop();
    } else {
        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);
        curl_multi_fdset(curlMultiHandle, &fdread, &fdwrite, &fdexcep, &maxfd);
        int nrunning;
        struct timeval timeout;
        int retval;
        timeout.tv_sec = 0;
        timeout.tv_usec = selectTimeoutMS * 1000;       // select waits microseconds
        retval = ::select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        switch (retval) {
            case -1:                        // select error
                /* fallthrough*/
            case 0:                 // select timeout
                /* fallthrough. this can be the first perform to be made */
            default:                        // 1+ descriptors have data
                while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(curlMultiHandle, &nrunning))
                    { }
        }

        // check the curl messages indicating completed transfers
        // and free their resources
        int nmsgs;
        while (CURLMsg* msg = curl_multi_info_read(curlMultiHandle, &nmsgs)) {
            if (msg->msg == CURLMSG_DONE) {
                // find the node which has same d->m_handle as completed transfer
                CURL* chandle = msg->easy_handle;
                assert(chandle);
                ResourceLoader* job;
                curl_easy_getinfo(chandle, CURLINFO_PRIVATE, &job);
                assert(job); //fixme: assert->if ?
                // if found, delete it
                if (job) {
                    ResourceLoaderInternal* d = job->getInternal();
                    switch (msg->data.result) {
                        case CURLE_OK: {
                            // use this to authenticate
                            long respCode = -1;
                            curl_easy_getinfo(d->m_handle, CURLINFO_RESPONSE_CODE, &respCode);
                            remove(job);
                            break;
                        }
                        default:
                            printf("Curl ERROR %s\n", curl_easy_strerror(msg->data.result));
                            job->setError(msg->data.result);
                            remove(job);
                            break;
                    }
                } else {
                    printf("CurlRequest not found, eventhough curl d->m_handle finished\n");
                    assert(0);
                }
            }

        }
    }
    if (!jobs->isEmpty())
        m_downloadTimer.startOneShot(pollTimeSeconds);
}

void ResourceLoaderManager::remove(ResourceLoader* job)
{
    ResourceLoaderInternal* d = job->getInternal();
    if (!d->m_handle)
        return;
    if (jobs->contains(job))
        jobs->remove(job);
    if (jobs->isEmpty())
        m_downloadTimer.stop();
    d->client->receivedAllData(job, 0);
    d->client->receivedAllData(job);
    if (d->m_handle) {
        curl_multi_remove_handle(curlMultiHandle, d->m_handle);
        curl_easy_cleanup(d->m_handle);
        d->m_handle = NULL;
    }
}

void ResourceLoaderManager::add(ResourceLoader* job)
{
    bool startTimer = jobs->isEmpty();
    ResourceLoaderInternal* d = job->getInternal();
    DeprecatedString url = d->URL.url();
    d->m_handle = curl_easy_init();
    curl_easy_setopt(d->m_handle, CURLOPT_PRIVATE, job);
    curl_easy_setopt(d->m_handle, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(d->m_handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(d->m_handle, CURLOPT_WRITEDATA, job);
    curl_easy_setopt(d->m_handle, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(d->m_handle, CURLOPT_WRITEHEADER, job);
    curl_easy_setopt(d->m_handle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(d->m_handle, CURLOPT_MAXREDIRS, 10);
    curl_easy_setopt(d->m_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
    // url ptr must remain valid through the request
    curl_easy_setopt(d->m_handle, CURLOPT_URL, url.ascii());

    if (job->method() == "POST") {
        DeprecatedString postData = job->postData().flattenToString();

        char* postDataString = (char*)malloc(postData.length() + 1);
        strncpy(postDataString, postData.ascii(), postData.length());
        postDataString[postData.length()] = '\0';

        // FIXME: Do it properly after we got rid of libcurl! (also leaks the headerlist. hmpf.)
        curl_easy_setopt(d->m_handle, CURLOPT_POSTFIELDS, postDataString);
    }

    if (m_useSimple)
        jobs->add(job);
    else {
        CURLMcode ret = curl_multi_add_handle(curlMultiHandle, d->m_handle);
        // don't call perform, because events must be async
        // timeout will occur and do curl_multi_perform
        if (ret && ret != CURLM_CALL_MULTI_PERFORM) {
            printf("Error %d starting job %s\n", ret, d->URL.url().ascii());
            job->setError(1);
            startTimer =false;
        } else
            jobs->add(job);
    }
    if (startTimer)
        m_downloadTimer.startOneShot(pollTimeSeconds);
}

void ResourceLoaderManager::cancel(ResourceLoader* job)
{
    remove(job);
    job->setError(1);
}

} // namespace WebCore

// vim: ts=4 sw=4 et
