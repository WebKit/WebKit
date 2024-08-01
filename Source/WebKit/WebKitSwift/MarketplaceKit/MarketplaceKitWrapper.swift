// Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if os(iOS) && !os(visionOS) && canImport(MarketplaceKit)

import Foundation
import OSLog

import MarketplaceKit

@objc(WKMarketplaceKit)
@available(iOS 17.4, *)
public final class MarketplaceKitWrapper : NSObject {
    private static let logger = Logger(subsystem: "com.apple.WebKit", category: "Loading")

    @objc
    @available(iOS 17.4, *)
    public static func requestAppInstallation(topOrigin: URL, url: URL, completionHandler: @escaping (Error?) -> Void) {
        Task { @MainActor in
            do {
                try await AppLibrary.current.requestAppInstallationFromBrowser(for: url, referrer: topOrigin)
                logger.debug("WKMarketplaceKit.requestAppInstallation with top origin \(topOrigin, privacy: .sensitive) for \(url, privacy: .sensitive) succeeded")
                completionHandler(nil);
            } catch {
                logger.error("WKMarketplaceKit.requestAppInstallation with top origin \(topOrigin, privacy: .sensitive) for \(url, privacy: .sensitive) failed: \(error, privacy: .public)")
                completionHandler(error);
            }
        }
    }
}

#endif
