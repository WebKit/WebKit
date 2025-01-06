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

#import "AuthenticationManager.h"
#import "NetworkConnectionToWebProcess.h"
#import "NetworkTransportStream.h"
#import <Security/Security.h>
#import <WebCore/AuthenticationChallenge.h>
#import <pal/spi/cocoa/NetworkSPI.h>
#import <pal/cocoa/NetworkSoftLink.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {

NetworkTransportSession::NetworkTransportSession(NetworkConnectionToWebProcess& connection, nw_connection_group_t connectionGroup, nw_endpoint_t endpoint)
    : m_connectionToWebProcess(connection)
    , m_connectionGroup(connectionGroup)
    , m_endpoint(endpoint)
{
    setupConnectionHandler();
}

#if HAVE(WEB_TRANSPORT)

static void didReceiveServerTrustChallenge(Ref<NetworkConnectionToWebProcess>&& connectionToWebProcess, URL&& url, WebKit::WebPageProxyIdentifier&& pageID, WebCore::ClientOrigin&& clientOrigin, sec_trust_t trust, sec_protocol_verify_complete_t completion)
{
    uint16_t port = url.port() ? *url.port() : *defaultPortForProtocol(url.protocol());
    RetainPtr secTrust = adoptCF(sec_trust_copy_ref(trust));
    RetainPtr protectionSpace = adoptNS([[NSURLProtectionSpace alloc] initWithHost:url.host().createNSString().get() port:port protocol:NSURLProtectionSpaceHTTPS realm:nil authenticationMethod:NSURLAuthenticationMethodServerTrust]);
    [protectionSpace _setServerTrust:secTrust.get()];

    id<NSURLAuthenticationChallengeSender> sender = nil;
    RetainPtr challenge = adoptNS([[NSURLAuthenticationChallenge alloc] initWithProtectionSpace:protectionSpace.get() proposedCredential:nil previousFailureCount:0 failureResponse:nil error:nil sender:sender]);

    auto challengeCompletionHandler = [completion = makeBlockPtr(completion), secTrust = WTFMove(secTrust)] (AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) {
        switch (disposition) {
        case AuthenticationChallengeDisposition::UseCredential: {
            if (!credential.isEmpty()) {
                completion(true);
                return;
            }
        }
        FALLTHROUGH;
        case AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue:
        case AuthenticationChallengeDisposition::PerformDefaultHandling: {
            OSStatus status = SecTrustEvaluateAsyncWithError(secTrust.get(), dispatch_get_main_queue(), makeBlockPtr([completion = completion](SecTrustRef trustRef, bool result, CFErrorRef error) {
                completion(result);
            }).get());
            if (status != errSecSuccess)
                completion(false);
            return;
        }
        case AuthenticationChallengeDisposition::Cancel:
            completion(false);
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto* sessionCocoa = static_cast<NetworkSessionCocoa*>(connectionToWebProcess->networkProcess().networkSession(connectionToWebProcess->sessionID()));

    if (sessionCocoa && sessionCocoa->fastServerTrustEvaluationEnabled()) {
        auto decisionHandler = makeBlockPtr([connectionToWebProcess = WTFMove(connectionToWebProcess), pageID = WTFMove(pageID), clientOrigin = WTFMove(clientOrigin), challengeCompletionHandler = WTFMove(challengeCompletionHandler)](NSURLAuthenticationChallenge *challenge, OSStatus trustResult) mutable {
            if (trustResult == noErr) {
                challengeCompletionHandler(AuthenticationChallengeDisposition::PerformDefaultHandling, WebCore::Credential());
                return;
            }

            connectionToWebProcess->networkProcess().protectedAuthenticationManager()->didReceiveAuthenticationChallenge(connectionToWebProcess->sessionID(), pageID, &clientOrigin.topOrigin, challenge, NegotiatedLegacyTLS::No, WTFMove(challengeCompletionHandler));
        });


        [NSURLSession _strictTrustEvaluate:challenge.get() queue:[NSOperationQueue mainQueue].underlyingQueue completionHandler:decisionHandler.get()];
        return;
    }

    connectionToWebProcess->networkProcess().protectedAuthenticationManager()->didReceiveAuthenticationChallenge(connectionToWebProcess->sessionID(), pageID, &clientOrigin.topOrigin, challenge.get(), NegotiatedLegacyTLS::No, WTFMove(challengeCompletionHandler));
}

static RetainPtr<nw_parameters_t> createParameters(NetworkConnectionToWebProcess& connectionToWebProcess, URL&& url, WebKit::WebPageProxyIdentifier&& pageID, WebCore::ClientOrigin&& clientOrigin)
{
    auto configureWebTransport = [clientOrigin = clientOrigin.clientOrigin.toString()](nw_protocol_options_t options) {
        nw_webtransport_options_set_is_unidirectional(options, false);
        nw_webtransport_options_set_is_datagram(options, true);
        nw_webtransport_options_add_connect_request_header(options, "origin", clientOrigin.utf8().data());
    };

    auto configureTLS = [connectionToWebProcess = Ref { connectionToWebProcess }, url = WTFMove(url), pageID = WTFMove(pageID), clientOrigin = WTFMove(clientOrigin)] (nw_protocol_options_t options) {
        RetainPtr securityOptions = adoptNS(nw_tls_copy_sec_protocol_options(options));
        sec_protocol_options_set_peer_authentication_required(securityOptions.get(), true);
        sec_protocol_options_set_verify_block(securityOptions.get(), makeBlockPtr([connectionToWebProcess = WTFMove(connectionToWebProcess), url = WTFMove(url), pageID = WTFMove(pageID), clientOrigin = WTFMove(clientOrigin)](sec_protocol_metadata_t metadata, sec_trust_t trust, sec_protocol_verify_complete_t completion) mutable {
            didReceiveServerTrustChallenge(WTFMove(connectionToWebProcess), WTFMove(url), WTFMove(pageID), WTFMove(clientOrigin), trust, completion);
        }).get(), dispatch_get_main_queue());
        // FIXME: Pipe client cert auth into this too, probably.
    };

    auto configureQUIC = [](nw_protocol_options_t options) {
        nw_quic_set_initial_max_streams_bidirectional(options, std::numeric_limits<uint32_t>::max());
        nw_quic_set_initial_max_streams_unidirectional(options, std::numeric_limits<uint32_t>::max());
        nw_quic_set_max_datagram_frame_size(options, std::numeric_limits<uint16_t>::max());
    };

    return adoptNS(nw_parameters_create_webtransport_http(configureWebTransport, configureTLS, configureQUIC, NW_PARAMETERS_DEFAULT_CONFIGURATION));
}
#endif // HAVE(WEB_TRANSPORT)

void NetworkTransportSession::initialize(NetworkConnectionToWebProcess& connectionToWebProcess, URL&& url, WebKit::WebPageProxyIdentifier&& pageID, WebCore::ClientOrigin&& clientOrigin, CompletionHandler<void(RefPtr<NetworkTransportSession>&&)>&& completionHandler)
{
#if HAVE(WEB_TRANSPORT)
    RetainPtr endpoint = adoptNS(nw_endpoint_create_url(url.string().utf8().data()));
    if (!endpoint) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    RetainPtr parameters = createParameters(connectionToWebProcess, WTFMove(url), WTFMove(pageID), WTFMove(clientOrigin));
    if (!parameters) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    RetainPtr groupDescriptor = adoptNS(nw_group_descriptor_create_multiplex(endpoint.get()));
    if (!groupDescriptor) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    RetainPtr connectionGroup = adoptNS(nw_connection_group_create(groupDescriptor.get(), parameters.get()));
    if (!connectionGroup) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    Ref networkTransportSession = NetworkTransportSession::create(connectionToWebProcess, connectionGroup.get(), endpoint.get());

    auto creationCompletionHandler = [
        completionHandler = WTFMove(completionHandler)
    ] (RefPtr<NetworkTransportSession>&& session) mutable {
        if (!completionHandler)
            return;
        if (!session)
            completionHandler(nullptr);
        session->setupDatagramConnection(WTFMove(completionHandler));
    };

    nw_connection_group_set_state_changed_handler(connectionGroup.get(), makeBlockPtr([
        networkTransportSession = WTFMove(networkTransportSession),
        creationCompletionHandler = WTFMove(creationCompletionHandler)
    ] (nw_connection_group_state_t state, nw_error_t error) mutable {
        if (error)
            return creationCompletionHandler(nullptr);
        switch (state) {
        case nw_connection_group_state_waiting:
            return; // We will get another callback with another state change.
        case nw_connection_group_state_ready:
            return creationCompletionHandler(WTFMove(networkTransportSession));
        case nw_connection_group_state_failed:
        case nw_connection_group_state_cancelled:
        case nw_connection_group_state_invalid:
            return creationCompletionHandler(nullptr);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }).get());

    nw_connection_group_set_queue(connectionGroup.get(), dispatch_get_main_queue());
    nw_connection_group_start(connectionGroup.get());
#else
    completionHandler(nullptr);
#endif // HAVE(WEB_TRANSPORT)
}

void NetworkTransportSession::createBidirectionalStream(CompletionHandler<void(std::optional<WebCore::WebTransportStreamIdentifier>)>&& completionHandler)
{
    createStream(NetworkTransportStreamType::Bidirectional, WTFMove(completionHandler));
}

void NetworkTransportSession::createOutgoingUnidirectionalStream(CompletionHandler<void(std::optional<WebCore::WebTransportStreamIdentifier>)>&& completionHandler)
{
    createStream(NetworkTransportStreamType::OutgoingUnidirectional, WTFMove(completionHandler));
}

void NetworkTransportSession::setupDatagramConnection(CompletionHandler<void(RefPtr<NetworkTransportSession>&&)>&& completionHandler)
{
#if HAVE(WEB_TRANSPORT)
    ASSERT(!m_datagramConnection);
    ASSERT(completionHandler);

    RetainPtr webtransportOptions = adoptNS(nw_webtransport_create_options());
    if (!webtransportOptions) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }
    nw_webtransport_options_set_is_unidirectional(webtransportOptions.get(), false);
    nw_webtransport_options_set_is_datagram(webtransportOptions.get(), true);

    m_datagramConnection = adoptNS(nw_connection_group_extract_connection(m_connectionGroup.get(), nil, webtransportOptions.get()));
    if (!m_datagramConnection) {
        ASSERT_NOT_REACHED();
        return completionHandler(nullptr);
    }

    nw_connection_set_state_changed_handler(m_datagramConnection.get(), makeBlockPtr([
        protectedThis = RefPtr { this },
        completionHandler = WTFMove(completionHandler)
    ] (nw_connection_state_t state, nw_error_t error) mutable {
        if (!protectedThis)
            return completionHandler(nullptr);
        if (error) {
            protectedThis = nullptr;
            return completionHandler(nullptr); // FIXME: Pipe the failure to JS
        }
        switch (state) {
        case nw_connection_state_waiting:
        case nw_connection_state_preparing: {
            return; // We will get another callback with another state change.
        }
        case nw_connection_state_ready: {
            protectedThis->receiveDatagramLoop();
            return completionHandler(std::exchange(protectedThis, nullptr));
        }
        case nw_connection_state_invalid:
        case nw_connection_state_failed:
        case nw_connection_state_cancelled: {
            protectedThis = nullptr;
            return completionHandler(nullptr); // FIXME: Pipe the failure to JS
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
    }).get());
    nw_connection_set_queue(m_datagramConnection.get(), dispatch_get_main_queue());
    nw_connection_start(m_datagramConnection.get());
#else
    completionHandler(nullptr);
#endif // HAVE(WEB_TRANSPORT)
}

void NetworkTransportSession::sendDatagram(std::span<const uint8_t> data, CompletionHandler<void()>&& completionHandler)
{
#if HAVE(WEB_TRANSPORT)
    ASSERT(m_datagramConnection);
    nw_connection_send(m_datagramConnection.get(), makeDispatchData(Vector(data)).get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, makeBlockPtr([completionHandler = WTFMove(completionHandler)] (nw_error_t error) mutable {
        ASSERT(!error);
        // FIXME: Pass any error through to JS.
        completionHandler();
    }).get());
#else
    completionHandler();
#endif // HAVE(WEB_TRANSPORT)
}

void NetworkTransportSession::setupConnectionHandler()
{
#if HAVE(WEB_TRANSPORT)
    nw_connection_group_set_new_connection_handler(m_connectionGroup.get(), makeBlockPtr([weakThis = WeakPtr { *this }] (nw_connection_t inboundConnection) mutable {
        ASSERT(inboundConnection);
        RefPtr strongThis = weakThis.get();
        if (!strongThis)
            return;
        nw_connection_set_state_changed_handler(inboundConnection, makeBlockPtr([
            weakThis = WeakPtr { *strongThis },
            inboundConnection = RetainPtr { inboundConnection }
        ] (nw_connection_state_t state, nw_error_t error) mutable {
            if (!inboundConnection)
                return;
            RefPtr strongThis = weakThis.get();
            if (!strongThis) {
                inboundConnection = nullptr;
                return;
            }
            if (error) {
                inboundConnection = nullptr;
                return; // FIXME: Pipe this error to JS.
            }
            switch (state) {
            case nw_connection_state_invalid:
                inboundConnection = nullptr;
                return;
            case nw_connection_state_waiting:
            case nw_connection_state_preparing:
                return; // We will get another callback with another state change.
            case nw_connection_state_ready:
                break;
            case nw_connection_state_failed:
            case nw_connection_state_cancelled:
                inboundConnection = nullptr;
                return;
            }
            RetainPtr metadata = adoptNS(nw_connection_copy_protocol_metadata(inboundConnection.get(), nw_protocol_copy_webtransport_definition()));
            if (nw_webtransport_metadata_get_is_unidirectional(metadata.get())) {
                Ref stream = NetworkTransportStream::create(*strongThis.get(), std::exchange(inboundConnection, nullptr).get(), NetworkTransportStreamType::IncomingUnidirectional);
                auto identifier = stream->identifier();
                ASSERT(!strongThis->m_streams.contains(identifier));
                strongThis->m_streams.set(identifier, stream);
                strongThis->receiveIncomingUnidirectionalStream(identifier);
                return;
            }
            Ref stream = NetworkTransportStream::create(*strongThis.get(), std::exchange(inboundConnection, nullptr).get(), NetworkTransportStreamType::Bidirectional);
            auto identifier = stream->identifier();
            ASSERT(!strongThis->m_streams.contains(identifier));
            strongThis->m_streams.set(identifier, stream);
            strongThis->receiveBidirectionalStream(identifier);
        }).get());
        nw_connection_set_queue(inboundConnection, dispatch_get_main_queue());
        nw_connection_start(inboundConnection);
    }).get());
#endif // HAVE(WEB_TRANSPORT)
}

void NetworkTransportSession::createStream(NetworkTransportStreamType streamType, CompletionHandler<void(std::optional<WebCore::WebTransportStreamIdentifier>)>&& completionHandler)
{
#if HAVE(WEB_TRANSPORT)
    ASSERT(streamType != NetworkTransportStreamType::IncomingUnidirectional);
    RetainPtr webtransportOptions = adoptNS(nw_webtransport_create_options());
    if (!webtransportOptions) {
        ASSERT_NOT_REACHED();
        return completionHandler(std::nullopt);
    }
    nw_webtransport_options_set_is_unidirectional(webtransportOptions.get(), streamType != NetworkTransportStreamType::Bidirectional);
    nw_webtransport_options_set_is_datagram(webtransportOptions.get(), false);

    RetainPtr connection = adoptNS(nw_connection_group_extract_connection(m_connectionGroup.get(), nil, webtransportOptions.get()));
    if (!connection) {
        ASSERT_NOT_REACHED();
        return completionHandler(std::nullopt);
    }

    auto creationCompletionHandler = [
        weakThis = WeakPtr { *this },
        completionHandler = WTFMove(completionHandler)
    ] (RefPtr<NetworkTransportStream>&& stream) mutable {
        if (!completionHandler)
            return;
        RefPtr strongThis = weakThis.get();
        if (!strongThis || !stream)
            return completionHandler(std::nullopt);
        auto identifier = stream->identifier();
        ASSERT(!strongThis->m_streams.contains(identifier));
        strongThis->m_streams.set(identifier, stream.releaseNonNull());
        completionHandler(identifier);
    };

    Ref stream = NetworkTransportStream::create(*this, connection.get(), streamType);

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
#else
    completionHandler(std::nullopt);
#endif // HAVE(WEB_TRANSPORT)
}

void NetworkTransportSession::receiveDatagramLoop()
{
#if HAVE(WEB_TRANSPORT)
    ASSERT(m_datagramConnection);
    nw_connection_receive(m_datagramConnection.get(), 1, std::numeric_limits<uint32_t>::max(), makeBlockPtr([weakThis = WeakPtr { *this }] (dispatch_data_t content, nw_content_context_t, bool withFin, nw_error_t error) {
        RefPtr strongThis = weakThis.get();
        if (!strongThis)
            return;
        if (error)
            return; // FIXME: Pipe this error to JS.

        // FIXME: Not only is this an unnecessary string copy, but it's also something that should probably be in WTF or FragmentedSharedBuffer.
        auto vectorFromData = [](dispatch_data_t content) {
            ASSERT(content);
            Vector<uint8_t> request;
            dispatch_data_apply_span(content, [&](std::span<const uint8_t> buffer) {
                request.append(buffer);
                return true;
            });
            return request;
        };

        strongThis->receiveDatagram(vectorFromData(content).span());
        strongThis->receiveDatagramLoop();
    }).get());
#endif // HAVE(WEB_TRANSPORT)
}
}
