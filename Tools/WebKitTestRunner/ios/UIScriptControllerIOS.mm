/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "HIDEventGenerator.h"
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

void UIScriptController::doAsyncTask(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
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
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    [[HIDEventGenerator sharedHIDEventGenerator] tap:globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), x, y) completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
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
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(TestController::singleton().mainWebView()->platformView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusTapAtPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
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

void UIScriptController::keyDownUsingHardwareKeyboard(JSStringRef character, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    // Assumes that the keyboard is already shown.
    [[HIDEventGenerator sharedHIDEventGenerator] keyDown:toWTFString(toWK(character)) completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::keyUpUsingHardwareKeyboard(JSStringRef character, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    // Assumes that the keyboard is already shown.
    [[HIDEventGenerator sharedHIDEventGenerator] keyUp:toWTFString(toWK(character)) completionBlock:^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::selectTextCandidateAtIndex(long index, JSValueRef callback)
{
#if USE(APPLE_INTERNAL_SDK)
    static const float textPredictionsPollingInterval = 0.1;
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    waitForTextPredictionsViewAndSelectCandidateAtIndex(index, callbackID, textPredictionsPollingInterval);
#else
    // FIXME: This is a no-op on non-internal builds due to UIKeyboardPredictionView being unavailable. Ideally, there should be a better way to
    // retrieve information and interact with the predictive text view that will be compatible with OpenSource.
    UNUSED_PARAM(index);
    UNUSED_PARAM(callback);
#endif
}

void UIScriptController::waitForTextPredictionsViewAndSelectCandidateAtIndex(long index, unsigned callbackID, float interval)
{
    id UIKeyboardPredictionViewClass = NSClassFromString(@"UIKeyboardPredictionView");
    if (!UIKeyboardPredictionViewClass)
        return;

#if USE(APPLE_INTERNAL_SDK)
    UIKeyboardPredictionView *predictionView = (UIKeyboardPredictionView *)[UIKeyboardPredictionViewClass activeInstance];
    if (![predictionView hasPredictions]) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, interval * NSEC_PER_SEC), dispatch_get_main_queue(), ^() {
            waitForTextPredictionsViewAndSelectCandidateAtIndex(index, callbackID, interval);
        });
        return;
    }

    PlatformWKView webView = TestController::singleton().mainWebView()->platformView();
    CGRect predictionViewFrame = [predictionView frame];
    // This assumes there are 3 predicted text cells of equal width, which is the case on iOS.
    float offsetX = (index * 2 + 1) * CGRectGetWidth(predictionViewFrame) / 6;
    float offsetY = CGRectGetHeight(webView.window.frame) - CGRectGetHeight([[predictionView superview] frame]) + CGRectGetHeight(predictionViewFrame) / 2;
    [[HIDEventGenerator sharedHIDEventGenerator] tap:CGPointMake(offsetX, offsetY) completionBlock:^{
        if (m_context)
            m_context->asyncTaskComplete(callbackID);
    }];
#else
    UNUSED_PARAM(index);
    UNUSED_PARAM(callbackID);
    UNUSED_PARAM(interval);
#endif
}

void UIScriptController::dismissFormAccessoryView()
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView dismissFormAccessoryView];
}

void UIScriptController::selectFormAccessoryPickerRow(long rowIndex)
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    [webView selectFormAccessoryPickerRow:rowIndex];
}
    
JSObjectRef UIScriptController::contentsOfUserInterfaceItem(JSStringRef interfaceItem) const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    NSDictionary *contentDictionary = [webView _contentsOfUserInterfaceItem:toWTFString(toWK(interfaceItem))];
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:contentDictionary inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
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

std::optional<bool> UIScriptController::stableStateOverride() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    if (webView._stableStateOverride)
        return webView._stableStateOverride.boolValue;

    return std::nullopt;
}

void UIScriptController::setStableStateOverride(std::optional<bool> overrideValue)
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

JSObjectRef UIScriptController::selectionRangeViewRects() const
{
    NSMutableArray *selectionRects = [[NSMutableArray alloc] init];
    NSArray *rects = TestController::singleton().mainWebView()->platformView()._uiTextSelectionRects;
    for (NSValue *rect in rects)
        [selectionRects addObject:toNSDictionary([rect CGRectValue])];

    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:selectionRects inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptController::textSelectionCaretRect() const
{
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary(TestController::singleton().mainWebView()->platformView()._uiTextCaretRect) inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
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
}

}

#endif // PLATFORM(IOS)
