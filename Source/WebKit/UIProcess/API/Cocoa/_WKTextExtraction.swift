// Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#if ENABLE_TEXT_EXTRACTION

import Foundation
#if compiler(>=6.0)
internal import WebKit_Internal
#else
@_implementationOnly import WebKit_Internal
#endif

// FIXME: Adopt `@objc @implementation` when support for macOS Sonoma is no longer needed.
// FIXME: (rdar://110719676) Remove all `@objc deinit`s when support for macOS Sonoma is no longer needed.
@_objcImplementation
extension WKTextExtractionItem {
    let rectInWebView: CGRect
    let children: [WKTextExtractionItem]
    let eventListeners: WKTextExtractionEventListenerTypes
    let ariaAttributes: [String: String]
    let accessibilityRole: String
    let nodeIdentifier: String?

    @objc
    fileprivate init(
        with rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.rectInWebView = rectInWebView
        self.children = children
        self.eventListeners = eventListeners
        self.nodeIdentifier = nodeIdentifier
        self.ariaAttributes = ariaAttributes
        self.accessibilityRole = accessibilityRole
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionContainerItem {
    let container: WKTextExtractionContainer

    init(
        container: WKTextExtractionContainer,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.container = container
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionContentEditableItem {
    let contentEditableType: WKTextExtractionEditableType

    @nonobjc
    private let backingIsFocused: Bool
    @objc(focused)
    var isFocused: Bool {
        @objc(isFocused)
        get { backingIsFocused }
    }

    init(
        contentEditableType: WKTextExtractionEditableType,
        isFocused: Bool,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.contentEditableType = contentEditableType
        self.backingIsFocused = isFocused
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionTextFormControlItem {
    final private let editable: WKTextExtractionEditable

    @objc(secure)
    var isSecure: Bool {
        editable.isSecure
    }

    @objc(focused)
    var isFocused: Bool {
        editable.isFocused
    }

    @objc
    var label: String {
        editable.label
    }

    @objc
    var placeholder: String {
        editable.placeholder
    }

    let controlType: String
    let autocomplete: String

    @nonobjc
    private let backingIsReadonly: Bool
    @objc(readonly)
    var isReadonly: Bool {
        @objc(isReadonly)
        get { backingIsReadonly }
    }

    @nonobjc
    private let backingIsDisabled: Bool
    @objc(disabled)
    var isDisabled: Bool {
        @objc(isDisabled)
        get { backingIsDisabled }
    }

    @nonobjc
    private let backingIsChecked: Bool
    @objc(checked)
    var isChecked: Bool {
        @objc(isChecked)
        get { backingIsChecked }
    }

    init(
        editable: WKTextExtractionEditable,
        controlType: String,
        autocomplete: String,
        isReadonly: Bool,
        isDisabled: Bool,
        isChecked: Bool,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.editable = editable
        self.controlType = controlType
        self.autocomplete = autocomplete
        self.backingIsReadonly = isReadonly
        self.backingIsDisabled = isDisabled
        self.backingIsChecked = isChecked
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionEditable {
    let label: String
    let placeholder: String

    // Properties with a customized getter are incorrectly mapped when using ObjCImplementation.
    @nonobjc
    private let backingIsSecure: Bool
    @objc(secure)
    var isSecure: Bool {
        @objc(isSecure)
        get { backingIsSecure }
    }

    // Properties with a customized getter are incorrectly mapped when using ObjCImplementation.
    @nonobjc
    private let backingIsFocused: Bool
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
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionLinkItem {
    let target: String
    @nonobjc
    private let backingURL: NSURL?

    var url: URL? { backingURL as URL? }

    init(
        target: String,
        url: URL?,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.target = target
        self.backingURL = url as NSURL?
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionLink {
    // Used to workaround the fact that `@_objcImplementation` does not support stored properties whose size can change
    // due to Library Evolution. Do not use this property directly.
    @nonobjc
    private let backingURL: NSURL

    var url: URL { backingURL as URL }

    let range: NSRange

    @objc(initWithURL:range:)
    init(url: URL, range: NSRange) {
        self.backingURL = url as NSURL
        self.range = range
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionTextItem {
    var content: String
    var selectedRange: NSRange
    let links: [WKTextExtractionLink]
    let editable: WKTextExtractionEditable?

    init(
        content: String,
        selectedRange: NSRange,
        links: [WKTextExtractionLink],
        editable: WKTextExtractionEditable?,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.content = content
        self.selectedRange = selectedRange
        self.links = links
        self.editable = editable
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionScrollableItem {
    let contentSize: CGSize

    init(
        contentSize: CGSize,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.contentSize = contentSize
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionSelectItem {
    let selectedValues: [String]
    let supportsMultiple: Bool

    init(
        selectedValues: [String],
        supportsMultiple: Bool,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.selectedValues = selectedValues
        self.supportsMultiple = supportsMultiple
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionImageItem {
    let name: String
    let altText: String

    init(
        name: String,
        altText: String,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.name = name
        self.altText = altText
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension _WKTextExtractionInteractionResult {
    let error: (any Error)?

    init(errorDescription: String?) {
        self.error = errorDescription.map {
            NSError(
                domain: WKErrorDomain,
                code: WKError.unknown.rawValue,
                userInfo: [
                    NSDebugDescriptionErrorKey: $0
                ]
            )
        }
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension _WKTextExtractionInteraction {
    let action: _WKTextExtractionAction

    var nodeIdentifier: String? = nil
    var text: String? = nil
    var replaceAll: Bool = false
    var scrollToVisible: Bool = false
    var scrollDelta: CGSize = .zero

    private final var backingLocation: CGPoint? = nil

    var location: CGPoint {
        get { backingLocation ?? .zero }
        set { backingLocation = newValue }
    }

    var hasSetLocation: Bool {
        backingLocation != nil
    }

    init(action: _WKTextExtractionAction) {
        self.action = action
    }

    @objc(debugDescriptionInWebView:completionHandler:)
    func debugDescription(in webView: WKWebView) async throws -> String {
        try await webView._describe(interaction: self)
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension _WKTextExtractionResult {
    let textContent: String
    let filteredOutAnyText: Bool

    init(textContent: String, filteredOutAnyText: Bool) {
        self.textContent = textContent
        self.filteredOutAnyText = filteredOutAnyText
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension _WKTextExtractionConfiguration {
    @objc(configurationForVisibleTextOnly)
    class var visibleTextOnly: _WKTextExtractionConfiguration {
        .init(onlyVisibleText: true)
    }

    var outputFormat: _WKTextExtractionOutputFormat = .textTree
    var nodeIdentifierInclusion: _WKTextExtractionNodeIdentifierInclusion
    var filterOptions: _WKTextExtractionFilterOptions = .all
    var targetNode: _WKJSHandle? = nil

    var targetRect: CGRect = CGRectNull

    var includeURLs: Bool
    var includeRects: Bool
    var includeEventListeners: Bool
    var includeAccessibilityAttributes: Bool
    var includeTextInAutoFilledControls: Bool

    var mergeParagraphs: Bool = false
    var skipNearlyTransparentContent: Bool = false
    var onlyIncludeVisibleText: Bool

    var maxWordsPerParagraph: Int = .max

    var replacementStrings: [String: String]? = nil

    final private var clientNodeAttributes: [String: [_WKJSHandle: String]] = [:]

    @objc
    private init(onlyVisibleText: Bool) {
        self.includeURLs = !onlyVisibleText
        self.includeRects = !onlyVisibleText
        self.nodeIdentifierInclusion = onlyVisibleText ? .none : .interactive
        self.includeEventListeners = !onlyVisibleText
        self.includeAccessibilityAttributes = !onlyVisibleText
        self.includeTextInAutoFilledControls = !onlyVisibleText
        self.onlyIncludeVisibleText = onlyVisibleText
        self.targetRect = CGRectNull
    }

    override convenience init() {
        self.init(onlyVisibleText: false)
    }

    func forEachClientNodeAttribute(_ block: (String, String, _WKJSHandle) -> Void) {
        for (attribute, values) in clientNodeAttributes {
            for (handle, value) in values {
                block(attribute, value, handle)
            }
        }
    }

    func addClientAttribute(_ attributeName: String, value attributeValue: String, forNode node: _WKJSHandle) {
        clientNodeAttributes[attributeName, default: [:]][node] = attributeValue
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

#endif // ENABLE_TEXT_EXTRACTION
