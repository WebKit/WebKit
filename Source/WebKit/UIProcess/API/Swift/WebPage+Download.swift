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

extension WebPage {
    @MainActor
    @_spi(Private)
    public struct Download: Sendable {
        public enum RedirectPolicy: Sendable {
            case allow

            case cancel
        }

        public enum PlaceholderPolicy: Sendable {
            case enable

            case disable(alternatePlaceholder: URL?)
        }

        init(_ wrapped: WKDownload) {
            self.wrapped = wrapped
        }

        public var originalRequest: URLRequest? { wrapped.originalRequest }

        public var isUserInitiated: Bool { wrapped.isUserInitiated }

        public var originatingFrame: WebPage.FrameInfo { .init(wrapped.originatingFrame) }

        public var progress: Progress { wrapped.progress }

        let wrapped: WKDownload

        nonisolated public var id: DownloadID {
            .init(wrapped)
        }

        @discardableResult
        public func cancel() async -> Data? {
            await withCheckedContinuation { continuation in
                wrapped.cancel {
                    continuation.resume(returning: $0)
                }
            }
        }
    }
}

extension WebPage {
    @_spi(Private)
    public struct DownloadID: Sendable, Hashable, Equatable {
        let rawValue: ObjectIdentifier

        init(_ cocoaDownload: WKDownload) {
            self.rawValue = ObjectIdentifier(cocoaDownload)
        }
    }

    @_spi(Private)
    public struct DownloadEvent: Sendable {
        public enum Kind: Sendable {
            case started

            case receivedPlaceholderURL(URL)

            case receivedFinalURL(URL)

            case finished

            case failed(underlyingError: any Error, resumeData: Data?)
        }

        @_spi(Testing)
        public init(kind: Kind, download: WebPage.Download) {
            self.kind = kind
            self.download = download
        }

        public let kind: Kind

        public let download: WebPage.Download
    }

    @_spi(Private)
    public struct Downloads: AsyncSequence, Sendable {
        public typealias AsyncIterator = Iterator

        public typealias Element = DownloadEvent

        public typealias Failure = Never

        init(source: AsyncStream<Element>) {
            self.source = source
        }

        private let source: AsyncStream<Element>

        public func makeAsyncIterator() -> AsyncIterator {
            Iterator(source: source.makeAsyncIterator())
        }
    }
}

extension WebPage.Downloads {
    public struct Iterator: AsyncIteratorProtocol {
        public typealias Element = WebPage.DownloadEvent

        public typealias Failure = Never

        init(source: AsyncStream<Element>.AsyncIterator) {
            self.source = source
        }

        private var source: AsyncStream<Element>.AsyncIterator

        public mutating func next() async -> Element? {
            await source.next()
        }
    }
}

#endif
