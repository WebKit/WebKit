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

// This file contains common JS "currency" types.

// MARK: JavaScriptSelection

/// A type representing the JavaScript `Selection` type.
public enum JavaScriptSelection: Sendable, Equatable {
    /// A position relative to some DOM element within a selection.
    public struct Position: Sendable, Equatable {
        /// The id of the DOM element this position is relative to.
        public let container: String

        /// The offset relative to the container.
        public let offset: Int

        /// Create a new selection.
        ///
        /// - Parameters:
        ///   - container: The container element for this selection.
        ///   - offset: The offset relative to the container.
        public init(in container: String, at offset: Int) {
            self.container = container
            self.offset = offset
        }
    }

    /// A collapsed selection.
    case collapsed(Position)

    /// A selection with a range.
    case range(base: Position, extent: Position)
}

extension JavaScriptSelection.Position: WebPage.JavaScriptEncodable {
    // Protocol conformance.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func encoded() -> [String: Any?] {
        [
            "container": container,
            "offset": offset,
        ]
    }
}

extension JavaScriptSelection.Position: WebPage.JavaScriptDecodable {
    // Protocol conformance.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public init?(decodedRepresentation: [String: Any?]) {
        guard let container = decodedRepresentation["container"] as? String else {
            return nil
        }

        guard let offset = decodedRepresentation["offset"] as? Int else {
            return nil
        }

        self.container = container
        self.offset = offset
    }
}

extension JavaScriptSelection: WebPage.JavaScriptEncodable {
    // Protocol conformance.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func encoded() -> [String: Any?] {
        switch self {
        case .collapsed(let position):
            [
                "kind": "collapsed",
                "position": position.encoded(),
            ]
        case .range(let base, let extent):
            [
                "kind": "range",
                "base": base.encoded(),
                "extent": extent.encoded(),
            ]
        }
    }
}

extension JavaScriptSelection: WebPage.JavaScriptDecodable {
    // Protocol conformance.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public init?(decodedRepresentation: [String: Any?]) {
        guard let kind = decodedRepresentation["kind"] as? String else {
            return nil
        }

        switch kind {
        case "collapsed":
            guard
                let position = decodedRepresentation["position"] as? [String: Any?],
                let decodedPosition = Position(decodedRepresentation: position)
            else {
                return nil
            }

            self = .collapsed(decodedPosition)

        case "range":
            guard
                let base = decodedRepresentation["base"] as? [String: Any?],
                let decodedBase = Position(decodedRepresentation: base)
            else {
                return nil
            }

            guard
                let extent = decodedRepresentation["extent"] as? [String: Any?],
                let decodedExtent = Position(decodedRepresentation: extent)
            else {
                return nil
            }

            self = .range(base: decodedBase, extent: decodedExtent)

        default:
            return nil
        }
    }
}

// MARK: DOMRect

/// A DOMRect describes the size and position of a rectangle.
public struct DOMRect: Sendable {
    /// The x coordinate of the DOMRect's origin (typically the top-left corner of the rectangle).
    public let x: Double

    /// The y coordinate of the DOMRect's origin (typically the top-left corner of the rectangle).
    public let y: Double

    /// The width of the DOMRect.
    public let width: Double

    /// The height of the DOMRect.
    public let height: Double
}

extension DOMRect: WebPage.JavaScriptDecodable {
    // Protocol conformance.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public init?(decodedRepresentation: [String: Any?]) {
        guard let x = decodedRepresentation["x"] as? Double else {
            return nil
        }

        guard let y = decodedRepresentation["y"] as? Double else {
            return nil
        }

        guard let width = decodedRepresentation["width"] as? Double else {
            return nil
        }

        guard let height = decodedRepresentation["height"] as? Double else {
            return nil
        }

        self = .init(x: x, y: y, width: width, height: height)
    }
}

extension CGRect {
    /// Converts a DOMRect to a CGRect.
    ///
    /// - Parameter rect: The value to convert.
    public init(_ rect: DOMRect) {
        self = .init(x: rect.x, y: rect.y, width: rect.width, height: rect.height)
    }
}

#endif // ENABLE_SWIFTUI
