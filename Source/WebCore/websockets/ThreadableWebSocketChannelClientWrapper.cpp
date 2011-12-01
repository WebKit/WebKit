/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include "config.h"
#if ENABLE(WEB_SOCKETS)
#include "ThreadableWebSocketChannelClientWrapper.h"

#include "CrossThreadCopier.h"
#include "CrossThreadTask.h"
#include "WebSocketChannelClient.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

ThreadableWebSocketChannelClientWrapper::ThreadableWebSocketChannelClientWrapper(WebSocketChannelClient* client)
    : m_client(client)
    , m_syncMethodDone(false)
    , m_useHixie76Protocol(true)
    , m_sendRequestResult(false)
    , m_bufferedAmount(0)
    , m_suspended(false)
{
}

PassRefPtr<ThreadableWebSocketChannelClientWrapper> ThreadableWebSocketChannelClientWrapper::create(WebSocketChannelClient* client)
{
    return adoptRef(new ThreadableWebSocketChannelClientWrapper(client));
}

void ThreadableWebSocketChannelClientWrapper::clearSyncMethodDone()
{
    m_syncMethodDone = false;
}

void ThreadableWebSocketChannelClientWrapper::setSyncMethodDone()
{
    m_syncMethodDone = true;
}

bool ThreadableWebSocketChannelClientWrapper::syncMethodDone() const
{
    return m_syncMethodDone;
}

bool ThreadableWebSocketChannelClientWrapper::useHixie76Protocol() const
{
    return m_useHixie76Protocol;
}

void ThreadableWebSocketChannelClientWrapper::setUseHixie76Protocol(bool useHixie76Protocol)
{
    m_useHixie76Protocol = useHixie76Protocol;
}

String ThreadableWebSocketChannelClientWrapper::subprotocol() const
{
    if (m_subprotocol.isEmpty())
        return String("");
    return String(m_subprotocol);
}

void ThreadableWebSocketChannelClientWrapper::setSubprotocol(const String& subprotocol)
{
    unsigned length = subprotocol.length();
    m_subprotocol.resize(length);
    if (length)
        memcpy(m_subprotocol.data(), subprotocol.characters(), sizeof(UChar) * length);
}

bool ThreadableWebSocketChannelClientWrapper::sendRequestResult() const
{
    return m_sendRequestResult;
}

void ThreadableWebSocketChannelClientWrapper::setSendRequestResult(bool sendRequestResult)
{
    m_sendRequestResult = sendRequestResult;
    m_syncMethodDone = true;
}

unsigned long ThreadableWebSocketChannelClientWrapper::bufferedAmount() const
{
    return m_bufferedAmount;
}

void ThreadableWebSocketChannelClientWrapper::setBufferedAmount(unsigned long bufferedAmount)
{
    m_bufferedAmount = bufferedAmount;
    m_syncMethodDone = true;
}

void ThreadableWebSocketChannelClientWrapper::clearClient()
{
    m_client = 0;
}

void ThreadableWebSocketChannelClientWrapper::didConnect()
{
    m_pendingTasks.append(createCallbackTask(&didConnectCallback, this));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::didReceiveMessage(const String& message)
{
    m_pendingTasks.append(createCallbackTask(&didReceiveMessageCallback, this, message));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData)
{
    m_pendingTasks.append(createCallbackTask(&didReceiveBinaryDataCallback, this, binaryData));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::didUpdateBufferedAmount(unsigned long bufferedAmount)
{
    m_pendingTasks.append(createCallbackTask(&didUpdateBufferedAmountCallback, this, bufferedAmount));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::didStartClosingHandshake()
{
    m_pendingTasks.append(createCallbackTask(&didStartClosingHandshakeCallback, this));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::didClose(unsigned long unhandledBufferedAmount, WebSocketChannelClient::ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    m_pendingTasks.append(createCallbackTask(&didCloseCallback, this, unhandledBufferedAmount, closingHandshakeCompletion, code, reason));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::suspend()
{
    m_suspended = true;
}

void ThreadableWebSocketChannelClientWrapper::resume()
{
    m_suspended = false;
    processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::processPendingTasks()
{
    ASSERT(!m_suspended);
    Vector<OwnPtr<ScriptExecutionContext::Task> > tasks;
    tasks.swap(m_pendingTasks);
    for (Vector<OwnPtr<ScriptExecutionContext::Task> >::const_iterator iter = tasks.begin(); iter != tasks.end(); ++iter)
        (*iter)->performTask(0);
}

void ThreadableWebSocketChannelClientWrapper::didConnectCallback(ScriptExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> wrapper)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didConnect();
}

void ThreadableWebSocketChannelClientWrapper::didReceiveMessageCallback(ScriptExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> wrapper, const String& message)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didReceiveMessage(message);
}

void ThreadableWebSocketChannelClientWrapper::didReceiveBinaryDataCallback(ScriptExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> wrapper, PassOwnPtr<Vector<char> > binaryData)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didReceiveBinaryData(binaryData);
}

void ThreadableWebSocketChannelClientWrapper::didUpdateBufferedAmountCallback(ScriptExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> wrapper, unsigned long bufferedAmount)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didUpdateBufferedAmount(bufferedAmount);
}

void ThreadableWebSocketChannelClientWrapper::didStartClosingHandshakeCallback(ScriptExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> wrapper)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didStartClosingHandshake();
}

void ThreadableWebSocketChannelClientWrapper::didCloseCallback(ScriptExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> wrapper, unsigned long unhandledBufferedAmount, WebSocketChannelClient::ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didClose(unhandledBufferedAmount, closingHandshakeCompletion, code, reason);
}

} // namespace WebCore

#endif
