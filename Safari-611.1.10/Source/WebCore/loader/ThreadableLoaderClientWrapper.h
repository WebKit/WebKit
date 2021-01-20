/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#pragma once

#include "ThreadableLoaderClient.h"
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class ThreadableLoaderClientWrapper : public ThreadSafeRefCounted<ThreadableLoaderClientWrapper> {
public:
    static Ref<ThreadableLoaderClientWrapper> create(ThreadableLoaderClient& client, const String& initiator)
    {
        return adoptRef(*new ThreadableLoaderClientWrapper(client, initiator));
    }

    void clearClient()
    {
        m_done = true;
        m_client = nullptr;
    }

    bool done() const
    {
        return m_done;
    }

    void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
    {
        if (m_client)
            m_client->didSendData(bytesSent, totalBytesToBeSent);
    }

    void didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
    {
        if (m_client)
            m_client->didReceiveResponse(identifier, response);
    }

    void didReceiveData(const char* data, int dataLength)
    {
        if (m_client)
            m_client->didReceiveData(data, dataLength);
    }

    void didFinishLoading(unsigned long identifier)
    {
        m_done = true;
        if (m_client)
            m_client->didFinishLoading(identifier);
    }

    void notifyIsDone(bool isDone)
    {
        if (m_client)
            m_client->notifyIsDone(isDone);
    }

    void didFail(const ResourceError& error)
    {
        m_done = true;
        if (m_client)
            m_client->didFail(error);
    }

    void didReceiveAuthenticationCancellation(unsigned long identifier, const ResourceResponse& response)
    {
        if (m_client)
            m_client->didReceiveResponse(identifier, response);
    }

    const String& initiator() const { return m_initiator; }

protected:
    explicit ThreadableLoaderClientWrapper(ThreadableLoaderClient&, const String&);

    ThreadableLoaderClient* m_client;
    String m_initiator;
    bool m_done { false };
};

inline ThreadableLoaderClientWrapper::ThreadableLoaderClientWrapper(ThreadableLoaderClient& client, const String& initiator)
    : m_client(&client)
    , m_initiator(initiator.isolatedCopy())
{
}

} // namespace WebCore
