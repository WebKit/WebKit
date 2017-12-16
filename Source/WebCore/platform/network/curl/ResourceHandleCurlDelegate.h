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
#include "CurlRequestClient.h"
#include "ResourceRequest.h"

namespace WebCore {

class CurlRequest;
class MultipartHandle;
class ResourceHandle;
class ResourceResponse;

class ResourceHandleCurlDelegate final : public ThreadSafeRefCounted<ResourceHandleCurlDelegate>, public CurlRequestClient {
public:
    ResourceHandleCurlDelegate(ResourceHandle*);
    ~ResourceHandleCurlDelegate();

    bool hasHandle() const;
    void releaseHandle();

    void start();
    void cancel();

    void setDefersLoading(bool);
    void setAuthentication(const String&, const String&);

    void dispatchSynchronousJob();

    void continueDidReceiveResponse();
    void platformContinueSynchronousDidReceiveResponse();

    void continueWillSendRequest(ResourceRequest&&);

private:
    // Called from main thread.
    ResourceResponse& response();

    std::pair<String, String> getCredential(ResourceRequest&, bool);

    bool cancelledOrClientless();

    Ref<CurlRequest> createCurlRequest(ResourceRequest&);
    void curlDidReceiveResponse(const CurlResponse&) override;
    void curlDidReceiveBuffer(Ref<SharedBuffer>&&) override;
    void curlDidComplete() override;
    void curlDidFailWithError(const ResourceError&) override;

    void continueAfterDidReceiveResponse();

    bool shouldRedirectAsGET(const ResourceRequest&, bool crossOrigin);
    void willSendRequest();
    void continueAfterWillSendRequest(ResourceRequest&&);

    void handleDataURL();

    // Used by main thread.
    ResourceHandle* m_handle;
    std::unique_ptr<MultipartHandle> m_multipartHandle;
    unsigned m_authFailureCount { 0 };
    unsigned m_redirectCount { 0 };

    ResourceRequest m_firstRequest;
    ResourceRequest m_currentRequest;
    bool m_shouldUseCredentialStorage;
    String m_user;
    String m_pass;
    Credential m_initialCredential;
    bool m_defersLoading;
    bool m_addedCacheValidationHeaders { false };
    RefPtr<CurlRequest> m_curlRequest;
};

} // namespace WebCore

#endif
