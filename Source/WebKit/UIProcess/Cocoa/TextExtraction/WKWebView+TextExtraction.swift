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

#if HAVE_UIINTELLIGENCESUPPORT_FRAMEWORK

#if compiler(>=6.0)
internal import WebKit_Internal
internal import WebKit_Private
#else
@_implementationOnly import WebKit_Internal
@_implementationOnly import WebKit_Private
#endif

@_spiOnly import UIIntelligenceSupport

#if canImport(UIKit)
@_spi(UIIntelligenceSupport) import UIKit
#else
@_spi(UIIntelligenceSupport) import AppKit
#endif

#if ENABLE_TEXT_EXTRACTION

private func createEditable(for editable: WKTextExtractionEditable?) -> IntelligenceElement.Text.Editable? {
    guard let editable else {
        return nil
    }

    return .init(
        label: editable.label,
        prompt: editable.placeholder,
        contentType: nil,
        isSecure: editable.isSecure,
        isFocused: editable.isFocused
    )
}

private func createElementContent(for item: WKTextExtractionItem) -> IntelligenceElement.Content {
    switch item {
    case let text as WKTextExtractionTextItem:
        var content = AttributedString(text.content)
        if text.selectedRange.location != NSNotFound {
            if let range = Range(text.selectedRange, in: content) {
                content[range].intelligenceSelected = true
            }
        }
        for link in text.links {
            if let range = Range(link.range, in: content) {
                content[range].intelligenceLink = link.url as URL
            }
        }
        return .text(IntelligenceElement.Text(attributedText: content, editable: createEditable(for: text.editable)))
    case let image as WKTextExtractionImageItem:
        return .image(IntelligenceElement.Image(name: image.name, textDescription: image.altText))
    default:
        return .base
    }
}

private func createIntelligenceElement(item: WKTextExtractionItem) -> IntelligenceElement {
    var element = IntelligenceElement(boundingBox: item.rectInWebView, content: createElementContent(for: item))
    element.subelements = item.children.map { child in createIntelligenceElement(item: child) }
    return element
}

#endif // ENABLE_TEXT_EXTRACTION

@_spi(WKIntelligenceSupport)
extension WKWebView {
    // swift-format-ignore: NoLeadingUnderscores
    open override var _intelligenceBaseClass: AnyClass {
        WKWebView.self
    }

    // swift-format-ignore: NoLeadingUnderscores
    open override func _intelligenceCollectContent(in visibleRect: CGRect, collector: UIIntelligenceElementCollector) {
        #if canImport(UIIntelligenceSupport, _version: 9007)
        let context = collector.context.createRemoteContext(description: "WKWebView")
        #else
        let context = collector.context.createRemoteContext()
        #endif
        collector.collect(.remote(context))
    }

    // swift-format-ignore: NoLeadingUnderscores
    open override func _intelligenceCollectRemoteContent(
        in visibleRect: CGRect,
        remoteContextWrapper: UIIntelligenceCollectionRemoteContextWrapper
    ) {
        #if ENABLE_TEXT_EXTRACTION
        Task { @MainActor in
            let coordinator = IntelligenceCollectionCoordinator.shared
            let collector = coordinator.createCollector(remoteContextWrapper: remoteContextWrapper)

            let configuration = _WKTextExtractionConfiguration()
            configuration.targetRect = visibleRect
            configuration.mergeParagraphs = true
            configuration.skipNearlyTransparentContent = true
            configuration.nodeIdentifierInclusion = .none
            configuration.includeEventListeners = false
            configuration.includeAccessibilityAttributes = false
            configuration.filterOptions = []
            if let rootItem = await _requestTextExtraction(configuration) {
                collector.collect(createIntelligenceElement(item: rootItem))
            }

            coordinator.finishCollection(collector)
        }
        #endif // ENABLE_TEXT_EXTRACTION
    }
}

#endif // HAVE_UIINTELLIGENCESUPPORT_FRAMEWORK
