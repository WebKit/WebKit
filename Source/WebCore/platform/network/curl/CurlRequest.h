/*
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CurlJobManager.h"
#include "CurlResponse.h"
#include "CurlSSLVerifier.h"
#include "FormDataStreamCurl.h"
#include "NetworkLoadMetrics.h"
#include "ResourceRequest.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class CurlRequestDelegate;
class ResourceError;
class SharedBuffer;

class CurlRequest : public ThreadSafeRefCounted<CurlRequest>, public CurlJobClient {
    WTF_MAKE_NONCOPYABLE(CurlRequest);

public:
    CurlRequest(const ResourceRequest&, CurlRequestDelegate* = nullptr, bool shouldSuspend = false);
    virtual ~CurlRequest() { }

    void setDelegate(CurlRequestDelegate* delegate) { m_delegate = delegate;  }
    void setUserPass(const String&, const String&);

    void start(bool isSyncRequest = false);
    void cancel();
    void suspend();
    void resume();

    bool isSyncRequest() { return m_isSyncRequest; }

    NetworkLoadMetrics getNetworkLoadMetrics() { return m_networkLoadMetrics.isolatedCopy(); }

private:
    void retain() override { ref(); }
    void release() override { deref(); }
    CURL* handle() override { return m_curlHandle ? m_curlHandle->handle() : nullptr; }

    void startWithJobManager();

    void callDelegate(WTF::Function<void(CurlRequestDelegate*)>);

    // Transfer processing of Request body, Response header/body
    // Called by worker thread in case of async, main thread in case of sync.
    CURL* setupTransfer() override;
    CURLcode willSetupSslCtx(void*);
    size_t willSendData(char*, size_t, size_t);
    size_t didReceiveHeader(String&&);
    size_t didReceiveData(Ref<SharedBuffer>&&);
    void didCompleteTransfer(CURLcode) override;
    void didCancelTransfer() override;

    // For POST and PUT method 
    void resolveBlobReferences(ResourceRequest&);
    void setupPOST(ResourceRequest&);
    void setupPUT(ResourceRequest&);
    void setupFormData(ResourceRequest&, bool);

    // Processing for DidResourceResponse
    void invokeDidReceiveResponseForFile(URL&);
    void invokeDidReceiveResponse();
    void setPaused(bool);

    // Callback functions for curl
    static CURLcode willSetupSslCtxCallback(CURL*, void*, void*);
    static size_t willSendDataCallback(char*, size_t, size_t, void*);
    static size_t didReceiveHeaderCallback(char*, size_t, size_t, void*);
    static size_t didReceiveDataCallback(char*, size_t, size_t, void*);


    std::atomic<CurlRequestDelegate*> m_delegate { };
    bool m_isSyncRequest { false };
    bool m_cancelled { false };

    // Used by worker thread in case of async, and main thread in case of sync.
    ResourceRequest m_request;
    String m_user;
    String m_password;
    bool m_shouldSuspend { false };

    std::unique_ptr<CurlHandle> m_curlHandle;
    std::unique_ptr<FormDataStream> m_formDataStream;
    Vector<char> m_postBuffer;
    CurlSSLVerifier m_sslVerifier;
    CurlResponse m_response;

    bool m_isPaused { false };

    NetworkLoadMetrics m_networkLoadMetrics;
};

}
