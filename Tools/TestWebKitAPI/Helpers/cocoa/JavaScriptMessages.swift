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

#if ENABLE_SWIFTUI

public import struct Swift.String
public import WebKit

/// A collection of common useful JavaScript expressions.
public enum JavaScriptMessages {
}

extension JavaScriptMessages {
    /// Gets the bounding client rect of an element.
    public struct BoundingClientRect: WebPage.JavaScriptExpression {
        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public typealias Output = DOMRect

        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public static var expression: String {
            """
            if (elementID) {
                return document.getElementById(elementID).getBoundingClientRect().toJSON();
            }

            const range = document.createRange();

            if (selection.kind === "range") {
                const baseNode = document.getElementById(selection.base.container).firstChild;
                const extentNode = document.getElementById(selection.extent.container).firstChild;
                range.setStart(baseNode, selection.base.offset)
                range.setEnd(extentNode, selection.extent.offset);
            } else {
                const node = document.getElementById(selection.position.container).firstChild;
                range.setStart(node, selection.position.offset)
                range.setEnd(node, selection.position.offset);
            }

            return range.getBoundingClientRect().toJSON();
            """
        }

        private enum Storage {
            case selection(JavaScriptSelection)
            case element(id: String)
        }

        private let storage: Storage

        /// Create a `BoundingClientRect` expression from the given selection.
        ///
        /// - Parameter selection: The selection that will be set.
        public init(_ selection: JavaScriptSelection) {
            self.storage = .selection(selection)
        }

        /// A convenience initializer for a range selection.
        ///
        /// - Parameters:
        ///   - container: The container the range is relative to.
        ///   - range: The range of the selection within the container.
        public init(in container: String, range: Range<Int>) {
            self.storage = .selection(
                .range(
                    base: .init(in: container, at: range.lowerBound),
                    extent: .init(in: container, at: range.upperBound),
                )
            )
        }

        /// Create a `BoundingClientRect` expression for the element with the given `id` attribute.
        ///
        /// - Parameter elementID: The `id` attribute of the target element.
        public init(elementID: String) {
            self.storage = .element(id: elementID)
        }

        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public func encoded() -> [String: Any?] {
            switch storage {
            case .selection(let selection):
                [
                    "selection": selection.encoded(),
                    "elementID": nil,
                ]
            case .element(let id):
                [
                    "selection": nil,
                    "elementID": id,
                ]
            }
        }
    }
}

extension JavaScriptMessages {
    /// An expression used to set the current selection in JavaScript.
    public struct SetSelection: WebPage.JavaScriptExpression {
        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public typealias Output = Void

        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public static var expression: String {
            """
            if (selection.kind === "range") {
                const baseNode = document.getElementById(selection.base.container).firstChild;
                const extentNode = document.getElementById(selection.extent.container).firstChild;
                getSelection().setBaseAndExtent(baseNode, selection.base.offset, extentNode, selection.extent.offset);
            } else {
                const node = document.getElementById(selection.position.container).firstChild;
                getSelection().setPosition(node, selection.position.offset);
            }
            """
        }

        private let selection: JavaScriptSelection

        /// Create a `SetSelection` expression from the given selection.
        ///
        /// - Parameter selection: The selection that will be set.
        public init(_ selection: JavaScriptSelection) {
            self.selection = selection
        }

        /// A convenience initializer for a range selection.
        ///
        /// - Parameters:
        ///   - container: The container the range is relative to.
        ///   - range: The range of the selection within the container.
        public init(in container: String, range: Range<Int>) {
            self.selection = .range(
                base: .init(in: container, at: range.lowerBound),
                extent: .init(in: container, at: range.upperBound),
            )
        }

        /// A convenience initializer for a collapsed range selection.
        ///
        /// - Parameters:
        ///   - container: The container the range is relative to.
        ///   - offset: The offset of the selection within the container.
        public init(in container: String, offset: Int) {
            self.selection = .collapsed(.init(in: container, at: offset))
        }

        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public func encoded() -> [String: Any?] {
            [
                "selection": selection.encoded()
            ]
        }
    }
}

extension JavaScriptMessages {
    /// Gets the scroll position of the window.
    public struct ScrollPosition: WebPage.JavaScriptExpression {
        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public typealias Output = DOMPoint

        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public static var expression: String {
            """
            return { "x": window.scrollX, "y": window.scrollY };
            """
        }

        /// Create a new `ScrollPosition`.
        public init() {
        }

        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public func encoded() -> [String: Any?] {
            [:]
        }
    }
}

extension JavaScriptMessages {
    /// Gets the current selection.
    public struct GetSelection: WebPage.JavaScriptExpression {
        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public typealias Output = JavaScriptSelection

        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public static var expression: String {
            """
            const selection = getSelection();
            if (selection.rangeCount === 0 || selection.anchorNode === null) {
                return { "kind": "none" };
            }
            if (selection.isCollapsed) {
                return {
                    "kind": "collapsed",
                    "position": {
                        "container": selection.anchorNode.parentElement.id,
                        "offset": selection.anchorOffset,
                    },
                };
            } else {
                return {
                    "kind": "range",
                    "base": {
                        "container": selection.anchorNode.parentElement.id,
                        "offset": selection.anchorOffset,
                    },
                    "extent": {
                        "container": selection.focusNode.parentElement.id,
                        "offset": selection.focusOffset,
                    },
                };
            }
            """
        }

        /// Create a new `GetSelection`.
        public init() {
        }

        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public func encoded() -> [String: Any?] {
            [:]
        }
    }
}

#endif // ENABLE_SWIFTUI
