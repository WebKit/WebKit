/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetworkTransportStream.h"

#import "NetworkTransportSession.h"
#import <WebCore/Exception.h>
#import <WebCore/ExceptionCode.h>
#import <WebCore/WebTransportReceiveStreamStats.h>
#import <WebCore/WebTransportSendStreamStats.h>
#import <pal/spi/cocoa/NetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#import "NetworkSoftLink.h"

namespace WebKit {

NetworkTransportStream::NetworkTransportStream(NetworkTransportSession& session, nw_connection_t connection, NetworkTransportStreamType streamType)
    : m_identifier(WebCore::WebTransportStreamIdentifier::generate())
    , m_session(session)
    , m_connection(connection)
    , m_streamType(streamType)
{
    ASSERT(m_connection);
    ASSERT(m_session);
    switch (m_streamType) {
    case NetworkTransportStreamType::Bidirectional:
        m_streamState = NetworkTransportStreamState::Ready;
        break;
    case NetworkTransportStreamType::IncomingUnidirectional:
        m_streamState = NetworkTransportStreamState::WriteClosed;
        break;
    case NetworkTransportStreamType::OutgoingUnidirectional:
        m_streamState = NetworkTransportStreamState::ReadClosed;
        break;
    }
    if (m_streamType != NetworkTransportStreamType::OutgoingUnidirectional)
        receiveLoop();
}

void NetworkTransportStream::sendBytes(std::span<const uint8_t> data, bool withFin, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    if (m_streamState == NetworkTransportStreamState::WriteClosed) {
        completionHandler(WebCore::Exception(WebCore::ExceptionCode::InvalidStateError));
        return;
    }
    nw_connection_send(m_connection.get(), makeDispatchData(Vector(data)).get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, withFin, makeBlockPtr([
        weakThis = WeakPtr { *this },
        withFin = withFin,
        bytesSent = data.size(),
        completionHandler = WTFMove(completionHandler)
    ] (nw_error_t error) mutable {
        RefPtr protectedThis = weakThis.get();
        if (error) {
            if (nw_error_get_error_domain(error) == nw_error_domain_posix && nw_error_get_error_code(error) == ECANCELED)
                completionHandler(std::nullopt);
            else
                completionHandler(WebCore::Exception(WebCore::ExceptionCode::NetworkError));
            return;
        }

        if (!protectedThis)
            return completionHandler(std::nullopt);

        protectedThis->m_bytesSent += bytesSent;

        if (withFin) {
            switch (protectedThis->m_streamState) {
            case NetworkTransportStreamState::Ready:
                protectedThis->m_streamState = NetworkTransportStreamState::WriteClosed;
                break;
            case NetworkTransportStreamState::ReadClosed:
                if (RefPtr session = protectedThis->m_session.get())
                    session->destroyStream(protectedThis->m_identifier, std::nullopt);
                break;
            case NetworkTransportStreamState::WriteClosed:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

        completionHandler(std::nullopt);
    }).get());
}

void NetworkTransportStream::receiveLoop()
{
    RELEASE_ASSERT(m_streamState != NetworkTransportStreamState::ReadClosed);
    nw_connection_receive(m_connection.get(), 0, std::numeric_limits<uint32_t>::max(), makeBlockPtr([weakThis = WeakPtr { *this }] (dispatch_data_t content, nw_content_context_t, bool withFin, nw_error_t error) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        RefPtr session = protectedThis->m_session.get();
        if (!session)
            return;
        if (error) {
            if (nw_error_get_error_domain(error) != nw_error_domain_posix || nw_error_get_error_code(error) != ECANCELED)
                session->streamReceiveError(protectedThis->m_identifier, nw_error_get_error_code(error));
            return;
        }

        ASSERT(content || withFin);

        // FIXME: Not only is this an unnecessary string copy, but it's also something that should probably be in WTF or FragmentedSharedBuffer.
        auto vectorFromData = [](dispatch_data_t content) {
            Vector<uint8_t> request;
            if (content) {
                dispatch_data_apply_span(content, [&](std::span<const uint8_t> buffer) {
                    request.append(buffer);
                    return true;
                });
            }
            return request;
        };

        auto vector = vectorFromData(content);
        protectedThis->m_bytesReceived += vector.size();
        session->streamReceiveBytes(protectedThis->m_identifier, vector.span(), withFin, std::nullopt);

        if (withFin) {
            switch (protectedThis->m_streamState) {
            case NetworkTransportStreamState::Ready:
                protectedThis->m_streamState = NetworkTransportStreamState::ReadClosed;
                break;
            case NetworkTransportStreamState::WriteClosed:
                session->destroyStream(protectedThis->m_identifier, std::nullopt);
                break;
            case NetworkTransportStreamState::ReadClosed:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else
            protectedThis->receiveLoop();
    }).get());
}

void NetworkTransportStream::cancel(std::optional<WebCore::WebTransportStreamErrorCode> errorCode)
{
    nw_connection_cancel(m_connection.get());
}

void NetworkTransportStream::cancelReceive(std::optional<WebCore::WebTransportStreamErrorCode> errorCode)
{
    switch (m_streamState) {
    case NetworkTransportStreamState::Ready: {
        m_streamState = NetworkTransportStreamState::ReadClosed;
#if HAVE(WEB_TRANSPORT)
        if (canLoad_Network_nw_connection_abort_reads())
            softLink_Network_nw_connection_abort_reads(m_connection.get(), errorCode.value_or(0));
#endif
        break;
    }
    case NetworkTransportStreamState::WriteClosed: {
#if HAVE(WEB_TRANSPORT)
        if (canLoad_Network_nw_connection_abort_reads())
            softLink_Network_nw_connection_abort_reads(m_connection.get(), errorCode.value_or(0));
#endif
        RefPtr session = m_session.get();
        if (!session)
            return;
        session->destroyStream(m_identifier, errorCode);
        break;
    }
    case NetworkTransportStreamState::ReadClosed:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void NetworkTransportStream::cancelSend(std::optional<WebCore::WebTransportStreamErrorCode> errorCode)
{
    switch (m_streamState) {
    case NetworkTransportStreamState::Ready: {
        m_streamState = NetworkTransportStreamState::WriteClosed;
#if HAVE(WEB_TRANSPORT)
        if (canLoad_Network_nw_connection_abort_writes())
            softLink_Network_nw_connection_abort_writes(m_connection.get(), errorCode.value_or(0));
#endif
        break;
    }
    case NetworkTransportStreamState::ReadClosed: {
#if HAVE(WEB_TRANSPORT)
        if (canLoad_Network_nw_connection_abort_writes())
            softLink_Network_nw_connection_abort_writes(m_connection.get(), errorCode.value_or(0));
#endif
        RefPtr session = m_session.get();
        if (!session)
            return;
        session->destroyStream(m_identifier, errorCode);
        break;
    }
    case NetworkTransportStreamState::WriteClosed:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

WebCore::WebTransportSendStreamStats NetworkTransportStream::getSendStreamStats()
{
    // FIXME: Get better data from the stream.
    return {
        m_bytesSent,
        m_bytesSent,
        m_bytesSent
    };
}

WebCore::WebTransportReceiveStreamStats NetworkTransportStream::getReceiveStreamStats()
{
    // FIXME: Get better data from the stream.
    return {
        m_bytesReceived,
        m_bytesReceived
    };
}

}
