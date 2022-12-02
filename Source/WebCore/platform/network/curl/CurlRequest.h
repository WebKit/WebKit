/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include "CurlFormDataStream.h"
#include "CurlMultipartHandle.h"
#include "CurlMultipartHandleClient.h"
#include "CurlRequestSchedulerClient.h"
#include "CurlResponse.h"
#include "ProtectionSpace.h"
#include "ResourceRequest.h"
#include <wtf/FileSystem.h>
#include <wtf/MessageQueue.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class CurlRequestClient;
class NetworkLoadMetrics;
class ResourceError;
class FragmentedSharedBuffer;
class SynchronousLoaderMessageQueue;

class CurlRequest : public ThreadSafeRefCounted<CurlRequest>, public CurlRequestSchedulerClient, public CurlMultipartHandleClient {
    WTF_MAKE_NONCOPYABLE(CurlRequest);

public:
    enum class ShouldSuspend : bool {
        No = false,
        Yes = true
    };

    enum class EnableMultipart : bool {
        No = false,
        Yes = true
    };

    enum class CaptureNetworkLoadMetrics : uint8_t {
        Basic,
        Extended
    };

    static Ref<CurlRequest> create(const ResourceRequest& request, CurlRequestClient& client, ShouldSuspend shouldSuspend = ShouldSuspend::No, EnableMultipart enableMultipart = EnableMultipart::No, CaptureNetworkLoadMetrics captureMetrics = CaptureNetworkLoadMetrics::Basic, RefPtr<SynchronousLoaderMessageQueue>&& messageQueue = nullptr)
    {
        return adoptRef(*new CurlRequest(request, &client, shouldSuspend, enableMultipart, captureMetrics, WTFMove(messageQueue)));
    }

    virtual ~CurlRequest();

    WEBCORE_EXPORT void invalidateClient();
    WEBCORE_EXPORT void setAuthenticationScheme(ProtectionSpace::AuthenticationScheme);
    WEBCORE_EXPORT void setUserPass(const String&, const String&);
    bool isServerTrustEvaluationDisabled() { return m_shouldDisableServerTrustEvaluation; }
    void disableServerTrustEvaluation() { m_shouldDisableServerTrustEvaluation = true; }

    WEBCORE_EXPORT void start();
    WEBCORE_EXPORT void cancel();
    WEBCORE_EXPORT void suspend();
    WEBCORE_EXPORT void resume();

    const ResourceRequest& resourceRequest() const { return m_request; }
    bool isCancelled();
    bool isCompletedOrCancelled();
    Seconds timeoutInterval() const;

    const String& user() const { return m_user; }
    const String& password() const { return m_password; }

    // Processing for DidReceiveResponse
    WEBCORE_EXPORT void completeDidReceiveResponse();

    // Download
    void enableDownloadToFile();
    const String& getDownloadedFilePath();

private:
    enum class Action {
        None,
        ReceiveData,
        StartTransfer,
        FinishTransfer
    };

    WEBCORE_EXPORT CurlRequest(const ResourceRequest&, CurlRequestClient*, ShouldSuspend, EnableMultipart, CaptureNetworkLoadMetrics, RefPtr<SynchronousLoaderMessageQueue>&&);

    void retain() override { ref(); }
    void release() override { deref(); }
    CURL* handle() override { return m_curlHandle ? m_curlHandle->handle() : nullptr; }

    void startWithJobManager();

    void callClient(Function<void(CurlRequest&, CurlRequestClient&)>&&);
    void runOnMainThread(Function<void()>&&);
    void runOnWorkerThreadIfRequired(Function<void()>&&);

    // Transfer processing of Request body, Response header/body
    // Called by worker thread in case of async, main thread in case of sync.
    CURL* setupTransfer() override;
    size_t willSendData(char*, size_t, size_t);
    size_t didReceiveHeader(String&&);
    size_t didReceiveData(const SharedBuffer&);
    void didReceiveHeaderFromMultipart(const Vector<String>&) override;
    void didReceiveDataFromMultipart(const SharedBuffer&) override;
    void didCompleteTransfer(CURLcode) override;
    void didCancelTransfer() override;
    void finalizeTransfer();
    void invokeCancel();

    int didReceiveDebugInfo(curl_infotype, char*, size_t);

    // For setup 
    void appendAcceptLanguageHeader(HTTPHeaderMap&);
    void setupPOST();
    void setupPUT();
    void setupSendData(bool forPutMethod);

    // Processing for DidReceiveResponse
    bool needToInvokeDidReceiveResponse() const { return m_didReceiveResponse && !m_didNotifyResponse; }
    bool needToInvokeDidCancelTransfer() const { return m_didNotifyResponse && !m_didReturnFromNotify && m_actionAfterInvoke == Action::FinishTransfer; }
    void invokeDidReceiveResponseForFile(const URL&);
    void invokeDidReceiveResponse(const CurlResponse&, Action);
    void setRequestPaused(bool);
    void setCallbackPaused(bool);
    void pausedStatusChanged();
    bool shouldBePaused() const { return m_isPausedOfRequest || m_isPausedOfCallback; };
    void updateHandlePauseState(bool);
    bool isHandlePaused() const;

    NetworkLoadMetrics networkLoadMetrics();

    // Download
    void writeDataToDownloadFileIfEnabled(const FragmentedSharedBuffer&);
    void closeDownloadFile();
    void cleanupDownloadFile();

    // Callback functions for curl
    static size_t willSendDataCallback(char*, size_t, size_t, void*);
    static size_t didReceiveHeaderCallback(char*, size_t, size_t, void*);
    static size_t didReceiveDataCallback(char*, size_t, size_t, void*);
    static int didReceiveDebugInfoCallback(CURL*, curl_infotype, char*, size_t, void*);


    CurlRequestClient* m_client { };
    Lock m_statusMutex;
    bool m_cancelled { false };
    bool m_completed { false };
    RefPtr<SynchronousLoaderMessageQueue> m_messageQueue;

    ResourceRequest m_request;
    String m_user;
    String m_password;
    unsigned long m_authType { CURLAUTH_ANY };
    bool m_shouldDisableServerTrustEvaluation { false };
    bool m_enableMultipart { false };

    enum class StartState : uint8_t { StartSuspended, WaitingForStart, DidStart };
    StartState m_startState;
    
    std::unique_ptr<CurlHandle> m_curlHandle;
    CurlFormDataStream m_formDataStream;
    std::unique_ptr<CurlMultipartHandle> m_multipartHandle;

    CurlResponse m_response;
    bool m_didReceiveResponse { false };
    bool m_didNotifyResponse { false };
    bool m_didReturnFromNotify { false };
    Action m_actionAfterInvoke { Action::None };
    CURLcode m_finishedResultCode { CURLE_OK };

    bool m_isPausedOfRequest { false };
    bool m_isPausedOfCallback { false };
    Lock m_pauseStateMutex;
    // Following `m_isHandlePaused` is actual paused state of CurlHandle. It's required because pause
    // request coming from main thread has a time lag until it invokes and receive callback can
    // change the state by returning a special value. So that is must be managed by this flag.
    // Unfortunately libcurl doesn't have an interface to check the state.
    // There's also no need to protect this flag by the mutex because it is and MUST BE accessed only
    // within worker thread. The access must be using accessor to detect irregular usage.
    // [TODO] When libcurl is updated to fetch paused state, remove this state variable and
    // setter/getter above.
    bool m_isHandlePaused { false };

    Lock m_downloadMutex;
    bool m_isEnabledDownloadToFile { false };
    String m_downloadFilePath;
    FileSystem::PlatformFileHandle m_downloadFileHandle { FileSystem::invalidPlatformFileHandle };

    bool m_captureExtraMetrics;
    HTTPHeaderMap m_requestHeaders;
    MonotonicTime m_performStartTime;
    size_t m_totalReceivedSize { 0 };
};

} // namespace WebCore
