// Copyright (C) 2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import Network
import Security

import struct Swift.String

@MainActor
final class HTTPServerCore {
    enum `Protocol`: Sendable {
        case http
        case https
        case httpsWithLegacyTLS
        case http2Raw
        case http2
        case http3
        case httpsProxy
        case httpsProxyWithAuthentication
        case http2Proxy
    }

    enum Error: Swift.Error {
        case invalidPath(String)
    }

    private struct ManagedConnection {
        let connection: NWConnection
        var isTerminated = false
    }

    private let listener: NWListener
    private let `protocol`: `Protocol`
    private let customHandler: ((NWConnection) -> Void)?

    private var connections: [ManagedConnection] = []
    private var responses: [String: HTTPResponseData] = [:]

    private(set) var requestCount = 0
    private(set) var lastRequestCookies = ""
    private(set) var sawAuthorizationHeader = false

    var port: UInt16 { listener.port?.rawValue ?? 0 }
    var totalConnections: Int { connections.count }
    var totalRequests: Int { requestCount }

    init(
        protocol: `Protocol`,
        responses: [String: HTTPResponseData] = [:],
        customHandler: ((NWConnection) -> Void)? = nil,
        port: UInt16? = nil,
        identity: SecIdentity? = nil,
        verifier: sec_protocol_verify_t? = nil
    ) throws {
        self.protocol = `protocol`
        self.responses = responses
        self.customHandler = customHandler

        let parameters = Self.makeParameters(protocol: `protocol`, identity: identity, verifier: verifier)
        let endpointPort: NWEndpoint.Port = port.flatMap { NWEndpoint.Port(rawValue: $0) } ?? .any
        self.listener = try NWListener(using: parameters, on: endpointPort)

        // Weak capture because `cancel()` is the only thing that clears this handler and most servers are simply destroyed instead.
        self.listener.newConnectionHandler = { [weak self] connection in
            MainActor.assumeIsolated {
                guard let self else {
                    connection.cancel()
                    return
                }
                self.didReceive(connection: connection)
            }
        }
    }

    deinit {
        listener.cancel()

        for managed in connections where !managed.isTerminated {
            managed.connection.cancel()
        }
    }

    // MARK: - Listener lifecycle

    func startListening() async throws {
        try await withCheckedThrowingContinuation { continuation in
            listener.stateUpdateHandler = { [self] state in
                MainActor.assumeIsolated {
                    switch state {
                    case .ready:
                        listener.stateUpdateHandler = nil
                        continuation.resume()
                    case .failed(let error):
                        continuation.resume(throwing: error)
                    default:
                        break
                    }
                }
            }
            listener.start(queue: .main)
        }
    }

    func cancel() async {
        await withCheckedContinuation { continuation in
            listener.stateUpdateHandler = { state in
                MainActor.assumeIsolated {
                    guard case .cancelled = state else { return }
                    continuation.resume()
                }
            }
            listener.cancel()
        }

        listener.stateUpdateHandler = nil
        listener.newConnectionHandler = nil

        await terminateAllConnections()
    }

    func terminateAllConnections() async {
        let terminating = exchange(&connections, with: [])
            .filter { !$0.isTerminated }
            .map(\.connection)

        await withTaskGroup { group in
            for connection in terminating {
                group.addTask {
                    await connection.terminate()
                }
            }
        }
    }

    private func terminateIfNeeded(_ connection: NWConnection) async {
        let index = connections.firstIndex { $0.connection === connection }
        guard let index, !connections[index].isTerminated else {
            return
        }

        connections[index].isTerminated = true
        await connection.terminate()
    }

    // MARK: - Routes

    func addResponse(_ response: HTTPResponseData, for path: String) {
        precondition(responses[path] == nil, "\(path) already has a response")
        responses[path] = response
    }

    func setResponse(_ response: HTTPResponseData, for path: String) {
        precondition(responses[path] != nil)
        responses[path] = response
    }

    // MARK: - Connections

    private func didReceive(connection: NWConnection) {
        connections.append(ManagedConnection(connection: connection))
        connection.start(queue: .main)

        if let customHandler {
            customHandler(connection)
        } else {
            Task {
                // FIXME: Handle errors better.
                // swift-format-ignore: NeverUseForceTry
                try! await respondToRequests(on: connection)
            }
        }
    }

    private func respondToRequests(on connection: NWConnection) async throws {
        while true {
            let request = await connection.receiveHTTPRequest()
            guard !request.isEmpty else {
                return
            }

            let parser = HTTPRequestComponentParser(request: request)

            requestCount += 1
            lastRequestCookies = parser.cookies

            if !parser.authorization.isEmpty {
                sawAuthorizationHeader = true
            }

            let path = try parser.path
            guard let response = responses[path] else {
                throw Self.Error.invalidPath(path)
            }

            if response.shouldRespondWith304ToConditionalRequests && parser.isConditionalRequest {
                let fields = response.headerFieldsFor304.map { (name: $0.key, value: $0.value) }
                let response = HTTPResponseData(statusCode: 304, headerFields: fields)
                let data = response.serialize(includingContentLength: false)
                try await connection.send(data)

                continue
            }

            switch response.behavior {
            case .terminateConnectionAfterReceivingRequest:
                await terminateIfNeeded(connection)
                return

            case .sendResponseNormally:
                try await connection.send(response.serialize())

            case .neverSendResponse:
                continue
            }
        }
    }
}

extension HTTPServerCore {
    fileprivate static func makeParameters(protocol: `Protocol`, identity: SecIdentity?, verifier: sec_protocol_verify_t?) -> NWParameters {
        func tls() -> NWProtocolTLS.Options {
            makeTLSOptions(protocol: `protocol`, identity: identity ?? TestCertificates.identity, verifier: verifier)
        }

        switch `protocol` {
        case .http:
            return .tcp

        case .https, .httpsWithLegacyTLS, .http2Raw:
            return NWParameters(tls: tls())

        case .httpsProxy, .httpsProxyWithAuthentication:
            let parameters = NWParameters(tls: nil)
            let framerDefinition = `protocol` == .httpsProxy ? HTTPSProxyFramer.definition : HTTPSProxyWithAuthenticationFramer.definition

            parameters.defaultProtocolStack.applicationProtocols.insert(NWProtocolFramer.Options(definition: framerDefinition), at: 0)
            parameters.defaultProtocolStack.applicationProtocols.insert(tls(), at: 0)
            return parameters

        default:
            fatalError("not yet ported")
        }
    }

    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses deprecated TLS version")
    #endif
    fileprivate static func makeTLSOptions(
        protocol: `Protocol`,
        identity: SecIdentity,
        verifier: sec_protocol_verify_t?
    ) -> NWProtocolTLS.Options {
        let options = NWProtocolTLS.Options()
        let securityOptions = options.securityProtocolOptions

        // `sec_identity_create` nullability is mis-annotated when exposed to Swift.
        // swift-format-ignore: NeverForceUnwrap
        sec_protocol_options_set_local_identity(securityOptions, sec_identity_create(identity)!)

        if `protocol` == .httpsWithLegacyTLS {
            #if ENABLE_TLS_1_2_DEFAULT_MINIMUM
            sec_protocol_options_set_min_tls_protocol_version(securityOptions, .TLSv10)
            #endif
            sec_protocol_options_set_max_tls_protocol_version(securityOptions, .TLSv10)
        }

        if let verifier {
            sec_protocol_options_set_peer_authentication_required(securityOptions, true)
            sec_protocol_options_set_verify_block(securityOptions, verifier, .main)
        }

        if `protocol` == .http2Raw || `protocol` == .http2 {
            unsafe sec_protocol_options_add_tls_application_protocol(securityOptions, "h2")
        }

        return options
    }
}
