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
    /// An observable representation of a webpage's previously loaded resources.
    ///
    /// This type can be used to facilitate navigating to prior or subsequent loaded resources
    /// and for observing when new entries get added or removed.
    ///
    /// In this example, the back-forward list is used to create a SwiftUI View to facilitate navigating to
    /// previous or next items:
    ///
    /// ```swift
    /// private struct BackForwardMenuView: View {
    ///     struct LabelConfiguration {
    ///         let text: String
    ///         let systemImage: String
    ///     }
    ///
    ///     let list: [WebPage.BackForwardList.Item]
    ///     let label: LabelConfiguration
    ///     let navigateToItem: (WebPage.BackForwardList.Item) -> Void
    ///
    ///     var body: some View {
    ///         Menu {
    ///             ForEach(list) { item in
    ///                 Button(item.title ?? item.url.absoluteString) {
    ///                     navigateToItem(item)
    ///                 }
    ///             }
    ///         } label: {
    ///             Label(label.text, systemImage: label.systemImage)
    ///                 .labelStyle(.iconOnly)
    ///         } primaryAction: {
    ///             navigateToItem(list.first!)
    ///         }
    ///         .disabled(list.isEmpty)
    ///     }
    /// }
    /// ```
    ///
    /// The view can then be used for both the back and forward list using a specific ``WebPage``:
    ///
    /// ```swift
    /// struct ContentView: some View {
    ///     @State private var page = WebPage()
    ///
    ///     var body: some View {
    ///         WebView(page)
    ///             .toolbar {
    ///                 ToolbarItemGroup {
    ///                     ToolbarBackForwardMenuView(
    ///                         list: page.backForwardList.backList.reversed(),
    ///                         label: .init(text: "Backward", systemImage: "chevron.backward")
    ///                     ) {
    ///                         viewModel.page.load($0)
    ///                     }
    ///
    ///                     ToolbarBackForwardMenuView(
    ///                         list: page.backForwardList.forwardList,
    ///                         label: .init(text: "Forward", systemImage: "chevron.forward")
    ///                     ) {
    ///                         viewModel.page.load($0)
    ///                     }
    ///                 }
    ///             }
    ///     }
    /// }
    /// ```
    ///
    /// Because ``WebPage/backForwardList`` is an observable property, the states of these buttons
    /// are automatically updated.
    @MainActor
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct BackForwardList: Equatable, Sendable {
        /// A representation of a resource that a webpage previously visited.
        ///
        /// Two items with equal titles, urls, and initial urls may not necessarily be equal to one another.
        public struct Item: Equatable, Identifiable, Sendable {
            /// An opaque type representing the identifier for an item.
            public struct ID: Hashable, Sendable {
                private let value = UUID()
            }

            @MainActor
            init(_ wrapped: WKBackForwardListItem) {
                self.wrapped = wrapped

                self.title = wrapped.title
                self.url = wrapped.url
                self.initialURL = wrapped.initialURL
            }

            /// The unique identifier for the item.
            public let id: ID = ID()

            /// The title of the page this item represents.
            ///
            /// If the resource this item represents does not have a title specified, this value will be `nil`.
            public let title: Swift.String?

            /// The url of the page this item represents, after having resolved all redirects.
            public let url: Foundation.URL

            /// The source URL that originally asked to load the resource.
            public let initialURL: Foundation.URL

            @MainActor
            let wrapped: WKBackForwardListItem
        }

        init(_ wrapped: WKBackForwardList? = nil) {
            self.wrapped = wrapped
        }

        /// The array of items that precede the current item.
        ///
        /// The items are in the order in which the page originally visited them.
        public var backList: [Item] {
            wrapped?.backList.map(Item.init(_:)) ?? []
        }

        /// The current item.
        ///
        /// When the webpage has not loaded any resources, this value will be `nil`.
        public var currentItem: Item? {
            wrapped?.currentItem.map(Item.init(_:))
        }

        /// The array of items that follow the current item.
        ///
        /// The items are in the order in which they were originally visited.
        public var forwardList: [Item] {
            wrapped?.forwardList.map(Item.init(_:)) ?? []
        }

        private var wrapped: WKBackForwardList? = nil

        /// Accesses the item at the relative offset from the current item.
        ///
        /// - Parameter index: The offset of the desired item from the current item. Specify `0` for the current item,
        /// `-1` for the immediately preceding item, `1` for the immediately following item, and so on.
        /// - Returns: The item at the specified offset from the current item, or `nil` if the index exceeds the limits of the list.
        public subscript(_ index: Int) -> Item? {
            wrapped?.item(at: index).map(Item.init(_:))
        }
    }
}

#endif
