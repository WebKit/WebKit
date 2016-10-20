/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if USE(NETWORK_SESSION)

#include "DownloadID.h"
#include "SandboxExtension.h"
#include <WebCore/Credential.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/ResourceHandleTypes.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/Timer.h>
#include <wtf/Function.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSURLSessionDataTask;
#endif

#if USE(SOUP)
#include <WebCore/ProtectionSpace.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>
#endif

namespace WebCore {
class AuthenticationChallenge;
class Credential;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SharedBuffer;
}

namespace WebKit {

class Download;
class NetworkSession;
class PendingDownload;
enum class AuthenticationChallengeDisposition;

typedef Function<void(const WebCore::ResourceRequest&)> RedirectCompletionHandler;
typedef Function<void(AuthenticationChallengeDisposition, const WebCore::Credential&)> ChallengeCompletionHandler;
typedef Function<void(WebCore::PolicyAction)> ResponseCompletionHandler;

class NetworkDataTaskClient {
public:
    virtual void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&) = 0;
    virtual void didReceiveChallenge(const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&&) = 0;
    virtual void didReceiveResponseNetworkSession(WebCore::ResourceResponse&&, ResponseCompletionHandler&&) = 0;
    virtual void didReceiveData(Ref<WebCore::SharedBuffer>&&) = 0;
    virtual void didCompleteWithError(const WebCore::ResourceError&) = 0;
    virtual void didBecomeDownload() = 0;
    virtual void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend) = 0;
    virtual void wasBlocked() = 0;
    virtual void cannotShowURL() = 0;
    
    virtual ~NetworkDataTaskClient() { }
};

class NetworkDataTask : public RefCounted<NetworkDataTask> {
    friend class NetworkSession;
public:
    static Ref<NetworkDataTask> create(NetworkSession& session, NetworkDataTaskClient& client, const WebCore::ResourceRequest& request, WebCore::StoredCredentials storedCredentials, WebCore::ContentSniffingPolicy shouldContentSniff, bool shouldClearReferrerOnHTTPSToHTTPRedirect)
    {
        return adoptRef(*new NetworkDataTask(session, client, request, storedCredentials, shouldContentSniff, shouldClearReferrerOnHTTPSToHTTPRedirect));
    }
    
    void suspend();
    void cancel();
    void resume();

    enum class State {
        Running,
        Suspended,
        Canceling,
        Completed
    };
    State state() const;

    typedef uint64_t TaskIdentifier;
    
    ~NetworkDataTask();

#if PLATFORM(COCOA)
    void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend);
    void didReceiveChallenge(const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&&);
    void didCompleteWithError(const WebCore::ResourceError&);
    void didReceiveResponse(WebCore::ResourceResponse&&, ResponseCompletionHandler&&);
    void didReceiveData(Ref<WebCore::SharedBuffer>&&);
    void didBecomeDownload();

    void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&);
    void transferSandboxExtensionToDownload(Download&);
#endif
    NetworkDataTaskClient* client() const { return m_client; }
    void clearClient() { m_client = nullptr; }
    
    DownloadID pendingDownloadID() const { return m_pendingDownloadID; }
    PendingDownload* pendingDownload() const { return m_pendingDownload; }
    void setPendingDownloadID(DownloadID downloadID)
    {
        ASSERT(!m_pendingDownloadID.downloadID());
        ASSERT(downloadID.downloadID());
        m_pendingDownloadID = downloadID;
    }
    void setPendingDownload(PendingDownload& pendingDownload)
    {
        ASSERT(!m_pendingDownload);
        m_pendingDownload = &pendingDownload;
    }
    void setPendingDownloadLocation(const String& filename, const SandboxExtension::Handle&, bool allowOverwrite);
    const String& pendingDownloadLocation() const { return m_pendingDownloadLocation; }
    bool isDownload() const { return !!m_pendingDownloadID.downloadID(); }

    const WebCore::ResourceRequest& firstRequest() const { return m_firstRequest; }
    String suggestedFilename();
    void setSuggestedFilename(const String&);
    bool allowsSpecificHTTPSCertificateForHost(const WebCore::AuthenticationChallenge&);
    
private:
    NetworkDataTask(NetworkSession&, NetworkDataTaskClient&, const WebCore::ResourceRequest&, WebCore::StoredCredentials, WebCore::ContentSniffingPolicy, bool shouldClearReferrerOnHTTPSToHTTPRedirect);

    bool tryPasswordBasedAuthentication(const WebCore::AuthenticationChallenge&, const ChallengeCompletionHandler&);

#if USE(SOUP)
    void timeoutFired();
    void startTimeout();
    void stopTimeout();
    void invalidateAndCancel();
    void createRequest(const WebCore::ResourceRequest&);
    void clearRequest();
    static void sendRequestCallback(SoupRequest*, GAsyncResult*, NetworkDataTask*);
    void didSendRequest(GRefPtr<GInputStream>&&);
    void didReceiveResponse();
    static void tlsErrorsChangedCallback(SoupMessage*, GParamSpec*, NetworkDataTask*);
    void tlsErrorsChanged();
    void applyAuthenticationToRequest(WebCore::ResourceRequest&);
    static void authenticateCallback(SoupSession*, SoupMessage*, SoupAuth*, gboolean retrying, NetworkDataTask*);
    void authenticate(WebCore::AuthenticationChallenge&&);
    void continueAuthenticate(WebCore::AuthenticationChallenge&&);
    static void skipInputStreamForRedirectionCallback(GInputStream*, GAsyncResult*, NetworkDataTask*);
    void skipInputStreamForRedirection();
    void didFinishSkipInputStreamForRedirection();
    bool shouldStartHTTPRedirection();
    void continueHTTPRedirection();
    static void readCallback(GInputStream*, GAsyncResult*, NetworkDataTask*);
    void read();
    void didRead(gssize bytesRead);
    void didFinishRead();
    static void requestNextPartCallback(SoupMultipartInputStream*, GAsyncResult*, NetworkDataTask*);
    void requestNextPart();
    void didRequestNextPart(GRefPtr<GInputStream>&&);
    void didFinishRequestNextPart();
    static void gotHeadersCallback(SoupMessage*, NetworkDataTask*);
    void didGetHeaders();
    static void wroteBodyDataCallback(SoupMessage*, SoupBuffer*, NetworkDataTask*);
    void didWriteBodyData(uint64_t bytesSent);
    void download();
    static void writeDownloadCallback(GOutputStream*, GAsyncResult*, NetworkDataTask*);
    void writeDownload();
    void didWriteDownload(gsize bytesWritten);
    void didFailDownload(const WebCore::ResourceError&);
    void didFinishDownload();
    void cleanDownloadFiles();
    void didFail(const WebCore::ResourceError&);
#if ENABLE(WEB_TIMING)
    static void networkEventCallback(SoupMessage*, GSocketClientEvent, GIOStream*, NetworkDataTask*);
    void networkEvent(GSocketClientEvent);
#if SOUP_CHECK_VERSION(2, 49, 91)
    static void startingCallback(SoupMessage*, NetworkDataTask*);
#else
    static void requestStartedCallback(SoupSession*, SoupMessage*, SoupSocket*, NetworkDataTask*);
#endif
    void didStartRequest();
    static void restartedCallback(SoupMessage*, NetworkDataTask*);
    void didRestart();
#endif
#endif
    
    enum FailureType {
        NoFailure,
        BlockedFailure,
        InvalidURLFailure
    };
    FailureType m_scheduledFailureType { NoFailure };
    WebCore::Timer m_failureTimer;
    void failureTimerFired();
    void scheduleFailure(FailureType);
    
    RefPtr<NetworkSession> m_session;
    NetworkDataTaskClient* m_client;
    PendingDownload* m_pendingDownload { nullptr };
    DownloadID m_pendingDownloadID;
    String m_user;
    String m_password;
#if USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
    WebCore::Credential m_initialCredential;
#endif
    WebCore::StoredCredentials m_storedCredentials;
    String m_lastHTTPMethod;
    String m_pendingDownloadLocation;
    WebCore::ResourceRequest m_firstRequest;
    bool m_shouldClearReferrerOnHTTPSToHTTPRedirect;
    RefPtr<SandboxExtension> m_sandboxExtension;
#if PLATFORM(COCOA)
    RetainPtr<NSURLSessionDataTask> m_task;
#endif
#if USE(SOUP)
    State m_state { State::Suspended };
    WebCore::ContentSniffingPolicy m_shouldContentSniff;
    GRefPtr<SoupRequest> m_soupRequest;
    GRefPtr<SoupMessage> m_soupMessage;
    GRefPtr<GInputStream> m_inputStream;
    GRefPtr<SoupMultipartInputStream> m_multipartInputStream;
    GRefPtr<GCancellable> m_cancellable;
    GRefPtr<GAsyncResult> m_pendingResult;
    WebCore::ProtectionSpace m_protectionSpaceForPersistentStorage;
    WebCore::Credential m_credentialForPersistentStorage;
    WebCore::ResourceResponse m_response;
    Vector<char> m_readBuffer;
    unsigned m_redirectCount { 0 };
    uint64_t m_bodyDataTotalBytesSent { 0 };
    GRefPtr<GFile> m_downloadDestinationFile;
    GRefPtr<GFile> m_downloadIntermediateFile;
    GRefPtr<GOutputStream> m_downloadOutputStream;
    bool m_allowOverwriteDownload { false };
#if ENABLE(WEB_TIMING)
    double m_startTime { 0 };
#endif
    RunLoop::Timer<NetworkDataTask> m_timeoutSource;
#endif
    String m_suggestedFilename;
};

#if PLATFORM(COCOA)
WebCore::Credential serverTrustCredential(const WebCore::AuthenticationChallenge&);
#endif
    
}

#endif // USE(NETWORK_SESSION)
