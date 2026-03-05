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

NetworkTransportStream::NetworkTransportStream(NetworkTransportSession& session, nw_connection_t connection)
    : m_identifier(WebCore::WebTransportStreamIdentifier::generate())
    , m_session(session)
    , m_connection(connection)
{
    ASSERT(m_connection);
    ASSERT(m_session);
}

void NetworkTransportStream::start(NetworkTransportStreamReadyHandler&& readyHandler)
{
    nw_connection_set_state_changed_handler(m_connection.get(), makeBlockPtr([
        weakThis = WeakPtr { *this },
        readyHandler = WTF::move(readyHandler)
    ] (nw_connection_state_t state, nw_error_t error) mutable {
        RefPtr protectedThis = weakThis.get();
        switch (state) {
        case nw_connection_state_invalid:
        case nw_connection_state_waiting:
        case nw_connection_state_preparing:
        case nw_connection_state_cancelled:
            return;
        case nw_connection_state_ready: {
            if (!protectedThis)
                return readyHandler(std::nullopt);
            protectedThis->initializeReadyConnection();
            readyHandler(protectedThis->m_streamType);
            // Stream should be created and accounted for before we start receiving data on it.
            protectedThis->receiveLoop();
            return;
        }
        case nw_connection_state_failed: {
            if (readyHandler)
                return readyHandler(std::nullopt);
            if (!protectedThis)
                return;
            RefPtr session = protectedThis->m_session.get();
            if (!session)
                return;

            if (!session->isSessionClosed()) {
                const auto defaultErrorCode = 0;
                switch (protectedThis->m_streamState) {
                case NetworkTransportStreamState::Ready: {
                    session->streamSendError(protectedThis->m_identifier, defaultErrorCode);
                    session->streamReceiveError(protectedThis->m_identifier, defaultErrorCode);
                    break;
                }
                case NetworkTransportStreamState::ReadClosed: {
                    session->streamSendError(protectedThis->m_identifier, defaultErrorCode);
                    break;
                }
                case NetworkTransportStreamState::WriteClosed: {
                    session->streamReceiveError(protectedThis->m_identifier, defaultErrorCode);
                    break;
                }
                case NetworkTransportStreamState::Complete:
                    break;
                }
            }
            protectedThis->m_streamState = NetworkTransportStreamState::Complete;
            session->destroyStream(protectedThis->m_identifier, std::nullopt);
            return;
        }
        }
    }).get());
    nw_connection_set_queue(m_connection.get(), mainDispatchQueueSingleton());
    nw_connection_start(m_connection.get());
}

void NetworkTransportStream::initializeReadyConnection()
{
#if HAVE(WEB_TRANSPORT)
    RetainPtr metadata = adoptNS(nw_connection_copy_protocol_metadata(m_connection.get(), adoptNS(nw_protocol_copy_webtransport_definition()).get()));

    bool isPeerInitiated = nw_webtransport_metadata_get_is_peer_initiated(metadata.get());
    bool isUnidirectional = nw_webtransport_metadata_get_is_unidirectional(metadata.get());

    if (!isUnidirectional)
        m_streamType = NetworkTransportStreamType::Bidirectional;
    else if (isPeerInitiated)
        m_streamType = NetworkTransportStreamType::IncomingUnidirectional;
    else
        m_streamType = NetworkTransportStreamType::OutgoingUnidirectional;

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

    if (m_streamType != NetworkTransportStreamType::IncomingUnidirectional && canLoad_Network_nw_webtransport_metadata_set_remote_receive_error_handler()) {
        softLink_Network_nw_webtransport_metadata_set_remote_receive_error_handler(metadata.get(), makeBlockPtr([weakThis = WeakPtr { *this }] (uint64_t errorCode) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            RefPtr session = protectedThis->m_session.get();
            if (!session)
                return;

            switch (protectedThis->m_streamState) {
            case NetworkTransportStreamState::Ready:
                protectedThis->m_streamState = NetworkTransportStreamState::ReadClosed;
                break;
            case NetworkTransportStreamState::WriteClosed:
                protectedThis->m_streamState = NetworkTransportStreamState::Complete;
                break;
            case NetworkTransportStreamState::ReadClosed:
            case NetworkTransportStreamState::Complete:
                break;
            }
            if (errorCode == webTransportSessionGoneErrorCode || session->isSessionClosed())
                return;

            session->streamReceiveError(protectedThis->m_identifier, errorCode);
        }).get(), mainDispatchQueueSingleton());
    }

    if (m_streamType != NetworkTransportStreamType::OutgoingUnidirectional && canLoad_Network_nw_webtransport_metadata_set_remote_send_error_handler()) {
        softLink_Network_nw_webtransport_metadata_set_remote_send_error_handler(metadata.get(), makeBlockPtr([weakThis = WeakPtr { *this }] (uint64_t errorCode) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            RefPtr session = protectedThis->m_session.get();
            if (!session)
                return;

            switch (protectedThis->m_streamState) {
            case NetworkTransportStreamState::Ready:
                protectedThis->m_streamState = NetworkTransportStreamState::WriteClosed;
                break;
            case NetworkTransportStreamState::ReadClosed:
                protectedThis->m_streamState = NetworkTransportStreamState::Complete;
                break;
            case NetworkTransportStreamState::WriteClosed:
            case NetworkTransportStreamState::Complete:
                break;
            }
            if (errorCode == webTransportSessionGoneErrorCode || session->isSessionClosed())
                return;

            session->streamSendError(protectedThis->m_identifier, errorCode);
        }).get(), mainDispatchQueueSingleton());
    }
#endif
}

void NetworkTransportStream::sendBytes(std::span<const uint8_t> data, bool withFin, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    if (m_streamState == NetworkTransportStreamState::WriteClosed || m_streamState == NetworkTransportStreamState::Complete) {
        completionHandler(WebCore::Exception(WebCore::ExceptionCode::InvalidStateError));
        return;
    }
    nw_connection_send(m_connection.get(), makeDispatchData(Vector(data)).get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, withFin, makeBlockPtr([
        weakThis = WeakPtr { *this },
        withFin = withFin,
        bytesSent = data.size(),
        completionHandler = WTF::move(completionHandler)
    ] (nw_error_t error) mutable {
        // Send stream is errored only on incoming STOP_SENDING or session error.
        if (error)
            return completionHandler(std::nullopt);

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler(std::nullopt);

        protectedThis->m_bytesSent += bytesSent;

        if (withFin) {
            switch (protectedThis->m_streamState) {
            case NetworkTransportStreamState::Ready:
                protectedThis->m_streamState = NetworkTransportStreamState::WriteClosed;
                break;
            case NetworkTransportStreamState::ReadClosed:
                protectedThis->m_streamState = NetworkTransportStreamState::Complete;
                break;
            case NetworkTransportStreamState::WriteClosed:
            case NetworkTransportStreamState::Complete:
                break;
            }
        }

        completionHandler(std::nullopt);
    }).get());
}

void NetworkTransportStream::receiveLoop()
{
    if (m_streamState == NetworkTransportStreamState::ReadClosed || m_streamState == NetworkTransportStreamState::Complete)
        return;

    nw_connection_receive(m_connection.get(), 0, std::numeric_limits<uint32_t>::max(), makeBlockPtr([weakThis = WeakPtr { *this }] (dispatch_data_t content, nw_content_context_t, bool withFin, nw_error_t error) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        RefPtr session = protectedThis->m_session.get();
        if (!session)
            return;
        if (error) {
            // Receive stream is errored only on incoming RESET_STREAM_AT or session error.
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
                protectedThis->m_streamState = NetworkTransportStreamState::Complete;
                break;
            case NetworkTransportStreamState::ReadClosed:
            case NetworkTransportStreamState::Complete:
                break;
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
        m_streamState = NetworkTransportStreamState::Complete;
#if HAVE(WEB_TRANSPORT)
        if (canLoad_Network_nw_connection_abort_reads())
            softLink_Network_nw_connection_abort_reads(m_connection.get(), errorCode.value_or(0));
#endif
        break;
    }
    case NetworkTransportStreamState::ReadClosed:
    case NetworkTransportStreamState::Complete:
        break;
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
        m_streamState = NetworkTransportStreamState::Complete;
#if HAVE(WEB_TRANSPORT)
        if (canLoad_Network_nw_connection_abort_writes())
            softLink_Network_nw_connection_abort_writes(m_connection.get(), errorCode.value_or(0));
#endif
        break;
    }
    case NetworkTransportStreamState::WriteClosed:
    case NetworkTransportStreamState::Complete:
        break;
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
