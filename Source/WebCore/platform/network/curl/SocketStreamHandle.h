/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SocketStreamHandle_h
#define SocketStreamHandle_h

#include "SocketStreamHandleBase.h"

#if PLATFORM(WIN)
#include <winsock2.h>
#endif

#include <curl/curl.h>

#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/RefCounted.h>
#include <wtf/Threading.h>

namespace WebCore {

class AuthenticationChallenge;
class Credential;
class NetworkingContext;
class SocketStreamHandleClient;

class SocketStreamHandle : public ThreadSafeRefCounted<SocketStreamHandle>, public SocketStreamHandleBase {
public:
    static Ref<SocketStreamHandle> create(const URL& url, SocketStreamHandleClient* client, NetworkingContext&) { return adoptRef(*new SocketStreamHandle(url, client)); }

    virtual ~SocketStreamHandle();

private:
    SocketStreamHandle(const URL&, SocketStreamHandleClient*);

    int platformSend(const char* data, int length) override;
    void platformClose() override;

    bool readData(CURL*);
    bool sendData(CURL*);
    bool waitForAvailableData(CURL*, std::chrono::milliseconds selectTimeout);

    void startThread();
    void stopThread();

    void didReceiveData();
    void didOpenSocket();

    static std::unique_ptr<char[]> createCopy(const char* data, int length);

    // No authentication for streams per se, but proxy may ask for credentials.
    void didReceiveAuthenticationChallenge(const AuthenticationChallenge&);
    void receivedCredential(const AuthenticationChallenge&, const Credential&);
    void receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&);
    void receivedCancellation(const AuthenticationChallenge&);
    void receivedRequestToPerformDefaultHandling(const AuthenticationChallenge&);
    void receivedChallengeRejection(const AuthenticationChallenge&);

    struct SocketData {
        SocketData(std::unique_ptr<char[]>&& source, int length)
        {
            data = WTFMove(source);
            size = length;
        }

        SocketData(SocketData&& other)
        {
            data = WTFMove(other.data);
            size = other.size;
            other.size = 0;
        }

        std::unique_ptr<char[]> data;
        int size { 0 };
    };

    ThreadIdentifier m_workerThread { 0 };
    std::atomic<bool> m_stopThread { false };
    Lock m_mutexSend;
    Lock m_mutexReceive;
    Deque<SocketData> m_sendData;
    Deque<SocketData> m_receiveData;
};

} // namespace WebCore

#endif // SocketStreamHandle_h
