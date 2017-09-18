/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * Copyright (C) 2017 NAVER Corp.
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
#include "CurlDownload.h"

#if USE(CURL)

#include "CurlContext.h"
#include "CurlSSLHandle.h"
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

    m_listener = listener;
    m_url = url;
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
    m_job = CurlJobManager::singleton().add(m_curlHandle, *this);
    return !!m_job;
}

bool CurlDownload::cancel()
{
    CurlJobTicket job = m_job;
    m_job = nullptr;
    CurlJobManager::singleton().cancel(job);
    return true;
}

ResourceResponse CurlDownload::getResponse() const
{
    LockHolder locker(m_mutex);
    ResourceResponse response(m_responseUrl, m_responseMIMEType, 0, m_responseTextEncodingName);
    response.setHTTPHeaderField(m_httpHeaderFieldName, m_httpHeaderFieldValue);
    return response;
}

void CurlDownload::retain()
{
    ref();
}

void CurlDownload::release()
{
    deref();
}

void CurlDownload::setupRequest()
{
    LockHolder locker(m_mutex);

    m_curlHandle.initialize();
    m_curlHandle.enableShareHandle();

    m_curlHandle.setUrl(m_url);
    m_curlHandle.setPrivateData(this);
    m_curlHandle.setHeaderCallbackFunction(headerCallback, this);
    m_curlHandle.setWriteCallbackFunction(writeCallback, this);
    m_curlHandle.enableFollowLocation();
    m_curlHandle.enableHttpAuthentication(CURLAUTH_ANY);
    m_curlHandle.setCACertPath(CurlContext::singleton().sslHandle().getCACertPath());

}

void CurlDownload::notifyFinish()
{
    callOnMainThread([protectedThis = makeRef(*this)] {
        if (!protectedThis->m_job)
            return;
        protectedThis->didFinish();
    });
}

void CurlDownload::notifyFail()
{
    callOnMainThread([protectedThis = makeRef(*this)] {
        if (!protectedThis->m_job)
            return;
        protectedThis->didFail();
    });
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
    m_curlHandle.appendRequestHeaders(request.httpHeaderFields());
}

void CurlDownload::didReceiveHeader(const String& header)
{
    LockHolder locker(m_mutex);

    if (header == "\r\n" || header == "\n") {

        auto httpCode = m_curlHandle.getResponseCode();

        if (httpCode && *httpCode >= 200 && *httpCode < 300) {
            auto url = m_curlHandle.getEffectiveURL();
            callOnMainThread([protectedThis = makeRef(*this), url = url.isolatedCopy()] {
                ResourceResponse localResponse = protectedThis->getResponse();
                protectedThis->m_responseUrl = url;
                protectedThis->m_responseMIMEType = extractMIMETypeFromMediaType(localResponse.httpHeaderField(HTTPHeaderName::ContentType));
                protectedThis->m_responseTextEncodingName = extractCharsetFromMediaType(localResponse.httpHeaderField(HTTPHeaderName::ContentType));

                protectedThis->didReceiveResponse();
            });
        }
    } else {
        callOnMainThread([protectedThis = makeRef(*this), header = header.isolatedCopy()] {
            int splitPos = header.find(":");
            if (splitPos != -1)
                protectedThis->m_httpHeaderFieldName = header.left(splitPos);
                protectedThis->m_httpHeaderFieldValue = header.substring(splitPos + 1).stripWhiteSpace();
        });
    }
}

void CurlDownload::didReceiveData(void* data, int size)
{
    LockHolder locker(m_mutex);

    callOnMainThread([protectedThis = makeRef(*this), size] {
        protectedThis->didReceiveDataOfLength(size);
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
    {
        LockHolder locker(m_mutex);

        if (m_deletesFileUponFailure)
            deleteFile(m_tempPath);
    }

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

}

#endif
