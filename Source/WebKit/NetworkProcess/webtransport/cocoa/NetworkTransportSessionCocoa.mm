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

#import "AuthenticationChallengeDisposition.h"
#import "AuthenticationManager.h"
#import "MessageSenderInlines.h"
#import "NetworkConnectionToWebProcess.h"
#import "NetworkProcess.h"
#import "NetworkSessionCocoa.h"
#import "NetworkTransportStream.h"
#import "WebTransportSessionMessages.h"
#import <Security/Security.h>
#import <WebCore/AuthenticationChallenge.h>
#import <WebCore/ClientOrigin.h>
#import <WebCore/Exception.h>
#import <WebCore/ExceptionCode.h>
#import <WebCore/WebTransportConnectionInfo.h>
#import <WebCore/WebTransportConnectionStats.h>
#import <WebCore/WebTransportReceiveStreamStats.h>
#import <WebCore/WebTransportSendStreamStats.h>
#import <pal/spi/cocoa/NetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>

#import "NetworkSoftLink.h"

namespace WebKit {

Ref<NetworkTransportSession> NetworkTransportSession::create(NetworkConnectionToWebProcess& connection, WebTransportSessionIdentifier identifier, WebCore::WebTransportOptions&& options, nw_connection_group_t group, nw_endpoint_t endpoint)
{
    return adoptRef(*new NetworkTransportSession(connection, identifier, WTFMove(options), group, endpoint));
}

NetworkTransportSession::NetworkTransportSession(NetworkConnectionToWebProcess& connection, WebTransportSessionIdentifier identifier, WebCore::WebTransportOptions&& options, nw_connection_group_t connectionGroup, nw_endpoint_t endpoint)
    : m_connectionToWebProcess(connection)
    , m_identifier(identifier)
    , m_options(WTFMove(options))
    , m_connectionGroup(connectionGroup)
    , m_endpoint(endpoint)
{
    setupConnectionHandler();
}

#if HAVE(WEB_TRANSPORT)

static bool leafCertificateMatchesWebTransportHash(sec_trust_t trust, const Vector<WebCore::WebTransportHash>& hashes)
{
    // https://www.w3.org/TR/webtransport/#verify-a-certificate-hash
    SUPPRESS_RETAINPTR_CTOR_ADOPT RetainPtr secTrust = adoptCF(sec_trust_copy_ref(trust));
    if (!secTrust)
        return false;

    RetainPtr chain = adoptCF(SecTrustCopyCertificateChain(secTrust.get()));
    if (!chain || !CFArrayGetCount(chain.get()))
        return false;

    RetainPtr leafCertificate = checked_cf_cast<SecCertificateRef>(CFArrayGetValueAtIndex(chain.get(), 0));
    if (!leafCertificate)
        return false;

    SUPPRESS_RETAINPTR_CTOR_ADOPT RetainPtr x509Data = adoptNS(bridge_cast(SecCertificateCopyData(leafCertificate.get())));
    if (!x509Data)
        return false;

    std::array<uint8_t, CC_SHA256_DIGEST_LENGTH> sha2;
    CC_SHA256([x509Data bytes], [x509Data length], sha2.data());

    bool hashMatches = std::ranges::any_of(hashes, [&] (auto& hash) {
        return hash.algorithm == "sha-256"_s && equalSpans(std::span(sha2), hash.value.span());
    });
    if (!hashMatches)
        return false;

    // https://www.w3.org/TR/webtransport/#custom-certificate-requirements
    RetainPtr validityBegin = adoptCF(SecCertificateCopyNotValidBeforeDate(leafCertificate.get()));
    if (!validityBegin)
        return false;
    RetainPtr validityEnd = adoptCF(SecCertificateCopyNotValidAfterDate(leafCertificate.get()));
    if (!validityEnd)
        return false;
    CFAbsoluteTime currentTime = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime beginTime = CFDateGetAbsoluteTime(validityBegin.get());
    CFAbsoluteTime endTime = CFDateGetAbsoluteTime(validityEnd.get());
    if (currentTime < beginTime || currentTime > endTime)
        return false;

    CFTimeInterval validityPeriod = endTime - beginTime;
    constexpr double twoWeeks = 2 * 7 * 24 * 60 * 60;
    if (validityPeriod <= 0 || validityPeriod > twoWeeks)
        return false;

    return true;
}

static void didReceiveServerTrustChallenge(NetworkConnectionToWebProcess& connectionToWebProcess, const URL& url, const Vector<WebCore::WebTransportHash>& hashes, WebKit::WebPageProxyIdentifier pageID, const WebCore::ClientOrigin& clientOrigin, sec_trust_t trust, sec_protocol_verify_complete_t completion)
{
    if (!hashes.isEmpty())
        return completion(leafCertificateMatchesWebTransportHash(trust, hashes));

    uint16_t port = url.port() ? *url.port() : *defaultPortForProtocol(url.protocol());
    SUPPRESS_RETAINPTR_CTOR_ADOPT RetainPtr secTrust = adoptCF(sec_trust_copy_ref(trust));
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
        [[fallthrough]];
        case AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue:
        case AuthenticationChallengeDisposition::PerformDefaultHandling: {
            OSStatus status = SecTrustEvaluateAsyncWithError(secTrust.get(), mainDispatchQueueSingleton(), makeBlockPtr([completion = completion](SecTrustRef trustRef, bool result, CFErrorRef error) {
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

    CheckedPtr sessionCocoa = downcast<NetworkSessionCocoa>(connectionToWebProcess.networkProcess().networkSession(connectionToWebProcess.sessionID()));

    if (sessionCocoa && sessionCocoa->fastServerTrustEvaluationEnabled()) {
        auto decisionHandler = makeBlockPtr([
            connectionToWebProcess = Ref { connectionToWebProcess },
            pageID = WTFMove(pageID),
            clientOrigin,
            challengeCompletionHandler = WTFMove(challengeCompletionHandler)
        ] (NSURLAuthenticationChallenge *challenge, OSStatus trustResult) mutable {
            if (trustResult == noErr) {
                challengeCompletionHandler(AuthenticationChallengeDisposition::PerformDefaultHandling, WebCore::Credential());
                return;
            }

            connectionToWebProcess->networkProcess().protectedAuthenticationManager()->didReceiveAuthenticationChallenge(connectionToWebProcess->sessionID(), pageID, &clientOrigin.topOrigin, challenge, NegotiatedLegacyTLS::No, WTFMove(challengeCompletionHandler));
        });


        [NSURLSession _strictTrustEvaluate:challenge.get() queue:[NSOperationQueue mainQueue].underlyingQueue completionHandler:decisionHandler.get()];
        return;
    }

    connectionToWebProcess.networkProcess().protectedAuthenticationManager()->didReceiveAuthenticationChallenge(connectionToWebProcess.sessionID(), pageID, &clientOrigin.topOrigin, challenge.get(), NegotiatedLegacyTLS::No, WTFMove(challengeCompletionHandler));
}

static String joinProtocolStrings(const Vector<String>& protocols)
{
    StringBuilder builder;
    for (size_t i = 0; i < protocols.size(); ++i) {
        if (i)
            builder.append(", "_s);
        builder.append('\"', protocols[i], '\"');
    }
    return builder.toString();
}

static RetainPtr<nw_parameters_t> createParameters(NetworkConnectionToWebProcess& connectionToWebProcess, URL&& url, WebCore::WebTransportOptions& options, WebKit::WebPageProxyIdentifier&& pageID, WebCore::ClientOrigin&& clientOrigin)
{
    // https://www.w3.org/TR/webtransport/#web-transport-configuration
    auto configureWebTransport = [
        clientOrigin = clientOrigin.clientOrigin.toString(),
        maxStreamsUni = options.anticipatedConcurrentIncomingUnidirectionalStreams.value_or(100),
        maxStreamsBidi = options.anticipatedConcurrentIncomingBidirectionalStreams.value_or(100),
        protocols = joinProtocolStrings(options.protocols)
    ](nw_protocol_options_t options) {
        nw_webtransport_options_set_is_unidirectional(options, false);
        nw_webtransport_options_set_is_datagram(options, true);
        nw_webtransport_options_add_connect_request_header(options, "origin", clientOrigin.utf8().data());
        if (canLoad_Network_nw_webtransport_options_set_allow_joining_before_ready())
            softLink_Network_nw_webtransport_options_set_allow_joining_before_ready(options, true);
        if (canLoad_Network_nw_webtransport_options_set_initial_max_streams_uni())
            softLink_Network_nw_webtransport_options_set_initial_max_streams_uni(options, maxStreamsUni);
        if (canLoad_Network_nw_webtransport_options_set_initial_max_streams_bidi())
            softLink_Network_nw_webtransport_options_set_initial_max_streams_bidi(options, maxStreamsBidi);
        nw_webtransport_options_add_connect_request_header(options, "wt-available-protocols", protocols.utf8().data());
    };

    auto configureTLS = [
        weakConnection = WeakPtr { connectionToWebProcess },
        url = WTFMove(url),
        pageID = WTFMove(pageID),
        hashes = std::exchange(options.serverCertificateHashes, { }),
        clientOrigin = WTFMove(clientOrigin)
    ](nw_protocol_options_t options) mutable {
        RetainPtr securityOptions = adoptNS(nw_tls_copy_sec_protocol_options(options));
        sec_protocol_options_set_peer_authentication_required(securityOptions.get(), true);
        sec_protocol_options_set_verify_block(securityOptions.get(), makeBlockPtr([
            // The configureTLS lambda can be called more than once, which means that
            // this inner lambda capture initialization will also run multiple times.
            // Therefore, do not WTFMove in this capture list or it will not work
            // after the first move.
            weakConnection,
            url,
            pageID,
            hashes,
            clientOrigin
        ] (sec_protocol_metadata_t metadata, sec_trust_t trust, sec_protocol_verify_complete_t completion) mutable {
            RefPtr connectionToWebProcess = weakConnection.get();
            if (!connectionToWebProcess) {
                completion(false);
                return;
            }
            didReceiveServerTrustChallenge(*connectionToWebProcess, url, hashes, pageID, clientOrigin, trust, completion);
        }).get(), mainDispatchQueueSingleton());
        // FIXME: Pipe client cert auth into this too, probably.
    };

    auto configureQUIC = [](nw_protocol_options_t options) {
        nw_quic_set_max_datagram_frame_size(options, std::numeric_limits<uint16_t>::max());
    };

    auto configureTCP = options.requireUnreliable ? NW_PARAMETERS_DISABLE_PROTOCOL : NW_PARAMETERS_DEFAULT_CONFIGURATION;

    return adoptNS(nw_parameters_create_webtransport_http(configureWebTransport, configureTLS, configureQUIC, configureTCP));
}
#endif // HAVE(WEB_TRANSPORT)

RefPtr<NetworkTransportSession> NetworkTransportSession::create(NetworkConnectionToWebProcess& connectionToWebProcess, WebTransportSessionIdentifier identifier, URL&& url, WebCore::WebTransportOptions&& options, WebKit::WebPageProxyIdentifier&& pageID, WebCore::ClientOrigin&& clientOrigin)
{
#if HAVE(WEB_TRANSPORT)
    RetainPtr endpoint = adoptNS(nw_endpoint_create_url(url.string().utf8().data()));
    if (!endpoint) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    RetainPtr parameters = createParameters(connectionToWebProcess, WTFMove(url), options, WTFMove(pageID), WTFMove(clientOrigin));
    if (!parameters) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    RetainPtr groupDescriptor = adoptNS(nw_group_descriptor_create_multiplex(endpoint.get()));
    if (!groupDescriptor) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    RetainPtr connectionGroup = adoptNS(nw_connection_group_create(groupDescriptor.get(), parameters.get()));
    if (!connectionGroup) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return NetworkTransportSession::create(connectionToWebProcess, identifier, WTFMove(options), connectionGroup.get(), endpoint.get());
#else
    return nullptr;
#endif // HAVE(WEB_TRANSPORT)
}

void NetworkTransportSession::initialize(CompletionHandler<void(std::optional<WebCore::WebTransportConnectionInfo>&&)>&& completionHandler)
{
#if HAVE(WEB_TRANSPORT)
    auto creationCompletionHandler = [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (std::optional<WebCore::WebTransportConnectionInfo>&& connectionInfo) mutable {
        if (!completionHandler)
            return;
        if (canLoad_Network_nw_webtransport_options_set_allow_joining_before_ready())
            return completionHandler(WTFMove(connectionInfo));
        if (!connectionInfo)
            return completionHandler(std::nullopt);
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler(std::nullopt);
        protectedThis->setupDatagramConnection(WTFMove(completionHandler));
    };

    nw_connection_group_set_state_changed_handler(m_connectionGroup.get(), makeBlockPtr([creationCompletionHandler = WTFMove(creationCompletionHandler), weakThis = WeakPtr { *this }] (nw_connection_group_state_t state, nw_error_t error) mutable {
        switch (state) {
        case nw_connection_group_state_invalid:
        case nw_connection_group_state_waiting:
            return; // We will get another callback with another state change.
        case nw_connection_group_state_ready: {
            __block String protocol;
            WebCore::WebTransportReliabilityMode reliabilityMode = WebCore::WebTransportReliabilityMode::Pending;
            if (RefPtr protectedThis = weakThis.get()) {
                protectedThis->m_sessionMetadata = nw_connection_group_copy_protocol_metadata(protectedThis->m_connectionGroup.get(), adoptNS(nw_protocol_copy_webtransport_definition()).get());
                if (RetainPtr metadata = protectedThis->m_sessionMetadata) {
                    if (canLoad_Network_nw_webtransport_metadata_set_remote_drain_handler()) {
                        softLink_Network_nw_webtransport_metadata_set_remote_drain_handler(metadata.get(), makeBlockPtr([weakThis = WeakPtr { *protectedThis }] () mutable {
                            RefPtr protectedThis = weakThis.get();
                            if (!protectedThis)
                                return;
                            protectedThis->send(Messages::WebTransportSession::DidDrain());
                        }).get(), mainDispatchQueueSingleton());
                    }
                    if (canLoad_Network_nw_webtransport_metadata_copy_connect_response()) {
                        RetainPtr response = adoptNS(softLink_Network_nw_webtransport_metadata_copy_connect_response(metadata.get()));
                        nw_http_fields_access_value_by_name(response.get(), "wt-protocol", ^void(const char *value) {
                            protocol = String::fromUTF8(unsafeSpan(value));
                        });
                    }
                    if (canLoad_Network_nw_webtransport_metadata_get_transport_mode()) {
                        nw_webtransport_transport_mode_t transportMode = softLink_Network_nw_webtransport_metadata_get_transport_mode(metadata.get());
                        if (transportMode == nw_webtransport_transport_mode_http3)
                            reliabilityMode = WebCore::WebTransportReliabilityMode::SupportsUnreliable;
                        else if (transportMode == nw_webtransport_transport_mode_http2)
                            reliabilityMode = WebCore::WebTransportReliabilityMode::ReliableOnly;
                    }
                }
            }
            return creationCompletionHandler(WebCore::WebTransportConnectionInfo { WTFMove(protocol), reliabilityMode });
        }
        case nw_connection_group_state_failed:
            if (RefPtr protectedThis = weakThis.get()) {
                if (RetainPtr metadata = protectedThis->m_sessionMetadata) {
                    std::optional<unsigned> sessionErrorCode = std::nullopt;
                    String sessionErrorMessage;
                    if (canLoad_Network_nw_webtransport_metadata_get_session_closed() && softLink_Network_nw_webtransport_metadata_get_session_closed(metadata.get())) {
                        sessionErrorCode = nw_webtransport_metadata_get_session_error_code(metadata.get());
                        if (const char* errorMessage = nw_webtransport_metadata_get_session_error_message(metadata.get()))
                            sessionErrorMessage = String::fromUTF8(unsafeSpan(errorMessage));
                    }
                    protectedThis->send(Messages::WebTransportSession::DidFail(WTFMove(sessionErrorCode), WTFMove(sessionErrorMessage)));
                    return;
                }
            }
            return creationCompletionHandler(std::nullopt);
        case nw_connection_group_state_cancelled:
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }).get());

    nw_connection_group_set_queue(m_connectionGroup.get(), RetainPtr { mainDispatchQueueSingleton() }.get());
    nw_connection_group_start(m_connectionGroup.get());

    if (canLoad_Network_nw_webtransport_options_set_allow_joining_before_ready())
        setupDatagramConnection([](std::optional<WebCore::WebTransportConnectionInfo>&&) { });
#else
    completionHandler(std::nullopt);
#endif
}

void NetworkTransportSession::createBidirectionalStream(CompletionHandler<void(std::optional<WebCore::WebTransportStreamIdentifier>)>&& completionHandler)
{
    createStream(NetworkTransportStreamType::Bidirectional, WTFMove(completionHandler));
}

void NetworkTransportSession::getStats(CompletionHandler<void(WebCore::WebTransportConnectionStats&&)>&& completionHandler)
{
    // FIXME: Implement.
    completionHandler({ });
}

void NetworkTransportSession::createOutgoingUnidirectionalStream(CompletionHandler<void(std::optional<WebCore::WebTransportStreamIdentifier>)>&& completionHandler)
{
    createStream(NetworkTransportStreamType::OutgoingUnidirectional, WTFMove(completionHandler));
}

void NetworkTransportSession::setupDatagramConnection(CompletionHandler<void(std::optional<WebCore::WebTransportConnectionInfo>&&)>&& completionHandler)
{
#if HAVE(WEB_TRANSPORT)
    ASSERT(!m_datagramConnection);

    RetainPtr webtransportOptions = adoptNS(nw_webtransport_create_options());
    if (!webtransportOptions) {
        ASSERT_NOT_REACHED();
        return;
    }
    nw_webtransport_options_set_is_unidirectional(webtransportOptions.get(), false);
    nw_webtransport_options_set_is_datagram(webtransportOptions.get(), true);
    if (canLoad_Network_nw_webtransport_options_set_allow_joining_before_ready())
        softLink_Network_nw_webtransport_options_set_allow_joining_before_ready(webtransportOptions.get(), true);

    m_datagramConnection = adoptNS(nw_connection_group_extract_connection(m_connectionGroup.get(), nil, webtransportOptions.get()));
    if (!m_datagramConnection) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto creationCompletionHandler = [completionHandler = WTFMove(completionHandler)] (bool success) mutable {
        if (!completionHandler)
            return;
        if (success)
            completionHandler(WebCore::WebTransportConnectionInfo { { }, WebCore::WebTransportReliabilityMode::Pending });
        else
            completionHandler(std::nullopt);
    };

    nw_connection_set_state_changed_handler(m_datagramConnection.get(), makeBlockPtr([weakThis = WeakPtr { *this }, creationCompletionHandler = WTFMove(creationCompletionHandler)] (nw_connection_state_t state, nw_error_t error) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return creationCompletionHandler(false);
        switch (state) {
        case nw_connection_state_invalid:
        case nw_connection_state_waiting:
        case nw_connection_state_preparing:
            return; // We will get another callback with another state change.
        case nw_connection_state_ready:
            if (!canLoad_Network_nw_webtransport_options_set_allow_joining_before_ready())
                protectedThis->receiveDatagramLoop();
            return creationCompletionHandler(true);
        case nw_connection_state_failed:
        case nw_connection_state_cancelled:
            return creationCompletionHandler(false);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }).get());
    nw_connection_set_queue(m_datagramConnection.get(), mainDispatchQueueSingleton());
    nw_connection_start(m_datagramConnection.get());

    if (canLoad_Network_nw_webtransport_options_set_allow_joining_before_ready())
        receiveDatagramLoop();

#else
    completionHandler(std::nullopt);
#endif // HAVE(WEB_TRANSPORT)
}

void NetworkTransportSession::sendDatagram(std::optional<WebCore::WebTransportSendGroupIdentifier> identifier, std::span<const uint8_t> data, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    if (identifier) {
        m_datagramStats.ensure(*identifier, [] {
            return uint64_t { };
        }).iterator->value += data.size();
    }
#if HAVE(WEB_TRANSPORT)
    ASSERT(m_datagramConnection);
    nw_connection_send(m_datagramConnection.get(), makeDispatchData(Vector(data)).get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, makeBlockPtr([completionHandler = WTFMove(completionHandler)] (nw_error_t error) mutable {
        if (error) {
            if (nw_error_get_error_domain(error) == nw_error_domain_posix && nw_error_get_error_code(error) == ECANCELED)
                completionHandler(std::nullopt);
            else
                completionHandler(WebCore::Exception(WebCore::ExceptionCode::NetworkError));
            return;
        }
        completionHandler(std::nullopt);
    }).get());
#else
    completionHandler(std::nullopt);
#endif // HAVE(WEB_TRANSPORT)
}

void NetworkTransportSession::setupConnectionHandler()
{
#if HAVE(WEB_TRANSPORT)
    nw_connection_group_set_new_connection_handler(m_connectionGroup.get(), makeBlockPtr([weakThis = WeakPtr { *this }] (nw_connection_t inboundConnection) mutable {
        ASSERT(inboundConnection);
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        nw_connection_set_state_changed_handler(inboundConnection, makeBlockPtr([
            weakThis = WeakPtr { *protectedThis },
            inboundConnection = RetainPtr { inboundConnection }
        ] (nw_connection_state_t state, nw_error_t error) mutable {
            if (!inboundConnection)
                return;
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis) {
                inboundConnection = nullptr;
                return;
            }
            switch (state) {
            case nw_connection_state_invalid:
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
            RetainPtr metadata = adoptNS(nw_connection_copy_protocol_metadata(inboundConnection.get(), adoptNS(nw_protocol_copy_webtransport_definition()).get()));
            bool unidirectional = nw_webtransport_metadata_get_is_unidirectional(metadata.get());
            auto streamType = unidirectional ? NetworkTransportStreamType::IncomingUnidirectional : NetworkTransportStreamType::Bidirectional;
            Ref stream = NetworkTransportStream::create(*protectedThis.get(), std::exchange(inboundConnection, nullptr).get(), streamType);
            auto identifier = stream->identifier();
            ASSERT(!protectedThis->m_streams.contains(identifier));
            protectedThis->m_streams.set(identifier, stream);

            if (unidirectional)
                protectedThis->receiveIncomingUnidirectionalStream(identifier);
            else
                protectedThis->receiveBidirectionalStream(identifier);

            if (!unidirectional && canLoad_Network_nw_webtransport_metadata_set_remote_receive_error_handler()) {
                softLink_Network_nw_webtransport_metadata_set_remote_receive_error_handler(metadata.get(), makeBlockPtr([weakThis = WeakPtr { *protectedThis }, identifier] (uint64_t errorCode) mutable {
                    RefPtr protectedThis = weakThis.get();
                    if (!protectedThis)
                        return;
                    protectedThis->send(Messages::WebTransportSession::StreamSendError(identifier, errorCode));
                }).get(), mainDispatchQueueSingleton());
            }

            if (canLoad_Network_nw_webtransport_metadata_set_remote_send_error_handler()) {
                softLink_Network_nw_webtransport_metadata_set_remote_send_error_handler(metadata.get(), makeBlockPtr([weakThis = WeakPtr { *protectedThis }, identifier] (uint64_t errorCode) mutable {
                    RefPtr protectedThis = weakThis.get();
                    if (!protectedThis)
                        return;
                    protectedThis->send(Messages::WebTransportSession::StreamReceiveError(identifier, errorCode));
                }).get(), mainDispatchQueueSingleton());
            }
        }).get());
        nw_connection_set_queue(inboundConnection, mainDispatchQueueSingleton());
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
    if (canLoad_Network_nw_webtransport_options_set_allow_joining_before_ready())
        softLink_Network_nw_webtransport_options_set_allow_joining_before_ready(webtransportOptions.get(), true);
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
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !stream)
            return completionHandler(std::nullopt);
        auto identifier = stream->identifier();
        ASSERT(!protectedThis->m_streams.contains(identifier));
        protectedThis->m_streams.set(identifier, stream.releaseNonNull());
        completionHandler(identifier);
    };

    Ref stream = NetworkTransportStream::create(*this, connection.get(), streamType);

    nw_connection_set_state_changed_handler(connection.get(), makeBlockPtr([
        creationCompletionHandler = WTFMove(creationCompletionHandler),
        stream = WTFMove(stream),
        connection,
        streamType,
        weakThis = WeakPtr { *this }
    ] (nw_connection_state_t state, nw_error_t error) mutable {
        switch (state) {
        case nw_connection_state_invalid:
        case nw_connection_state_waiting:
        case nw_connection_state_preparing:
            return; // We will get another callback with another state change.
        case nw_connection_state_ready:
            break;
        case nw_connection_state_failed:
        case nw_connection_state_cancelled:
            return creationCompletionHandler(nullptr);
        }
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return creationCompletionHandler(nullptr);
        RetainPtr metadata = adoptNS(nw_connection_copy_protocol_metadata(connection.get(), adoptNS(nw_protocol_copy_webtransport_definition()).get()));
        bool unidirectional = streamType == NetworkTransportStreamType::OutgoingUnidirectional;
        auto identifier = stream->identifier();
        if (canLoad_Network_nw_webtransport_metadata_set_remote_receive_error_handler()) {
            softLink_Network_nw_webtransport_metadata_set_remote_receive_error_handler(metadata.get(), makeBlockPtr([weakThis = WeakPtr { *protectedThis }, identifier] (uint64_t errorCode) mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                protectedThis->send(Messages::WebTransportSession::StreamSendError(identifier, errorCode));
            }).get(), mainDispatchQueueSingleton());
        }

        if (!unidirectional && canLoad_Network_nw_webtransport_metadata_set_remote_send_error_handler()) {
            softLink_Network_nw_webtransport_metadata_set_remote_send_error_handler(metadata.get(), makeBlockPtr([weakThis = WeakPtr { *protectedThis }, identifier] (uint64_t errorCode) mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                protectedThis->send(Messages::WebTransportSession::StreamReceiveError(identifier, errorCode));
            }).get(), mainDispatchQueueSingleton());
        }
        return creationCompletionHandler(WTFMove(stream));
    }).get());
    nw_connection_set_queue(connection.get(), mainDispatchQueueSingleton());
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
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (error) {
            if (nw_error_get_error_domain(error) != nw_error_domain_posix || nw_error_get_error_code(error) != ECANCELED)
                protectedThis->receiveDatagram({ }, false, WebCore::Exception(WebCore::ExceptionCode::NetworkError));
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

        bool completed = !content && withFin;
        protectedThis->receiveDatagram(vectorFromData(content).span(), completed, std::nullopt);
        if (!completed)
            protectedThis->receiveDatagramLoop();
    }).get());
#endif // HAVE(WEB_TRANSPORT)
}

void NetworkTransportSession::terminate(WebCore::WebTransportSessionErrorCode code, CString&& message)
{
#if HAVE(WEB_TRANSPORT)
    if (m_sessionMetadata) {
        nw_webtransport_metadata_set_session_error_code(m_sessionMetadata.get(), code);
        nw_webtransport_metadata_set_session_error_message(m_sessionMetadata.get(), message.data());
    }

    if (m_datagramConnection)
        nw_connection_cancel(m_datagramConnection.get());

    for (auto& stream : std::exchange(m_streams, { }).values())
        stream->cancel(code);

    nw_connection_group_cancel(m_connectionGroup.get());
#endif // HAVE(WEB_TRANSPORT)
}
}
