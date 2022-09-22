/*
 * Copyright (C) 2021 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RTCDataChannelRemoteManager.h"

#if ENABLE(WEB_RTC)

#include "Connection.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "RTCDataChannelRemoteManagerMessages.h"
#include "RTCDataChannelRemoteManagerProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/RTCDataChannel.h>
#include <WebCore/RTCError.h>
#include <WebCore/ScriptExecutionContext.h>

namespace WebKit {

RTCDataChannelRemoteManager& RTCDataChannelRemoteManager::sharedManager()
{
    static RTCDataChannelRemoteManager* sharedManager = [] {
        auto instance = new RTCDataChannelRemoteManager;
        instance->initialize();
        return instance;
    }();
    return *sharedManager;
}

RTCDataChannelRemoteManager::RTCDataChannelRemoteManager()
    : m_queue(WorkQueue::create("RTCDataChannelRemoteManager"))
    , m_connection(&WebProcess::singleton().ensureNetworkProcessConnection().connection())
{
}

void RTCDataChannelRemoteManager::initialize()
{
    // FIXME: If the network process crashes, all RTC data will be misdelivered for the web process.
    // https://bugs.webkit.org/show_bug.cgi?id=245062
    m_connection->addMessageReceiver(m_queue, *this, Messages::RTCDataChannelRemoteManager::messageReceiverName());
}

bool RTCDataChannelRemoteManager::connectToRemoteSource(WebCore::RTCDataChannelIdentifier localIdentifier, WebCore::RTCDataChannelIdentifier remoteIdentifier)
{
    ASSERT(WebCore::Process::identifier() == localIdentifier.processIdentifier);
    if (WebCore::Process::identifier() != localIdentifier.processIdentifier)
        return false;

    auto handler = WebCore::RTCDataChannel::handlerFromIdentifier(localIdentifier.channelIdentifier);
    if (!handler)
        return false;

    auto iterator = m_sources.add(remoteIdentifier.channelIdentifier, makeUniqueRef<WebCore::RTCDataChannelRemoteSource>(remoteIdentifier, makeUniqueRefFromNonNullUniquePtr(WTFMove(handler)), remoteSourceConnection()));
    return iterator.isNewEntry;
}

WebCore::RTCDataChannelRemoteHandlerConnection& RTCDataChannelRemoteManager::remoteHandlerConnection()
{
    if (!m_remoteHandlerConnection)
        m_remoteHandlerConnection = RemoteHandlerConnection::create(m_queue.copyRef());
    return *m_remoteHandlerConnection;
}

WebCore::RTCDataChannelRemoteSourceConnection& RTCDataChannelRemoteManager::remoteSourceConnection()
{
    if (!m_remoteSourceConnection)
        m_remoteSourceConnection = RemoteSourceConnection::create();
    return *m_remoteSourceConnection;
}

void RTCDataChannelRemoteManager::postTaskToHandler(WebCore::RTCDataChannelIdentifier handlerIdentifier, Function<void(WebCore::RTCDataChannelRemoteHandler&)>&& function)
{
    ASSERT(WebCore::Process::identifier() == handlerIdentifier.processIdentifier);
    if (WebCore::Process::identifier() != handlerIdentifier.processIdentifier)
        return;

    auto iterator = m_handlers.find(handlerIdentifier.channelIdentifier);
    if (iterator == m_handlers.end())
        return;
    auto& remoteHandler = iterator->value;

    WebCore::ScriptExecutionContext::postTaskTo(remoteHandler.contextIdentifier, [handler = remoteHandler.handler, function = WTFMove(function)](auto&) mutable {
        if (handler)
            function(*handler);
    });
}

WebCore::RTCDataChannelRemoteSource* RTCDataChannelRemoteManager::sourceFromIdentifier(WebCore::RTCDataChannelIdentifier sourceIdentifier)
{
    ASSERT(WebCore::Process::identifier() == sourceIdentifier.processIdentifier);
    if (WebCore::Process::identifier() != sourceIdentifier.processIdentifier)
        return nullptr;

    return m_sources.get(sourceIdentifier.channelIdentifier);
}

void RTCDataChannelRemoteManager::sendData(WebCore::RTCDataChannelIdentifier sourceIdentifier, bool isRaw, const IPC::DataReference& data)
{
    if (auto* source = sourceFromIdentifier(sourceIdentifier)) {
        if (isRaw)
            source->sendRawData(data.data(), data.size());
        else
            source->sendStringData(CString(data.data(), data.size()));
    }
}

void RTCDataChannelRemoteManager::close(WebCore::RTCDataChannelIdentifier sourceIdentifier)
{
    if (auto* source = sourceFromIdentifier(sourceIdentifier))
        source->close();
}

void RTCDataChannelRemoteManager::changeReadyState(WebCore::RTCDataChannelIdentifier handlerIdentifier, WebCore::RTCDataChannelState state)
{
    postTaskToHandler(handlerIdentifier, [state](auto& handler) {
        handler.didChangeReadyState(state);
    });
}

void RTCDataChannelRemoteManager::receiveData(WebCore::RTCDataChannelIdentifier handlerIdentifier, bool isRaw, const IPC::DataReference& data)
{
    Vector<uint8_t> buffer;
    String text;
    if (isRaw)
        buffer = Vector(data);
    else
        text = String::fromUTF8(data.data(), data.size());

    postTaskToHandler(handlerIdentifier, [isRaw, text = WTFMove(text).isolatedCopy(), buffer = WTFMove(buffer)](auto& handler) mutable {
        if (isRaw)
            handler.didReceiveRawData(buffer.data(), buffer.size());
        else
            handler.didReceiveStringData(WTFMove(text));
    });
}

void RTCDataChannelRemoteManager::detectError(WebCore::RTCDataChannelIdentifier handlerIdentifier, WebCore::RTCErrorDetailType detail, String&& message)
{
    postTaskToHandler(handlerIdentifier, [detail, message = WTFMove(message)](auto& handler) mutable {
        handler.didDetectError(WebCore::RTCError::create(detail, WTFMove(message)));
    });
}

void RTCDataChannelRemoteManager::bufferedAmountIsDecreasing(WebCore::RTCDataChannelIdentifier handlerIdentifier, size_t amount)
{
    postTaskToHandler(handlerIdentifier, [amount](auto& handler) {
        handler.bufferedAmountIsDecreasing(amount);
    });
}

Ref<RTCDataChannelRemoteManager::RemoteHandlerConnection> RTCDataChannelRemoteManager::RemoteHandlerConnection::create(Ref<WorkQueue>&& queue)
{
    return adoptRef(*new RemoteHandlerConnection(WTFMove(queue)));
}

RTCDataChannelRemoteManager::RemoteHandlerConnection::RemoteHandlerConnection(Ref<WorkQueue>&& queue)
    : m_connection(WebProcess::singleton().ensureNetworkProcessConnection().connection())
    , m_queue(WTFMove(queue))
{
}

void RTCDataChannelRemoteManager::RemoteHandlerConnection::connectToSource(WebCore::RTCDataChannelRemoteHandler& handler, WebCore::ScriptExecutionContextIdentifier contextIdentifier, WebCore::RTCDataChannelIdentifier localIdentifier, WebCore::RTCDataChannelIdentifier remoteIdentifier)
{
    m_queue->dispatch([handler = WeakPtr { handler }, contextIdentifier, localIdentifier]() mutable {
        RTCDataChannelRemoteManager::sharedManager().m_handlers.add(localIdentifier.channelIdentifier, RemoteHandler { WTFMove(handler), contextIdentifier });
    });
    m_connection->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::ConnectToRTCDataChannelRemoteSource { localIdentifier, remoteIdentifier }, [localIdentifier](auto&& result) {
        RTCDataChannelRemoteManager::sharedManager().postTaskToHandler(localIdentifier, [result](auto& handler) {
            if (!result || !*result) {
                handler.didDetectError(WebCore::RTCError::create(WebCore::RTCErrorDetailType::DataChannelFailure, "Unable to find data channel"_s));
                return;
            }
            handler.readyToSend();
        });
    }, 0);
}

void RTCDataChannelRemoteManager::RemoteHandlerConnection::sendData(WebCore::RTCDataChannelIdentifier identifier, bool isRaw, const unsigned char* data, size_t size)
{
    m_connection->send(Messages::RTCDataChannelRemoteManagerProxy::SendData { identifier, isRaw, IPC::DataReference { data, size } }, 0);
}

void RTCDataChannelRemoteManager::RemoteHandlerConnection::close(WebCore::RTCDataChannelIdentifier identifier)
{
    // FIXME: We need to wait to send this message until RTCDataChannelRemoteManagerProxy::ConnectToSource is actually sent.
    m_connection->send(Messages::RTCDataChannelRemoteManagerProxy::Close { identifier }, 0);
}

Ref<RTCDataChannelRemoteManager::RemoteSourceConnection> RTCDataChannelRemoteManager::RemoteSourceConnection::create()
{
    return adoptRef(*new RemoteSourceConnection);
}

RTCDataChannelRemoteManager::RemoteSourceConnection::RemoteSourceConnection()
    : m_connection(WebProcess::singleton().ensureNetworkProcessConnection().connection())
{
}

void RTCDataChannelRemoteManager::RemoteSourceConnection::didChangeReadyState(WebCore::RTCDataChannelIdentifier identifier, WebCore::RTCDataChannelState state)
{
    m_connection->send(Messages::RTCDataChannelRemoteManagerProxy::ChangeReadyState { identifier, state }, 0);
}

void RTCDataChannelRemoteManager::RemoteSourceConnection::didReceiveStringData(WebCore::RTCDataChannelIdentifier identifier, const String& string)
{
    auto text = string.utf8();
    m_connection->send(Messages::RTCDataChannelRemoteManagerProxy::ReceiveData { identifier, false, IPC::DataReference { text.dataAsUInt8Ptr(), text.length() } }, 0);
}

void RTCDataChannelRemoteManager::RemoteSourceConnection::didReceiveRawData(WebCore::RTCDataChannelIdentifier identifier, const uint8_t* data, size_t size)
{
    m_connection->send(Messages::RTCDataChannelRemoteManagerProxy::ReceiveData { identifier, true, IPC::DataReference { data, size  } }, 0);
}

void RTCDataChannelRemoteManager::RemoteSourceConnection::didDetectError(WebCore::RTCDataChannelIdentifier identifier, WebCore::RTCErrorDetailType type, const String& message)
{
    m_connection->send(Messages::RTCDataChannelRemoteManagerProxy::DetectError { identifier, type, message }, 0);
}

void RTCDataChannelRemoteManager::RemoteSourceConnection::bufferedAmountIsDecreasing(WebCore::RTCDataChannelIdentifier identifier, size_t amount)
{
    m_connection->send(Messages::RTCDataChannelRemoteManagerProxy::BufferedAmountIsDecreasing { identifier, amount }, 0);
}

}

#endif
