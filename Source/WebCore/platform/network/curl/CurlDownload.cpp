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

#include "CurlDownload.h"

#include "CurlContext.h"

#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "ResourceRequest.h"
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebCore {

// CurlDownload --------------------------------------------------------------------------

CurlDownload::CurlDownload() = default;

CurlDownload::~CurlDownload()
{
    closeFile();
    moveFileToDestination();
}

void CurlDownload::init(CurlDownloadListener* listener, const URL& url)
{
    if (!listener)
        return;

    LockHolder locker(m_mutex);

    m_curlHandle.enableShareHandle();

    m_curlHandle.setUrl(url);
    m_curlHandle.setPrivateData(this);
    m_curlHandle.setHeaderCallbackFunction(headerCallback, this);
    m_curlHandle.setWriteCallbackFunction(writeCallback, this);
    m_curlHandle.enableFollowLocation();
    m_curlHandle.enableHttpAuthentication(CURLAUTH_ANY);
    m_curlHandle.enableCAInfoIfExists();

    m_listener = listener;
}

void CurlDownload::init(CurlDownloadListener* listener, ResourceHandle*, const ResourceRequest& request, const ResourceResponse&)
{
    if (!listener)
        return;

    URL url(ParsedURLString, request.url());

    init(listener, url);

    addHeaders(request);
}

bool CurlDownload::start()
{
    ref(); // CurlJobManager will call deref when the download has finished.
    return CurlJobManager::singleton().add(m_curlHandle.handle());
}

bool CurlDownload::cancel()
{
    return CurlJobManager::singleton().remove(m_curlHandle.handle());
}

String CurlDownload::getTempPath() const
{
    LockHolder locker(m_mutex);
    return m_tempPath;
}

String CurlDownload::getUrl() const
{
    LockHolder locker(m_mutex);
    return String(m_curlHandle.url());
}

ResourceResponse CurlDownload::getResponse() const
{
    LockHolder locker(m_mutex);
    return m_response;
}

void CurlDownload::closeFile()
{
    LockHolder locker(m_mutex);

    if (m_tempHandle != invalidPlatformFileHandle) {
        WebCore::closeFile(m_tempHandle);
        m_tempHandle = invalidPlatformFileHandle;
    }
}

void CurlDownload::moveFileToDestination()
{
    LockHolder locker(m_mutex);

    if (m_destination.isEmpty())
        return;

    moveFile(m_tempPath, m_destination);
}

void CurlDownload::writeDataToFile(const char* data, int size)
{
    if (m_tempPath.isEmpty())
        m_tempPath = openTemporaryFile("download", m_tempHandle);

    if (m_tempHandle != invalidPlatformFileHandle)
        writeToFile(m_tempHandle, data, size);
}

void CurlDownload::addHeaders(const ResourceRequest& request)
{
    LockHolder locker(m_mutex);

    m_curlHandle.clearRequestHeaders();

    if (request.httpHeaderFields().size() > 0) {
        HTTPHeaderMap customHeaders = request.httpHeaderFields();
        HTTPHeaderMap::const_iterator end = customHeaders.end();
        for (HTTPHeaderMap::const_iterator it = customHeaders.begin(); it != end; ++it) {
            const String& value = it->value;
            String headerString(it->key);
            if (value.isEmpty())
                // Insert the ; to tell curl that this header has an empty value.
                headerString.append(";");
            else {
                headerString.append(": ");
                headerString.append(value);
            }

            m_curlHandle.appendRequestHeader(headerString);
        }

        m_curlHandle.enableRequestHeaders();
    }
}

void CurlDownload::didReceiveHeader(const String& header)
{
    LockHolder locker(m_mutex);

    if (header == "\r\n" || header == "\n") {

        long httpCode = 0;
        m_curlHandle.getResponseCode(httpCode);

        if (httpCode >= 200 && httpCode < 300) {
            URL url = m_curlHandle.getEffectiveURL();
            callOnMainThread([this, url = url.isolatedCopy(), protectedThis = makeRef(*this)] {
                m_response.setURL(url);
                m_response.setMimeType(extractMIMETypeFromMediaType(m_response.httpHeaderField(HTTPHeaderName::ContentType)));
                m_response.setTextEncodingName(extractCharsetFromMediaType(m_response.httpHeaderField(HTTPHeaderName::ContentType)));

                didReceiveResponse();
            });
        }
    } else {
        callOnMainThread([this, header = header.isolatedCopy(), protectedThis = makeRef(*this)] {
            int splitPos = header.find(":");
            if (splitPos != -1)
                m_response.setHTTPHeaderField(header.left(splitPos), header.substring(splitPos + 1).stripWhiteSpace());
        });
    }
}

void CurlDownload::didReceiveData(void* data, int size)
{
    LockHolder locker(m_mutex);

    RefPtr<CurlDownload> protectedThis(this);

    callOnMainThread([this, size, protectedThis] {
        didReceiveDataOfLength(size);
    });

    writeDataToFile(static_cast<const char*>(data), size);
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
    closeFile();

    LockHolder locker(m_mutex);

    if (m_deletesFileUponFailure)
        deleteFile(m_tempPath);

    if (m_listener)
        m_listener->didFail();
}

size_t CurlDownload::writeCallback(char* ptr, size_t size, size_t nmemb, void* data)
{
    size_t totalSize = size * nmemb;
    CurlDownload* download = reinterpret_cast<CurlDownload*>(data);

    if (download)
        download->didReceiveData(static_cast<void*>(ptr), totalSize);

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

CurlJobAction CurlDownload::handleCurlMsg(CURLMsg* msg)
{
    switch (msg->msg) {
    case CURLMSG_DONE: {
        if (msg->data.result == CURLE_OK) {
            callOnMainThread([this] {
                didFinish();
                deref(); // This matches the ref() in CurlDownload::start().
            });
        } else {
            callOnMainThread([this] {
                didFail();
                deref(); // This matches the ref() in CurlDownload::start().
            });
        }
        return CurlJobAction::Finished;
    }
    default: {
        return CurlJobAction::None;
    }
    }
}

}

#endif
