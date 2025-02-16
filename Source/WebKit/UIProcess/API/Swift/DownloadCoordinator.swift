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

#if ENABLE_SWIFTUI && compiler(>=6.0)

import Foundation

// MARK: DownloadCoordinator protocol

@_spi(Private)
public protocol DownloadCoordinator {
    @MainActor
    func destination(forDownload download: WebPage.DownloadID, response: URLResponse, suggestedFilename: String) async -> URL?

    @MainActor
    func authenticationChallengeDisposition(forDownload download: WebPage.DownloadID, challenge: URLAuthenticationChallenge) async -> (URLSession.AuthChallengeDisposition, URLCredential?)

    @MainActor
    func httpRedirectionPolicy(forDownload download: WebPage.DownloadID, response: HTTPURLResponse, newRequest request: URLRequest) async -> WebPage.Download.RedirectPolicy

    @MainActor
    func placeholderPolicy(forDownload download: WebPage.DownloadID) async -> WebPage.Download.PlaceholderPolicy
}

// MARK: Default implementation

@_spi(Private)
public extension DownloadCoordinator {
    @MainActor
    func destination(forDownload download: WebPage.DownloadID, response: URLResponse, suggestedFilename: String) async -> URL? {
        nil
    }

    @MainActor
    func authenticationChallengeDisposition(forDownload download: WebPage.DownloadID, challenge: URLAuthenticationChallenge) async -> (URLSession.AuthChallengeDisposition, URLCredential?) {
        (.performDefaultHandling, nil)
    }

    @MainActor
    func httpRedirectionPolicy(forDownload download: WebPage.DownloadID, response: HTTPURLResponse, newRequest request: URLRequest) async -> WebPage.Download.RedirectPolicy {
        .allow
    }

    @MainActor
    func placeholderPolicy(forDownload download: WebPage.DownloadID) async -> WebPage.Download.PlaceholderPolicy {
        .disable(alternatePlaceholder: nil)
    }
}

#endif
