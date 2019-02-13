/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#import "UIScriptController.h"

#if PLATFORM(IOS_FAMILY)

#import "HIDEventGenerator.h"
#import "PencilKitTestSPI.h"
#import "PlatformWebView.h"
#import "StringFunctions.h"
#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import "UIKitSPI.h"
#import "UIScriptContext.h"
#import <JavaScriptCore/JavaScriptCore.h>
#import <JavaScriptCore/OpaqueJSString.h>
#import <UIKit/UIKit.h>
#import <WebCore/FloatRect.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/SoftLinking.h>
#import <wtf/Vector.h>

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIPhysicalKeyboardEvent)

@interface UIPhysicalKeyboardEvent (UIPhysicalKeyboardEventHack)
@property (nonatomic, assign) NSInteger _modifierFlags;
@end

namespace WTR {

static NSDictionary *toNSDictionary(CGRect rect)
{
    return @{
        @"left": @(rect.origin.x),
        @"top": @(rect.origin.y),
        @"width": @(rect.size.width),
        @"height": @(rect.size.height)
    };
}

static unsigned arrayLength(JSContextRef context, JSObjectRef array)
{
    auto lengthString = adopt(JSStringCreateWithUTF8CString("length"));
    if (auto lengthValue = JSObjectGetProperty(context, array, lengthString.get(), nullptr))
        return static_cast<unsigned>(JSValueToNumber(context, lengthValue, nullptr));
    return 0;
}

static Vector<String> parseModifierArray(JSContextRef context, JSValueRef arrayValue)
{
    if (!arrayValue)
        return { };

    // The value may either be a string with a single modifier or an array of modifiers.
    if (JSValueIsString(context, arrayValue)) {
        auto string = toWTFString(toWK(adopt(JSValueToStringCopy(context, arrayValue, nullptr))));
        return { string };
    }

    if (!JSValueIsObject(context, arrayValue))
        return { };
    JSObjectRef array = const_cast<JSObjectRef>(arrayValue);
    unsigned length = arrayLength(context, array);
    Vector<String> modifiers;
    modifiers.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        JSValueRef exception = nullptr;
        JSValueRef value = JSObjectGetPropertyAtIndex(context, array, i, &exception);
        if (exception)
            continue;
        auto string = adopt(JSValueToStringCopy(context, value, &exception));
        if (exception)
            continue;
        modifiers.append(toWTFString(toWK(string.get())));
    }
    return modifiers;
}

void UIScriptController::checkForOutstandingCallbacks()
{
    if (![[HIDEventGenerator sharedHIDEventGenerator] checkForOutstandingCallbacks])
        [NSException raise:@"WebKitTestRunnerTestProblem" format:@"The test completed before all synthesized events had been handled. Perhaps you're calling notifyDone() too early?"];
}

void UIScriptController::doAfterPresentationUpdate(JSValueRef callback)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView _doAfterNextPresentationUpdate:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::doAfterNextStablePresentationUpdate(JSValueRef callback)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView _doAfterNextStablePresentationUpdate:^() {
        if (m_context)
            m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::doAfterVisibleContentRectUpdate(JSValueRef callback)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView _doAfterNextVisibleContentRectUpdate:^ {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::zoomToScale(double scale, JSValueRef callback)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    [webView zoomToScale:scale animated:YES completionHandler:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::retrieveSpeakSelectionContent(JSValueRef callback)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    [webView accessibilityRetrieveSpeakSelectionContentWithCompletionHandler:^() {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

JSRetainPtr<JSStringRef> UIScriptController::accessibilitySpeakSelectionContent() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    return JSStringCreateWithCFString((CFStringRef)webView.accessibilitySpeakSelectionContent);
}

void UIScriptController::simulateAccessibilitySettingsChangeNotification(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto* webView = TestController::singleton().mainWebView()->platformView();
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center postNotificationName:UIAccessibilityInvertColorsStatusDidChangeNotification object:webView];

    [webView _doAfterNextPresentationUpdate: ^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

double UIScriptController::zoomScale() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    return webView.scrollView.zoomScale;
}

static CGPoint globalToContentCoordinates(TestRunnerWKWebView *webView, long x, long y)
{
    CGPoint point = CGPointMake(x, y);
    point = [webView _convertPointFromContentsToView:point];
    point = [webView convertPoint:point toView:nil];
    point = [webView.window convertPoint:point toWindow:nil];
    return point;
}

void UIScriptController::touchDownAtPoint(long x, long y, long touchCount, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] touchDown:location touchCount:touchCount completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::liftUpAtPoint(long x, long y, long touchCount, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    auto location = globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] liftUp:location touchCount:touchCount completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::singleTapAtPoint(long x, long y, JSValueRef callback)
{
    singleTapAtPointWithModifiers(x, y, nullptr, callback);
}

void UIScriptController::singleTapAtPointWithModifiers(long x, long y, JSValueRef modifierArray, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto modifierFlags = parseModifierArray(m_context->jsContext(), modifierArray);
    for (auto& modifierFlag : modifierFlags)
        [[HIDEventGenerator sharedHIDEventGenerator] keyDown:modifierFlag];

    [[HIDEventGenerator sharedHIDEventGenerator] tap:globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), x, y) completionBlock:^{
        if (!m_context)
            return;
        for (size_t i = modifierFlags.size(); i; ) {
            --i;
            [[HIDEventGenerator sharedHIDEventGenerator] keyUp:modifierFlags[i]];
        }
        [[HIDEventGenerator sharedHIDEventGenerator] sendMarkerHIDEventWithCompletionBlock:^{
            if (!m_context)
                return;
            m_context->asyncTaskComplete(callbackID);
        }];
    }];
}

void UIScriptController::doubleTapAtPoint(long x, long y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    [[HIDEventGenerator sharedHIDEventGenerator] doubleTap:globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), x, y) completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::stylusDownAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusDownAtPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::stylusMoveToPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusMoveToPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::stylusUpAtPoint(long x, long y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusUpAtPoint:location completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::stylusTapAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
    stylusTapAtPointWithModifiers(x, y, azimuthAngle, altitudeAngle, pressure, nullptr, callback);
}

void UIScriptController::stylusTapAtPointWithModifiers(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef modifierArray, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto modifierFlags = parseModifierArray(m_context->jsContext(), modifierArray);
    for (auto& modifierFlag : modifierFlags)
        [[HIDEventGenerator sharedHIDEventGenerator] keyDown:modifierFlag];

    auto location = globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusTapAtPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure completionBlock:^{
        if (!m_context)
            return;
        for (size_t i = modifierFlags.size(); i; ) {
            --i;
            [[HIDEventGenerator sharedHIDEventGenerator] keyUp:modifierFlags[i]];
        }
        [[HIDEventGenerator sharedHIDEventGenerator] sendMarkerHIDEventWithCompletionBlock:^{
            if (!m_context)
                return;
            m_context->asyncTaskComplete(callbackID);
        }];
    }];
}

void convertCoordinates(NSMutableDictionary *event)
{
    if (event[HIDEventTouchesKey]) {
        for (NSMutableDictionary *touch in event[HIDEventTouchesKey]) {
            auto location = globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), (long)[touch[HIDEventXKey] doubleValue], (long)[touch[HIDEventYKey]doubleValue]);
            touch[HIDEventXKey] = @(location.x);
            touch[HIDEventYKey] = @(location.y);
        }
    }
}

void UIScriptController::sendEventStream(JSStringRef eventsJSON, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    String jsonString = eventsJSON->string();
    auto eventInfo = dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:[(NSString *)jsonString dataUsingEncoding:NSUTF8StringEncoding] options:NSJSONReadingMutableContainers | NSJSONReadingMutableLeaves error:nil]);
    
    for (NSMutableDictionary *event in eventInfo[TopLevelEventInfoKey]) {
        if (![event[HIDEventCoordinateSpaceKey] isEqualToString:HIDEventCoordinateSpaceTypeContent])
            continue;
        
        if (event[HIDEventStartEventKey])
            convertCoordinates(event[HIDEventStartEventKey]);
        
        if (event[HIDEventEndEventKey])
            convertCoordinates(event[HIDEventEndEventKey]);
        
        if (event[HIDEventTouchesKey])
            convertCoordinates(event);
    }
    
    if (!eventInfo || ![eventInfo isKindOfClass:[NSDictionary class]]) {
        WTFLogAlways("JSON is not convertible to a dictionary");
        return;
    }
    
    [[HIDEventGenerator sharedHIDEventGenerator] sendEventStream:eventInfo completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::dragFromPointToPoint(long startX, long startY, long endX, long endY, double durationSeconds, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    CGPoint startPoint = globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), startX, startY);
    CGPoint endPoint = globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), endX, endY);
    
    [[HIDEventGenerator sharedHIDEventGenerator] dragWithStartPoint:startPoint endPoint:endPoint duration:durationSeconds completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}
    
void UIScriptController::longPressAtPoint(long x, long y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    [[HIDEventGenerator sharedHIDEventGenerator] longPress:globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), x, y) completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::enterText(JSStringRef text)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    auto textAsCFString = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, text));
    [webView _simulateTextEntered:(NSString *)textAsCFString.get()];
}

void UIScriptController::typeCharacterUsingHardwareKeyboard(JSStringRef character, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    // Assumes that the keyboard is already shown.
    [[HIDEventGenerator sharedHIDEventGenerator] keyPress:toWTFString(toWK(character)) completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

static UIPhysicalKeyboardEvent *createUIPhysicalKeyboardEvent(NSString *hidInputString, NSString *uiEventInputString, UIKeyModifierFlags modifierFlags, UIKeyboardInputFlags inputFlags, bool isKeyDown)
{
    auto* keyboardEvent = [getUIPhysicalKeyboardEventClass() _eventWithInput:uiEventInputString inputFlags:inputFlags];
    keyboardEvent._modifierFlags = modifierFlags;
    auto hidEvent = createHIDKeyEvent(hidInputString, keyboardEvent.timestamp, isKeyDown);
    [keyboardEvent _setHIDEvent:hidEvent.get() keyboard:nullptr];
    return keyboardEvent;
}

void UIScriptController::keyDown(JSStringRef character, JSValueRef modifierArray)
{
    // Character can be either a single Unicode code point or the name of a special key (e.g. "downArrow").
    // HIDEventGenerator knows how to map these special keys to the appropriate keycode.
    String inputString = toWTFString(toWK(character));
    auto modifierFlags = parseModifierArray(m_context->jsContext(), modifierArray);

    for (auto& modifierFlag : modifierFlags)
        [[HIDEventGenerator sharedHIDEventGenerator] keyDown:modifierFlag];

    [[HIDEventGenerator sharedHIDEventGenerator] keyDown:inputString];
    [[HIDEventGenerator sharedHIDEventGenerator] keyUp:inputString];

    for (size_t i = modifierFlags.size(); i; ) {
        --i;
        [[HIDEventGenerator sharedHIDEventGenerator] keyUp:modifierFlags[i]];
    }

    [[HIDEventGenerator sharedHIDEventGenerator] sendMarkerHIDEventWithCompletionBlock:^{ /* Do nothing */ }];
}

void UIScriptController::dismissFormAccessoryView()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView dismissFormAccessoryView];
}

JSRetainPtr<JSStringRef> UIScriptController::selectFormPopoverTitle() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    return JSStringCreateWithCFString((CFStringRef)webView.selectFormPopoverTitle);
}

JSRetainPtr<JSStringRef> UIScriptController::textContentType() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    return JSStringCreateWithCFString((CFStringRef)(webView.textContentTypeForTesting ?: @""));
}

JSRetainPtr<JSStringRef> UIScriptController::formInputLabel() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    return JSStringCreateWithCFString((CFStringRef)webView.formInputLabel);
}

void UIScriptController::selectFormAccessoryPickerRow(long rowIndex)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView selectFormAccessoryPickerRow:rowIndex];
}

void UIScriptController::setTimePickerValue(long hour, long minute)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView setTimePickerValueToHour:hour minute:minute];
}

static CGPoint contentOffsetBoundedInValidRange(UIScrollView *scrollView, CGPoint contentOffset)
{
    UIEdgeInsets contentInsets = scrollView.contentInset;
    CGSize contentSize = scrollView.contentSize;
    CGSize scrollViewSize = scrollView.bounds.size;

    CGFloat maxHorizontalOffset = contentSize.width + contentInsets.right - scrollViewSize.width;
    contentOffset.x = std::min(maxHorizontalOffset, contentOffset.x);
    contentOffset.x = std::max(-contentInsets.left, contentOffset.x);

    CGFloat maxVerticalOffset = contentSize.height + contentInsets.bottom - scrollViewSize.height;
    contentOffset.y = std::min(maxVerticalOffset, contentOffset.y);
    contentOffset.y = std::max(-contentInsets.top, contentOffset.y);
    return contentOffset;
}

double UIScriptController::contentOffsetX() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    return webView.scrollView.contentOffset.x;
}

double UIScriptController::contentOffsetY() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    return webView.scrollView.contentOffset.y;
}

void UIScriptController::scrollToOffset(long x, long y)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView.scrollView setContentOffset:contentOffsetBoundedInValidRange(webView.scrollView, CGPointMake(x, y)) animated:YES];
}

void UIScriptController::immediateScrollToOffset(long x, long y)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView.scrollView setContentOffset:contentOffsetBoundedInValidRange(webView.scrollView, CGPointMake(x, y)) animated:NO];
}

void UIScriptController::immediateZoomToScale(double scale)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView.scrollView setZoomScale:scale animated:NO];
}

void UIScriptController::keyboardAccessoryBarNext()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView keyboardAccessoryBarNext];
}

void UIScriptController::keyboardAccessoryBarPrevious()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView keyboardAccessoryBarPrevious];
}

bool UIScriptController::isShowingKeyboard() const
{
    return TestController::singleton().mainWebView()->platformView().showingKeyboard;
}

void UIScriptController::applyAutocorrection(JSStringRef newString, JSStringRef oldString, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView applyAutocorrection:toWTFString(toWK(newString)) toString:toWTFString(toWK(oldString)) withCompletionHandler:^ {
        // applyAutocorrection can call its completion handler synchronously,
        // which makes UIScriptController unhappy (see bug 172884).
        dispatch_async(dispatch_get_main_queue(), ^ {
            if (!m_context)
                return;
            m_context->asyncTaskComplete(callbackID);
        });
    }];
}

double UIScriptController::minimumZoomScale() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    return webView.scrollView.minimumZoomScale;
}

double UIScriptController::maximumZoomScale() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    return webView.scrollView.maximumZoomScale;
}

Optional<bool> UIScriptController::stableStateOverride() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    if (webView._stableStateOverride)
        return webView._stableStateOverride.boolValue;

    return WTF::nullopt;
}

void UIScriptController::setStableStateOverride(Optional<bool> overrideValue)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    if (overrideValue)
        webView._stableStateOverride = @(overrideValue.value());
    else
        webView._stableStateOverride = nil;
}

JSObjectRef UIScriptController::contentVisibleRect() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();

    CGRect contentVisibleRect = webView._contentVisibleRect;
    
    WebCore::FloatRect rect(contentVisibleRect.origin.x, contentVisibleRect.origin.y, contentVisibleRect.size.width, contentVisibleRect.size.height);
    return m_context->objectFromRect(rect);
}

JSObjectRef UIScriptController::textSelectionRangeRects() const
{
    auto selectionRects = adoptNS([[NSMutableArray alloc] init]);
    NSArray *rects = TestController::singleton().mainWebView()->platformView()._uiTextSelectionRects;
    for (NSValue *rect in rects)
        [selectionRects addObject:toNSDictionary(rect.CGRectValue)];

    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:selectionRects.get() inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptController::textSelectionCaretRect() const
{
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary(TestController::singleton().mainWebView()->platformView()._uiTextCaretRect) inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptController::selectionStartGrabberViewRect() const
{
    WKWebView *webView = TestController::singleton().mainWebView()->platformView();
    UIView *contentView = [webView valueForKeyPath:@"_currentContentView"];
    UIView *selectionRangeView = [contentView valueForKeyPath:@"interactionAssistant.selectionView.rangeView"];
    auto frameInContentCoordinates = [selectionRangeView convertRect:[[selectionRangeView valueForKeyPath:@"startGrabber"] frame] toView:contentView];
    frameInContentCoordinates = CGRectIntersection(contentView.bounds, frameInContentCoordinates);
    auto jsContext = m_context->jsContext();
    return JSValueToObject(jsContext, [JSValue valueWithObject:toNSDictionary(frameInContentCoordinates) inContext:[JSContext contextWithJSGlobalContextRef:jsContext]].JSValueRef, nullptr);
}

JSObjectRef UIScriptController::selectionEndGrabberViewRect() const
{
    WKWebView *webView = TestController::singleton().mainWebView()->platformView();
    UIView *contentView = [webView valueForKeyPath:@"_currentContentView"];
    UIView *selectionRangeView = [contentView valueForKeyPath:@"interactionAssistant.selectionView.rangeView"];
    auto frameInContentCoordinates = [selectionRangeView convertRect:[[selectionRangeView valueForKeyPath:@"endGrabber"] frame] toView:contentView];
    frameInContentCoordinates = CGRectIntersection(contentView.bounds, frameInContentCoordinates);
    auto jsContext = m_context->jsContext();
    return JSValueToObject(jsContext, [JSValue valueWithObject:toNSDictionary(frameInContentCoordinates) inContext:[JSContext contextWithJSGlobalContextRef:jsContext]].JSValueRef, nullptr);
}

JSObjectRef UIScriptController::selectionCaretViewRect() const
{
    WKWebView *webView = TestController::singleton().mainWebView()->platformView();
    UIView *contentView = [webView valueForKeyPath:@"_currentContentView"];
    UIView *caretView = [contentView valueForKeyPath:@"interactionAssistant.selectionView.caretView"];
    auto rectInContentViewCoordinates = CGRectIntersection([caretView convertRect:caretView.bounds toView:contentView], contentView.bounds);
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary(rectInContentViewCoordinates) inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptController::selectionRangeViewRects() const
{
    WKWebView *webView = TestController::singleton().mainWebView()->platformView();
    UIView *contentView = [webView valueForKeyPath:@"_currentContentView"];
    UIView *rangeView = [contentView valueForKeyPath:@"interactionAssistant.selectionView.rangeView"];
    auto rectsAsDictionaries = adoptNS([[NSMutableArray alloc] init]);
    NSArray *textRectInfoArray = [rangeView valueForKeyPath:@"rects"];
    for (id textRectInfo in textRectInfoArray) {
        NSValue *rectValue = [textRectInfo valueForKeyPath:@"rect"];
        auto rangeRectInContentViewCoordinates = [rangeView convertRect:rectValue.CGRectValue toView:contentView];
        [rectsAsDictionaries addObject:toNSDictionary(CGRectIntersection(rangeRectInContentViewCoordinates, contentView.bounds))];
    }
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:rectsAsDictionaries.get() inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptController::inputViewBounds() const
{
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary(TestController::singleton().mainWebView()->platformView()._inputViewBounds) inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

void UIScriptController::removeAllDynamicDictionaries()
{
    [UIKeyboard removeAllDynamicDictionaries];
}

JSRetainPtr<JSStringRef> UIScriptController::scrollingTreeAsText() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    return JSStringCreateWithCFString((CFStringRef)[webView _scrollingTreeAsText]);
}

JSObjectRef UIScriptController::propertiesOfLayerWithID(uint64_t layerID) const
{
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:[TestController::singleton().mainWebView()->platformView() _propertiesOfLayerWithID:layerID] inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

static UIDeviceOrientation toUIDeviceOrientation(DeviceOrientation* orientation)
{
    if (!orientation)
        return UIDeviceOrientationPortrait;
        
    switch (*orientation) {
    case DeviceOrientation::Portrait:
        return UIDeviceOrientationPortrait;
    case DeviceOrientation::PortraitUpsideDown:
        return UIDeviceOrientationPortraitUpsideDown;
    case DeviceOrientation::LandscapeLeft:
        return UIDeviceOrientationLandscapeLeft;
    case DeviceOrientation::LandscapeRight:
        return UIDeviceOrientationLandscapeRight;
    }
    
    return UIDeviceOrientationPortrait;
}

void UIScriptController::simulateRotation(DeviceOrientation* orientation, JSValueRef callback)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.usesSafariLikeRotation = NO;
    
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    webView.rotationDidEndCallback = ^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    };
    
    [[UIDevice currentDevice] setOrientation:toUIDeviceOrientation(orientation) animated:YES];
}

void UIScriptController::simulateRotationLikeSafari(DeviceOrientation* orientation, JSValueRef callback)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.usesSafariLikeRotation = YES;
    
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    webView.rotationDidEndCallback = ^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    };
    
    [[UIDevice currentDevice] setOrientation:toUIDeviceOrientation(orientation) animated:YES];
}

void UIScriptController::platformSetDidStartFormControlInteractionCallback()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.didStartFormControlInteractionCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidStartFormControlInteraction);
    };
}

void UIScriptController::platformSetDidEndFormControlInteractionCallback()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.didEndFormControlInteractionCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidEndFormControlInteraction);
    };
}
    
void UIScriptController::platformSetDidShowForcePressPreviewCallback()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.didShowForcePressPreviewCallback = ^ {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidShowForcePressPreview);
    };
}

void UIScriptController::platformSetDidDismissForcePressPreviewCallback()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.didDismissForcePressPreviewCallback = ^ {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidEndFormControlInteraction);
    };
}

void UIScriptController::platformSetWillBeginZoomingCallback()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.willBeginZoomingCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeWillBeginZooming);
    };
}

void UIScriptController::platformSetDidEndZoomingCallback()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.didEndZoomingCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidEndZooming);
    };
}

void UIScriptController::platformSetDidShowKeyboardCallback()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.didShowKeyboardCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidShowKeyboard);
    };
}

void UIScriptController::platformSetDidHideKeyboardCallback()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.didHideKeyboardCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidHideKeyboard);
    };
}

void UIScriptController::platformSetDidEndScrollingCallback()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.didEndScrollingCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidEndScrolling);
    };
}

void UIScriptController::platformClearAllCallbacks()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    
    webView.didStartFormControlInteractionCallback = nil;
    webView.didEndFormControlInteractionCallback = nil;
    webView.didShowForcePressPreviewCallback = nil;
    webView.didDismissForcePressPreviewCallback = nil;
    webView.didEndZoomingCallback = nil;
    webView.willBeginZoomingCallback = nil;
    webView.didHideKeyboardCallback = nil;
    webView.didShowKeyboardCallback = nil;
    webView.didEndScrollingCallback = nil;
    webView.rotationDidEndCallback = nil;
}

void UIScriptController::setSafeAreaInsets(double top, double right, double bottom, double left)
{
    UIEdgeInsets insets = UIEdgeInsetsMake(top, left, bottom, right);
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    webView.overrideSafeAreaInsets = insets;
}

void UIScriptController::beginBackSwipe(JSValueRef callback)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView _beginBackSwipeForTesting];
}

void UIScriptController::completeBackSwipe(JSValueRef callback)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView _completeBackSwipeForTesting];
}

static BOOL forEachViewInHierarchy(UIView *view, void(^mapFunction)(UIView *subview, BOOL *stop))
{
    BOOL stop = NO;
    mapFunction(view, &stop);
    if (stop)
        return YES;

    for (UIView *subview in view.subviews) {
        stop = forEachViewInHierarchy(subview, mapFunction);
        if (stop)
            break;
    }
    return stop;
}

bool UIScriptController::isShowingDataListSuggestions() const
{
    Class remoteKeyboardWindowClass = NSClassFromString(@"UIRemoteKeyboardWindow");
    Class suggestionsPickerViewClass = NSClassFromString(@"WKDataListSuggestionsPickerView");
    UIWindow *remoteInputHostingWindow = nil;
    for (UIWindow *window in UIApplication.sharedApplication.windows) {
        if ([window isKindOfClass:remoteKeyboardWindowClass])
            remoteInputHostingWindow = window;
    }

    if (!remoteInputHostingWindow)
        return false;

    __block bool foundDataListSuggestionsPickerView = false;
    forEachViewInHierarchy(remoteInputHostingWindow, ^(UIView *subview, BOOL *stop) {
        if (![subview isKindOfClass:suggestionsPickerViewClass])
            return;

        foundDataListSuggestionsPickerView = true;
        *stop = YES;
    });
    return foundDataListSuggestionsPickerView;
}

#if HAVE(PENCILKIT)
static PKCanvasView *findEditableImageCanvas()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    Class pkCanvasViewClass = NSClassFromString(@"PKCanvasView");
    __block PKCanvasView *canvasView = nil;
    forEachViewInHierarchy(webView.window, ^(UIView *subview, BOOL *stop) {
        if (![subview isKindOfClass:pkCanvasViewClass])
            return;

        canvasView = (PKCanvasView *)subview;
        *stop = YES;
    });
    return canvasView;
}
#endif

void UIScriptController::drawSquareInEditableImage()
{
#if HAVE(PENCILKIT)
    Class pkDrawingClass = NSClassFromString(@"PKDrawing");
    Class pkInkClass = NSClassFromString(@"PKInk");
    Class pkStrokeClass = NSClassFromString(@"PKStroke");

    PKCanvasView *canvasView = findEditableImageCanvas();
    RetainPtr<PKDrawing> drawing = canvasView.drawing ?: adoptNS([[pkDrawingClass alloc] init]);
    RetainPtr<CGPathRef> path = adoptCF(CGPathCreateWithRect(CGRectMake(0, 0, 50, 50), NULL));
    RetainPtr<PKInk> ink = [pkInkClass inkWithType:0 color:UIColor.greenColor weight:100.0];
    RetainPtr<PKStroke> stroke = adoptNS([[pkStrokeClass alloc] _initWithPath:path.get() ink:ink.get() inputScale:1]);
    [drawing _addStroke:stroke.get()];

    [canvasView setDrawing:drawing.get()];
#endif
}

long UIScriptController::numberOfStrokesInEditableImage()
{
#if HAVE(PENCILKIT)
    PKCanvasView *canvasView = findEditableImageCanvas();
    return canvasView.drawing._allStrokes.count;
#else
    return 0;
#endif
}

void UIScriptController::setKeyboardInputModeIdentifier(JSStringRef identifier)
{
    TestController::singleton().setKeyboardInputModeIdentifier(toWTFString(toWK(identifier)));
}

// FIXME: Write this in terms of HIDEventGenerator once we know how to reset caps lock state
// on test completion to avoid it effecting subsequent tests.
void UIScriptController::toggleCapsLock(JSValueRef callback)
{
    m_capsLockOn = !m_capsLockOn;
    auto *keyboardEvent = createUIPhysicalKeyboardEvent(@"capsLock", [NSString string], m_capsLockOn ? UIKeyModifierAlphaShift : 0,
        kUIKeyboardInputModifierFlagsChanged, m_capsLockOn);
    [[UIApplication sharedApplication] handleKeyUIEvent:keyboardEvent];
    doAsyncTask(callback);
}

JSObjectRef UIScriptController::attachmentInfo(JSStringRef jsAttachmentIdentifier)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();

    auto attachmentIdentifier = toWTFString(toWK(jsAttachmentIdentifier));
    _WKAttachment *attachment = [webView _attachmentForIdentifier:attachmentIdentifier];
    _WKAttachmentInfo *attachmentInfo = attachment.info;

    NSDictionary *attachmentInfoDictionary = @{
        @"id": attachmentIdentifier,
        @"name": attachmentInfo.name,
        @"contentType": attachmentInfo.contentType,
        @"filePath": attachmentInfo.filePath,
        @"size": @(attachmentInfo.data.length),
    };

    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:attachmentInfoDictionary inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

NSUndoManager *UIScriptController::platformUndoManager() const
{
    return [(UIView *)[TestController::singleton().mainWebView()->platformView() valueForKeyPath:@"_currentContentView"] undoManager];
}

}

#endif // PLATFORM(IOS_FAMILY)
