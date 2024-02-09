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

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionItem) public class WKTextExtractionItem: NSObject {
    public let rectInRootView: CGRect
    @objc public var children: [WKTextExtractionItem] = []

    @objc public init(with rectInRootView: CGRect) {
        self.rectInRootView = rectInRootView
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc public enum WKTextExtractionContainer: Int {
    case root
    case viewportConstrained
    case link
    case list
    case listItem
    case blockQuote
    case article
    case section
    case nav
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionContainerItem) public class WKTextExtractionContainerItem: WKTextExtractionItem {
    public let container: WKTextExtractionContainer

    @objc public init(container: WKTextExtractionContainer, rectInRootView: CGRect) {
        self.container = container
        super.init(with: rectInRootView)
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionTextItem) public class WKTextExtractionTextItem: WKTextExtractionItem {
    public let content: String

    @objc public init(content: String, rectInRootView: CGRect) {
        self.content = content
        super.init(with: rectInRootView)
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionScrollableItem) public class WKTextExtractionScrollableItem: WKTextExtractionItem {
    public let contentSize: CGSize

    @objc public init(contentSize: CGSize, rectInRootView: CGRect) {
        self.contentSize = contentSize
        super.init(with: rectInRootView)
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionEditableItem) public class WKTextExtractionEditableItem: WKTextExtractionItem {
    public let isFocused: Bool

    @objc public init(isFocused: Bool = false, rectInRootView: CGRect) {
        self.isFocused = isFocused
        super.init(with: rectInRootView)
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionInteractiveItem) public class WKTextExtractionInteractiveItem: WKTextExtractionItem {
    public let isEnabled: Bool

    @objc public init(isEnabled: Bool = false, rectInRootView: CGRect) {
        self.isEnabled = isEnabled
        super.init(with: rectInRootView)
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionImageItem) public class WKTextExtractionImageItem: WKTextExtractionItem {
    public let name: String
    public let altText: String

    @objc public init(name: String, altText: String, rectInRootView: CGRect) {
        self.name = name
        self.altText = altText
        super.init(with: rectInRootView)
    }
}
