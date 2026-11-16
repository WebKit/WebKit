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

@available(anyAppleOSAndDownlevels 26.0, *)
@_spi_available(watchOSAndOpenSourceTBA, *)
@_spi_available(tvOSAndOpenSourceTBA, *)
extension WebView {
    /// Represents the attributes used for the `<meta name="viewport">` HTML element.
    @_spi(Experimental)
    public struct ViewportConfiguration_v0: Sendable {
        /// The pixel width of the viewport.
        @_spi(Experimental)
        public struct Width: Sendable, ExpressibleByIntegerLiteral {
            enum Storage: Sendable {
                case value(Int)
                case deviceWidth
            }

            let storage: Storage

            /// The special sentinel width value representing the physical size of the device screen in CSS pixels.
            public static var deviceWidth: Width {
                Width(storage: .deviceWidth)
            }

            private init(storage: Storage) {
                self.storage = storage
            }

            // Protocol conformance.
            // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
            @_spi(Experimental)
            public init(integerLiteral value: Int) {
                self.init(storage: .value(value))
            }
        }

        /// The pixel height of the viewport.
        @_spi(Experimental)
        public struct Height: Sendable, ExpressibleByIntegerLiteral {
            enum Storage: Sendable {
                case value(Int)
                case deviceHeight
            }

            let storage: Storage

            /// The special sentinel height value representing the physical size of the device screen in CSS pixels.
            public static var deviceHeight: Height {
                Height(storage: .deviceHeight)
            }

            private init(storage: Storage) {
                self.storage = storage
            }

            // Protocol conformance.
            // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
            @_spi(Experimental)
            public init(integerLiteral value: Int) {
                self.init(storage: .value(value))
            }
        }

        /// The effect that interactive UI widgets, such as virtual keyboards, have on a page's viewport.
        @_spi(Experimental)
        public enum InteractiveWidget: Sendable {
            /// The visual viewport gets resized by the interactive widget. This is the default.
            case resizesVisual

            /// The viewport gets resized by the interactive widget.
            case resizesContent

            /// Neither the viewport nor the visual viewport gets resized by the interactive widget.
            case overlaysContent
        }

        /// Defines the viewable portions of the webpage.
        @_spi(Experimental)
        public enum ViewportFit: Sendable {
            /// Doesn't affect the initial layout viewport, and the whole web page is viewable.
            case auto

            /// The viewport is scaled to fit the largest rectangle inscribed within the display.
            case contain

            /// The viewport is scaled to fill the device display.
            case cover
        }

        let width: Width?
        let height: Height?
        let initialScale: Float?
        let minimumScale: Float?
        let maximumScale: Float?
        let userScalable: Bool?
        let interactiveWidget: InteractiveWidget?
        let viewportFit: ViewportFit?
    }
}

extension WebView.ViewportConfiguration_v0.Width {
    var dictionaryRepresentation: String {
        switch storage {
        case .deviceWidth: "device-width"
        case .value(let value): "\(value)"
        }
    }
}

extension WebView.ViewportConfiguration_v0.Height {
    var dictionaryRepresentation: String {
        switch storage {
        case .deviceHeight: "device-height"
        case .value(let value): "\(value)"
        }
    }
}

extension WebView.ViewportConfiguration_v0.InteractiveWidget {
    var dictionaryRepresentation: String {
        switch self {
        case .resizesVisual: "resizes-visual"
        case .resizesContent: "resizes-content"
        case .overlaysContent: "overlays-content"
        }
    }
}

extension WebView.ViewportConfiguration_v0.ViewportFit {
    var dictionaryRepresentation: String {
        switch self {
        case .auto: "auto"
        case .contain: "contain"
        case .cover: "cover"
        }
    }
}

extension WebView.ViewportConfiguration_v0 {
    var dictionaryRepresentation: [String: String] {
        var result: [String: String] = [:]

        result["width"] = width?.dictionaryRepresentation
        result["height"] = height?.dictionaryRepresentation

        result["initial-scale"] = initialScale.map { "\($0)" }
        result["minimum-scale"] = minimumScale.map { "\($0)" }
        result["maximum-scale"] = maximumScale.map { "\($0)" }
        result["user-scalable"] = userScalable.map { "\($0)" }

        result["interactive-widget"] = interactiveWidget?.dictionaryRepresentation
        result["viewport-fit"] = viewportFit?.dictionaryRepresentation

        return result
    }
}

#endif // ENABLE_SWIFTUI
