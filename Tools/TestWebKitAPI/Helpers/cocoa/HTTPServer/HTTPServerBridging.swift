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

import Foundation
import Security
private import TestWebKitAPILibrary.Helpers.cocoa.HTTPServer.HTTPServerBridging

import struct Swift.String

#if USE_APPLE_INTERNAL_SDK
@_spi(CTypeConversion) import Network
#else
import Network
import Network_SPI
#endif

@objc
@implementation
extension HTTPResponseDataBridge {
    final let storage: HTTPResponseData

    init(
        statusCode: UInt,
        headerFields: [[String]],
        body: Data,
        behavior: HTTPResponseBehaviorBridge,
        shouldRespondWith304: Bool,
        headerFieldsFor304: [String: String]
    ) {
        self.storage = HTTPResponseData(
            statusCode: statusCode,
            headerFields: headerFields.map { (name: $0[0], value: $0[1]) },
            body: body,
            behavior: .init(behavior),
            shouldRespondWith304ToConditionalRequests: shouldRespondWith304,
            headerFieldsFor304: headerFieldsFor304
        )
    }
}

@objc
@implementation
extension HTTPServerBridge {
    final private let storage: HTTPServerCore

    var port: UInt16 { storage.port }
    var totalRequests: Int { storage.totalRequests }
    var totalConnections: Int { storage.totalConnections }
    var lastRequestCookies: String { storage.lastRequestCookies }
    var sawAuthorizationHeader: Bool { storage.sawAuthorizationHeader }

    init?(
        routes: [String: HTTPResponseDataBridge],
        protocol: HTTPServerProtocolBridge,
        port: UInt16,
        identity: SecIdentity?,
        certificateVerifier: sec_protocol_verify_t?
    ) {
        let core = try? HTTPServerCore(
            protocol: .init(`protocol`),
            responses: routes.mapValues(\.storage),
            port: port == 0 ? nil : port,
            identity: identity,
            verifier: certificateVerifier
        )
        guard let core else {
            return nil
        }
        self.storage = core
    }

    @objc(initWithProtocol:connectionHandler:)
    init?(
        withProtocol protocol: HTTPServerProtocolBridge,
        connectionHandler: @escaping (nw_connection_t) -> Void
    ) {
        let core = try? HTTPServerCore(protocol: .init(`protocol`)) { connection in
            connectionHandler(connection.nw)
        }
        guard let core else {
            return nil
        }
        self.storage = core
    }

    func startListening() async throws {
        try await storage.startListening()
    }

    func cancel() async {
        await storage.cancel()
    }

    func terminateAllConnections() async {
        await storage.terminateAllConnections()
    }

    func addResponse(_ response: HTTPResponseDataBridge, forPath path: String) {
        storage.addResponse(response.storage, for: path)
    }

    func setResponse(_ response: HTTPResponseDataBridge, forPath path: String) {
        storage.setResponse(response.storage, for: path)
    }
}

extension HTTPServerCore.`Protocol` {
    init(_ `protocol`: HTTPServerProtocolBridge) {
        self =
            switch `protocol` {
            case .HTTP: .http
            case .HTTPS: .https
            case .httpsWithLegacyTLS: .httpsWithLegacyTLS
            case .http2Raw: .http2Raw
            case .HTTP2: .http2
            case .HTTP3: .http3
            case .httpsProxy: .httpsProxy
            case .httpsProxyWithAuthentication: .httpsProxyWithAuthentication
            case .http2Proxy: .http2Proxy
            @unknown default: fatalError()
            }
    }
}

extension HTTPResponseData.Behavior {
    init(_ behavior: HTTPResponseBehaviorBridge) {
        self =
            switch behavior {
            case .sendResponseNormally: .sendResponseNormally
            case .terminateConnectionAfterReceivingRequest: .terminateConnectionAfterReceivingRequest
            case .neverSendResponse: .neverSendResponse
            @unknown default: fatalError()
            }
    }
}
