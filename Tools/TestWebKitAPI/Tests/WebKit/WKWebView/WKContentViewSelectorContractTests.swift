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

#if WTF_PLATFORM_IOS_FAMILY

import CoreGraphics
import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
private import UIKit
import WebKit
private import WebKit_Private.WKWebViewConfigurationPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKFocusedElementInfo
private import WebKit_Private._WKFormInputSession
private import WebKit_Private._WKInputDelegate

#if HAVE_LINK_PREVIEW && USE_UICONTEXTMENU
private import TestWebKitAPILibrary.Helpers.cocoa.TestContextMenuDriver
#endif

import struct Swift.String

// Software outside WebKit, such as accessibility features, overrides or calls these WKContentView methods by selector.
// These tests fail if one of them is renamed, changes type, or stops being on WebKit's own call path.
// If one of these tests fails after a refactor, keep the old method working, for example as a method that forwards
// to the new code, rather than changing the test.

/// The Objective-C type encodings of an instance method's return value and of its arguments after self and _cmd.
private struct MethodTypeEncodings {
    let returnType: String
    let argumentTypes: [String]

    init?(_ targetClass: AnyClass?, _ selector: Selector) {
        guard let targetClass, let method = unsafe class_getInstanceMethod(targetClass, selector) else {
            return nil
        }

        let argumentCount = unsafe method_getNumberOfArguments(method)
        returnType = unsafe Self.string(consuming: method_copyReturnType(method))
        argumentTypes = (2..<argumentCount).map { index in unsafe Self.string(consuming: method_copyArgumentType(method, index)) }
    }

    private static func string(consuming encoding: UnsafeMutablePointer<CChar>?) -> String {
        guard let encoding = unsafe encoding else {
            return ""
        }

        defer { unsafe free(encoding) }
        return unsafe String(cString: encoding)
    }
}

/// The current implementation of an instance method, as a C function of the given type.
private func implementation<Function>(of targetClass: AnyClass, _ selector: Selector, as type: Function.Type) -> Function? {
    guard let method = unsafe class_getInstanceMethod(targetClass, selector) else {
        return nil
    }

    return unsafe unsafeBitCast(method_getImplementation(method), to: type)
}

/// Runs `body` with each method in `replacements` swizzled into `targetClass`.
@MainActor
private func withSwizzledInstanceMethods(
    of targetClass: AnyClass,
    _ replacements: [(name: Selector, implementation: Any)],
    perform body: () async throws -> Void
) async throws {
    guard let (name, implementation) = replacements.first else {
        try await body()
        return
    }

    try await withSwizzledObjectiveCInstanceMethod(replacing: targetClass, name: name, with: implementation) {
        try await withSwizzledInstanceMethods(of: targetClass, Array(replacements.dropFirst()), perform: body)
    }
}

private typealias MethodWithoutArguments = @convention(c) (UIView, Selector) -> Void

@MainActor
private final class InputSessionStartingDelegate: NSObject, @preconcurrency _WKInputDelegate {
    var didStartInputSession = false

    // swift-format-ignore: NoLeadingUnderscores
    func _webView(_ webView: WKWebView, decidePolicyForFocusedElement info: any _WKFocusedElementInfo) -> _WKFocusStartsInputSessionPolicy {
        .allow
    }

    // swift-format-ignore: NoLeadingUnderscores
    func _webView(_ webView: WKWebView, didStart inputSession: any _WKFormInputSession) {
        didStartInputSession = true
    }
}

@MainActor
struct WKContentViewSelectorContractTests {
    private func expectMethod(
        _ selectorName: String,
        of className: String = "WKContentView",
        returns returnType: String,
        arguments argumentTypes: [String] = [],
        sourceLocation: SourceLocation = #_sourceLocation
    ) {
        let encodings = MethodTypeEncodings(NSClassFromString(className), NSSelectorFromString(selectorName))
        #expect(encodings != nil, "\(className) no longer responds to -\(selectorName)", sourceLocation: sourceLocation)
        guard let encodings else {
            return
        }

        #expect(encodings.returnType == returnType, "return type of -\(selectorName)", sourceLocation: sourceLocation)
        #expect(encodings.argumentTypes.count == argumentTypes.count, "argument count of -\(selectorName)", sourceLocation: sourceLocation)
        for (index, (argumentType, expectedArgumentType)) in zip(encodings.argumentTypes, argumentTypes).enumerated() {
            #expect(argumentType == expectedArgumentType, "argument \(index) of -\(selectorName)", sourceLocation: sourceLocation)
        }
    }

    // Runs a script the way TestWKWebView's -evaluateJavaScriptAndWaitForInputSessionToChange: does, which Swift can't call,
    // but without its wait. Each caller waits for the effect it checks instead.
    private func evaluateJavaScriptChangingInputSession(
        _ script: String,
        in webView: TestWKWebView,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws {
        webView.window?.makeKey()
        let contentView: UIView = try #require(webView.textInputContentView, sourceLocation: sourceLocation)
        contentView.becomeFirstResponder()
        await webView.nextPresentationUpdate()
        try await webView.callJavaScript { script }
    }

    @Test
    func methodsExistWithExpectedTypes() throws {
        // Swift has no @encode, so read each encoding from a method known to use the type. Like @encode, this gives the
        // encodings of the architecture the test runs on.
        let voidType = try #require(MethodTypeEncodings(UIView.self, #selector(UIView.layoutSubviews))?.returnType)
        let objectType = try #require(MethodTypeEncodings(UIView.self, #selector(getter: UIView.superview))?.returnType)
        let boolType = try #require(MethodTypeEncodings(NSObject.self, #selector(NSObject.isProxy))?.returnType)
        let blockType = try #require(
            MethodTypeEncodings(BlockOperation.self, #selector(BlockOperation.addExecutionBlock(_:)))?.argumentTypes.first
        )
        let integerType = try #require(MethodTypeEncodings(UIView.self, #selector(getter: UIView.tag))?.returnType)
        let unsignedIntegerType = try #require(MethodTypeEncodings(NSArray.self, #selector(getter: NSArray.count))?.returnType)
        let pointType = try #require(MethodTypeEncodings(UIView.self, #selector(getter: UIView.center))?.returnType)
        let rectType = try #require(MethodTypeEncodings(UIView.self, #selector(getter: UIView.frame))?.returnType)
        let selectorType = try #require(MethodTypeEncodings(NSObject.self, #selector(NSObject.responds(to:)))?.argumentTypes.first)

        // Overridden or called by accessibility features.
        expectMethod("webView", returns: objectType)
        expectMethod("_setAccessibilityWebProcessToken:", returns: voidType, arguments: [objectType])
        expectMethod("_accessibilityRegisterUIProcessTokens", returns: voidType)
        expectMethod("_processDidExit", returns: voidType)
        expectMethod("_didRelaunchProcess", returns: voidType)
        expectMethod("_selectionChanged", returns: voidType)
        expectMethod("_updateChangedSelection", returns: voidType)
        expectMethod("_requiresKeyboardWhenFirstResponder", returns: boolType)
        expectMethod("_zoomToRevealFocusedElement", returns: voidType)
        expectMethod("_hideKeyboard:", returns: voidType, arguments: [boolType])
        expectMethod("shareForWebView:", returns: voidType, arguments: [objectType])
        expectMethod("handleKeyWebEvent:withCompletionHandler:", returns: voidType, arguments: [objectType, blockType])
        #if USE_BROWSERENGINEKIT
        expectMethod("handleKeyEntry:withCompletionHandler:", returns: voidType, arguments: [objectType, blockType])
        #endif
        for action in [
            "copy:", "cut:", "paste:", "selectAll:", "toggleBoldface:", "toggleItalics:", "toggleUnderline:", "increaseSizeForWebView:",
            "decreaseSizeForWebView:",
        ] {
            expectMethod(action, returns: voidType, arguments: [objectType])
        }
        expectMethod("undoManagerForWebView", returns: objectType)
        expectMethod("canBecomeFocused", returns: boolType)

        // Context menus and actions.
        #if HAVE_LINK_PREVIEW && USE_UICONTEXTMENU
        expectMethod("continueContextMenuInteraction:completion:", returns: voidType, arguments: [objectType, blockType])
        expectMethod(
            "_contextMenuInteraction:overrideSuggestedActionsForConfiguration:",
            returns: objectType,
            arguments: [objectType, objectType]
        )
        #if ENABLE_DATA_DETECTION
        expectMethod("continueContextMenuInteractionWithDataDetectors:", returns: voidType, arguments: [blockType])
        #endif
        #endif
        expectMethod("_contentsOfUserInterfaceItem:", returns: objectType, arguments: [objectType])
        expectMethod("canPerformAction:withSender:", returns: boolType, arguments: [selectorType, objectType])

        // Speech.
        expectMethod("selectedText", returns: objectType)
        expectMethod("selectedTextRange", returns: objectType)
        expectMethod("accessibilityRetrieveSpeakSelectionContent", returns: voidType)
        expectMethod(
            "_accessibilityRetrieveRectsEnclosingSelectionOffset:withGranularity:",
            returns: voidType,
            arguments: [integerType, integerType]
        )
        expectMethod("_accessibilityRetrieveRectsAtSelectionOffset:withText:", returns: voidType, arguments: [integerType, objectType])
        expectMethod("_accessibilityStoreSelection", returns: voidType)
        expectMethod("_accessibilityClearSelection", returns: voidType)

        // Internal classes that software outside WebKit validates.
        expectMethod("selectedTextLength", of: "WKTextRange", returns: unsignedIntegerType)
        expectMethod("rect", of: "WebSelectionRect", returns: rectType)

        // Text selection.
        expectMethod("setUpTextSelectionAssistant", returns: voidType)
        expectMethod("clearSelection", returns: voidType)
        expectMethod("webSelectionRects", returns: objectType)
        expectMethod("selectForWebView:", returns: voidType, arguments: [objectType])
        expectMethod("interactionAssistant", returns: objectType)
        expectMethod("hasSelectablePositionAtPoint:", returns: boolType, arguments: [pointType])
        expectMethod(
            "selectTextWithGranularity:atPoint:completionHandler:",
            returns: voidType,
            arguments: [integerType, pointType, blockType]
        )
        expectMethod(
            "updateSelectionWithExtentPoint:withBoundary:completionHandler:",
            returns: voidType,
            arguments: [pointType, integerType, blockType]
        )
        expectMethod("_nonBlockingDoubleTapRecognized:", returns: voidType, arguments: [objectType])

        // Keyboard and text input.
        expectMethod("textInputTraits", returns: objectType)
        expectMethod("_disableAutomaticKeyboardUI", returns: boolType)
        #if ENABLE_IMAGE_ANALYSIS
        expectMethod("captureTextFromCamera:", returns: voidType, arguments: [objectType])
        #endif

        // Replaced by software outside WebKit, so only its existence matters here.
        #expect(
            MethodTypeEncodings(
                NSClassFromString("WKContentView"),
                NSSelectorFromString("_elementDidFocus:userIsInteracting:blurPreviousNode:activityStateChanges:userObject:")
            ) != nil
        )
    }

    @Test
    func focusAndBlurDispatchThroughContentView() async throws {
        typealias HideKeyboard = @convention(c) (UIView, Selector, ObjCBool) -> Void

        let contentViewClass: AnyClass = try #require(NSClassFromString("WKContentView"))
        let hideKeyboardSelector = NSSelectorFromString("_hideKeyboard:")
        let zoomToRevealFocusedElementSelector = NSSelectorFromString("_zoomToRevealFocusedElement")
        let originalHideKeyboard = try #require(implementation(of: contentViewClass, hideKeyboardSelector, as: HideKeyboard.self))
        let originalZoomToRevealFocusedElement = try #require(
            implementation(of: contentViewClass, zoomToRevealFocusedElementSelector, as: MethodWithoutArguments.self)
        )

        var contentViewUnderTest: UIView?
        var hideKeyboardCallCount = 0
        let hideKeyboardSpy: @convention(block) (UIView, ObjCBool) -> Void = { contentView, editableChanged in
            if contentView === contentViewUnderTest {
                hideKeyboardCallCount += 1
            }
            originalHideKeyboard(contentView, hideKeyboardSelector, editableChanged)
        }

        var zoomToRevealFocusedElementCallCount = 0
        let zoomToRevealFocusedElementSpy: @convention(block) (UIView) -> Void = { contentView in
            if contentView === contentViewUnderTest {
                zoomToRevealFocusedElementCallCount += 1
            }
            originalZoomToRevealFocusedElement(contentView, zoomToRevealFocusedElementSelector)
        }

        try await withSwizzledInstanceMethods(
            of: contentViewClass,
            [(hideKeyboardSelector, hideKeyboardSpy), (zoomToRevealFocusedElementSelector, zoomToRevealFocusedElementSpy)]
        ) {
            let inputDelegate = InputSessionStartingDelegate()
            let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 390, height: 844), configuration: WKWebViewConfiguration())
            defer { extendLifetime(inputDelegate) }
            let contentView: UIView = try #require(webView.textInputContentView)
            contentViewUnderTest = contentView
            webView._inputDelegate = inputDelegate
            try await webView.load(
                html: """
                    <meta name='viewport' content='width=device-width, initial-scale=1'>\
                    <body style='margin: 0; height: 200vh;'>\
                    <input id='input' \
                    style='position: absolute; right: 10px; top: calc(100vh - 50px); width: 150px; height: 30px; font-size: 12px;'>\
                    </body>
                    """
            )
            await webView.nextPresentationUpdate()

            // Accessibility features override -_zoomToRevealFocusedElement and -_hideKeyboard: to learn when a form control gains
            // and loses focus.
            zoomToRevealFocusedElementCallCount = 0
            try await evaluateJavaScriptChangingInputSession("input.focus()", in: webView)
            try await waitForCondition("focusing the input to start an input session", timeout: .seconds(10)) {
                inputDelegate.didStartInputSession
            }
            try await waitForCondition("focusing the input to call -_zoomToRevealFocusedElement", timeout: .seconds(10)) {
                zoomToRevealFocusedElementCallCount > 0
            }

            hideKeyboardCallCount = 0
            try await evaluateJavaScriptChangingInputSession("input.blur()", in: webView)
            try await waitForCondition("blurring the input to call -_hideKeyboard:", timeout: .seconds(10)) {
                hideKeyboardCallCount > 0
            }
        }
    }

    @Test
    func selectionChangeDispatchesThroughContentView() async throws {
        let contentViewClass: AnyClass = try #require(NSClassFromString("WKContentView"))
        let selectionChangedSelector = NSSelectorFromString("_selectionChanged")
        let originalSelectionChanged = try #require(
            implementation(of: contentViewClass, selectionChangedSelector, as: MethodWithoutArguments.self)
        )

        var contentViewUnderTest: UIView?
        var selectionChangedCallCount = 0
        let selectionChangedSpy: @convention(block) (UIView) -> Void = { contentView in
            if contentView === contentViewUnderTest {
                selectionChangedCallCount += 1
            }
            originalSelectionChanged(contentView, selectionChangedSelector)
        }

        try await withSwizzledInstanceMethods(of: contentViewClass, [(selectionChangedSelector, selectionChangedSpy)]) {
            let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 320, height: 500), configuration: WKWebViewConfiguration())
            try await webView.load(html: "<body contenteditable><p id='text'>Hello world</p></body>")
            await webView.nextPresentationUpdate()

            let contentView: UIView = try #require(webView.textInputContentView)
            contentViewUnderTest = contentView
            selectionChangedCallCount = 0
            try await webView.callJavaScript { "document.body.focus(); getSelection().selectAllChildren(text)" }
            await webView.nextPresentationUpdate()

            // Accessibility features observe selection changes here.
            try await waitForCondition("changing the selection to call -_selectionChanged", timeout: .seconds(10)) {
                selectionChangedCallCount > 0
            }
        }
    }

    @Test
    func interactionSetUpDispatchesThroughContentView() async throws {
        let contentViewClass: AnyClass = try #require(NSClassFromString("WKContentView"))
        let setUpTextSelectionAssistantSelector = NSSelectorFromString("setUpTextSelectionAssistant")
        let originalSetUpTextSelectionAssistant = try #require(
            implementation(of: contentViewClass, setUpTextSelectionAssistantSelector, as: MethodWithoutArguments.self)
        )

        var contentViewUnderTest: UIView?
        var setUpTextSelectionAssistantCallCount = 0
        let setUpTextSelectionAssistantSpy: @convention(block) (UIView) -> Void = { contentView in
            if contentView === contentViewUnderTest {
                setUpTextSelectionAssistantCallCount += 1
            }
            originalSetUpTextSelectionAssistant(contentView, setUpTextSelectionAssistantSelector)
        }

        try await withSwizzledInstanceMethods(
            of: contentViewClass,
            [(setUpTextSelectionAssistantSelector, setUpTextSelectionAssistantSpy)]
        ) {
            // Software outside WebKit customizes text interaction from this method. -setUpInteraction calls it once the web
            // process is running. Delay the process launch, which iOS only does by default with site isolation, so that the
            // call comes after the web view is created. Then launch the process without loading anything, because committing
            // a load also reaches this method through -_hideKeyboard:.
            let configuration = WKWebViewConfiguration()
            configuration._delaysWebProcessLaunchUntilFirstLoad = true
            let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 320, height: 500), configuration: configuration)
            defer { extendLifetime(webView) }
            let contentView: UIView = try #require(webView.textInputContentView)
            contentViewUnderTest = contentView
            webView._launchInitialProcessIfNecessary()
            try await waitForCondition("launching the web process to call -setUpTextSelectionAssistant", timeout: .seconds(10)) {
                setUpTextSelectionAssistantCallCount > 0
            }
        }
    }

    #if HAVE_LINK_PREVIEW && USE_UICONTEXTMENU
    @Test
    func contextMenuDispatchesThroughContentView() async throws {
        typealias ContextMenuCompletion = @convention(block) (UIContextMenuConfiguration?) -> Void
        typealias ContinueContextMenuInteraction =
            @convention(c) (UIView, Selector, UIContextMenuInteraction, ContextMenuCompletion) -> Void
        typealias ContinueContextMenuInteractionSpy = @convention(block) (UIView, UIContextMenuInteraction, ContextMenuCompletion) -> Void

        let contentViewClass: AnyClass = try #require(NSClassFromString("WKContentView"))
        let continueSelector = NSSelectorFromString("continueContextMenuInteraction:completion:")
        let originalContinue = try #require(implementation(of: contentViewClass, continueSelector, as: ContinueContextMenuInteraction.self))

        let contentsSelector = NSSelectorFromString("_contentsOfUserInterfaceItem:")
        var contentViewUnderTest: UIView?
        var continueContextMenuInteractionCalled = false
        var contextMenuContentsWhenContinuing: NSDictionary?
        let continueSpy: ContinueContextMenuInteractionSpy = { contentView, interaction, completion in
            if contentView === contentViewUnderTest {
                continueContextMenuInteractionCalled = true
                // Accessibility features read these keys here. -_contentsOfUserInterfaceItem: is declared on WKWebView
                // (WKWebViewPrivateForTesting.h), but WKContentView implements it too.
                let contents = unsafe contentView.perform(contentsSelector, with: "contextMenu")?.takeUnretainedValue()
                contextMenuContentsWhenContinuing = contents as? NSDictionary
            }
            originalContinue(contentView, continueSelector, interaction, completion)
        }

        var replacements: [(name: Selector, implementation: Any)] = [(continueSelector, continueSpy)]

        #if ENABLE_DATA_DETECTION
        typealias ContinueContextMenuInteractionWithDataDetectors = @convention(c) (UIView, Selector, ContextMenuCompletion) -> Void

        let dataDetectorsSelector = NSSelectorFromString("continueContextMenuInteractionWithDataDetectors:")
        let originalDataDetectors = try #require(
            implementation(of: contentViewClass, dataDetectorsSelector, as: ContinueContextMenuInteractionWithDataDetectors.self)
        )

        var continueContextMenuInteractionWithDataDetectorsCalled = false
        let dataDetectorsSpy: @convention(block) (UIView, ContextMenuCompletion) -> Void = { contentView, completion in
            if contentView === contentViewUnderTest {
                continueContextMenuInteractionWithDataDetectorsCalled = true
            }
            originalDataDetectors(contentView, dataDetectorsSelector, completion)
        }
        replacements.append((dataDetectorsSelector, dataDetectorsSpy))
        #endif

        try await withSwizzledInstanceMethods(of: contentViewClass, replacements) {
            // Drives the menu like ContextMenu.End, but without a UI delegate, so the flow also reaches data detectors.
            let driver = TestContextMenuDriver()
            let configuration = WKWebViewConfiguration()
            configuration._clickInteractionDriverForTesting = driver
            let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 200, height: 200), configuration: configuration)
            defer { extendLifetime(webView) }
            let contentView: UIView = try #require(webView.textInputContentView)
            contentViewUnderTest = contentView
            try await webView.load(html: "<a href='https://webkit.org/'>This is a link</a>")

            let didBegin = await driver.begin()
            driver.end()
            #expect(didBegin)
            try await waitForCondition(
                "the context menu interaction to call -continueContextMenuInteraction:completion:",
                timeout: .seconds(10)
            ) {
                continueContextMenuInteractionCalled
            }
            #if ENABLE_DATA_DETECTION
            try await waitForCondition(
                "the context menu interaction to call -continueContextMenuInteractionWithDataDetectors:",
                timeout: .seconds(10)
            ) {
                continueContextMenuInteractionWithDataDetectorsCalled
            }
            #endif

            let contents = contextMenuContentsWhenContinuing?["contextMenu"] as? NSDictionary
            #expect(contents?["isLink"] is NSNumber)
            #expect(contents?["isImage"] is NSNumber)
            #expect((contents?["isLink"] as? NSNumber)?.boolValue == true)
            #expect((contents?["isImage"] as? NSNumber)?.boolValue == false)
        }
    }
    #endif // HAVE_LINK_PREVIEW && USE_UICONTEXTMENU
}

#endif // WTF_PLATFORM_IOS_FAMILY
