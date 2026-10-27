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

#if ENABLE_PHONE_NUMBER_POSSESSION

import Foundation
import JavaScriptCore_Private.Cxx.ArrayBuffer
import WebKit_Internal
import wtf

#if WTF_PLATFORM_IOS_FAMILY
import UIKit
#else
import AppKit
#endif

// Stands in for the platform in tests, which choose how it answers with a WebCore.MockPhoneNumberPossessionConfiguration.
final class MockPhoneNumberPossessionManager: PhoneNumberPossessionManaging {
    private let configuration: WebCore.MockPhoneNumberPossessionConfiguration

    nonisolated init(configuration: WebCore.MockPhoneNumberPossessionConfiguration) {
        self.configuration = configuration
    }

    var canRequestPhoneNumberPossession: Bool {
        get async { configuration.isAvailable }
    }

    func requestPhoneNumberPossession(
        request: WebKit.RefCountedPhoneNumberPossessionRequest,
        origin: WTF.String,
        presentationAnchor: PhoneNumberPossessionPresentationAnchor
    ) async throws(CxxUnexpected<WebKit.PhoneNumberPossessionError>) -> WebCore.PhoneNumberPossessionResponse {
        // Like the platform, fail a request when the preflight check would fail.
        guard configuration.isAvailable else {
            throw CxxUnexpected(error: .Unavailable)
        }

        switch configuration.outcome {
        case .Success:
            return response(for: request)
        case .UserCancelled:
            throw CxxUnexpected(error: .UserCancelled)
        case .Failure:
            throw CxxUnexpected(error: .Failed)
        case .NoResponse:
            // Never answers, so that tests can cancel the request.
            do {
                while true {
                    try await Task.sleep(for: .seconds(60))
                }
            } catch {
                throw CxxUnexpected(error: .Cancelled)
            }
        @unknown default:
            throw CxxUnexpected(error: .Failed)
        }
    }

    private func response(for request: WebKit.RefCountedPhoneNumberPossessionRequest) -> WebCore.PhoneNumberPossessionResponse {
        let challengeBytes = WebKit.challengeBytes(request)

        let clientData: [String: Any] = [
            "challenge": base64URLEncoded(data(challengeBytes.span())),
            "requestContext": "web browser",
            "rpId": request.pointee.relyingPartyIdentifier.description,
        ]

        // A real one is encrypted.
        let pnpiWithProof: [String: Any] = [
            "pnpi": base64URLEncoded(filledData(0x01, count: 32)),
            "zkProof": base64URLEncoded(filledData(0x02, count: 64)),
        ]

        var response = WebCore.PhoneNumberPossessionResponse(
            encryptedPNPIWithProof: arrayBuffer(jsonData(pnpiWithProof)),
            aggregatorData: .init(),
            carrierPubKey: .init(),
            encryptedMSISDN: .init(),
            clientDataJSON: arrayBuffer(jsonData(clientData)),
            portingZKP: .init()
        )

        if configuration.includesOptionalFields {
            var aggregatorData = WebCore.AggregatorResponseDataFields()
            aggregatorData.encryptedProviderErrorInfo = .init(arrayBuffer(filledData(0x03, count: 16)).get())
            aggregatorData.encryptedZKPEncryptionKey = .init(arrayBuffer(filledData(0x04, count: 32)).get())
            response.aggregatorData = .init(aggregatorData)
            response.carrierPubKey = .init(arrayBuffer(filledData(0x05, count: 65)).get())
            response.portingZKP = .init(arrayBuffer(filledData(0x09, count: 32)).get())
        }

        if request.pointee.desiredResponseType == .PossessionIdentifierAndPhoneNumber {
            response.encryptedMSISDN = .init(arrayBuffer(filledData(0x07, count: 16)).get())
        }

        return response
    }
}

private func arrayBuffer(_ data: Data) -> JSC.RefArrayBuffer {
    JSC.ArrayBuffer.createFromSpan(data.span)
}

private func filledData(_ value: UInt8, count: Int) -> Data {
    Data(repeating: value, count: count)
}

// Data has no initializer that takes a Span.
private func data(_ bytes: Span<UInt8>) -> Data {
    var data = Data(capacity: bytes.count)
    for index in bytes.indices {
        data.append(bytes[index])
    }
    return data
}

private func base64URLEncoded(_ data: Data) -> String {
    data.base64EncodedString()
        .replacingOccurrences(of: "+", with: "-")
        .replacingOccurrences(of: "/", with: "_")
        .replacingOccurrences(of: "=", with: "")
}

private func jsonData(_ object: [String: Any]) -> Data {
    (try? JSONSerialization.data(withJSONObject: object)) ?? Data()
}

#endif // ENABLE_PHONE_NUMBER_POSSESSION
