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

#import "NetworkTransportBidirectionalStream.h"
#import "NetworkTransportReceiveStream.h"
#import "NetworkTransportSendStream.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/RetainPtr.h>

// FIXME: This is only needed for the internal Ventura build. Remove when appropriate.
extern "C" {
nw_protocol_options_t nw_http2_create_options();
}

namespace WebKit {

NetworkTransportSession::NetworkTransportSession(NetworkConnectionToWebProcess& connection, nw_connection_group_t connectionGroup, nw_endpoint_t endpoint)
    : m_connectionToWebProcess(connection)
    , m_connectionGroup(connectionGroup)
    , m_endpoint(endpoint)
{
    constexpr uint32_t maximumMessageSize { std::numeric_limits<uint32_t>::max() };
    constexpr bool rejectOversizedMessages { false };
    nw_connection_group_set_receive_handler(connectionGroup, maximumMessageSize, rejectOversizedMessages, makeBlockPtr([weakThis = WeakPtr { *this }] (dispatch_data_t, nw_content_context_t, bool) {
        // FIXME: Implement.
    }).get());

    // FIXME: Use nw_connection_group_set_new_connection_handler to receive incoming connections.
}

void NetworkTransportSession::initialize(NetworkConnectionToWebProcess& connectionToWebProcess, URL&& url, CompletionHandler<void(std::unique_ptr<NetworkTransportSession>&&)>&& completionHandler)
{
    RetainPtr endpoint = adoptNS(nw_endpoint_create_url(url.string().utf8().data()));
    if (!endpoint) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    RetainPtr descriptor = adoptNS(nw_group_descriptor_create_multiplex(endpoint.get()));
    if (!descriptor) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    RetainPtr parameters = adoptNS(nw_parameters_create_secure_tcp(^(nw_protocol_options_t options) {
        RetainPtr securityOptions = adoptNS(nw_tls_copy_sec_protocol_options(options));
        sec_protocol_options_set_peer_authentication_required(securityOptions.get(), true);
        sec_protocol_options_set_verify_block(securityOptions.get(), makeBlockPtr([](sec_protocol_metadata_t metadata, sec_trust_t trust, sec_protocol_verify_complete_t completion) {
            // FIXME: Hook this up with WKNavigationDelegate.didReceiveChallenge.
            completion(true);
        }).get(), dispatch_get_main_queue());
        // FIXME: Pipe client cert auth into this too, probably.

        sec_protocol_options_add_tls_application_protocol(securityOptions.get(), "h2");
    }, NW_PARAMETERS_DEFAULT_CONFIGURATION));

    if (!parameters) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    RetainPtr stack = adoptNS(nw_parameters_copy_default_protocol_stack(parameters.get()));
    nw_protocol_stack_prepend_application_protocol(stack.get(), adoptNS(nw_http2_create_options()).get());

    RetainPtr connectionGroup = adoptNS(nw_connection_group_create(descriptor.get(), parameters.get()));
    if (!connectionGroup) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    auto networkTransportSession = makeUnique<NetworkTransportSession>(connectionToWebProcess, connectionGroup.get(), endpoint.get());
    WeakPtr weakNetworkTransportSession { *networkTransportSession };
    auto creationCompletionHandler = [completionHandler = WTFMove(completionHandler)] (std::unique_ptr<NetworkTransportSession>&& session) mutable {
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

    nw_connection_group_send_message(m_connectionGroup.get(), dataFromVector(Vector(data)).get(),  m_endpoint.get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, makeBlockPtr([completionHandler = WTFMove(completionHandler)] (nw_error_t) mutable {
        // FIXME: Pass any error through to JS.
        completionHandler();
    }).get());
}

}
