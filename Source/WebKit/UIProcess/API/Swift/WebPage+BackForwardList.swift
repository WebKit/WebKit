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
    @MainActor
    @_spi(Private)
    public struct BackForwardList: Equatable, Sendable {
        @MainActor
        public struct Item: Equatable, Identifiable, Sendable {
            public struct ID: Hashable, Sendable {
                private let value = UUID()
            }

            init(_ wrapped: WKBackForwardListItem) {
                self.wrapped = wrapped
            }

            nonisolated public let id: ID = ID()

            public var title: String? { wrapped.title }

            public var url: URL { wrapped.url }

            public var initialURL: URL { wrapped.initialURL }

            let wrapped: WKBackForwardListItem
        }

        init(_ wrapped: WKBackForwardList? = nil) {
            self.wrapped = wrapped
        }

        public var backList: [Item] {
            wrapped?.backList.map(Item.init(_:)) ?? []
        }

        public var currentItem: Item? {
            wrapped?.currentItem.map(Item.init(_:))
        }

        public var forwardList: [Item] {
            wrapped?.forwardList.map(Item.init(_:)) ?? []
        }

        private var wrapped: WKBackForwardList? = nil

        public subscript(_ index: Int) -> Item? {
            wrapped?.item(at: index).map(Item.init(_:))
        }
    }
}

#endif
