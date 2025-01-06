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

extension WebPage_v0 {
    public struct NavigationID: Sendable, Hashable, Equatable {
        let rawValue: ObjectIdentifier

        init(_ cocoaNavigation: WKNavigation) {
            self.rawValue = ObjectIdentifier(cocoaNavigation)
        }
    }

    @_spi(Private)
    public struct NavigationEvent: Sendable {
        public enum Kind: Sendable {
            case startedProvisionalNavigation

            case receivedServerRedirect

            case committed

            case finished

            case failedProvisionalNavigation(underlyingError: any Error)

            case failed(underlyingError: any Error)
        }

        @_spi(Testing)
        public init(kind: Kind, navigationID: NavigationID) {
            self.kind = kind
            self.navigationID = navigationID
        }

        public let kind: Kind

        public let navigationID: NavigationID
    }

    @_spi(Private)
    public struct Navigations: AsyncSequence, Sendable {
        public typealias AsyncIterator = Iterator
        
        public typealias Element = NavigationEvent

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

extension WebPage_v0.Navigations {
    @_spi(Private)
    public struct Iterator: AsyncIteratorProtocol {
        public typealias Element = WebPage_v0.NavigationEvent

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
