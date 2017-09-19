/*
 * Copyright (C) 2017 NAVER Corp. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(CURL)

#include "Credential.h"
#include "CurlContext.h"
#include "CurlJobManager.h"
#include "CurlSSLVerifier.h"
#include "FormDataStreamCurl.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/Condition.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class MultipartHandle;
class ProtectionSpace;
class ResourceHandle;
class ThreadSafeDataBuffer;

class ResourceHandleCurlDelegate final : public ThreadSafeRefCounted<ResourceHandleCurlDelegate>, public CurlJobClient {
public:
    ResourceHandleCurlDelegate(ResourceHandle*);
    ~ResourceHandleCurlDelegate();

    bool hasHandle() const;
    void releaseHandle();

    bool start();
    void cancel();

    void setDefersLoading(bool);
    void setAuthentication(const String&, const String&);

    void dispatchSynchronousJob();

private:
    void retain() override;
    void release() override;

    void setupRequest() override;
    void notifyFinish() override;
    void notifyFail() override;

    // Called from main thread.
    ResourceResponse& response();

    void setupAuthentication();

    void didReceiveAllHeaders(long httpCode, long long contentLength, uint16_t connectPort, long availableHttpAuth);
    void didReceiveContentData(ThreadSafeDataBuffer);
    void handleLocalReceiveResponse();
    void prepareSendData(char*, size_t blockSize, size_t numberOfBlocks);

    void didFinish(NetworkLoadMetrics);
    void didFail(const ResourceError&);

    void handleDataURL();

    // Called from worker thread.
    void setupPOST();
    void setupPUT();
    size_t getFormElementsCount();
    void setupFormData(bool);
    void applyAuthentication();
    NetworkLoadMetrics getNetworkLoadMetrics();

    CURLcode willSetupSslCtx(void*);
    size_t didReceiveHeader(String&&);
    size_t didReceiveData(ThreadSafeDataBuffer);
    size_t willSendData(char*, size_t blockSize, size_t numberOfBlocks);

    static CURLcode willSetupSslCtxCallback(CURL*, void*, void*);
    static size_t didReceiveHeaderCallback(char*, size_t blockSize, size_t numberOfBlocks, void*);
    static size_t didReceiveDataCallback(char*, size_t blockSize, size_t numberOfBlocks, void*);
    static size_t willSendDataCallback(char*, size_t blockSize, size_t numberOfBlocks, void*);

    // Used by main thread.
    ResourceHandle* m_handle;
    FormDataStream m_formDataStream;
    std::unique_ptr<MultipartHandle> m_multipartHandle;
    unsigned short m_authFailureCount { 0 };
    CurlJobTicket m_job { nullptr };
    // Used by worker thread.
    ResourceRequest m_firstRequest;
    HTTPHeaderMap m_customHTTPHeaderFields;
    bool m_shouldUseCredentialStorage;
    String m_user;
    String m_pass;
    Credential m_initialCredential;
    bool m_defersLoading;
    bool m_addedCacheValidationHeaders { false };
    Vector<char> m_postBytes;
    CurlHandle m_curlHandle;
    CurlSSLVerifier m_sslVerifier;
    // Used by both threads.
    Condition m_workerThreadConditionVariable;
    Lock m_workerThreadMutex;
    size_t m_sendBytes { 0 };
};

} // namespace WebCore

#endif
