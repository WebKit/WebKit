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

#import "CocoaColorSerialization.h"
#import "HIDEventGenerator.h"
#import "PlatformViewHelpers.h"
#import "PlatformWebView.h"
#import "StringFunctions.h"
#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "UIScriptContext.h"
#import <JavaScriptCore/JavaScriptCore.h>
#import <JavaScriptCore/OpaqueJSString.h>
#import <UIKit/UIKit.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/FloatRect.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WebKit.h>
#import <pal/spi/ios/BrowserEngineKitSPI.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MonotonicTime.h>
#import <wtf/SoftLinking.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIPhysicalKeyboardEvent)

@interface UIPhysicalKeyboardEvent (UIPhysicalKeyboardEventHack)
@property (nonatomic, assign, setter=_setModifierFlags:) NSInteger _modifierFlags;
@end

@interface UIScrollView (WebKitTestRunner)
- (CGRect)_wtr_visibleBoundsInCoordinateSpace:(id<UICoordinateSpace>)coordinateSpace;
@end

@implementation UIScrollView (WebKitTestRunner)

- (CGRect)_wtr_visibleBoundsInCoordinateSpace:(id<UICoordinateSpace>)coordinateSpace
{
    auto scrollOffset = self.contentOffset;
    auto scrollSize = self.bounds.size;
    auto visibleRect = CGRectMake(scrollOffset.x, scrollOffset.y, scrollSize.width, scrollSize.height);
    return [self convertRect:visibleRect toCoordinateSpace:coordinateSpace];
}

@end

namespace WTR {

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)

static bool isHiddenOrHasHiddenAncestor(UIView *view)
{
    for (auto currentAncestor = view; currentAncestor; currentAncestor = currentAncestor.superview) {
        if (currentAncestor.hidden)
            return true;
    }
    return false;
}

#endif // HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)

static NSDictionary *toNSDictionary(CGRect rect)
{
    return @{
        @"left": @(rect.origin.x),
        @"top": @(rect.origin.y),
        @"width": @(rect.size.width),
        @"height": @(rect.size.height)
    };
}

static NSDictionary *toNSDictionary(UIEdgeInsets insets)
{
    return @{
        @"top" : @(insets.top),
        @"left" : @(insets.left),
        @"bottom" : @(insets.bottom),
        @"right" : @(insets.right)
    };
}

static RetainPtr<NSDictionary> toNSDictionary(CGPathRef path)
{
    auto pathElementTypeToString = [](CGPathElementType type) {
        switch (type) {
        case kCGPathElementMoveToPoint:
            return @"MoveToPoint";

        case kCGPathElementAddLineToPoint:
            return @"AddLineToPoint";

        case kCGPathElementAddQuadCurveToPoint:
            return @"AddQuadCurveToPoint";

        case kCGPathElementAddCurveToPoint:
            return @"AddCurveToPoint";

        case kCGPathElementCloseSubpath:
            return @"CloseSubpath";

        default:
            return @"Unknown";
        }
    };

    auto attributes = adoptNS([[NSMutableDictionary alloc] init]);

    CGPathApplyWithBlock(path, ^(const CGPathElement *element) {
        if (!element)
            return;

        NSString *typeString = pathElementTypeToString(element->type);
        [attributes setObject:@{
            @"x": @(element->points->x),
            @"y": @(element->points->y),
        } forKey:typeString];
    });

    return attributes;
}

static Vector<String> parseModifierArray(JSContextRef context, JSValueRef arrayValue)
{
    if (!arrayValue)
        return { };

    // The value may either be a string with a single modifier or an array of modifiers.
    if (JSValueIsString(context, arrayValue))
        return { toWTFString(context, arrayValue) };

    if (!JSValueIsObject(context, arrayValue))
        return { };
    JSObjectRef array = const_cast<JSObjectRef>(arrayValue);
    unsigned length = arrayLength(context, array);
    Vector<String> modifiers;
    modifiers.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i)
        modifiers.append(toWTFString(context, JSObjectGetPropertyAtIndex(context, array, i, nullptr)));
    return modifiers;
}

static Class internalClassNamed(NSString *className)
{
    auto result = NSClassFromString(className);
    if (!result)
        NSLog(@"Warning: an internal class named '%@' does not exist.", className);
    return result;
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

void UIScriptControllerIOS::doAfterNextStablePresentationUpdate(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView() _doAfterNextStablePresentationUpdate:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::ensurePositionInformationIsUpToDateAt(long x, long y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView() _requestActivatedElementAtPosition:CGPointMake(x, y) completionBlock:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] (_WKActivatedElementInfo *) {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::doAfterVisibleContentRectUpdate(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView() _doAfterNextVisibleContentRectUpdate:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::doAfterNextVisibleContentRectAndStablePresentationUpdate(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView() _doAfterNextVisibleContentRectAndStablePresentationUpdate:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::zoomToScale(double scale, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    [webView() zoomToScale:scale animated:YES completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::retrieveSpeakSelectionContent(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    [webView() accessibilityRetrieveSpeakSelectionContentWithCompletionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
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

    [webView _doAfterNextPresentationUpdate:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

double UIScriptControllerIOS::zoomScale() const
{
    return webView().scrollView.zoomScale;
}

bool UIScriptControllerIOS::isZoomingOrScrolling() const
{
    return webView().zoomingOrScrolling;
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
    [[HIDEventGenerator sharedHIDEventGenerator] touchDown:location touchCount:touchCount completionBlock:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::liftUpAtPoint(long x, long y, long touchCount, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] liftUp:location touchCount:touchCount completionBlock:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
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
    auto allPendingSingleTapGesturesHaveBeenReset = [&]() -> bool {
        for (UIGestureRecognizer *gesture in [platformContentView() gestureRecognizers]) {
            if (!gesture.enabled || ![gesture isKindOfClass:UITapGestureRecognizer.class])
                continue;

            UITapGestureRecognizer *tapGesture = (UITapGestureRecognizer *)gesture;
            if (tapGesture.numberOfTapsRequired != 1 || tapGesture.numberOfTouches != 1 || tapGesture.state == UIGestureRecognizerStatePossible)
                continue;

            return false;
        }
        return true;
    };

    auto startTime = MonotonicTime::now();
    while (!allPendingSingleTapGesturesHaveBeenReset()) {
        [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:NSDate.distantFuture];
        if (MonotonicTime::now() - startTime > 1_s)
            break;
    }
}

void UIScriptControllerIOS::twoFingerSingleTapAtPoint(long x, long y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] twoFingerTap:location completionBlock:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::singleTapAtPointWithModifiers(long x, long y, JSValueRef modifierArray, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    singleTapAtPointWithModifiers(WebCore::FloatPoint(x, y), parseModifierArray(m_context->jsContext(), modifierArray), makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
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

    [[HIDEventGenerator sharedHIDEventGenerator] tap:globalToContentCoordinates(webView(), location.x(), location.y()) completionBlock:[this, protectedThis = Ref { *this }, modifierFlags = WTFMove(modifierFlags), block = WTFMove(block)] () mutable {
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

    [[HIDEventGenerator sharedHIDEventGenerator] doubleTap:globalToContentCoordinates(webView(), x, y) delay:delay completionBlock:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::stylusDownAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusDownAtPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure completionBlock:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::stylusMoveToPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusMoveToPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure completionBlock:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerIOS::stylusUpAtPoint(long x, long y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto location = globalToContentCoordinates(webView(), x, y);
    [[HIDEventGenerator sharedHIDEventGenerator] stylusUpAtPoint:location completionBlock:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
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
    [[HIDEventGenerator sharedHIDEventGenerator] stylusTapAtPoint:location azimuthAngle:azimuthAngle altitudeAngle:altitudeAngle pressure:pressure completionBlock:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID, modifierFlags = WTFMove(modifierFlags)] {
        if (!m_context)
            return;
        for (size_t i = modifierFlags.size(); i; ) {
            --i;
            [[HIDEventGenerator sharedHIDEventGenerator] keyUp:modifierFlags[i]];
        }
        [[HIDEventGenerator sharedHIDEventGenerator] sendMarkerHIDEventWithCompletionBlock:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
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
            NSNumber *touchX = touch[HIDEventXKey] == [NSNull null] ? nil : touch[HIDEventXKey];
            NSNumber *touchY = touch[HIDEventYKey] == [NSNull null] ? nil : touch[HIDEventYKey];
            
            auto location = globalToContentCoordinates(webView, (long)[touchX doubleValue], (long)[touchY doubleValue]);
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

    auto completion = makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
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

    auto completion = makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });

    [[HIDEventGenerator sharedHIDEventGenerator] sendEventStream:eventStream completionBlock:completion.get()];
}
    
void UIScriptControllerIOS::longPressAtPoint(long x, long y, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    
    [[HIDEventGenerator sharedHIDEventGenerator] longPress:globalToContentCoordinates(webView(), x, y) completionBlock:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
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
    [[HIDEventGenerator sharedHIDEventGenerator] keyPress:toWTFString(character) completionBlock:makeBlockPtr([protectedThis = Ref { *this }, callbackID] {
        if (protectedThis->m_context)
            protectedThis->m_context->asyncTaskComplete(callbackID);
    }).get()];
}

enum class IsKeyDown : bool { No, Yes };

static UIPhysicalKeyboardEvent *createUIPhysicalKeyboardEvent(NSString *hidInputString, NSString *uiEventInputString, UIKeyModifierFlags modifierFlags, UIKeyboardInputFlags inputFlags, IsKeyDown isKeyDown)
{
    auto* keyboardEvent = [getUIPhysicalKeyboardEventClass() _eventWithInput:uiEventInputString inputFlags:inputFlags];
    [keyboardEvent _setModifierFlags:modifierFlags];
    auto hidEvent = createHIDKeyEvent(hidInputString, keyboardEvent.timestamp, isKeyDown == IsKeyDown::Yes);
    [keyboardEvent _setHIDEvent:hidEvent.get() keyboard:nullptr];
    return keyboardEvent;
}

void UIScriptControllerIOS::rawKeyDown(JSStringRef key)
{
    // Key can be either a single Unicode code point or the name of a special key (e.g. "downArrow").
    // HIDEventGenerator knows how to map these special keys to the appropriate keycode.
    [[HIDEventGenerator sharedHIDEventGenerator] keyDown:toWTFString(key)];
    [[HIDEventGenerator sharedHIDEventGenerator] sendMarkerHIDEventWithCompletionBlock:^{ /* Do nothing */ }];
}

void UIScriptControllerIOS::rawKeyUp(JSStringRef key)
{
    // Key can be either a single Unicode code point or the name of a special key (e.g. "downArrow").
    // HIDEventGenerator knows how to map these special keys to the appropriate keycode.
    [[HIDEventGenerator sharedHIDEventGenerator] keyUp:toWTFString(key)];
    [[HIDEventGenerator sharedHIDEventGenerator] sendMarkerHIDEventWithCompletionBlock:^{ /* Do nothing */ }];
}

void UIScriptControllerIOS::keyDown(JSStringRef character, JSValueRef modifierArray)
{
    // Character can be either a single Unicode code point or the name of a special key (e.g. "downArrow").
    // HIDEventGenerator knows how to map these special keys to the appropriate keycode.
    auto inputString = toWTFString(character);
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

JSObjectRef UIScriptControllerIOS::filePickerAcceptedTypeIdentifiers()
{
    NSArray *acceptedTypeIdentifiers = [webView() _filePickerAcceptedTypeIdentifiers];
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:acceptedTypeIdentifiers inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

void UIScriptControllerIOS::dismissFilePicker(JSValueRef callback)
{
    TestRunnerWKWebView *webView = this->webView();
    [webView _dismissFilePicker];

    // Round-trip with the WebProcess to make sure it has been notified of the dismissal.
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView evaluateJavaScript:@"" completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] (id result, NSError *error) {
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

static CGPoint contentOffsetBoundedIfNecessary(UIScrollView *scrollView, long x, long y, ScrollToOptions* options)
{
    auto contentOffset = CGPointMake(x, y);
    bool constrain = !options || !options->unconstrained;
    if (constrain) {
        UIEdgeInsets contentInsets = scrollView.contentInset;
        CGSize contentSize = scrollView.contentSize;
        CGSize scrollViewSize = scrollView.bounds.size;

        CGFloat maxHorizontalOffset = contentSize.width + contentInsets.right - scrollViewSize.width;
        contentOffset.x = std::min(maxHorizontalOffset, contentOffset.x);
        contentOffset.x = std::max(-contentInsets.left, contentOffset.x);

        CGFloat maxVerticalOffset = contentSize.height + contentInsets.bottom - scrollViewSize.height;
        contentOffset.y = std::min(maxVerticalOffset, contentOffset.y);
        contentOffset.y = std::max(-contentInsets.top, contentOffset.y);
    }

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

JSObjectRef UIScriptControllerIOS::adjustedContentInset() const
{
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary(webView().scrollView.adjustedContentInset) inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

bool UIScriptControllerIOS::scrollUpdatesDisabled() const
{
    return webView()._scrollingUpdatesDisabledForTesting;
}

void UIScriptControllerIOS::setScrollUpdatesDisabled(bool disabled)
{
    webView()._scrollingUpdatesDisabledForTesting = disabled;
}

void UIScriptControllerIOS::scrollToOffset(long x, long y, ScrollToOptions* options)
{
    TestRunnerWKWebView *webView = this->webView();
    auto offset = contentOffsetBoundedIfNecessary(webView.scrollView, x, y, options);
    [webView.scrollView setContentOffset:offset animated:YES];
}

void UIScriptControllerIOS::immediateScrollToOffset(long x, long y, ScrollToOptions* options)
{
    TestRunnerWKWebView *webView = this->webView();
    auto offset = contentOffsetBoundedIfNecessary(webView.scrollView, x, y, options);
    [webView.scrollView setContentOffset:offset animated:NO];
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

void UIScriptControllerIOS::selectWordForReplacement()
{
#if USE(BROWSERENGINEKIT)
    if (auto asyncInput = asyncTextInput()) {
        [asyncInput selectWordForReplacement];
        return;
    }
#endif // USE(BROWSERENGINEKIT)

    auto contentView = static_cast<id<UIWKInteractionViewProtocol>>(platformContentView());
    [contentView selectWordForReplacement];
}

void UIScriptControllerIOS::applyAutocorrection(JSStringRef newString, JSStringRef oldString, JSValueRef callback, bool underline)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

#if USE(BROWSERENGINEKIT)
    if (auto asyncInput = asyncTextInput()) {
        auto completionWrapper = makeBlockPtr([this, protectedThis = Ref { *this }, callbackID](NSArray<UITextSelectionRect *> *) {
            dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
                if (m_context)
                    m_context->asyncTaskComplete(callbackID);
            }).get());
        });
        auto options = underline ? BETextReplacementOptionsAddUnderline : BETextReplacementOptionsNone;
        [asyncInput replaceText:toWTFString(oldString) withText:toWTFString(newString) options:options completionHandler:completionWrapper.get()];
        return;
    }
#endif // USE(BROWSERENGINEKIT)

    auto contentView = static_cast<id<UIWKInteractionViewProtocol>>(platformContentView());
    [contentView applyAutocorrection:toWTFString(newString) toString:toWTFString(oldString) shouldUnderline:underline withCompletionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID](UIWKAutocorrectionRects *) {
        dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
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

std::optional<bool> UIScriptControllerIOS::stableStateOverride() const
{
    TestRunnerWKWebView *webView = this->webView();
    if (webView._stableStateOverride)
        return webView._stableStateOverride.boolValue;

    return std::nullopt;
}

void UIScriptControllerIOS::setStableStateOverride(std::optional<bool> overrideValue)
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

CGRect UIScriptControllerIOS::selectionViewBoundsClippedToContentView(UIView *view, std::optional<CGRect>&& rectInView) const
{
    auto contentView = platformContentView();
    auto rect = [view convertRect:rectInView.value_or(view.bounds) toView:contentView];
    rect = CGRectIntersection(contentView.bounds, rect);

    for (RetainPtr parent = [view superview]; parent; parent = [parent superview]) {
        RetainPtr scroller = dynamic_objc_cast<UIScrollView>(parent.get());
        if (!scroller)
            continue;

        if (scroller == webView().scrollView)
            break;

        CGRect visibleRectInContentView = [scroller _wtr_visibleBoundsInCoordinateSpace:contentView];
        rect = CGRectIntersection(visibleRectInContentView, rect);
    }

#if USE(BROWSERENGINEKIT)
    if (auto asyncInput = asyncTextInput()) {
        auto selectionClipRect = asyncInput.selectionClipRect;
        if (!CGRectIsNull(selectionClipRect))
            rect = CGRectIntersection(selectionClipRect, rect);
        return rect;
    }
#endif

    auto selectionClipRect = [(UIView <UITextInputInternal> *)contentView _selectionClipRect];
    if (!CGRectIsNull(selectionClipRect))
        rect = CGRectIntersection(selectionClipRect, rect);
    return rect;
}

JSObjectRef UIScriptControllerIOS::selectionStartGrabberViewRect() const
{
    UIView *handleView = nil;

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    if (auto view = textSelectionDisplayInteraction().handleViews.firstObject; !isHiddenOrHasHiddenAncestor(view))
        handleView = view;
#endif

    auto frameInContentViewCoordinates = selectionViewBoundsClippedToContentView(handleView);
    auto jsContext = m_context->jsContext();
    return JSValueToObject(jsContext, [JSValue valueWithObject:toNSDictionary(frameInContentViewCoordinates) inContext:[JSContext contextWithJSGlobalContextRef:jsContext]].JSValueRef, nullptr);
}

JSObjectRef UIScriptControllerIOS::selectionEndGrabberViewRect() const
{
    UIView *handleView = nil;

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    if (auto view = textSelectionDisplayInteraction().handleViews.lastObject; !isHiddenOrHasHiddenAncestor(view))
        handleView = view;
#endif

    auto frameInContentViewCoordinates = selectionViewBoundsClippedToContentView(handleView);
    auto jsContext = m_context->jsContext();
    return JSValueToObject(jsContext, [JSValue valueWithObject:toNSDictionary(frameInContentViewCoordinates) inContext:[JSContext contextWithJSGlobalContextRef:jsContext]].JSValueRef, nullptr);
}

JSObjectRef UIScriptControllerIOS::selectionEndGrabberViewShapePathDescription() const
{
    UIView *handleView = nil;

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    if (auto view = textSelectionDisplayInteraction().handleViews.lastObject; !isHiddenOrHasHiddenAncestor(view))
        handleView = view;
#endif

    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary((CGPathRef)[handleView valueForKeyPath:@"stemView.shapeLayer.path"]).get() inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptControllerIOS::selectionCaretViewRect(id<UICoordinateSpace> coordinateSpace) const
{
    UIView *contentView = platformContentView();
    UIView *caretView = nil;

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    if (auto view = textSelectionDisplayInteraction().cursorView; !isHiddenOrHasHiddenAncestor(view))
        caretView = view;
#endif

    auto contentRect = selectionViewBoundsClippedToContentView(caretView);
    if (coordinateSpace != contentView)
        contentRect = [coordinateSpace convertRect:contentRect fromCoordinateSpace:contentView];
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary(contentRect) inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptControllerIOS::selectionCaretViewRect() const
{
    return selectionCaretViewRect(platformContentView());
}

JSObjectRef UIScriptControllerIOS::selectionCaretViewRectInGlobalCoordinates() const
{
    return selectionCaretViewRect(webView());
}

JSObjectRef UIScriptControllerIOS::selectionRangeViewRects() const
{
    UIView *contentView = platformContentView();
    UIView *rangeView = nil;
    auto rectsAsDictionaries = adoptNS([[NSMutableArray alloc] init]);
    NSArray<UITextSelectionRect *> *textRectInfoArray = nil;

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    if (!textRectInfoArray) {
        if (auto view = textSelectionDisplayInteraction().highlightView; !isHiddenOrHasHiddenAncestor(view)) {
            rangeView = view;
            textRectInfoArray = view.selectionRects;
        }
    }
#endif

    for (UITextSelectionRect *textRectInfo in textRectInfoArray) {
        auto rangeRectInContentViewCoordinates = selectionViewBoundsClippedToContentView(rangeView, textRectInfo.rect);
        [rectsAsDictionaries addObject:toNSDictionary(CGRectIntersection(rangeRectInContentViewCoordinates, contentView.bounds))];
    }
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:rectsAsDictionaries.get() inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptControllerIOS::inputViewBounds() const
{
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary(webView()._inputViewBoundsInWindow) inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSRetainPtr<JSStringRef> UIScriptControllerIOS::scrollingTreeAsText() const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)[webView() _scrollingTreeAsText]));
}

JSRetainPtr<JSStringRef> UIScriptControllerIOS::uiViewTreeAsText() const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)[webView() _uiViewTreeAsText]));
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

void UIScriptControllerIOS::simulateRotation(DeviceOrientation* orientation, JSValueRef callback)
{
    if (!orientation) {
        ASSERT_NOT_REACHED();
        return;
    }

    webView().usesSafariLikeRotation = NO;
    simulateRotation(*orientation, callback);
}

void UIScriptControllerIOS::simulateRotationLikeSafari(DeviceOrientation* orientation, JSValueRef callback)
{
    if (!orientation) {
        ASSERT_NOT_REACHED();
        return;
    }

    webView().usesSafariLikeRotation = YES;
    simulateRotation(*orientation, callback);
}

#if HAVE(UI_WINDOW_SCENE_GEOMETRY_PREFERENCES)

static RetainPtr<UIWindowSceneGeometryPreferences> toWindowSceneGeometryPreferences(DeviceOrientation orientation)
{
    UIInterfaceOrientationMask orientations = 0;
    switch (orientation) {
    case DeviceOrientation::Portrait:
        orientations = UIInterfaceOrientationMaskPortrait;
        break;
    case DeviceOrientation::PortraitUpsideDown:
        orientations = UIInterfaceOrientationMaskPortraitUpsideDown;
        break;
    case DeviceOrientation::LandscapeLeft:
        orientations = UIInterfaceOrientationMaskLandscapeLeft;
        break;
    case DeviceOrientation::LandscapeRight:
        orientations = UIInterfaceOrientationMaskLandscapeRight;
        break;
    }
    ASSERT(orientations);
    return adoptNS([[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:orientations]);
}

#else

static UIDeviceOrientation toUIDeviceOrientation(DeviceOrientation orientation)
{
    switch (orientation) {
    case DeviceOrientation::Portrait:
        return UIDeviceOrientationPortrait;
    case DeviceOrientation::PortraitUpsideDown:
        return UIDeviceOrientationPortraitUpsideDown;
    case DeviceOrientation::LandscapeLeft:
        return UIDeviceOrientationLandscapeLeft;
    case DeviceOrientation::LandscapeRight:
        return UIDeviceOrientationLandscapeRight;
    }
    ASSERT_NOT_REACHED();
    return UIDeviceOrientationPortrait;
}

#endif // HAVE(UI_WINDOW_SCENE_GEOMETRY_PREFERENCES)

void UIScriptControllerIOS::simulateRotation(DeviceOrientation orientation, JSValueRef callback)
{
    auto callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    webView().rotationDidEndCallback = makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (m_context)
            m_context->asyncTaskComplete(callbackID);
        webView().rotationDidEndCallback = nil;
    }).get();

#if HAVE(UI_WINDOW_SCENE_GEOMETRY_PREFERENCES)
    [webView().window.windowScene requestGeometryUpdateWithPreferences:toWindowSceneGeometryPreferences(orientation).get() errorHandler:^(NSError *error) {
        NSLog(@"Failed to simulate rotation with error: %@", error);
    }];
#else
    [[UIDevice currentDevice] setOrientation:toUIDeviceOrientation(orientation) animated:YES];
#endif
}

void UIScriptControllerIOS::setDidStartFormControlInteractionCallback(JSValueRef callback)
{
    UIScriptController::setDidStartFormControlInteractionCallback(callback);
    webView().didStartFormControlInteractionCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidStartFormControlInteraction);
    }).get();
}

void UIScriptControllerIOS::setDidEndFormControlInteractionCallback(JSValueRef callback)
{
    UIScriptController::setDidEndFormControlInteractionCallback(callback);
    webView().didEndFormControlInteractionCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidEndFormControlInteraction);
    }).get();
}

void UIScriptControllerIOS::setWillBeginZoomingCallback(JSValueRef callback)
{
    UIScriptController::setWillBeginZoomingCallback(callback);
    webView().willBeginZoomingCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeWillBeginZooming);
    }).get();
}

void UIScriptControllerIOS::setDidEndZoomingCallback(JSValueRef callback)
{
    UIScriptController::setDidEndZoomingCallback(callback);
    webView().didEndZoomingCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidEndZooming);
    }).get();
}

void UIScriptControllerIOS::setDidShowKeyboardCallback(JSValueRef callback)
{
    UIScriptController::setDidShowKeyboardCallback(callback);
    webView().didShowKeyboardCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidShowKeyboard);
    }).get();
}

void UIScriptControllerIOS::setDidHideKeyboardCallback(JSValueRef callback)
{
    UIScriptController::setDidHideKeyboardCallback(callback);
    webView().didHideKeyboardCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidHideKeyboard);
    }).get();
}

void UIScriptControllerIOS::setWillStartInputSessionCallback(JSValueRef callback)
{
    UIScriptController::setWillStartInputSessionCallback(callback);
    webView().willStartInputSessionCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeWillStartInputSession);
    }).get();
}

void UIScriptControllerIOS::chooseMenuAction(JSStringRef jsAction, JSValueRef callback)
{
    auto action = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, jsAction));
    auto rect = rectForMenuAction(action.get());
    if (rect.isEmpty())
        return;

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    singleTapAtPointWithModifiers(rect.center(), { }, makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (m_context)
            m_context->asyncTaskComplete(callbackID);
    }));
}

bool UIScriptControllerIOS::isShowingPopover() const
{
    return webView().showingPopover;
}

bool UIScriptControllerIOS::isShowingFormValidationBubble() const
{
    return webView().showingFormValidationBubble;
}

void UIScriptControllerIOS::setWillPresentPopoverCallback(JSValueRef callback)
{
    UIScriptController::setWillPresentPopoverCallback(callback);
    webView().willPresentPopoverCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeWillPresentPopover);
    }).get();
}

void UIScriptControllerIOS::setDidDismissPopoverCallback(JSValueRef callback)
{
    UIScriptController::setDidDismissPopoverCallback(callback);
    webView().didDismissPopoverCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
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
    UIView *viewForAction = nil;

    auto searchForLabel = [&](UIWindow *window) -> UILabel * {
        for (UILabel *label in findAllViewsInHierarchyOfType(window, UILabel.class)) {
            if ([label.text isEqualToString:(__bridge NSString *)action])
                return label;
        }
        return nil;
    };

    if (!viewForAction)
        viewForAction = searchForLabel(webView().window) ?: searchForLabel(webView().textEffectsWindow);

    if (!viewForAction)
        return { };

    CGRect rectInRootViewCoordinates = [viewForAction convertRect:viewForAction.bounds toView:platformContentView()];
    return WebCore::FloatRect(rectInRootViewCoordinates.origin.x, rectInRootViewCoordinates.origin.y, rectInRootViewCoordinates.size.width, rectInRootViewCoordinates.size.height);
}

JSObjectRef UIScriptControllerIOS::menuRect() const
{
    auto containerView = findAllViewsInHierarchyOfType(webView().textEffectsWindow, internalClassNamed(@"_UIEditMenuListView")).firstObject;
    return containerView ? toObject([containerView convertRect:containerView.bounds toView:platformContentView()]) : nullptr;
}

JSObjectRef UIScriptControllerIOS::contextMenuPreviewRect() const
{
    auto *container = findAllViewsInHierarchyOfType(webView().window, internalClassNamed(@"_UIMorphingPlatterView")).firstObject;
    if (!container)
        return nullptr;

    return toObject([container convertRect:container.bounds toView:nil]);
}

JSObjectRef UIScriptControllerIOS::contextMenuRect() const
{
    auto *window = webView().window;
    auto *contextMenuView = findAllViewsInHierarchyOfType(window, internalClassNamed(@"_UIContextMenuView")).firstObject;
    if (!contextMenuView)
        return nullptr;

    return toObject([contextMenuView convertRect:contextMenuView.bounds toView:nil]);
}

JSObjectRef UIScriptControllerIOS::toObject(CGRect rect) const
{
    return m_context->objectFromRect(WebCore::FloatRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height));
}

bool UIScriptControllerIOS::isDismissingMenu() const
{
    return webView().dismissingMenu;
}

void UIScriptControllerIOS::setDidEndScrollingCallback(JSValueRef callback)
{
    UIScriptController::setDidEndScrollingCallback(callback);
    webView().didEndScrollingCallback = makeBlockPtr([this, protectedThis = Ref { *this }] {
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

void UIScriptControllerIOS::beginInteractiveObscuredInsetsChange()
{
    [webView() _beginInteractiveObscuredInsetsChange];
}

void UIScriptControllerIOS::setObscuredInsets(double top, double right, double bottom, double left)
{
    webView()._obscuredInsets = UIEdgeInsetsMake(top, left, bottom, right);
}

void UIScriptControllerIOS::endInteractiveObscuredInsetsChange()
{
    [webView() _endInteractiveObscuredInsetsChange];
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
    [webView() _selectDataListOption:index];

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get());
}

bool UIScriptControllerIOS::isShowingDataListSuggestions() const
{
    return [webView() _isShowingDataListSuggestions];
}

void UIScriptControllerIOS::setSelectedColorForColorPicker(double red, double green, double blue)
{
    UIColor *color = [UIColor colorWithRed:red green:green blue:blue alpha:1.0f];
    [webView() setSelectedColorForColorPicker:color];
}

void UIScriptControllerIOS::setKeyboardInputModeIdentifier(JSStringRef identifier)
{
    TestController::singleton().setKeyboardInputModeIdentifier(toWTFString(identifier));
}

// FIXME: Write this in terms of HIDEventGenerator once we know how to reset caps lock state
// on test completion to avoid it effecting subsequent tests.
void UIScriptControllerIOS::toggleCapsLock(JSValueRef callback)
{
    m_capsLockOn = !m_capsLockOn;
    auto uiKeyModifierFlag = m_capsLockOn ? UIKeyModifierAlphaShift : 0;
    auto *keyboardEventDown = createUIPhysicalKeyboardEvent(@"capsLock", @"", uiKeyModifierFlag, kUIKeyboardInputModifierFlagsChanged, IsKeyDown::Yes);
    auto *keyboardEventUp = createUIPhysicalKeyboardEvent(@"capsLock", @"", uiKeyModifierFlag, kUIKeyboardInputModifierFlagsChanged, IsKeyDown::No);
    auto *pressInfo = [[UIApplication sharedApplication] _pressInfoForPhysicalKeyboardEvent:keyboardEventDown];
    auto press = adoptNS([[UIPress alloc] init]);
    [press _loadStateFromPressInfo:pressInfo];
    auto *presses = [NSSet setWithObject:press.get()];
    [platformContentView() pressesBegan:presses withEvent:keyboardEventDown];
    [platformContentView() pressesEnded:presses withEvent:keyboardEventUp];
    doAsyncTask(callback);
}

bool UIScriptControllerIOS::keyboardIsAutomaticallyShifted() const
{
    return UIKeyboardImpl.activeInstance.isAutoShifted;
}

unsigned UIScriptControllerIOS::keyboardWillHideCount() const
{
    return static_cast<unsigned>(webView().keyboardWillHideCount);
}

bool UIScriptControllerIOS::isAnimatingDragCancel() const
{
    return webView()._animatingDragCancel;
}

JSRetainPtr<JSStringRef> UIScriptControllerIOS::selectionCaretBackgroundColor() const
{
    UIColor *backgroundColor = nil;
#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)
    if (auto view = textSelectionDisplayInteraction().cursorView; !isHiddenOrHasHiddenAncestor(view))
        backgroundColor = view.tintColor;
#endif

    if (!backgroundColor)
        return nil;

    auto serialization = WebCoreTestSupport::serializationForCSS(backgroundColor).createCFString();
    return adopt(JSStringCreateWithCFString(serialization.get()));
}

JSObjectRef UIScriptControllerIOS::tapHighlightViewRect() const
{
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:toNSDictionary(webView()._tapHighlightViewRect) inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

JSObjectRef UIScriptControllerIOS::attachmentInfo(JSStringRef jsAttachmentIdentifier)
{
    auto attachmentIdentifier = toWTFString(jsAttachmentIdentifier);
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
    TestController::singleton().setIsInHardwareKeyboardMode(attached);
}

void UIScriptControllerIOS::setAllowsViewportShrinkToFit(bool allows)
{
    webView()._allowsViewportShrinkToFit = allows;
}

void UIScriptControllerIOS::setScrollViewKeyboardAvoidanceEnabled(bool enabled)
{
    webView().scrollView.firstResponderKeyboardAvoidanceEnabled = enabled;
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

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(maximumIntervalBetweenSuccessiveTaps * NSEC_PER_SEC)), dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get());
}

void UIScriptControllerIOS::copyText(JSStringRef text)
{
    UIPasteboard.generalPasteboard.string = text->string();
}

int64_t UIScriptControllerIOS::pasteboardChangeCount() const
{
    return UIPasteboard.generalPasteboard.changeCount;
}

void UIScriptControllerIOS::installTapGestureOnWindow(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeWindowTapRecognized);
    webView().windowTapRecognizedCallback = makeBlockPtr([this, strongThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeWindowTapRecognized);
    }).get();
}

bool UIScriptControllerIOS::suppressSoftwareKeyboard() const
{
    return webView()._suppressSoftwareKeyboard;
}

void UIScriptControllerIOS::setSuppressSoftwareKeyboard(bool suppressSoftwareKeyboard)
{
    webView()._suppressSoftwareKeyboard = suppressSoftwareKeyboard;
}

void UIScriptControllerIOS::presentFindNavigator()
{
#if HAVE(UIFINDINTERACTION)
    [webView().findInteraction presentFindNavigatorShowingReplace:NO];
#endif
}

void UIScriptControllerIOS::dismissFindNavigator()
{
#if HAVE(UIFINDINTERACTION)
    [webView().findInteraction dismissFindNavigator];
#endif
}

bool UIScriptControllerIOS::isWebContentFirstResponder() const
{
    return [webView() _contentViewIsFirstResponder];
}

void UIScriptControllerIOS::setInlinePrediction(JSStringRef text, unsigned startIndex)
{
    NSString *plainText = text->string().substring(startIndex);
    auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:plainText attributes:@{
        NSBackgroundColorAttributeName : UIColor.clearColor,
        NSForegroundColorAttributeName : UIColor.systemGrayColor,
    }]);

    [UIKeyboardImpl.activeInstance setInlineCompletionAsMarkedText:attributedText.get() selectedRange:NSMakeRange(0, 0) inputString:plainText searchString:@""];
}

void UIScriptControllerIOS::acceptInlinePrediction()
{
    if (!UIKeyboardImpl.activeInstance.hasInlineCompletionAsMarkedText)
        return;

    [(id<UITextInput>)platformContentView() unmarkText];
    auto emptyText = adoptNS([[NSAttributedString alloc] initWithString:@""]);
    [UIKeyboardImpl.activeInstance setInlineCompletionAsMarkedText:emptyText.get() selectedRange:NSMakeRange(0, 0) inputString:@"" searchString:@""];
}

void UIScriptControllerIOS::becomeFirstResponder()
{
    [webView() becomeFirstResponder];
}

void UIScriptControllerIOS::resignFirstResponder()
{
    [webView() resignFirstResponder];
}

#if USE(BROWSERENGINEKIT)

id<BETextInput> UIScriptControllerIOS::asyncTextInput() const
{
    static BOOL conformsToAsyncTextInput = class_conformsToProtocol(NSClassFromString(@"WKContentView"), @protocol(BETextInput));
    if (!conformsToAsyncTextInput)
        return nil;

    return (id<BETextInput>)platformContentView();
}

#endif // USE(BROWSERENGINEKIT)

#if HAVE(UI_TEXT_SELECTION_DISPLAY_INTERACTION)

UITextSelectionDisplayInteraction *UIScriptControllerIOS::textSelectionDisplayInteraction() const
{
    return dynamic_objc_cast<UITextSelectionDisplayInteraction>([platformContentView() valueForKeyPath:@"interactionAssistant._selectionViewManager"]);
}

#endif


JSRetainPtr<JSStringRef> UIScriptControllerIOS::scrollbarStateForScrollingNodeID(unsigned long long scrollingNodeID, unsigned long long processID, bool isVertical) const
{
    return adopt(JSStringCreateWithCFString((CFStringRef) [webView() _scrollbarState:scrollingNodeID processID:processID isVertical:isVertical]));
}

}

#endif // PLATFORM(IOS_FAMILY)
