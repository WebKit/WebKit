// Copyright © 2024 ___ORGANIZATIONNAME___ All rights reserved.

import Foundation

extension WebPage_v0 {
    @MainActor
    @_spi(Private)
    public struct BackForwardList: Equatable, Sendable {
        @MainActor
        public struct Item: Equatable, Identifiable, Sendable {
            public struct ID: Hashable {
                private let value = UUID()
            }

            nonisolated public let id: ID = ID()

            public var title: String? { wrapped.title }

            public var url: URL { wrapped.url }

            public var initialURL: URL { wrapped.initialURL }

            let wrapped: WKBackForwardListItem

            init(wrapping wrapped: WKBackForwardListItem) {
                self.wrapped = wrapped
            }
        }

        public var backList: [Item] {
            wrapped?.backList.map(Item.init(wrapping:)) ?? []
        }

        public var currentItem: Item? {
            wrapped?.currentItem.map(Item.init(wrapping:))
        }

        public var forwardList: [Item] {
            wrapped?.forwardList.map(Item.init(wrapping:)) ?? []
        }

        private var wrapped: WKBackForwardList? = nil

        init(wrapping wrapped: WKBackForwardList? = nil) {
            self.wrapped = wrapped
        }

        public subscript(_ index: Int) -> Item? {
            wrapped?.item(at: index).map(Item.init(wrapping:))
        }
    }
}
