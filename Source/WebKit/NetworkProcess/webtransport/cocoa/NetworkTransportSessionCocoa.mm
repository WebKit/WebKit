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
#import "NetworkTransportSession.h"

#import "NetworkConnectionToWebProcess.h"
#import "NetworkTransportBidirectionalStream.h"
#import "NetworkTransportReceiveStream.h"
#import "NetworkTransportSendStream.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/SpanCocoa.h>

namespace WebKit {

NetworkTransportSession::NetworkTransportSession(NetworkConnectionToWebProcess& connection, nw_connection_group_t connectionGroup, nw_endpoint_t endpoint)
    : m_connectionToWebProcess(connection)
    , m_connectionGroup(connectionGroup)
    , m_endpoint(endpoint)
{
    constexpr uint32_t maximumMessageSize { std::numeric_limits<uint32_t>::max() };
    constexpr bool rejectOversizedMessages { false };
    nw_connection_group_set_receive_handler(connectionGroup, maximumMessageSize, rejectOversizedMessages, makeBlockPtr([weakThis = WeakPtr { *this }] (dispatch_data_t datagram, nw_content_context_t, bool) {
        RefPtr strongThis = weakThis.get();
        if (!strongThis)
            return;

        // FIXME: Not only is this an unnecessary string copy, but it's also something that should probably be in WTF or FragmentedSharedBuffer.
        auto vectorFromData = [](dispatch_data_t content) {
            ASSERT(content);
            Vector<uint8_t> request;
            dispatch_data_apply_span(content, [&](std::span<const uint8_t> data) {
                request.append(data);
                return true;
            });
            return request;
        };

        strongThis->receiveDatagram(vectorFromData(datagram).span());
    }).get());

    // FIXME: Use nw_connection_group_set_new_connection_handler to receive incoming connections.
}

void NetworkTransportSession::initialize(NetworkConnectionToWebProcess& connectionToWebProcess, URL&& url, CompletionHandler<void(RefPtr<NetworkTransportSession>&&)>&& completionHandler)
{
    // FIXME: Use nw_endpoint_create_url to support all URLs on systems where rdar://135036170 is fixed.
    auto port = url.port() ? url.port() : defaultPortForProtocol(url.protocol());
    if (!port)
        return completionHandler(nullptr);

    RetainPtr endpoint = adoptNS(nw_endpoint_create_host(url.host().utf8().data(), makeString(*port).utf8().data()));
    if (!endpoint) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    RetainPtr descriptor = adoptNS(nw_group_descriptor_create_multiplex(endpoint.get()));
    if (!descriptor) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    auto configureTLS = [](nw_protocol_options_t options) {
        RetainPtr securityOptions = adoptNS(nw_quic_connection_copy_sec_protocol_options(options));
        sec_protocol_options_set_peer_authentication_required(securityOptions.get(), true);
        sec_protocol_options_set_verify_block(securityOptions.get(), makeBlockPtr([](sec_protocol_metadata_t metadata, sec_trust_t trust, sec_protocol_verify_complete_t completion) {
            // FIXME: Hook this up with WKNavigationDelegate.didReceiveChallenge.
            completion(true);
        }).get(), dispatch_get_main_queue());
        // FIXME: Pipe client cert auth into this too, probably.
        sec_protocol_options_add_tls_application_protocol(securityOptions.get(), "h3");
    };

    RetainPtr parameters = adoptNS(nw_parameters_create_quic_stream(NW_PARAMETERS_DEFAULT_CONFIGURATION, configureTLS));

    if (!parameters) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    RetainPtr connectionGroup = adoptNS(nw_connection_group_create(descriptor.get(), parameters.get()));
    if (!connectionGroup) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    Ref networkTransportSession = NetworkTransportSession::create(connectionToWebProcess, connectionGroup.get(), endpoint.get());
    WeakPtr weakNetworkTransportSession { networkTransportSession };
    auto creationCompletionHandler = [completionHandler = WTFMove(completionHandler)] (RefPtr<NetworkTransportSession>&& session) mutable {
        if (completionHandler)
            completionHandler(WTFMove(session));
    };
    nw_connection_group_set_state_changed_handler(connectionGroup.get(), makeBlockPtr([
        networkTransportSession = WTFMove(networkTransportSession),
        weakNetworkTransportSession = WTFMove(weakNetworkTransportSession),
        creationCompletionHandler = WTFMove(creationCompletionHandler)
    ] (nw_connection_group_state_t state, nw_error_t error) mutable {
        if (error)
            return creationCompletionHandler(nullptr);
        switch (state) {
        case nw_connection_group_state_invalid:
            return creationCompletionHandler(nullptr);
        case nw_connection_group_state_waiting:
            return; // We will get another callback with another state change.
        case nw_connection_group_state_ready:
            return creationCompletionHandler(WTFMove(networkTransportSession));
        case nw_connection_group_state_failed:
        case nw_connection_group_state_cancelled:
            // FIXME: Use weakNetworkTransportSession to pipe the failure to JS.
            return creationCompletionHandler(nullptr);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }).get());

    nw_connection_group_set_queue(connectionGroup.get(), dispatch_get_main_queue());
    nw_connection_group_start(connectionGroup.get());
}

void NetworkTransportSession::sendDatagram(std::span<const uint8_t> data, CompletionHandler<void()>&& completionHandler)
{
    // FIXME: This exists in several places. Make a common place in WTF for it.
    auto dataFromVector = [] (Vector<uint8_t>&& v) {
        auto bufferSize = v.size();
        auto rawPointer = v.releaseBuffer().leakPtr();
        return adoptNS(dispatch_data_create(rawPointer, bufferSize, dispatch_get_main_queue(), ^{
            fastFree(rawPointer);
        }));
    };

    nw_connection_group_send_message(m_connectionGroup.get(), dataFromVector(Vector(data)).get(),  m_endpoint.get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, makeBlockPtr([completionHandler = WTFMove(completionHandler)] (nw_error_t error) mutable {
        // FIXME: Pass any error through to JS.
        completionHandler();
    }).get());
}

void NetworkTransportSession::createBidirectionalStream(CompletionHandler<void(std::optional<WebTransportStreamIdentifier>)>&& completionHandler)
{
    RetainPtr connection = adoptNS(nw_connection_group_extract_connection(m_connectionGroup.get(), m_endpoint.get(), nullptr));
    if (!connection) {
        ASSERT_NOT_REACHED();
        return completionHandler(std::nullopt);
    }

    auto creationCompletionHandler = [
        weakThis = WeakPtr { *this },
        completionHandler = WTFMove(completionHandler)
    ] (RefPtr<NetworkTransportBidirectionalStream>&& stream) mutable {
        if (!completionHandler)
            return;
        RefPtr strongThis = weakThis.get();
        if (!strongThis || !stream)
            return completionHandler(std::nullopt);
        auto identifier = stream->identifier();
        ASSERT(!strongThis->m_bidirectionalStreams.contains(identifier));
        strongThis->m_bidirectionalStreams.set(identifier, stream.releaseNonNull());
        completionHandler(identifier);
    };

    Ref stream = NetworkTransportBidirectionalStream::create(*this, connection.get());

    nw_connection_set_state_changed_handler(connection.get(), makeBlockPtr([
        creationCompletionHandler = WTFMove(creationCompletionHandler),
        stream = WTFMove(stream)
    ] (nw_connection_state_t state, nw_error_t error) mutable {
        if (error)
            return creationCompletionHandler(nullptr);
        switch (state) {
        case nw_connection_state_invalid:
            return creationCompletionHandler(nullptr);
        case nw_connection_state_waiting:
        case nw_connection_state_preparing:
            return; // We will get another callback with another state change.
        case nw_connection_state_ready:
            return creationCompletionHandler(WTFMove(stream));
        case nw_connection_state_failed:
        case nw_connection_state_cancelled:
            return creationCompletionHandler(nullptr);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }).get());
    nw_connection_set_queue(connection.get(), dispatch_get_main_queue());
    nw_connection_start(connection.get());
}

void NetworkTransportSession::createOutgoingUnidirectionalStream(CompletionHandler<void(std::optional<WebTransportStreamIdentifier>)>&& completionHandler)
{
    // FIXME: Call nw_connection_group_extract_connection and make a NetworkTransportSendStream and add to m_sendStreams like NetworkTransportSession::createBidirectionalStream.
    completionHandler(std::nullopt);
}

}
