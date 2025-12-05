/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "UIScriptControllerCocoa.h"

#import "CocoaColorSerialization.h"
#import "LayoutTestSpellChecker.h"
#import "PlatformWebView.h"
#import "StringFunctions.h"
#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import "UIScriptContext.h"
#import "WKTextExtractionTestingHelpers.h"
#import "_WKTextExtractionInternal.h"
#import <JavaScriptCore/JavaScriptCore.h>
#import <WebKit/WKURLCF.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKTargetedElementInfo.h>
#import <WebKit/_WKTargetedElementRequest.h>
#import <WebKit/_WKTextExtraction.h>
#import <wtf/BlockPtr.h>
#import <wtf/darwin/DispatchExtras.h>

@interface WKWebView (WKWebViewInternal)
- (void)paste:(id)sender;
@end

namespace WTR {

UIScriptControllerCocoa::UIScriptControllerCocoa(UIScriptContext& context)
    : UIScriptControllerCommon(context)
{
}

TestRunnerWKWebView *UIScriptControllerCocoa::webView() const
{
    return TestController::singleton().mainWebView()->platformView();
}

void UIScriptControllerCocoa::setViewScale(double scale)
{
    webView()._viewScale = scale;
}

void UIScriptControllerCocoa::setMinimumEffectiveWidth(double effectiveWidth)
{
    webView()._minimumEffectiveDeviceWidth = effectiveWidth;
}

void UIScriptControllerCocoa::setWebViewEditable(bool editable)
{
    webView()._editable = editable;
}

void UIScriptControllerCocoa::doAsyncTask(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    dispatch_async(mainDispatchQueueSingleton(), ^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerCocoa::doAfterPresentationUpdate(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView() _doAfterNextPresentationUpdate:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerCocoa::completeTaskAsynchronouslyAfterActivityStateUpdate(unsigned callbackID)
{
    dispatch_async(mainDispatchQueueSingleton(), ^{
        auto* mainWebView = TestController::singleton().mainWebView();
        ASSERT(mainWebView);

        [mainWebView->platformView() _doAfterActivityStateUpdate: ^{
            if (!m_context)
                return;

            m_context->asyncTaskComplete(callbackID);
        }];
    });
}

JSRetainPtr<JSStringRef> UIScriptControllerCocoa::scrollingTreeAsText() const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)[webView() _scrollingTreeAsText]));
}

void UIScriptControllerCocoa::removeViewFromWindow(JSValueRef callback)
{
    // FIXME: On iOS, we never invoke the completion callback that's passed in. Fixing this causes the layout
    // test pageoverlay/overlay-remove-reinsert-view.html to start failing consistently on iOS. It seems like
    // this warrants some more investigation.
#if PLATFORM(MAC)
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
#else
    UNUSED_PARAM(callback);
#endif

    auto* mainWebView = TestController::singleton().mainWebView();
    mainWebView->removeFromWindow();

#if PLATFORM(MAC)
    completeTaskAsynchronouslyAfterActivityStateUpdate(callbackID);
#endif // PLATFORM(MAC)
}

void UIScriptControllerCocoa::addViewToWindow(JSValueRef callback)
{
#if PLATFORM(MAC)
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
#else
    UNUSED_PARAM(callback);
#endif

    auto* mainWebView = TestController::singleton().mainWebView();
    mainWebView->addToWindow();

#if PLATFORM(MAC)
    completeTaskAsynchronouslyAfterActivityStateUpdate(callbackID);
#endif // PLATFORM(MAC)
}

void UIScriptControllerCocoa::overridePreference(JSStringRef preferenceRef, JSStringRef valueRef)
{
    if (toWTFString(preferenceRef) == "WebKitMinimumFontSize"_s)
        webView().configuration.preferences.minimumFontSize = toWTFString(valueRef).toDouble();
}

void UIScriptControllerCocoa::findString(JSStringRef string, unsigned long options, unsigned long maxCount)
{
    [webView() _findString:toWTFString(string).createNSString().get() options:options maxCount:maxCount];
}

JSObjectRef UIScriptControllerCocoa::contentsOfUserInterfaceItem(JSStringRef interfaceItem) const
{
    NSDictionary *contentDictionary = [webView() _contentsOfUserInterfaceItem:toWTFString(interfaceItem).createNSString().get()];
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:contentDictionary inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

void UIScriptControllerCocoa::setDefaultCalendarType(JSStringRef calendarIdentifier, JSStringRef localeIdentifier)
{
    auto cfCalendarIdentifier = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, calendarIdentifier));
    auto cfLocaleIdentifier = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, localeIdentifier));
    TestController::singleton().setDefaultCalendarType((__bridge NSString *)cfCalendarIdentifier.get(), (__bridge NSString *)cfLocaleIdentifier.get());
}

JSRetainPtr<JSStringRef> UIScriptControllerCocoa::lastUndoLabel() const
{
    return adopt(JSStringCreateWithCFString((__bridge CFStringRef)platformUndoManager().undoActionName));
}

JSRetainPtr<JSStringRef> UIScriptControllerCocoa::caLayerTreeAsText() const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)[webView() _caLayerTreeAsText]));
}

JSRetainPtr<JSStringRef> UIScriptControllerCocoa::caLayerTreeAsTextForLayerWithID(uint64_t layerID) const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)[webView() _caLayerTreeAsTextForLayerWithID:layerID]));
}

JSObjectRef UIScriptControllerCocoa::propertiesOfLayerWithID(uint64_t layerID) const
{
    RetainPtr jsValue = [JSValue valueWithObject:[webView() _propertiesOfLayerWithID:layerID] inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]];
    return JSValueToObject(m_context->jsContext(), [jsValue JSValueRef], nullptr);
}

JSRetainPtr<JSStringRef> UIScriptControllerCocoa::firstRedoLabel() const
{
    return adopt(JSStringCreateWithCFString((__bridge CFStringRef)platformUndoManager().redoActionName));
}

NSUndoManager *UIScriptControllerCocoa::platformUndoManager() const
{
    return platformContentView().undoManager;
}

void UIScriptControllerCocoa::setDidShowContextMenuCallback(JSValueRef callback)
{
    UIScriptController::setDidShowContextMenuCallback(callback);
    webView().didShowContextMenuCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidShowContextMenu);
    }).get();
}

void UIScriptControllerCocoa::setDidDismissContextMenuCallback(JSValueRef callback)
{
    UIScriptController::setDidDismissContextMenuCallback(callback);
    webView().didDismissContextMenuCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidDismissContextMenu);
    }).get();
}

bool UIScriptControllerCocoa::isShowingContextMenu() const
{
    return webView().isShowingContextMenu;
}

void UIScriptControllerCocoa::setDidShowMenuCallback(JSValueRef callback)
{
    UIScriptController::setDidShowMenuCallback(callback);
    webView().didShowMenuCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidShowMenu);
    };
}

void UIScriptControllerCocoa::setDidHideMenuCallback(JSValueRef callback)
{
    UIScriptController::setDidHideMenuCallback(callback);
    webView().didHideMenuCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidHideMenu);
    };
}

void UIScriptControllerCocoa::dismissMenu()
{
    [webView() dismissActiveMenu];
}

bool UIScriptControllerCocoa::isShowingMenu() const
{
    return webView().showingMenu;
}

void UIScriptControllerCocoa::setContinuousSpellCheckingEnabled(bool enabled)
{
    [webView() _setContinuousSpellCheckingEnabledForTesting:enabled];
}

void UIScriptControllerCocoa::paste()
{
    [webView() paste:nil];
}

void UIScriptControllerCocoa::insertAttachmentForFilePath(JSStringRef filePath, JSStringRef contentType, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    auto testURL = adoptCF(WKURLCopyCFURL(kCFAllocatorDefault, TestController::singleton().currentTestURL()));
    auto attachmentURL = [NSURL fileURLWithPath:toWTFString(filePath).createNSString().get() relativeToURL:(__bridge NSURL *)testURL.get()];
    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initWithURL:attachmentURL options:0 error:nil]);
    [webView() _insertAttachmentWithFileWrapper:fileWrapper.get() contentType:toWTFString(contentType).createNSString().get() completion:^(BOOL success) {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptControllerCocoa::setDidShowContactPickerCallback(JSValueRef callback)
{
    UIScriptController::setDidShowContactPickerCallback(callback);
    webView().didShowContactPickerCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidShowContactPicker);
    };
}

void UIScriptControllerCocoa::setDidHideContactPickerCallback(JSValueRef callback)
{
    UIScriptController::setDidHideContactPickerCallback(callback);
    webView().didHideContactPickerCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidHideContactPicker);
    };
}

bool UIScriptControllerCocoa::isShowingContactPicker() const
{
    return webView().showingContactPicker;
}

void UIScriptControllerCocoa::dismissContactPickerWithContacts(JSValueRef contacts)
{
    JSContext *context = [JSContext contextWithJSGlobalContextRef:m_context->jsContext()];
    JSValue *value = [JSValue valueWithJSValueRef:contacts inContext:context];
    [webView() _dismissContactPickerWithContacts:[value toArray]];
}

unsigned long UIScriptControllerCocoa::countOfUpdatesWithLayerChanges() const
{
    return webView()._countOfUpdatesWithLayerChanges;
}

#if ENABLE(IMAGE_ANALYSIS)

uint64_t UIScriptControllerCocoa::currentImageAnalysisRequestID() const
{
    return TestController::currentImageAnalysisRequestID();
}

void UIScriptControllerCocoa::installFakeMachineReadableCodeResultsForImageAnalysis()
{
    TestController::singleton().installFakeMachineReadableCodeResultsForImageAnalysis();
}

#endif // ENABLE(IMAGE_ANALYSIS)

void UIScriptControllerCocoa::setSpellCheckerResults(JSValueRef results)
{
    [[LayoutTestSpellChecker checker] setResultsFromJSValue:results inContext:m_context->jsContext()];
}

RetainPtr<_WKTextExtractionConfiguration> createTextExtractionConfiguration(WKWebView *webView, TextExtractionTestOptions* options)
{
    auto extractionRect = CGRectNull;
    if (options && options->clipToBounds)
        extractionRect = webView.bounds;

    RetainPtr configuration = adoptNS([_WKTextExtractionConfiguration new]);
    [configuration setIncludeRects:options && options->includeRects];
    [configuration setIncludeURLs:options && options->includeURLs];
    [configuration setNodeIdentifierInclusion:^{
        if (!options)
            return _WKTextExtractionNodeIdentifierInclusionNone;

        auto inclusion = toWTFString(options->nodeIdentifierInclusion.get());
        if (equalLettersIgnoringASCIICase(inclusion, "interactive"_s))
            return _WKTextExtractionNodeIdentifierInclusionInteractive;

        if (equalLettersIgnoringASCIICase(inclusion, "editableonly"_s))
            return _WKTextExtractionNodeIdentifierInclusionEditableOnly;

        return _WKTextExtractionNodeIdentifierInclusionNone;
    }()];
    [configuration setIncludeEventListeners:options && options->includeEventListeners];
    [configuration setIncludeAccessibilityAttributes:options && options->includeAccessibilityAttributes];
    [configuration setIncludeTextInAutoFilledControls:options && options->includeTextInAutoFilledControls];

    auto outputFormat = [&] -> std::optional<_WKTextExtractionOutputFormat> {
        if (!options)
            return std::nullopt;

        auto outputFormat = toWTFString(options->outputFormat.get());
        if (equalLettersIgnoringASCIICase(outputFormat, "html"_s))
            return _WKTextExtractionOutputFormatHTML;

        if (equalLettersIgnoringASCIICase(outputFormat, "markdown"_s))
            return _WKTextExtractionOutputFormatMarkdown;

        if (equalLettersIgnoringASCIICase(outputFormat, "texttree"_s))
            return _WKTextExtractionOutputFormatTextTree;

        return std::nullopt;
    }();
    if (outputFormat)
        [configuration setOutputFormat:*outputFormat];

    if (auto wordLimit = options ? options->wordLimit : 0)
        [configuration setMaxWordsPerParagraph:static_cast<NSUInteger>(wordLimit)];
    [configuration setTargetRect:extractionRect];
    [configuration setMergeParagraphs:options && options->mergeParagraphs];
    [configuration setSkipNearlyTransparentContent:options && options->skipNearlyTransparentContent];
    return configuration;
}

void UIScriptControllerCocoa::requestTextExtraction(JSValueRef callback, TextExtractionTestOptions* options)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    RetainPtr configuration = createTextExtractionConfiguration(webView(), options);
    auto includeRects = [configuration includeRects] ? IncludeRects::Yes : IncludeRects::No;
    [webView() _requestTextExtraction:configuration.get() completionHandler:^(WKTextExtractionItem *rootItem) {
        if (!m_context)
            return;

        auto description = adopt(JSStringCreateWithCFString((__bridge CFStringRef)recursiveDescription(rootItem, includeRects)));
        m_context->asyncTaskComplete(callbackID, { JSValueMakeString(m_context->jsContext(), description.get()) });
    }];
}

void UIScriptControllerCocoa::requestDebugText(JSValueRef callback, TextExtractionTestOptions* options)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    RetainPtr configuration = createTextExtractionConfiguration(webView(), options);
    [webView() _debugTextWithConfiguration:configuration.get() completionHandler:^(NSString *text) {
        if (!m_context)
            return;

        auto description = adopt(JSStringCreateWithCFString((__bridge CFStringRef)text));
        m_context->asyncTaskComplete(callbackID, { JSValueMakeString(m_context->jsContext(), description.get()) });
    }];
}

void UIScriptControllerCocoa::performTextExtractionInteraction(JSStringRef jsAction, TextExtractionInteractionOptions* options, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    if (!options) {
        ASSERT_NOT_REACHED();
        return m_context->asyncTaskComplete(callbackID, { JSValueMakeBoolean(m_context->jsContext(), false) });
    }

    auto actionName = toWTFString(jsAction);
    std::optional<_WKTextExtractionAction> action;
    if (equalLettersIgnoringASCIICase(actionName, "click"))
        action = _WKTextExtractionActionClick;
    if (equalLettersIgnoringASCIICase(actionName, "selecttext"))
        action = _WKTextExtractionActionSelectText;
    if (equalLettersIgnoringASCIICase(actionName, "selectmenuitem"))
        action = _WKTextExtractionActionSelectMenuItem;
    if (equalLettersIgnoringASCIICase(actionName, "textinput"))
        action = _WKTextExtractionActionTextInput;
    if (equalLettersIgnoringASCIICase(actionName, "keypress"))
        action = _WKTextExtractionActionKeyPress;
    if (equalLettersIgnoringASCIICase(actionName, "highlighttext"))
        action = _WKTextExtractionActionHighlightText;
    if (equalLettersIgnoringASCIICase(actionName, "scrollby"))
        action = _WKTextExtractionActionScrollBy;

    if (!action) {
        ASSERT_NOT_REACHED();
        return m_context->asyncTaskComplete(callbackID, { JSValueMakeBoolean(m_context->jsContext(), false) });
    }

    RetainPtr interaction = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:*action]);

    if (options->nodeIdentifier && JSStringGetLength(options->nodeIdentifier.get()) > 0)
        [interaction setNodeIdentifier:toWTFString(options->nodeIdentifier.get()).createNSString().get()];

    if (options->text && JSStringGetLength(options->text.get()) > 0)
        [interaction setText:toWTFString(options->text.get()).createNSString().get()];

    [interaction setReplaceAll:options->replaceAll];
    [interaction setScrollToVisible:options->scrollToVisible];

    if (auto location = options->location) {
        auto [x, y] = *location;
        [interaction setLocation:CGPointMake(x, y)];
    }

    if (auto scrollDelta = options->scrollDelta)
        [interaction setScrollDelta:std::apply(CGSizeMake, *scrollDelta)];

    [webView() _performInteraction:interaction.get() completionHandler:^(_WKTextExtractionInteractionResult *result) {
        if (!m_context)
            return;

        RetainPtr description = [result.error.userInfo objectForKey:NSDebugDescriptionErrorKey] ?: @"";
        JSRetainPtr jsDescription = adopt(JSStringCreateWithCFString((__bridge CFStringRef)description.get()));
        m_context->asyncTaskComplete(callbackID, { JSValueMakeString(m_context->jsContext(), jsDescription.get()) });
    }];
}

void UIScriptControllerCocoa::requestRenderedTextForFrontmostTarget(int x, int y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    auto request = adoptNS([[_WKTargetedElementRequest alloc] initWithPoint:CGPointMake(x, y)]);
    [webView() _requestTargetedElementInfo:request.get() completionHandler:^(NSArray<_WKTargetedElementInfo *> *elements) {
        if (!m_context)
            return;

        JSRetainPtr result = adopt(JSStringCreateWithCFString((__bridge CFStringRef)(elements.firstObject.renderedText ?: @"")));
        m_context->asyncTaskComplete(callbackID, { JSValueMakeString(m_context->jsContext(), result.get()) });
    }];
}

void UIScriptControllerCocoa::adjustVisibilityForFrontmostTarget(int x, int y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    auto request = adoptNS([[_WKTargetedElementRequest alloc] initWithPoint:CGPointMake(x, y)]);
    [webView() _requestTargetedElementInfo:request.get() completionHandler:[callbackID, this](NSArray<_WKTargetedElementInfo *> *elements) {
        if (!elements.count) {
            m_context->asyncTaskComplete(callbackID);
            return;
        }

        RetainPtr<_WKTargetedElementInfo> frontTarget = elements.firstObject;
        [webView() _adjustVisibilityForTargetedElements:@[ frontTarget.get() ] completionHandler:[callbackID, frontTarget, this] (BOOL success) {
            if (!success) {
                m_context->asyncTaskComplete(callbackID);
                return;
            }

            JSRetainPtr firstSelector = adopt(JSStringCreateWithCFString((__bridge CFStringRef)[frontTarget selectors].firstObject));
            m_context->asyncTaskComplete(callbackID, { JSValueMakeString(m_context->jsContext(), firstSelector.get()) });
        }];
    }];
}

void UIScriptControllerCocoa::resetVisibilityAdjustments(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView() _resetVisibilityAdjustmentsForTargetedElements:nil completionHandler:[callbackID, this](BOOL success) {
        m_context->asyncTaskComplete(callbackID, { JSValueMakeBoolean(m_context->jsContext(), success) });
    }];
}

JSObjectRef UIScriptControllerCocoa::fixedContainerEdgeColors() const
{
    auto colorDescriptionOrNull = [fixedEdges = webView()._fixedContainerEdges](WebCore::CocoaColor *color, _WKRectEdge edge) -> id {
        if (color)
            return WebCoreTestSupport::serializationForCSS(color).createNSString().autorelease();
        if (fixedEdges & edge)
            return @"multiple";
        return NSNull.null;
    };

    RetainPtr jsValue = [JSValue valueWithObject:@{
        @"top": colorDescriptionOrNull(webView()._sampledTopFixedPositionContentColor, _WKRectEdgeTop),
        @"left": colorDescriptionOrNull(webView()._sampledLeftFixedPositionContentColor, _WKRectEdgeLeft),
        @"bottom": colorDescriptionOrNull(webView()._sampledBottomFixedPositionContentColor, _WKRectEdgeBottom),
        @"right": colorDescriptionOrNull(webView()._sampledRightFixedPositionContentColor, _WKRectEdgeRight)
    } inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]];
    return JSValueToObject(m_context->jsContext(), [jsValue JSValueRef], nullptr);
}

static NSDictionary *propertyDictionaryForJS(NSHTTPCookie *cookie)
{
    return @{
        @"name"             : cookie.name ?: [NSNull null],
        @"value"            : cookie.value ?: [NSNull null],
        @"domain"           : cookie.domain ?: [NSNull null],
        @"path"             : cookie.path ?: [NSNull null],
        @"expires"          : cookie.expiresDate ? @(1000 * [cookie.expiresDate timeIntervalSinceReferenceDate]) : [NSNull null],
        @"isHttpOnly"       : @(cookie.HTTPOnly),
        @"isSecure"         : @(cookie.secure),
        @"isSession"        : @(cookie.sessionOnly),
        @"isSameSiteNone"   : @(!cookie.sameSitePolicy),
        @"isSameSiteLax"    : @([cookie.sameSitePolicy isEqualToString:NSHTTPCookieSameSiteLax]),
        @"isSameSiteStrict" : @([cookie.sameSitePolicy isEqualToString:NSHTTPCookieSameSiteStrict]),
    };
}

void UIScriptControllerCocoa::cookiesForDomain(JSStringRef jsDomain, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    RetainPtr cookieStore = [webView().configuration.websiteDataStore httpCookieStore];
    [cookieStore getAllCookies:[this, callbackID, domain = toWTFString(jsDomain)](NSArray<NSHTTPCookie *> *cookies) {
        RetainPtr matchingCookieProperties = adoptNS([NSMutableArray new]);
        for (NSHTTPCookie *cookie in cookies) {
            if (![cookie.domain isEqualToString:domain.createNSString().get()])
                continue;
            [matchingCookieProperties addObject:propertyDictionaryForJS(cookie)];
        }
        RetainPtr jsValue = [JSValue valueWithObject:matchingCookieProperties.get() inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]];
        m_context->asyncTaskComplete(callbackID, { JSValueToObject(m_context->jsContext(), [jsValue JSValueRef], nullptr) });
    }];
}

void UIScriptControllerCocoa::cancelFixedColorExtensionFadeAnimations() const
{
    [webView() _cancelFixedColorExtensionFadeAnimationsForTesting];
}

void UIScriptControllerCocoa::setObscuredInsets(double top, double right, double bottom, double left)
{
#if PLATFORM(IOS_FAMILY)
    auto insets = UIEdgeInsetsMake(top, left, bottom, right);
    [webView() scrollView].contentInset = insets;
#else
    auto insets = NSEdgeInsetsMake(top, left, bottom, right);
#endif
    [webView() setObscuredContentInsets:insets];
}

#if ENABLE(THREADED_ANIMATIONS)
JSRetainPtr<JSStringRef> UIScriptControllerCocoa::animationStackForLayerWithID(uint64_t layerID) const
{
    return adopt(JSStringCreateWithCFString((CFStringRef) [webView() _animationStackForLayerWithID:layerID]));
}
#endif

} // namespace WTR
