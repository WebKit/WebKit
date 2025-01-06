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

private struct DefaultDownloadCoordinator: DownloadCoordinator {
}

@MainActor
final class WKDownloadDelegateAdapter: NSObject, WKDownloadDelegate {
    init(downloadProgressContinuation: AsyncStream<WebPage_v0.DownloadEvent>.Continuation, downloadCoordinator: (any DownloadCoordinator)?) {
        self.downloadProgressContinuation = downloadProgressContinuation
        self.downloadCoordinator = downloadCoordinator ?? DefaultDownloadCoordinator()
    }

    weak var owner: WebPage_v0? = nil

    private let downloadProgressContinuation: AsyncStream<WebPage_v0.DownloadEvent>.Continuation
    private let downloadCoordinator: any DownloadCoordinator

    // MARK: Progress reporting

    func downloadDidFinish(_ download: WKDownload) {
        downloadProgressContinuation.yield(.init(kind: .finished, download: .init(download)))
    }

    func download(_ download: WKDownload, didReceivePlaceholderURL url: URL) async {
        downloadProgressContinuation.yield(.init(kind: .receivedPlaceholderURL(url), download: .init(download)))
    }

    func download(_ download: WKDownload, didFailWithError error: any Error, resumeData: Data?) {
        downloadProgressContinuation.yield(.init(kind: .failed(underlyingError: error, resumeData: resumeData), download: .init(download)))
    }

    func download(_ download: WKDownload, didReceiveFinalURL url: URL) {
        downloadProgressContinuation.yield(.init(kind: .receivedFinalURL(url), download: .init(download)))
    }

    // MARK: Policies

    func download(_ download: WKDownload, decideDestinationUsing response: URLResponse, suggestedFilename: String) async -> URL? {
        await downloadCoordinator.destination(forDownload: .init(download), response: response, suggestedFilename: suggestedFilename)
    }

    func download(_ download: WKDownload, willPerformHTTPRedirection response: HTTPURLResponse, newRequest request: URLRequest) async -> WKDownload.RedirectPolicy {
        let result = await downloadCoordinator.httpRedirectionPolicy(forDownload: .init(download), response: response, newRequest: request)
        return switch result {
        case .allow: .allow
        case .cancel: .cancel
        }
    }

    func download(_ download: WKDownload, respondTo challenge: URLAuthenticationChallenge) async -> (URLSession.AuthChallengeDisposition, URLCredential?) {
        await downloadCoordinator.authenticationChallengeDisposition(forDownload: .init(download), challenge: challenge)
    }

    func placeholderPolicy(forDownload download: WKDownload) async -> (WebKit.WKDownload.PlaceholderPolicy, URL?) {
        let result = await downloadCoordinator.placeholderPolicy(forDownload: .init(download))
        return switch result {
        case let .disable(alternatePlaceholder):
            (.disable, alternatePlaceholder)

        case .enable:
            (.enable, nil)
        }
    }
}

#endif
