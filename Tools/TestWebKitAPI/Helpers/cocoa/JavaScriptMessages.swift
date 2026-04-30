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
    /// Gets the bounding client rect of the current selection.
    public struct SelectionBoundingClientRect: WebPage.JavaScriptExpression {
        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public typealias Output = DOMRect

        // Protocol conformance.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        public static var expression: String {
            """
            const selection = window.getSelection();

            const range = selection.getRangeAt(0);
            return range.getBoundingClientRect().toJSON();
            """
        }

        /// Create a new `SelectionBoundingClientRect`.
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
