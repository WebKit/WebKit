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

#if USE_APPLE_INTERNAL_SDK || (!os(tvOS) && !os(watchOS))

import Foundation
#if compiler(>=6.0)
internal import WebKit_Internal
#else
@_implementationOnly import WebKit_Internal
#endif

// FIXME: Adopt `@objc @implementation` when support for macOS Sonoma is no longer needed.
// FIXME: (rdar://110719676) Remove all `@objc deinit`s when support for macOS Sonoma is no longer needed.

@_objcImplementation extension WKTextExtractionItem {
    let rectInWebView: CGRect
    let children: [WKTextExtractionItem]

    @objc
    fileprivate init(with rectInWebView: CGRect, children: [WKTextExtractionItem]) {
        self.rectInWebView = rectInWebView
        self.children = children
    }

#if compiler(<6.0)
    @objc deinit { }
#endif
}

@_objcImplementation extension WKTextExtractionContainerItem {
    let container: WKTextExtractionContainer

    init(container: WKTextExtractionContainer, rectInWebView: CGRect, children: [WKTextExtractionItem]) {
        self.container = container
        super.init(with: rectInWebView, children: children)
    }

#if compiler(<6.0)
    @objc deinit { }
#endif
}

@_objcImplementation extension WKTextExtractionEditable {
    let label: String
    let placeholder: String

    // Properties with a customized getter are incorrectly mapped when using ObjCImplementation.
    @nonobjc private let backingIsSecure: Bool
    @objc(secure)
    var isSecure: Bool {
        @objc(isSecure)
        get { backingIsSecure }
    }

    // Properties with a customized getter are incorrectly mapped when using ObjCImplementation.
    @nonobjc private let backingIsFocused: Bool
    @objc(focused)
    var isFocused: Bool {
        @objc(isFocused)
        get { backingIsFocused }
    }

    init(label: String, placeholder: String, isSecure: Bool, isFocused: Bool) {
        self.label = label
        self.placeholder = placeholder
        self.backingIsSecure = isSecure
        self.backingIsFocused = isFocused
    }

#if compiler(<6.0)
    @objc deinit { }
#endif
}

@_objcImplementation extension WKTextExtractionLink {
    // Used to workaround the fact that `@_objcImplementation` does not support stored properties whose size can change
    // due to Library Evolution. Do not use this property directly.
    @nonobjc private let _url: NSURL

    var url: URL { _url as URL }

    let range: NSRange

    @objc(initWithURL:range:)
    init(url: URL, range: NSRange) {
        self._url = url as NSURL
        self.range = range
    }

#if compiler(<6.0)
    @objc deinit { }
#endif
}

@_objcImplementation extension WKTextExtractionTextItem {
    let content: String
    let selectedRange: NSRange
    let links: [WKTextExtractionLink]
    let editable: WKTextExtractionEditable?

    init(content: String, selectedRange: NSRange, links: [WKTextExtractionLink], editable: WKTextExtractionEditable?, rectInWebView: CGRect, children: [WKTextExtractionItem]) {
        self.content = content
        self.selectedRange = selectedRange
        self.links = links
        self.editable = editable
        super.init(with: rectInWebView, children: children)
    }

#if compiler(<6.0)
    @objc deinit { }
#endif
}

@_objcImplementation extension WKTextExtractionScrollableItem {
    let contentSize: CGSize

    init(contentSize: CGSize, rectInWebView: CGRect, children: [WKTextExtractionItem]) {
        self.contentSize = contentSize
        super.init(with: rectInWebView, children: children)
    }

#if compiler(<6.0)
    @objc deinit { }
#endif
}

@_objcImplementation extension WKTextExtractionImageItem {
    let name: String
    let altText: String

    init(name: String, altText: String, rectInWebView: CGRect, children: [WKTextExtractionItem]) {
        self.name = name
        self.altText = altText
        super.init(with: rectInWebView, children: children)
    }

#if compiler(<6.0)
    @objc deinit { }
#endif
}

#endif // USE_APPLE_INTERNAL_SDK || (!os(tvOS) && !os(watchOS))
