/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
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
#include "CurlDownload.h"

#include <WebCore/HTTPParsers.h>
#include <WebCore/MainThreadTask.h>

#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

using namespace WebCore;

template<> struct CrossThreadCopierBase<false, false, CurlDownload*> : public CrossThreadCopierPassThrough<CurlDownload*> {
};

// CurlDownloadManager -------------------------------------------------------------------

CurlDownloadManager::CurlDownloadManager()
: m_threadId(0)
, m_curlMultiHandle(0)
, m_activeDownloadCount(0)
, m_runThread(false)
{
    curl_global_init(CURL_GLOBAL_ALL);
    m_curlMultiHandle = curl_multi_init();
}

CurlDownloadManager::~CurlDownloadManager()
{
    stopThread();
    curl_multi_cleanup(m_curlMultiHandle);
    curl_global_cleanup();
}

bool CurlDownloadManager::add(CURL* curlHandle)
{
    MutexLocker locker(m_mutex);

    m_pendingHandleList.append(curlHandle);
    startThreadIfNeeded();

    return true;
}

bool CurlDownloadManager::remove(CURL* curlHandle)
{
    MutexLocker locker(m_mutex);

    m_removedHandleList.append(curlHandle);

    return true;
}

int CurlDownloadManager::getActiveDownloadCount() const
{
    return m_activeDownloadCount;
}

int CurlDownloadManager::getPendingDownloadCount() const
{
    MutexLocker locker(m_mutex);
    return m_pendingHandleList.size();
}

void CurlDownloadManager::startThreadIfNeeded()
{
    if (!m_runThread) {
        if (m_threadId)
            waitForThreadCompletion(m_threadId);
        m_runThread = true;
        m_threadId = createThread(downloadThread, this, "downloadThread");
    }
}

void CurlDownloadManager::stopThread()
{
    m_runThread = false;

    if (m_threadId) {
        waitForThreadCompletion(m_threadId);
        m_threadId = 0;
    }
}

void CurlDownloadManager::stopThreadIfIdle()
{
    MutexLocker locker(m_mutex);

    if (!getActiveDownloadCount() && !getPendingDownloadCount())
        setRunThread(false);
}

void CurlDownloadManager::updateHandleList()
{
    MutexLocker locker(m_mutex);

    // Add pending curl easy handles to multi list 
    int size = m_pendingHandleList.size();
    for (int i = 0; i < size; i++) {
        CURLMcode retval = curl_multi_add_handle(m_curlMultiHandle, m_pendingHandleList[0]);

        if (retval == CURLM_OK)
            m_pendingHandleList.remove(0);
    }

    // Remove curl easy handles from multi list 
    size = m_removedHandleList.size();
    for (int i = 0; i < size; i++) {
        CURLMcode retval = curl_multi_remove_handle(m_curlMultiHandle, m_removedHandleList[0]);

        if (retval == CURLM_OK)
            m_removedHandleList.remove(0);
    }
}

void CurlDownloadManager::downloadThread(void* data)
{
    CurlDownloadManager* downloadManager = reinterpret_cast<CurlDownloadManager*>(data);

    while (downloadManager->runThread()) {

        downloadManager->updateHandleList();

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

            FD_ZERO(&fdread);
            FD_ZERO(&fdwrite);
            FD_ZERO(&fdexcep);
            curl_multi_fdset(downloadManager->getMultiHandle(), &fdread, &fdwrite, &fdexcep, &maxfd);
            // When the 3 file descriptors are empty, winsock will return -1
            // and bail out, stopping the file download. So make sure we
            // have valid file descriptors before calling select.
            if (maxfd >= 0)
                rc = ::select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        } while (rc == -1 && errno == EINTR);

        while (curl_multi_perform(downloadManager->getMultiHandle(), &downloadManager->m_activeDownloadCount) == CURLM_CALL_MULTI_PERFORM) { }

        int messagesInQueue = 0;
        CURLMsg* msg = curl_multi_info_read(downloadManager->getMultiHandle(), &messagesInQueue);

        if (!msg)
            continue;

        CurlDownload* download = 0;
        CURLcode err = curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &download);

        if (msg->msg == CURLMSG_DONE) {
            if (msg->data.result == CURLE_OK)
                callOnMainThread<CurlDownload*, CurlDownload*>(CurlDownload::downloadFinishedCallback, download);
            else
                callOnMainThread<CurlDownload*, CurlDownload*>(CurlDownload::downloadFailedCallback, download);

            curl_multi_remove_handle(downloadManager->getMultiHandle(), msg->easy_handle);
        }

        downloadManager->stopThreadIfIdle();
    }
}

// CurlDownload --------------------------------------------------------------------------

CurlDownloadManager CurlDownload::m_downloadManager;

CurlDownload::CurlDownload()
: m_curlHandle(0)
, m_url(0)
, m_tempHandle(invalidPlatformFileHandle)
, m_deletesFileUponFailure(false)
, m_listener(0)
{
}

CurlDownload::~CurlDownload()
{
    MutexLocker locker(m_mutex);

    if (m_curlHandle)
        curl_easy_cleanup(m_curlHandle);

    if (m_url)
        fastFree(m_url);

    closeFile();
    moveFileToDestination();
}

void CurlDownload::init(CurlDownloadListener* listener, const KURL& url)
{
    if (!listener)
        return;

    MutexLocker locker(m_mutex);

    m_curlHandle = curl_easy_init();

    String urlStr = url.string();
    m_url = fastStrDup(urlStr.latin1().data());

    curl_easy_setopt(m_curlHandle, CURLOPT_URL, m_url);
    curl_easy_setopt(m_curlHandle, CURLOPT_PRIVATE, this);
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(m_curlHandle, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEHEADER, this);
    curl_easy_setopt(m_curlHandle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(m_curlHandle, CURLOPT_MAXREDIRS, 10);
    curl_easy_setopt(m_curlHandle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    m_listener = listener;
}

bool CurlDownload::start()
{
    return m_downloadManager.add(m_curlHandle);
}

bool CurlDownload::cancel()
{
    return m_downloadManager.remove(m_curlHandle);
}

String CurlDownload::getTempPath() const
{
    MutexLocker locker(m_mutex);
    return m_tempPath;
}

String CurlDownload::getUrl() const
{
    MutexLocker locker(m_mutex);
    return String(m_url);
}

ResourceResponse CurlDownload::getResponse() const
{
    MutexLocker locker(m_mutex);
    return m_response;
}

void CurlDownload::closeFile()
{
    MutexLocker locker(m_mutex);

    if (m_tempHandle) {
        WebCore::closeFile(m_tempHandle);
        m_tempHandle = invalidPlatformFileHandle;
    }
}

void CurlDownload::moveFileToDestination()
{
    if (m_destination.isEmpty())
        return;

    ::MoveFile(m_tempPath.charactersWithNullTermination(), m_destination.charactersWithNullTermination());
}

void CurlDownload::didReceiveHeader(const String& header)
{
    MutexLocker locker(m_mutex);

    if (header == "\r\n" || header == "\n") {

        long httpCode = 0;
        CURLcode err = curl_easy_getinfo(m_curlHandle, CURLINFO_RESPONSE_CODE, &httpCode);

        if (httpCode >= 200 && httpCode < 300) {
            const char* url = 0;
            err = curl_easy_getinfo(m_curlHandle, CURLINFO_EFFECTIVE_URL, &url);
            m_response.setURL(KURL(ParsedURLString, url));

            m_response.setMimeType(extractMIMETypeFromMediaType(m_response.httpHeaderField("Content-Type")));
            m_response.setTextEncodingName(extractCharsetFromMediaType(m_response.httpHeaderField("Content-Type")));
            m_response.setSuggestedFilename(filenameFromHTTPContentDisposition(m_response.httpHeaderField("Content-Disposition")));

            callOnMainThread<CurlDownload*, CurlDownload*>(receivedResponseCallback, this);
        }
    } else {
        int splitPos = header.find(":");
        if (splitPos != -1)
            m_response.setHTTPHeaderField(header.left(splitPos), header.substring(splitPos+1).stripWhiteSpace());
    }
}

void CurlDownload::didReceiveData(void* data, int size)
{
    MutexLocker locker(m_mutex);

    callOnMainThread<CurlDownload*, CurlDownload*, int, int>(receivedDataCallback, this, size);

    if (m_tempPath.isEmpty())
        m_tempPath = openTemporaryFile("download", m_tempHandle);

    if (m_tempHandle != invalidPlatformFileHandle) {
        const char* fileData = static_cast<const char*>(data);
        writeToFile(m_tempHandle, fileData, size);
    }
}

void CurlDownload::didReceiveResponse()
{
    if (m_listener)
        m_listener->didReceiveResponse();
}

void CurlDownload::didReceiveDataOfLength(int size)
{
    if (m_listener)
        m_listener->didReceiveDataOfLength(size);
}

void CurlDownload::didFinish()
{
    closeFile();
    moveFileToDestination();

    if (m_listener)
        m_listener->didFinish();
}

void CurlDownload::didFail()
{
    MutexLocker locker(m_mutex);

    closeFile();

    if (m_deletesFileUponFailure)
        deleteFile(m_tempPath);

    if (m_listener)
        m_listener->didFail();
}

size_t CurlDownload::writeCallback(void* ptr, size_t size, size_t nmemb, void* data)
{
    size_t totalSize = size * nmemb;
    CurlDownload* download = reinterpret_cast<CurlDownload*>(data);

    if (download)
        download->didReceiveData(ptr, totalSize);

    return totalSize;
}

size_t CurlDownload::headerCallback(char* ptr, size_t size, size_t nmemb, void* data)
{
    size_t totalSize = size * nmemb;
    CurlDownload* download = reinterpret_cast<CurlDownload*>(data);

    String header(static_cast<const char*>(ptr), totalSize);

    if (download)
        download->didReceiveHeader(header);

    return totalSize;
}

void CurlDownload::downloadFinishedCallback(CurlDownload* download)
{
    if (download)
        download->didFinish();
}

void CurlDownload::downloadFailedCallback(CurlDownload* download)
{
    if (download)
        download->didFail();
}

void CurlDownload::receivedDataCallback(CurlDownload* download, int size)
{
    if (download)
        download->didReceiveDataOfLength(size);
}

void CurlDownload::receivedResponseCallback(CurlDownload* download)
{
    if (download)
        download->didReceiveResponse();
}
