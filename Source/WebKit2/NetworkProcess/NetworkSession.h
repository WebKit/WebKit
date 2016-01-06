/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef NetworkSession_h
#define NetworkSession_h

#if PLATFORM(COCOA)
OBJC_CLASS NSURLSession;
OBJC_CLASS NSURLSessionDataTask;
OBJC_CLASS NSOperationQueue;
OBJC_CLASS WKNetworkSessionDelegate;
#endif

#include "DownloadID.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class AuthenticationChallenge;
class Credential;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SharedBuffer;
}

namespace WebKit {

enum class AuthenticationChallengeDisposition {
    UseCredential,
    PerformDefaultHandling,
    Cancel,
    RejectProtectionSpace
};

class NetworkSession;

class NetworkSessionTaskClient {
public:
    typedef std::function<void(const WebCore::ResourceRequest&)> RedirectCompletionHandler;
    virtual void willPerformHTTPRedirection(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, RedirectCompletionHandler) = 0;
    typedef std::function<void(AuthenticationChallengeDisposition, const WebCore::Credential&)> ChallengeCompletionHandler;
    virtual void didReceiveChallenge(const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler) = 0;
    typedef std::function<void(WebCore::PolicyAction)> ResponseCompletionHandler;
    virtual void didReceiveResponse(const WebCore::ResourceResponse&, ResponseCompletionHandler) = 0;
    virtual void didReceiveData(RefPtr<WebCore::SharedBuffer>&&) = 0;
    virtual void didCompleteWithError(const WebCore::ResourceError&) = 0;
    virtual void didBecomeDownload() = 0;

    virtual ~NetworkSessionTaskClient() { }
};

class NetworkDataTask {
    friend class NetworkSession;
public:
#if PLATFORM(COCOA)
    explicit NetworkDataTask(NetworkSession&, NetworkSessionTaskClient&, RetainPtr<NSURLSessionDataTask>&&);
#else
    explicit NetworkDataTask(NetworkSession&, NetworkSessionTaskClient&);
#endif

    void suspend();
    void cancel();
    void resume();

    typedef uint64_t TaskIdentifier;
    TaskIdentifier taskIdentifier();

    ~NetworkDataTask();

    NetworkSessionTaskClient& client() { return m_client; }

    DownloadID downloadID() { return m_downloadID; }
    void setDownloadID(DownloadID downloadID)
    {
        ASSERT(!m_downloadID.downloadID());
        ASSERT(downloadID.downloadID());
        m_downloadID = downloadID;
    }
    
private:
    NetworkSession& m_session;
    NetworkSessionTaskClient& m_client;
    DownloadID m_downloadID;
#if PLATFORM(COCOA)
    RetainPtr<NSURLSessionDataTask> m_task;
#endif
};

class NetworkSession {
    friend class NetworkDataTask;
public:
    enum class Type {
        Normal,
        Ephemeral
    };
    NetworkSession(Type, WebCore::SessionID);
    ~NetworkSession();

    static NetworkSession& defaultSession();
    
    std::unique_ptr<NetworkDataTask> createDataTaskWithRequest(const WebCore::ResourceRequest&, NetworkSessionTaskClient&);

    NetworkDataTask* dataTaskForIdentifier(NetworkDataTask::TaskIdentifier);

    void addDownloadID(NetworkDataTask::TaskIdentifier, DownloadID);
    DownloadID downloadID(NetworkDataTask::TaskIdentifier);
    DownloadID takeDownloadID(NetworkDataTask::TaskIdentifier);
    
private:
    HashMap<NetworkDataTask::TaskIdentifier, NetworkDataTask*> m_dataTaskMap;
    HashMap<NetworkDataTask::TaskIdentifier, DownloadID> m_downloadMap;
#if PLATFORM(COCOA)
    RetainPtr<NSURLSession> m_session;
    RetainPtr<WKNetworkSessionDelegate> m_sessionDelegate;
#endif
};

} // namespace WebKit

#endif // NetworkSession_h
