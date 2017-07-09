/*
 * Copyright (C) 2004, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Appcelerator Inc.
 * Copyright (C) 2009 Brent Fulgham <bfulgham@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "MultipartHandle.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SSLHandle.h"
#include "TextEncoding.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringView.h>

#if OS(WINDOWS)
#else
#include <sys/param.h>
#define MAX_PATH MAXPATHLEN
#endif

#include <errno.h>
#include <stdio.h>
#if ENABLE(WEB_TIMING)
#include <wtf/CurrentTime.h>
#endif
#include <wtf/Lock.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace WebCore {

const int selectTimeoutMS = 5;
static const Seconds pollTime { 50_ms };
const int maxRunningJobs = 128;

ResourceHandleManager::ResourceHandleManager()
    : m_downloadTimer(*this, &ResourceHandleManager::downloadTimerCallback)
    , m_runningJobs(0)
{

}

ResourceHandleManager::~ResourceHandleManager()
{

}

ResourceHandleManager* ResourceHandleManager::sharedInstance()
{
    static ResourceHandleManager* sharedInstance = 0;
    if (!sharedInstance)
        sharedInstance = new ResourceHandleManager();
    return sharedInstance;
}

void ResourceHandleManager::downloadTimerCallback()
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
        m_curlMultiHandle.getFdSet(fdread, fdwrite, fdexcep, maxfd);

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
    while (m_curlMultiHandle.perform(runningHandles) == CURLM_CALL_MULTI_PERFORM) { }

    // check the curl messages indicating completed transfers
    // and free their resources
    while (true) {
        int messagesInQueue = 0;
        CURLMsg* msg = m_curlMultiHandle.readInfo(messagesInQueue);
        if (!msg)
            break;

        // find the node which has same d->m_handle as completed transfer
        CURL* handle = msg->easy_handle;
        ASSERT(handle);
        ResourceHandle* job = nullptr;
        CURLcode err = curl_easy_getinfo(handle, CURLINFO_PRIVATE, &job);
        ASSERT_UNUSED(err, CURLE_OK == err);
        ASSERT(job);
        if (!job)
            continue;
        ResourceHandleInternal* d = job->getInternal();

        if (d->m_cancelled) {
            removeFromCurl(job);
            continue;
        }

        if (CURLMSG_DONE != msg->msg)
            continue;

        job->handleCurlMsg(msg);
        removeFromCurl(job);
    }

    bool started = startScheduledJobs(); // new jobs might have been added in the meantime

    if (!m_downloadTimer.isActive() && (started || (runningHandles > 0)))
        m_downloadTimer.startOneShot(pollTime);
}

void ResourceHandleManager::removeFromCurl(ResourceHandle* job)
{
    ResourceHandleInternal* d = job->getInternal();
    ASSERT(d->m_curlHandle.handle());
    if (!d->m_curlHandle.handle())
        return;
    m_runningJobs--;

    m_curlMultiHandle.removeHandle(d->m_curlHandle.handle());
    job->deref();
}

void ResourceHandleManager::add(ResourceHandle* job)
{
    // we can be called from within curl, so to avoid re-entrancy issues
    // schedule this job to be added the next time we enter curl download loop
    job->ref();
    m_resourceHandleList.append(job);
    if (!m_downloadTimer.isActive())
        m_downloadTimer.startOneShot(pollTime);
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

void ResourceHandleManager::startJob(ResourceHandle* job)
{
    job->initialize();

    m_runningJobs++;

    CURLMcode ret = m_curlMultiHandle.addHandle(job->getInternal()->m_curlHandle.handle());

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

void ResourceHandleManager::cancel(ResourceHandle* job)
{
    if (removeScheduledJob(job))
        return;

    ResourceHandleInternal* d = job->getInternal();
    d->m_cancelled = true;
    if (!m_downloadTimer.isActive())
        m_downloadTimer.startOneShot(pollTime);
}

} // namespace WebCore

#endif
