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

import WebKit_Internal

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
        origin: WTF.String,
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
        origin: WTF.String,
        presentationAnchor: PhoneNumberPossessionPresentationAnchor
    ) async throws(CxxUnexpected<WebKit.PhoneNumberPossessionError>) -> WebCore.PhoneNumberPossessionResponse {
        throw CxxUnexpected(error: .Unavailable)
    }
}

#endif // ENABLE_PHONE_NUMBER_POSSESSION
