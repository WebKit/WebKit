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

import Foundation
import WebKit
import Observation
import os
@_spi(Private) import WebKit

@Observable
@MainActor
final class DownloadCoordinator {
    private static let logger = Logger(subsystem: Bundle.main.bundleIdentifier!, category: String(describing: DownloadCoordinator.self))

    var downloads: [DownloadCoordinator.DownloadItem] = []

    func didReceiveDownloadEvent(_ event: WebPage_v0.DownloadEvent) {
        Self.logger.info("Did receive download event \(String(describing: event.kind)) for download \(String(describing: event.download.id))")

        switch event.kind {
        case .started:
            downloads.append(.init(source: event.download))

        case .receivedPlaceholderURL, .receivedFinalURL:
            break

        case .finished:
            guard let downloadIndex = downloads.firstIndex(where: { $0.id == event.download.id }) else {
                fatalError()
            }
            downloads[downloadIndex].finished = true

        case let .failed(underlyingError, _):
            Self.logger.error("Failed download: \(underlyingError)")

        @unknown default:
            fatalError()
        }
    }
}

extension DownloadCoordinator: WebKit.DownloadCoordinator {
    func destination(forDownload download: WebPage_v0.DownloadID, response: URLResponse, suggestedFilename: String) async -> URL? {
        let url = URL.downloadsDirectory.appending(component: suggestedFilename)
        guard !FileManager.default.fileExists(atPath: url.path()) else {
            Self.logger.error("\(url.path()) already exists")
            return nil
        }

        guard let downloadIndex = downloads.firstIndex(where: { $0.id == download }) else {
            fatalError()
        }
        downloads[downloadIndex].url = url

        return url
    }
}

extension DownloadCoordinator {
    @MainActor
    struct DownloadItem: Downloadable {
        init(source: WebPage_v0.Download) {
            self.id = source.id
            self.progress = source.progress
            self.source = source
        }

        let id: WebPage_v0.DownloadID
        let progress: Progress

        var url: URL? = nil
        var finished: Bool = false

        private let source: WebPage_v0.Download

        func cancel() async {
            await source.cancel()
        }

        nonisolated static func == (lhs: Self, rhs: Self) -> Bool {
            lhs.id == rhs.id && lhs.progress == rhs.progress && lhs.url == rhs.url && lhs.finished == rhs.finished
        }

        nonisolated func hash(into hasher: inout Hasher) {
            id.hash(into: &hasher)
            progress.hash(into: &hasher)
            url.hash(into: &hasher)
            finished.hash(into: &hasher)
        }
    }
}
