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
    @objc public let rectInWebView: CGRect
    @objc public let children: [WKTextExtractionItem]

    public init(with rectInWebView: CGRect, children: [WKTextExtractionItem]) {
        self.rectInWebView = rectInWebView
        self.children = children
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc public enum WKTextExtractionContainer: Int {
    case root
    case viewportConstrained
    case list
    case listItem
    case blockQuote
    case article
    case section
    case nav
    case button
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionContainerItem) public class WKTextExtractionContainerItem: WKTextExtractionItem {
    @objc public let container: WKTextExtractionContainer

    @objc public init(container: WKTextExtractionContainer, rectInWebView: CGRect, children: [WKTextExtractionItem]) {
        self.container = container
        super.init(with: rectInWebView, children: children)
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionEditable) public class WKTextExtractionEditable: NSObject {
    @objc public let label: String
    @objc public let placeholder: String
    @objc public let isSecure: Bool
    @objc public let isFocused: Bool

    @objc public init(label: String, placeholder: String, isSecure: Bool, isFocused: Bool) {
        self.label = label
        self.placeholder = placeholder
        self.isSecure = isSecure
        self.isFocused = isFocused
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionLink) public class WKTextExtractionLink: NSObject {
    @objc public let url: NSURL
    @objc public let range: NSRange

    @objc(initWithURL:range:) public init(url: NSURL, range: NSRange) {
        self.url = url
        self.range = range
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionTextItem) public class WKTextExtractionTextItem: WKTextExtractionItem {
    @objc public let content: String
    @objc public let selectedRange: NSRange
    @objc public let links: [WKTextExtractionLink]
    @objc public let editable: WKTextExtractionEditable?

    @objc public init(content: String, selectedRange: NSRange, links: [WKTextExtractionLink], editable: WKTextExtractionEditable?, rectInWebView: CGRect, children: [WKTextExtractionItem]) {
        self.content = content
        self.selectedRange = selectedRange
        self.links = links
        self.editable = editable
        super.init(with: rectInWebView, children: children)
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionScrollableItem) public class WKTextExtractionScrollableItem: WKTextExtractionItem {
    @objc public let contentSize: CGSize

    @objc public init(contentSize: CGSize, rectInWebView: CGRect, children: [WKTextExtractionItem]) {
        self.contentSize = contentSize
        super.init(with: rectInWebView, children: children)
    }
}

@available(iOS 17.0, macOS 12.0, *)
@objc(WKTextExtractionImageItem) public class WKTextExtractionImageItem: WKTextExtractionItem {
    @objc public let name: String
    @objc public let altText: String

    @objc public init(name: String, altText: String, rectInWebView: CGRect, children: [WKTextExtractionItem]) {
        self.name = name
        self.altText = altText
        super.init(with: rectInWebView, children: children)
    }
}

@objc(WKTextExtractionRequest) class WKTextExtractionRequest: NSObject {
    @objc public let rectInWebView: CGRect
    private var completionHandler: ((WKTextExtractionItem?) -> Void)?

    @objc public init(rectInWebView: CGRect, _ completionHandler: @escaping (WKTextExtractionItem?) -> Void) {
        self.rectInWebView = rectInWebView
        self.completionHandler = completionHandler
    }

    @objc(fulfill:) public func fulfill(item: WKTextExtractionItem) {
        guard let completionHandler = self.completionHandler else { return }
        completionHandler(item)
        self.completionHandler = nil
    }
}
