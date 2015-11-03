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

OBJC_CLASS NSURLSession;
OBJC_CLASS NSURLSessionDataTask;
OBJC_CLASS NSOperationQueue;
OBJC_CLASS NetworkSessionDelegate;

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

enum class ResponseDisposition {
    Cancel,
    Allow,
    BecomeDownload
};

enum class AuthenticationChallengeDisposition {
    UseCredential,
    PerformDefaultHandling,
    Cancel,
    RejectProtectionSpace
};

class NetworkSession;

class NetworkSessionTaskClient {
public:
    virtual void willPerformHTTPRedirection(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, std::function<void(const WebCore::ResourceRequest&)>) = 0;
    virtual void didReceiveChallenge(const WebCore::AuthenticationChallenge&, std::function<void(AuthenticationChallengeDisposition, const WebCore::Credential&)>) = 0;
    virtual void didReceiveResponse(const WebCore::ResourceResponse&, std::function<void(ResponseDisposition)>) = 0;
    virtual void didReceiveData(RefPtr<WebCore::SharedBuffer>&&) = 0;
    virtual void didCompleteWithError(const WebCore::ResourceError&) = 0;

    virtual ~NetworkSessionTaskClient() { }
};

class NetworkingDataTask : public RefCounted<NetworkingDataTask> {
    friend class NetworkSession;
public:
    void suspend();
    void resume();

    uint64_t taskIdentifier();

    ~NetworkingDataTask();

    NetworkSessionTaskClient* client() { return m_client; }
    void clearClient() { m_client = nullptr; }

private:
    explicit NetworkingDataTask(NetworkSession&, NetworkSessionTaskClient&, RetainPtr<NSURLSessionDataTask>);

    NetworkSession& m_session;
    RetainPtr<NSURLSessionDataTask> m_task;
    NetworkSessionTaskClient* m_client;
};

class NetworkSession : public RefCounted<NetworkSession> {
    friend class NetworkingDataTask;
public:
    enum class Type {
        Normal,
        Ephemeral
    };

    Ref<NetworkingDataTask> createDataTaskWithRequest(const WebCore::ResourceRequest&, NetworkSessionTaskClient&);

    static Ref<NetworkSession> singleton(); // FIXME: This shouldn't actually be a singleton.
    NetworkingDataTask* dataTaskForIdentifier(uint64_t);

    ~NetworkSession() { ASSERT(m_dataTaskMap.isEmpty()); }
private:
    static Ref<NetworkSession> create(Type);

    NetworkSession(Type);
    HashMap<uint64_t, NetworkingDataTask*> m_dataTaskMap;
    RetainPtr<NSURLSession> m_session;
    RetainPtr<NetworkSessionDelegate> m_sessionDelegate;
};

} // namespace WebKit

#endif // NetworkSession_h
