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
#include "CurlJobManager.h"

#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebCore {

CurlJobManager::CurlJobManager()
{

}

CurlJobManager::~CurlJobManager()
{
    stopThread();
}

bool CurlJobManager::add(CURL* curlHandle)
{
    ASSERT(isMainThread());

    {
        LockHolder locker(m_mutex);
        m_pendingHandleList.append(curlHandle);
    }

    startThreadIfNeeded();

    return true;
}

bool CurlJobManager::remove(CURL* curlHandle)
{
    LockHolder locker(m_mutex);

    m_removedHandleList.append(curlHandle);

    return true;
}

int CurlJobManager::getActiveCount() const
{
    LockHolder locker(m_mutex);
    return m_activeHandleList.size();
}

int CurlJobManager::getPendingCount() const
{
    LockHolder locker(m_mutex);
    return m_pendingHandleList.size();
}

void CurlJobManager::startThreadIfNeeded()
{
    ASSERT(isMainThread());

    if (!runThread()) {
        if (m_thread)
            m_thread->waitForCompletion();
        setRunThread(true);
        m_thread = Thread::create("curlThread", [this] {
            workerThread();
        });
    }
}

void CurlJobManager::stopThread()
{
    setRunThread(false);

    if (m_thread) {
        m_thread->waitForCompletion();
        m_thread = nullptr;
    }
}

void CurlJobManager::stopThreadIfIdle()
{
    if (!getActiveCount() && !getPendingCount())
        setRunThread(false);
}

void CurlJobManager::updateHandleList()
{
    LockHolder locker(m_mutex);

    // Remove curl easy handles from multi list 
    int size = m_removedHandleList.size();
    for (int i = 0; i < size; i++) {
        removeFromCurl(m_removedHandleList[0]);
        m_removedHandleList.remove(0);
    }

    // Add pending curl easy handles to multi list 
    size = m_pendingHandleList.size();
    for (int i = 0; i < size; i++) {
        addToCurl(m_pendingHandleList[0]);
        m_pendingHandleList.remove(0);
    }
}

bool CurlJobManager::addToCurl(CURL* curlHandle)
{
    CURLMcode retval = m_curlMultiHandle.addHandle(curlHandle);
    if (retval == CURLM_OK) {
        m_activeHandleList.append(curlHandle);
        return true;
    }
    return false;
}

bool CurlJobManager::removeFromCurl(CURL* curlHandle)
{
    int handlePos = m_activeHandleList.find(curlHandle);

    if (handlePos < 0)
        return true;

    CURLMcode retval = m_curlMultiHandle.removeHandle(curlHandle);
    if (retval == CURLM_OK) {
        m_activeHandleList.remove(handlePos);
        return true;
    }
    return false;
}

void CurlJobManager::workerThread()
{
    ASSERT(!isMainThread());

    while (runThread()) {

        updateHandleList();

        // Retry 'select' if it was interrupted by a process signal.
        int rc = 0;
        do {
            fd_set fdread;
            fd_set fdwrite;
            fd_set fdexcep;
            int maxfd = 0;

            const int selectTimeoutMS = 5;

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = selectTimeoutMS * 1000; // select waits microseconds

            m_curlMultiHandle.getFdSet(fdread, fdwrite, fdexcep, maxfd);

            // When the 3 file descriptors are empty, winsock will return -1
            // and bail out, stopping the file download. So make sure we
            // have valid file descriptors before calling select.
            if (maxfd >= 0)
                rc = ::select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        } while (rc == -1 && errno == EINTR);

        int runningHandles = 0;
        while (m_curlMultiHandle.perform(runningHandles) == CURLM_CALL_MULTI_PERFORM) { }

        int messagesInQueue = 0;
        CURLMsg* msg = m_curlMultiHandle.readInfo(messagesInQueue);
        if (!msg)
            continue;


        CurlJob* job = nullptr;
        CURLcode err = curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &job);
        UNUSED_PARAM(err);
        ASSERT(job);

        CurlJobAction action = job->handleCurlMsg(msg);

        if (action == CurlJobAction::Finished)
            removeFromCurl(msg->easy_handle);

        stopThreadIfIdle();
    }
}

}

#endif
