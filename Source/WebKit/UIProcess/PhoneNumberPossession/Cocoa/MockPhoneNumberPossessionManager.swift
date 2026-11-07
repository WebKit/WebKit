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
import WebCore_Private
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
            return try response(for: request)
        case .UserCancelled:
            throw CxxUnexpected(error: .UserCancelled)
        case .Failure:
            throw CxxUnexpected(error: .Failed)
        case .NoResponse:
            // Never answers, so that tests can cancel the request.
            try? await Task.sleep(nanoseconds: .max)
            throw CxxUnexpected(error: .Cancelled)
        @unknown default:
            throw CxxUnexpected(error: .Failed)
        }
    }

    private func response(
        for request: WebKit.RefCountedPhoneNumberPossessionRequest
    ) throws(CxxUnexpected<WebKit.PhoneNumberPossessionError>) -> WebCore.PhoneNumberPossessionResponse {
        #if compiler(>=6.5)
        let challengeBuffer = request.pointee.challenge
        let challenge = Data(copying: challengeBuffer.span().bytes)
        #else
        // FIXME: rdar://191033367 Reading a request's BufferSource copies it, which crashes the compiler, so read
        // the challenge's bytes through a copy that C++ makes. Remove the copy and this #if once that's fixed.
        let challengeBytes = WebKit.challengeBytes(request)
        let challenge = data(challengeBytes.span())
        #endif

        let clientData = ClientData(
            challenge: challenge.base64EncodedString(options: [.base64URLAlphabet, .omitPaddingCharacter]),
            requestContext: "web browser",
            rpId: request.pointee.relyingPartyIdentifier.description
        )

        let phoneNumberRequested = request.pointee.desiredResponseType == .PossessionIdentifierAndPhoneNumber

        // A real one is encrypted.
        let result = EncryptedResult(
            pnpi: Data(repeating: 0x01, count: 32).base64EncodedString(options: [.base64URLAlphabet, .omitPaddingCharacter]),
            zkProof: Data(repeating: 0x02, count: 64).base64EncodedString(options: [.base64URLAlphabet, .omitPaddingCharacter]),
            msisdn: phoneNumberRequested
                ? Data(repeating: 0x07, count: 16).base64EncodedString(options: [.base64URLAlphabet, .omitPaddingCharacter]) : nil
        )

        let encoder = JSONEncoder()
        encoder.outputFormatting = .sortedKeys

        let resultJSON: Data
        let clientDataJSON: Data
        let decryptionKeyJSON: Data
        do {
            resultJSON = try encoder.encode(result)
            clientDataJSON = try encoder.encode(clientData)
            decryptionKeyJSON = try encoder.encode(DecryptionKeyOrError(decryptionKey: Array(repeating: 0x04, count: 32)))
        } catch {
            throw CxxUnexpected(error: .Failed)
        }

        var response = WebCore.PhoneNumberPossessionResponse(
            encryptedResult: JSC.ArrayBuffer.createFromSpan(resultJSON.span),
            carrierDomain: WTF.String("carrier.example" as CFString),
            carrierConfigurationID: 1,
            encryptedDecryptionKeyOrError: .init(),
            clientDataJSON: JSC.ArrayBuffer.createFromSpan(clientDataJSON.span),
            portingZKP: .init()
        )

        if configuration.includesOptionalFields {
            response.encryptedDecryptionKeyOrError = .init(JSC.ArrayBuffer.createFromSpan(decryptionKeyJSON.span).get())
            response.portingZKP = .init(JSC.ArrayBuffer.createFromSpan(Data(repeating: 0x09, count: 32).span).get())
        }

        return response
    }
}

private struct ClientData: Encodable {
    let challenge: String
    let requestContext: String
    let rpId: String
}

private struct EncryptedResult: Encodable {
    let pnpi: String
    let zkProof: String
    let msisdn: String?
}

private struct DecryptionKeyOrError: Encodable {
    let decryptionKey: [UInt8]
}

#if compiler(<6.5)
// Data has no initializer that takes a Span.
private func data(_ bytes: Span<UInt8>) -> Data {
    var data = Data(capacity: bytes.count)
    for index in bytes.indices {
        data.append(bytes[index])
    }
    return data
}
#endif

#endif // ENABLE_PHONE_NUMBER_POSSESSION
