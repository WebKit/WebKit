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
import Network

private struct ProxyHandshake {
    enum State {
        case willRequestCredentials
        case didRequestCredentials
        case willNotRequestCredentials
        case passThrough
    }

    private static let proxyAuthenticationRequired = """
        HTTP/1.1 407 Proxy Authentication Required\r\n\
        Proxy-Authenticate: Basic realm="testrealm"\r\n\
        Content-Length: 0\r\n\
        \r\n
        """
        .utf8

    private static let connectionEstablished = """
        HTTP/1.1 200 Connection Established\r\n\
        Connection: close\r\n\
        \r\n
        """
        .utf8

    private static let expectedAuthorization = """
        Proxy-Authorization: Basic dGVzdHVzZXI6dGVzdHBhc3N3b3Jk\r\n
        """
        .utf8

    private var state: State

    init(requiresAuthentication: Bool) {
        self.state = requiresAuthentication ? .willRequestCredentials : .willNotRequestCredentials
    }

    mutating func handleInput(framer: NWProtocolFramer.Instance, definition: NWProtocolFramer.Definition) -> Int {
        framer.passThroughOutput()

        _ = unsafe framer.parseInput(minimumIncompleteLength: 1, maximumLength: .max) { buffer, isComplete in
            let length = unsafe buffer?.count ?? 0

            switch state {
            case .willRequestCredentials:
                framer.writeOutput(data: Data(Self.proxyAuthenticationRequired))
                state = .didRequestCredentials

            case .didRequestCredentials:
                if let buffer = unsafe buffer, unsafe Data(buffer).range(of: Data(Self.expectedAuthorization)) == nil {
                    fatalError("The proxy did not receive the expected credentials")
                }
                fallthrough

            case .willNotRequestCredentials:
                framer.writeOutput(data: Data(Self.connectionEstablished))
                framer.markReady()
                state = .passThrough

            case .passThrough:
                _ = framer.deliverInputNoCopy(length: length, message: .init(definition: definition), isComplete: isComplete)
                return 0
            }

            return length
        }

        return 0
    }
}

protocol HTTPSProxyFramerImplementation: NWProtocolFramerImplementation {
}

extension HTTPSProxyFramerImplementation {
    static var label: String { "HttpsProxy" }

    func start(framer: NWProtocolFramer.Instance) -> NWProtocolFramer.StartResult {
        .willMarkReady
    }

    func handleOutput(framer: NWProtocolFramer.Instance, message: NWProtocolFramer.Message, messageLength: Int, isComplete: Bool) {
    }

    func wakeup(framer: NWProtocolFramer.Instance) {
    }

    func stop(framer: NWProtocolFramer.Instance) -> Bool {
        true
    }

    func cleanup(framer: NWProtocolFramer.Instance) {
    }
}

final class HTTPSProxyFramer: HTTPSProxyFramerImplementation {
    static let definition = NWProtocolFramer.Definition(implementation: HTTPSProxyFramer.self)

    private var handshake = ProxyHandshake(requiresAuthentication: false)

    init(framer: NWProtocolFramer.Instance) {
    }

    func handleInput(framer: NWProtocolFramer.Instance) -> Int {
        handshake.handleInput(framer: framer, definition: Self.definition)
    }
}

final class HTTPSProxyWithAuthenticationFramer: HTTPSProxyFramerImplementation {
    static let definition = NWProtocolFramer.Definition(implementation: HTTPSProxyWithAuthenticationFramer.self)

    private var handshake = ProxyHandshake(requiresAuthentication: true)

    init(framer: NWProtocolFramer.Instance) {
    }

    func handleInput(framer: NWProtocolFramer.Instance) -> Int {
        handshake.handleInput(framer: framer, definition: Self.definition)
    }
}
