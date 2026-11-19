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

#if WTF_PLATFORM_IOS_FAMILY
typealias PhoneNumberPossessionPresentationAnchor = UIWindow
#else
typealias PhoneNumberPossessionPresentationAnchor = NSWindow
#endif

@MainActor
protocol PhoneNumberPossessionManaging: AnyObject, Sendable {
    var canRequestPhoneNumberPossession: Bool { get async }

    func requestPhoneNumberPossession(
        request: WebKit.RefCountedPhoneNumberPossessionRequest,
        presentationAnchor: PhoneNumberPossessionPresentationAnchor
    ) async throws(CxxUnexpected<WebKit.PhoneNumberPossessionError>) -> WebCore.PhoneNumberPossessionResponse
}

final class UnavailablePhoneNumberPossessionManager: PhoneNumberPossessionManaging {
    nonisolated init() {
    }

    var canRequestPhoneNumberPossession: Bool {
        get async { false }
    }

    func requestPhoneNumberPossession(
        request: WebKit.RefCountedPhoneNumberPossessionRequest,
        presentationAnchor: PhoneNumberPossessionPresentationAnchor
    ) async throws(CxxUnexpected<WebKit.PhoneNumberPossessionError>) -> WebCore.PhoneNumberPossessionResponse {
        throw CxxUnexpected(error: .Unavailable)
    }
}

func phoneNumberPossessionChallenge(_ request: WebKit.RefCountedPhoneNumberPossessionRequest) -> Data {
    #if compiler(>=6.5)
    let challengeBuffer = request.pointee.challenge
    return Data(copying: challengeBuffer.span().bytes)
    #else
    // FIXME: rdar://191033367 Reading a request's BufferSource copies it, which crashes the compiler, so read
    // the challenge's bytes through a copy that C++ makes. Remove the copy and this #if once that's fixed.
    let challengeBytes = WebKit.challengeBytes(request)
    return data(challengeBytes.span())
    #endif
}

extension WebCore.PhoneNumberPossessionResponse {
    init(
        encryptedResult: Data,
        carrierDomain: String,
        carrierConfigurationID: UInt16,
        encryptedDecryptionKey: Data?,
        clientDataJSON: Data
    ) {
        // FIXME: rdar://192714761 Fill in carrierPubKey and portingZKP.
        self.init(
            encryptedResult: JSC.ArrayBuffer.createFromSpan(encryptedResult.span),
            carrierDomain: WTF.String(carrierDomain as CFString),
            carrierConfigurationID: carrierConfigurationID,
            encryptedDecryptionKey: encryptedDecryptionKey.map { .init(JSC.ArrayBuffer.createFromSpan($0.span).get()) } ?? .init(),
            carrierPubKey: .init(),
            clientDataJSON: JSC.ArrayBuffer.createFromSpan(clientDataJSON.span),
            portingZKP: .init()
        )
    }
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
