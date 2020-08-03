/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#import "UIScriptControllerIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "HIDEventGenerator.h"
#import "PencilKitTestSPI.h"
#import "PlatformViewHelpers.h"
#import "PlatformWebView.h"
#import "StringFunctions.h"
#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import "UIKitSPI.h"
#import "UIScriptContext.h"
#import <JavaScriptCore/JavaScriptCore.h>
#import <JavaScriptCore/OpaqueJSString.h>
#import <UIKit/UIKit.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/FloatRect.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WebKit.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <wtf/BlockPtr.h>
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

Ref<UIScriptController> UIScriptController::create(UIScriptContext& context)
{
    return adoptRef(*new UIScriptControllerIOS(context));
}

void UIScriptControllerIOS::waitForOutstandingCallbacks()
{
    HIDEventGenerator *eventGenerator = HIDEventGenerator.sharedHIDEventGenerator;
    NSDate *timeoutDate = [NSDate dateWithTimeIntervalSinceNow:1];
    while (eventGenerator.hasOutstandingCallbacks) {
        [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:timeoutDate];
        if ([timeoutDate compare:NSDate.date] == NSOrderedAscending)
            [NSException raise:@"WebKitTestRunnerTestProblem" format:@"The previous test completed before all synthesized events had been handled. Perhaps you're calling notifyDone() too early?"];
    }
}

void UIScriptControllerIOS::doAfterPresentationUpdate(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView() _doAfterNextPresentationUpdate:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::doAfterNextStablePresentationUpdate(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView() _doAfterNextStablePresentationUpdate:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::ensurePositionInformationIsUpToDateAt(long x, long y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView() _requestActivatedElementAtPosition:CGPointMake(x, y) completionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] (_WKActivatedElementInfo *) {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::doAfterVisibleContentRectUpdate(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView() _doAfterNextVisibleContentRectUpdate:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::zoomToScale(double scale, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    [webView() zoomToScale:scale animated:YES completionHandler:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::retrieveSpeakSelectionContent(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    [webView() accessibilityRetrieveSpeakSelectionContentWithCompletionHandler:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

JSRetainPtr<JSStringRef> UIScriptControllerIOS::accessibilitySpeakSelectionContent() const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)webView().accessibilitySpeakSelectionContent));
}

void UIScriptControllerIOS::simulateAccessibilitySettingsChangeNotification(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto* webView = this->webView();
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center postNotificationName:UIAccessibilityInvertColorsStatusDidChangeNotification object:webView];

    [webView _doAfterNextPresentationUpdate:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

double UIScriptControllerIOS::zoomScale() const
{
    return webView().scrollView.zoomScale;
}

static CGPoint globalToContentCoordinates(TestRunnerWKWebView *webView, long x, long y)
{
    CGPoint point = CGPointMake(x, y);
    point = [webView _convertPointFromContentsToView:point];
    point = [webView convertPoint:point toView:nil];
    point = [webView.window convertPoint:point toWindow:nil];
    return point;
}

void UIScriptControllerIOS::touchDownAtPoint(long x, long y, long touchCount, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] touchDown:location touchCount:touchCount completionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::liftUpAtPoint(long x, long y, long touchCount, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] liftUp:location touchCount:touchCount completionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::singleTapAtPoint(long x, long y, JSValueRef callback)
{
    singleTapAtPointWithModifiers(x, y, nullptr, callback);
}

void UIScriptControllerIOS::activateAtPoint(long x, long y, JSValueRef callback)
{
    singleTapAtPoint(x, y, callback);
}

void UIScriptControllerIOS::waitForModalTransitionToFinish() const
{
    while ([webView().window.rootViewController isPerformingModalTransition])
        [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:NSDate.distantFuture];
}

void UIScriptControllerIOS::waitForSingleTapToReset() const
{
    bool doneWaitingForSingleTapToReset = false;
    [webView() _doAfterResettingSingleTapGesture:[&doneWaitingForSingleTapToReset] {
        doneWaitingForSingleTapToReset = true;
    }];
    TestController::singleton().runUntil(doneWaitingForSingleTapToReset, 0.5_s);
}

void UIScriptControllerIOS::twoFingerSingleTapAtPoint(long x, long y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] twoFingerTap:location completionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::singleTapAtPointWithModifiers(long x, long y, JSValueRef modifierArray, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    singleTapAtPointWithModifiers(WebCore::FloatPoint(x, y), parseModifierArray(m_context->jsContext(), modifierArray), makeBlockPtr([this, protectedThis = makeRefPtr(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }));
}

void UIScriptControllerIOS::singleTapAtPointWithModifiers(WebCore::FloatPoint location, Vector<String>&& modifierFlags, BlockPtr<void()>&& block)
{
    // Animations on the scroll view could be in progress to reveal a form control which may interfere with hit testing (see wkb.ug/205458).
    [webView().scrollView _removeAllAnimations:NO];

    // Necessary for popovers on iPad (used for elements such as <select>) to finish dismissing (see wkb.ug/206759).
    waitForModalTransitionToFinish();

    waitForSingleTapToReset();

    for (auto& modifierFlag : modifierFlags)
        [[HIDEventGenerator sharedHIDEventGenerator] keyDown:modifierFlag];

    [[HIDEventGenerator sharedHIDEventGenerator] tap:globalToContentCoordinates(webView(), location.x(), location.y()) completionBlock:[this, protectedThis = makeRef(*this), modifierFlags = WTFMove(modifierFlags), block = WTFMove(block)] () mutable {
        if (!m_context)
            return;

        for (size_t i = modifierFlags.size(); i; ) {
            --i;
            [[HIDEventGenerator sharedHIDEventGenerator] keyUp:modifierFlags[i]];
        }
        [[HIDEventGenerator sharedHIDEventGenerator] sendMarkerHIDEventWithCompletionBlock:block.get()];
    }];
}

void UIScriptControllerIOS::doubleTapAtPoint(long x, long y, float delay, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    [[HIDEventGenerator sharedHIDEventGenerator] doubleTap:globalToContentCoordinates(webView(), x, y) delay:delay completionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::stylusDownAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusDownAtPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure completionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::stylusMoveToPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusMoveToPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure completionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::stylusUpAtPoint(long x, long y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusUpAtPoint:location completionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::stylusTapAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
    stylusTapAtPointWithModifiers(x, y, azimuthAngle, altitudeAngle, pressure, nullptr, callback);
}

void UIScriptControllerIOS::stylusTapAtPointWithModifiers(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef modifierArray, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    waitForSingleTapToReset();

    auto modifierFlags = parseModifierArray(m_context->jsContext(), modifierArray);
    for (auto& modifierFlag : modifierFlags)
        [[HIDEventGenerator sharedHIDEventGenerator] keyDown:modifierFlag];

    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusTapAtPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure completionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID, modifierFlags = WTFMove(modifierFlags)] {
        if (!m_context)
            return;
        for (size_t i = modifierFlags.size(); i; ) {
            --i;
            [[HIDEventGenerator sharedHIDEventGenerator] keyUp:modifierFlags[i]];
        }
        [[HIDEventGenerator sharedHIDEventGenerator] sendMarkerHIDEventWithCompletionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
            if (!m_context)
                return;
            m_context->asyncTaskComplete(callbackID);
        }).get()];
    }).get()];
}

void convertCoordinates(TestRunnerWKWebView *webView, NSMutableDictionary *event)
{
    if (event[HIDEventTouchesKey]) {
        for (NSMutableDictionary *touch in event[HIDEventTouchesKey]) {
            auto location = globalToContentCoordinates(webView, (long)[touch[HIDEventXKey] doubleValue], (long)[touch[HIDEventYKey]doubleValue]);
            touch[HIDEventXKey] = @(location.x);
            touch[HIDEventYKey] = @(location.y);
        }
    }
}

void UIScriptControllerIOS::sendEventStream(JSStringRef eventsJSON, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    String jsonString = eventsJSON->string();
    auto eventInfo = dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:[(NSString *)jsonString dataUsingEncoding:NSUTF8StringEncoding] options:NSJSONReadingMutableContainers | NSJSONReadingMutableLeaves error:nil]);

    auto *webView = this->webView();
    
    for (NSMutableDictionary *event in eventInfo[TopLevelEventInfoKey]) {
        if (![event[HIDEventCoordinateSpaceKey] isEqualToString:HIDEventCoordinateSpaceTypeContent])
            continue;
        
        if (event[HIDEventStartEventKey])
            convertCoordinates(webView, event[HIDEventStartEventKey]);
        
        if (event[HIDEventEndEventKey])
            convertCoordinates(webView, event[HIDEventEndEventKey]);
        
        if (event[HIDEventTouchesKey])
            convertCoordinates(webView, event);
    }
    
    if (!eventInfo || ![eventInfo isKindOfClass:[NSDictionary class]]) {
        WTFLogAlways("JSON is not convertible to a dictionary");
        return;
    }

    auto completion = makeBlockPtr([this, protectedThis = makeRefPtr(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });

    [[HIDEventGenerator sharedHIDEventGenerator] sendEventStream:eventInfo completionBlock:completion.get()];
}

static NSDictionary *dictionaryForFingerEventWithContentPoint(CGPoint point, NSString* phase, Seconds timeOffset)
{
    return @{
        HIDEventCoordinateSpaceKey : HIDEventCoordinateSpaceTypeContent,
        HIDEventTimeOffsetKey : @(timeOffset.seconds()),
        HIDEventInputType : HIDEventInputTypeHand,
        HIDEventTouchesKey : @[
            @{
                HIDEventTouchIDKey : @1,
                HIDEventInputType : HIDEventInputTypeFinger,
                HIDEventPhaseKey : phase,
                HIDEventXKey : @(point.x),
                HIDEventYKey : @(point.y),
            },
        ],
    };
}

void UIScriptControllerIOS::dragFromPointToPoint(long startX, long startY, long endX, long endY, double durationSeconds, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    CGPoint startPoint = globalToContentCoordinates(webView(), startX, startY);
    CGPoint endPoint = globalToContentCoordinates(webView(), endX, endY);

    NSDictionary *touchDownInfo = dictionaryForFingerEventWithContentPoint(startPoint, HIDEventPhaseBegan, 0_s);
    NSDictionary *interpolatedEvents = @{
        HIDEventInterpolateKey : HIDEventInterpolationTypeLinear,
        HIDEventCoordinateSpaceKey : HIDEventCoordinateSpaceTypeContent,
        HIDEventTimestepKey : @(0.016),
        HIDEventStartEventKey : dictionaryForFingerEventWithContentPoint(startPoint, HIDEventPhaseMoved, 0_s),
        HIDEventEndEventKey : dictionaryForFingerEventWithContentPoint(endPoint, HIDEventPhaseMoved, Seconds(durationSeconds)),
    };
    NSDictionary *liftUpInfo = dictionaryForFingerEventWithContentPoint(endPoint, HIDEventPhaseEnded, Seconds(durationSeconds));

    NSDictionary *eventStream = @{
        TopLevelEventInfoKey : @[
            touchDownInfo,
            interpolatedEvents,
            liftUpInfo,
        ],
    };

    auto completion = makeBlockPtr([this, protectedThis = makeRefPtr(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });

    [[HIDEventGenerator sharedHIDEventGenerator] sendEventStream:eventStream completionBlock:completion.get()];
}
    
void UIScriptControllerIOS::longPressAtPoint(long x, long y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    [[HIDEventGenerator sharedHIDEventGenerator] longPress:globalToContentCoordinates(webView(), x, y) completionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::enterText(JSStringRef text)
{
    auto textAsCFString = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, text));
    [webView() _simulateTextEntered:(NSString *)textAsCFString.get()];
}

void UIScriptControllerIOS::typeCharacterUsingHardwareKeyboard(JSStringRef character, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    // Assumes that the keyboard is already shown.
    [[HIDEventGenerator sharedHIDEventGenerator] keyPress:toWTFString(toWK(character)) completionBlock:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

static UIPhysicalKeyboardEvent *createUIPhysicalKeyboardEvent(NSString *hidInputString, NSString *uiEventInputString, UIKeyModifierFlags modifierFlags, UIKeyboardInputFlags inputFlags, bool isKeyDown)
{
    auto* keyboardEvent = [getUIPhysicalKeyboardEventClass() _eventWithInput:uiEventInputString inputFlags:inputFlags];
    keyboardEvent._modifierFlags = modifierFlags;
    auto hidEvent = createHIDKeyEvent(hidInputString, keyboardEvent.timestamp, isKeyDown);
    [keyboardEvent _setHIDEvent:hidEvent.get() keyboard:nullptr];
    return keyboardEvent;
}

void UIScriptControllerIOS::rawKeyDown(JSStringRef key)
{
    // Key can be either a single Unicode code point or the name of a special key (e.g. "downArrow").
    // HIDEventGenerator knows how to map these special keys to the appropriate keycode.
    [[HIDEventGenerator sharedHIDEventGenerator] keyDown:toWTFString(toWK(key))];
    [[HIDEventGenerator sharedHIDEventGenerator] sendMarkerHIDEventWithCompletionBlock:^{ /* Do nothing */ }];
}

void UIScriptControllerIOS::rawKeyUp(JSStringRef key)
{
    // Key can be either a single Unicode code point or the name of a special key (e.g. "downArrow").
    // HIDEventGenerator knows how to map these special keys to the appropriate keycode.
    [[HIDEventGenerator sharedHIDEventGenerator] keyUp:toWTFString(toWK(key))];
    [[HIDEventGenerator sharedHIDEventGenerator] sendMarkerHIDEventWithCompletionBlock:^{ /* Do nothing */ }];
}

void UIScriptControllerIOS::keyDown(JSStringRef character, JSValueRef modifierArray)
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

void UIScriptControllerIOS::dismissFormAccessoryView()
{
    [webView() dismissFormAccessoryView];
}

void UIScriptControllerIOS::dismissFilePicker(JSValueRef callback)
{
    TestRunnerWKWebView *webView = this->webView();
    [webView _dismissFilePicker];

    // Round-trip with the WebProcess to make sure it has been notified of the dismissal.
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView evaluateJavaScript:@"" completionHandler:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] (id result, NSError *error) {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

JSRetainPtr<JSStringRef> UIScriptControllerIOS::selectFormPopoverTitle() const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)webView().selectFormPopoverTitle));
}

JSRetainPtr<JSStringRef> UIScriptControllerIOS::textContentType() const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)(webView().textContentTypeForTesting ?: @"")));
}

JSRetainPtr<JSStringRef> UIScriptControllerIOS::formInputLabel() const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)webView().formInputLabel));
}

void UIScriptControllerIOS::selectFormAccessoryPickerRow(long rowIndex)
{
    [webView() selectFormAccessoryPickerRow:rowIndex];
}

bool UIScriptControllerIOS::selectFormAccessoryHasCheckedItemAtRow(long rowIndex) const
{
    return [webView() selectFormAccessoryHasCheckedItemAtRow:rowIndex];
}

void UIScriptControllerIOS::setTimePickerValue(long hour, long minute)
{
    [webView() setTimePickerValueToHour:hour minute:minute];
}

double UIScriptControllerIOS::timePickerValueHour() const
{
    return [webView() timePickerValueHour];
}

double UIScriptControllerIOS::timePickerValueMinute() const
{
    return [webView() timePickerValueMinute];
}

bool UIScriptControllerIOS::isPresentingModally() const
{
    return !!webView().window.rootViewController.presentedViewController;
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

double UIScriptControllerIOS::contentOffsetX() const
{
    return webView().scrollView.contentOffset.x;
}

double UIScriptControllerIOS::contentOffsetY() const
{
    return webView().scrollView.contentOffset.y;
}

bool UIScriptControllerIOS::scrollUpdatesDisabled() const
{
    return webView()._scrollingUpdatesDisabledForTesting;
}

void UIScriptControllerIOS::setScrollUpdatesDisabled(bool disabled)
{
    webView()._scrollingUpdatesDisabledForTesting = disabled;
}

void UIScriptControllerIOS::scrollToOffset(long x, long y)
{
    TestRunnerWKWebView *webView = this->webView();
    [webView.scrollView setContentOffset:contentOffsetBoundedInValidRange(webView.scrollView, CGPointMake(x, y)) animated:YES];
}

void UIScriptControllerIOS::immediateScrollToOffset(long x, long y)
{
    TestRunnerWKWebView *webView = this->webView();
    [webView.scrollView setContentOffset:contentOffsetBoundedInValidRange(webView.scrollView, CGPointMake(x, y)) animated:NO];
}

static UIScrollView *enclosingScrollViewIncludingSelf(UIView *view)
{
    do {
        if ([view isKindOfClass:[UIScrollView class]])
            return static_cast<UIScrollView *>(view);
    } while ((view = [view superview]));

    return nil;
}

void UIScriptControllerIOS::immediateScrollElementAtContentPointToOffset(long x, long y, long xScrollOffset, long yScrollOffset)
{
    UIView *contentView = platformContentView();
    UIView *hitView = [contentView hitTest:CGPointMake(x, y) withEvent:nil];
    UIScrollView *enclosingScrollView = enclosingScrollViewIncludingSelf(hitView);
    [enclosingScrollView setContentOffset:CGPointMake(xScrollOffset, yScrollOffset)];
}

void UIScriptControllerIOS::immediateZoomToScale(double scale)
{
    [webView().scrollView setZoomScale:scale animated:NO];
}

void UIScriptControllerIOS::keyboardAccessoryBarNext()
{
    [webView() keyboardAccessoryBarNext];
}

void UIScriptControllerIOS::keyboardAccessoryBarPrevious()
{
    [webView() keyboardAccessoryBarPrevious];
}

bool UIScriptControllerIOS::isShowingKeyboard() const
{
    return webView().showingKeyboard;
}

bool UIScriptControllerIOS::hasInputSession() const
{
    return webView().isInteractingWithFormControl;
}

void UIScriptControllerIOS::applyAutocorrection(JSStringRef newString, JSStringRef oldString, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    TestRunnerWKWebView *webView = this->webView();
    [webView applyAutocorrection:toWTFString(toWK(newString)) toString:toWTFString(toWK(oldString)) withCompletionHandler:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
            // applyAutocorrection can call its completion handler synchronously,
            // which makes UIScriptController unhappy (see bug 172884).
            if (!m_context)
                return;
            m_context->asyncTaskComplete(callbackID);
        }).get());
    }).get()];
}

double UIScriptControllerIOS::minimumZoomScale() const
{
    return webView().scrollView.minimumZoomScale;
}

double UIScriptControllerIOS::maximumZoomScale() const
{
    return webView().scrollView.maximumZoomScale;
}

Optional<bool> UIScriptControllerIOS::stableStateOverride() const
{
    TestRunnerWKWebView *webView = this->webView();
    if (webView._stableStateOverride)
        return webView._stableStateOverride.boolValue;

    return WTF::nullopt;
}

void UIScriptControllerIOS::setStableStateOverride(Optional<bool> overrideValue)
{
    TestRunnerWKWebView *webView = this->webView();
    if (overrideValue)
        webView._stableStateOverride = @(overrideValue.value());
    else
        webView._stableStateOverride = nil;
}

JSObjectRef UIScriptControllerIOS::contentVisibleRect() const
{
    CGRect contentVisibleRect = webView()._contentVisibleRect;
    
    WebCore::FloatRect rect(contentVisibleRect.origin.x, contentVisibleRect.origin.y, contentVisibleRect.size.width, contentVisibleRect.size.height);
    return m_context->objectFromRect(rect);
}

JSObjectRef UIScriptControllerIOS::textSelectionRangeRects() const
{
    auto selectionRects = adoptNS([[NSMutableArray alloc] init]);
    NSArray *rects = webView()._uiTextSelectionRects;
    for (NSValue *rect in rects)
        [selectionRects addObject:toNSDictionary(rect.CGRectValue)];

    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:selectionRects.get() inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptControllerIOS::textSelectionCaretRect() const
{
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary(webView()._uiTextCaretRect) inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptControllerIOS::selectionStartGrabberViewRect() const
{
    UIView *contentView = platformContentView();
    UIView *selectionRangeView = [contentView valueForKeyPath:@"interactionAssistant.selectionView.rangeView"];
    auto frameInContentCoordinates = [selectionRangeView convertRect:[[selectionRangeView valueForKeyPath:@"startGrabber"] frame] toView:contentView];
    frameInContentCoordinates = CGRectIntersection(contentView.bounds, frameInContentCoordinates);
    auto jsContext = m_context->jsContext();
    return JSValueToObject(jsContext, [JSValue valueWithObject:toNSDictionary(frameInContentCoordinates) inContext:[JSContext contextWithJSGlobalContextRef:jsContext]].JSValueRef, nullptr);
}

JSObjectRef UIScriptControllerIOS::selectionEndGrabberViewRect() const
{
    UIView *contentView = platformContentView();
    UIView *selectionRangeView = [contentView valueForKeyPath:@"interactionAssistant.selectionView.rangeView"];
    auto frameInContentCoordinates = [selectionRangeView convertRect:[[selectionRangeView valueForKeyPath:@"endGrabber"] frame] toView:contentView];
    frameInContentCoordinates = CGRectIntersection(contentView.bounds, frameInContentCoordinates);
    auto jsContext = m_context->jsContext();
    return JSValueToObject(jsContext, [JSValue valueWithObject:toNSDictionary(frameInContentCoordinates) inContext:[JSContext contextWithJSGlobalContextRef:jsContext]].JSValueRef, nullptr);
}

JSObjectRef UIScriptControllerIOS::selectionCaretViewRect() const
{
    UIView *contentView = platformContentView();
    UIView *caretView = [contentView valueForKeyPath:@"interactionAssistant.selectionView.caretView"];
    auto rectInContentViewCoordinates = CGRectIntersection([caretView convertRect:caretView.bounds toView:contentView], contentView.bounds);
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary(rectInContentViewCoordinates) inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptControllerIOS::selectionRangeViewRects() const
{
    UIView *contentView = platformContentView();
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

JSObjectRef UIScriptControllerIOS::inputViewBounds() const
{
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary(webView()._inputViewBounds) inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

void UIScriptControllerIOS::removeAllDynamicDictionaries()
{
    [UIKeyboard removeAllDynamicDictionaries];
}

JSRetainPtr<JSStringRef> UIScriptControllerIOS::scrollingTreeAsText() const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)[webView() _scrollingTreeAsText]));
}

JSObjectRef UIScriptControllerIOS::propertiesOfLayerWithID(uint64_t layerID) const
{
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:[webView() _propertiesOfLayerWithID:layerID] inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

bool UIScriptControllerIOS::mayContainEditableElementsInRect(unsigned x, unsigned y, unsigned width, unsigned height)
{
    auto contentRect = CGRectMake(x, y, width, height);
    return [webView() _mayContainEditableElementsInRect:[webView() convertRect:contentRect fromView:platformContentView()]];
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

void UIScriptControllerIOS::simulateRotation(DeviceOrientation* orientation, JSValueRef callback)
{
    TestRunnerWKWebView *webView = this->webView();
    webView.usesSafariLikeRotation = NO;
    
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    webView.rotationDidEndCallback = makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get();
    
    [[UIDevice currentDevice] setOrientation:toUIDeviceOrientation(orientation) animated:YES];
}

void UIScriptControllerIOS::simulateRotationLikeSafari(DeviceOrientation* orientation, JSValueRef callback)
{
    TestRunnerWKWebView *webView = this->webView();
    webView.usesSafariLikeRotation = YES;
    
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    webView.rotationDidEndCallback = makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get();
    
    [[UIDevice currentDevice] setOrientation:toUIDeviceOrientation(orientation) animated:YES];
}

void UIScriptControllerIOS::setDidStartFormControlInteractionCallback(JSValueRef callback)
{
    UIScriptController::setDidStartFormControlInteractionCallback(callback);
    webView().didStartFormControlInteractionCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidStartFormControlInteraction);
    }).get();
}

void UIScriptControllerIOS::setDidEndFormControlInteractionCallback(JSValueRef callback)
{
    UIScriptController::setDidEndFormControlInteractionCallback(callback);
    webView().didEndFormControlInteractionCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidEndFormControlInteraction);
    }).get();
}
    
void UIScriptControllerIOS::setDidShowContextMenuCallback(JSValueRef callback)
{
    UIScriptController::setDidShowContextMenuCallback(callback);
    webView().didShowContextMenuCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidShowContextMenu);
    }).get();
}

void UIScriptControllerIOS::setDidDismissContextMenuCallback(JSValueRef callback)
{
    UIScriptController::setDidDismissContextMenuCallback(callback);
    webView().didDismissContextMenuCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidDismissContextMenu);
    }).get();
}

void UIScriptControllerIOS::setWillBeginZoomingCallback(JSValueRef callback)
{
    UIScriptController::setWillBeginZoomingCallback(callback);
    webView().willBeginZoomingCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeWillBeginZooming);
    }).get();
}

void UIScriptControllerIOS::setDidEndZoomingCallback(JSValueRef callback)
{
    UIScriptController::setDidEndZoomingCallback(callback);
    webView().didEndZoomingCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidEndZooming);
    }).get();
}

void UIScriptControllerIOS::setDidShowKeyboardCallback(JSValueRef callback)
{
    UIScriptController::setDidShowKeyboardCallback(callback);
    webView().didShowKeyboardCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidShowKeyboard);
    }).get();
}

void UIScriptControllerIOS::setDidHideKeyboardCallback(JSValueRef callback)
{
    UIScriptController::setDidHideKeyboardCallback(callback);
    webView().didHideKeyboardCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidHideKeyboard);
    }).get();
}

void UIScriptControllerIOS::chooseMenuAction(JSStringRef jsAction, JSValueRef callback)
{
    auto action = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, jsAction));
    auto rect = rectForMenuAction(action.get());
    if (rect.isEmpty())
        return;

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    singleTapAtPointWithModifiers(rect.center(), { }, makeBlockPtr([this, protectedThis = makeRef(*this), callbackID] {
        if (m_context)
            m_context->asyncTaskComplete(callbackID);
    }));
}

bool UIScriptControllerIOS::isShowingPopover() const
{
    return webView().showingPopover;
}

void UIScriptControllerIOS::setWillPresentPopoverCallback(JSValueRef callback)
{
    UIScriptController::setWillPresentPopoverCallback(callback);
    webView().willPresentPopoverCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeWillPresentPopover);
    }).get();
}

void UIScriptControllerIOS::setDidDismissPopoverCallback(JSValueRef callback)
{
    UIScriptController::setDidDismissPopoverCallback(callback);
    webView().didDismissPopoverCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidDismissPopover);
    }).get();
}

JSObjectRef UIScriptControllerIOS::rectForMenuAction(JSStringRef jsAction) const
{
    auto action = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, jsAction));
    auto rect = rectForMenuAction(action.get());
    if (rect.isEmpty())
        return nullptr;

    return m_context->objectFromRect(rect);
}

WebCore::FloatRect UIScriptControllerIOS::rectForMenuAction(CFStringRef action) const
{
    UIWindow *windowForButton = nil;
    UIButton *buttonForAction = nil;
    UIView *calloutBar = UICalloutBar.activeCalloutBar;
    if (!calloutBar.window)
        return { };

    for (UIButton *button in findAllViewsInHierarchyOfType(calloutBar, UIButton.class)) {
        NSString *buttonTitle = [button titleForState:UIControlStateNormal];
        if (!buttonTitle.length)
            continue;

        if (![buttonTitle isEqualToString:(__bridge NSString *)action])
            continue;

        buttonForAction = button;
        windowForButton = calloutBar.window;
        break;
    }

    if (!buttonForAction)
        return { };

    CGRect rectInRootViewCoordinates = [buttonForAction convertRect:buttonForAction.bounds toView:platformContentView()];
    return WebCore::FloatRect(rectInRootViewCoordinates.origin.x, rectInRootViewCoordinates.origin.y, rectInRootViewCoordinates.size.width, rectInRootViewCoordinates.size.height);
}

JSObjectRef UIScriptControllerIOS::menuRect() const
{
    UIView *calloutBar = UICalloutBar.activeCalloutBar;
    if (!calloutBar.window)
        return nullptr;

    CGRect rectInRootViewCoordinates = [calloutBar convertRect:calloutBar.bounds toView:platformContentView()];
    return m_context->objectFromRect(WebCore::FloatRect(rectInRootViewCoordinates.origin.x, rectInRootViewCoordinates.origin.y, rectInRootViewCoordinates.size.width, rectInRootViewCoordinates.size.height));
}

bool UIScriptControllerIOS::isDismissingMenu() const
{
    return webView().dismissingMenu;
}

void UIScriptControllerIOS::setDidEndScrollingCallback(JSValueRef callback)
{
    UIScriptController::setDidEndScrollingCallback(callback);
    webView().didEndScrollingCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidEndScrolling);
    }).get();
}

void UIScriptControllerIOS::clearAllCallbacks()
{
    [webView() resetInteractionCallbacks];
}

void UIScriptControllerIOS::setSafeAreaInsets(double top, double right, double bottom, double left)
{
    UIEdgeInsets insets = UIEdgeInsetsMake(top, left, bottom, right);
    webView().overrideSafeAreaInsets = insets;
}

void UIScriptControllerIOS::beginBackSwipe(JSValueRef callback)
{
    [webView() _beginBackSwipeForTesting];
}

void UIScriptControllerIOS::completeBackSwipe(JSValueRef callback)
{
    [webView() _completeBackSwipeForTesting];
}

void UIScriptControllerIOS::activateDataListSuggestion(unsigned index, JSValueRef callback)
{
    // FIXME: Not implemented.
    UNUSED_PARAM(index);

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get());
}

bool UIScriptControllerIOS::isShowingDataListSuggestions() const
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

PKCanvasView *UIScriptControllerIOS::findEditableImageCanvas() const
{
    Class pkCanvasViewClass = NSClassFromString(@"PKCanvasView");
    __block PKCanvasView *canvasView = nil;
    forEachViewInHierarchy(webView().window, ^(UIView *subview, BOOL *stop) {
        if (![subview isKindOfClass:pkCanvasViewClass])
            return;

        canvasView = (PKCanvasView *)subview;
        *stop = YES;
    });
    return canvasView;
}

#endif // HAVE(PENCILKIT)

void UIScriptControllerIOS::drawSquareInEditableImage()
{
#if HAVE(PENCILKIT)
    Class pkDrawingClass = NSClassFromString(@"PKDrawing");
    Class pkInkClass = NSClassFromString(@"PKInk");
    Class pkStrokeClass = NSClassFromString(@"PKStroke");

    PKCanvasView *canvasView = findEditableImageCanvas();
    RetainPtr<PKDrawing> drawing = canvasView.drawing ?: adoptNS([[pkDrawingClass alloc] init]);
    RetainPtr<CGPathRef> path = adoptCF(CGPathCreateWithRect(CGRectMake(0, 0, 50, 50), NULL));
    RetainPtr<PKInk> ink = [pkInkClass inkWithIdentifier:@"com.apple.ink.pen" color:UIColor.greenColor weight:100.0];
    RetainPtr<PKStroke> stroke = adoptNS([[pkStrokeClass alloc] _initWithPath:path.get() ink:ink.get() inputScale:1]);
    [drawing _addStroke:stroke.get()];

    [canvasView setDrawing:drawing.get()];
#endif
}

long UIScriptControllerIOS::numberOfStrokesInEditableImage()
{
#if HAVE(PENCILKIT)
    PKCanvasView *canvasView = findEditableImageCanvas();
    return canvasView.drawing._allStrokes.count;
#else
    return 0;
#endif
}

void UIScriptControllerIOS::setKeyboardInputModeIdentifier(JSStringRef identifier)
{
    TestController::singleton().setKeyboardInputModeIdentifier(toWTFString(toWK(identifier)));
}

// FIXME: Write this in terms of HIDEventGenerator once we know how to reset caps lock state
// on test completion to avoid it effecting subsequent tests.
void UIScriptControllerIOS::toggleCapsLock(JSValueRef callback)
{
    m_capsLockOn = !m_capsLockOn;
    auto *keyboardEvent = createUIPhysicalKeyboardEvent(@"capsLock", [NSString string], m_capsLockOn ? UIKeyModifierAlphaShift : 0,
        kUIKeyboardInputModifierFlagsChanged, m_capsLockOn);
    [[UIApplication sharedApplication] handleKeyUIEvent:keyboardEvent];
    doAsyncTask(callback);
}

bool UIScriptControllerIOS::keyboardIsAutomaticallyShifted() const
{
    return UIKeyboardImpl.activeInstance.isAutoShifted;
}

JSObjectRef UIScriptControllerIOS::attachmentInfo(JSStringRef jsAttachmentIdentifier)
{
    auto attachmentIdentifier = toWTFString(toWK(jsAttachmentIdentifier));
    _WKAttachment *attachment = [webView() _attachmentForIdentifier:attachmentIdentifier];
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

UIView *UIScriptControllerIOS::platformContentView() const
{
    return webView().contentView;
}

JSObjectRef UIScriptControllerIOS::calendarType() const
{
    UIView *contentView = webView().contentView;
    NSString *calendarTypeString = [contentView valueForKeyPath:@"dateTimeInputControl.dateTimePickerCalendarType"];
    auto jsContext = m_context->jsContext();
    return JSValueToObject(jsContext, [JSValue valueWithObject:calendarTypeString inContext:[JSContext contextWithJSGlobalContextRef:jsContext]].JSValueRef, nullptr);
}

void UIScriptControllerIOS::setHardwareKeyboardAttached(bool attached)
{
    GSEventSetHardwareKeyboardAttached(attached, 0);
}

void UIScriptControllerIOS::setAllowsViewportShrinkToFit(bool allows)
{
    webView()._allowsViewportShrinkToFit = allows;
}

void UIScriptControllerIOS::doAfterDoubleTapDelay(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    NSTimeInterval maximumIntervalBetweenSuccessiveTaps = 0;
    for (UIGestureRecognizer *gesture in [platformContentView() gestureRecognizers]) {
        if (![gesture isKindOfClass:[UITapGestureRecognizer class]])
            continue;

        UITapGestureRecognizer *tapGesture = (UITapGestureRecognizer *)gesture;
        if (tapGesture.numberOfTapsRequired < 2)
            continue;

        if (tapGesture.maximumIntervalBetweenSuccessiveTaps > maximumIntervalBetweenSuccessiveTaps)
            maximumIntervalBetweenSuccessiveTaps = tapGesture.maximumIntervalBetweenSuccessiveTaps;
    }

    if (maximumIntervalBetweenSuccessiveTaps) {
        const NSTimeInterval additionalDelayBetweenSuccessiveTaps = 0.01;
        maximumIntervalBetweenSuccessiveTaps += additionalDelayBetweenSuccessiveTaps;
    }

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(maximumIntervalBetweenSuccessiveTaps * NSEC_PER_SEC)), dispatch_get_main_queue(), makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get());
}

void UIScriptControllerIOS::copyText(JSStringRef text)
{
    UIPasteboard.generalPasteboard.string = text->string();
}

void UIScriptControllerIOS::installTapGestureOnWindow(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeWindowTapRecognized);
    webView().windowTapRecognizedCallback = makeBlockPtr([this, strongThis = makeRef(*this)] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeWindowTapRecognized);
    }).get();
}

bool UIScriptControllerIOS::isShowingContextMenu() const
{
    return webView().showingContextMenu;
}

}

#endif // PLATFORM(IOS_FAMILY)
