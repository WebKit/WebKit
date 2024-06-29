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
#import "UIScriptControllerMac.h"

#import "EventSenderProxy.h"
#import "EventSerializerMac.h"
#import "LayoutTestSpellChecker.h"
#import "PlatformViewHelpers.h"
#import "PlatformWebView.h"
#import "PlatformWebView.h"
#import "SharedEventStreamsMac.h"
#import "StringFunctions.h"
#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import "UIScriptContext.h"
#import <JavaScriptCore/JSContext.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <JavaScriptCore/JSValue.h>
#import <JavaScriptCore/JavaScriptCore.h>
#import <JavaScriptCore/OpaqueJSString.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <mach/mach_time.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/WorkQueue.h>

namespace WTR {

Ref<UIScriptController> UIScriptController::create(UIScriptContext& context)
{
    return adoptRef(*new UIScriptControllerMac(context));
}

static RetainPtr<CFStringRef> cfString(JSStringRef string)
{
    return adoptCF(JSStringCopyCFString(kCFAllocatorDefault, string));
}

void UIScriptControllerMac::replaceTextAtRange(JSStringRef text, int location, int length)
{
    [webView() _insertText:(__bridge NSString *)cfString(text).get() replacementRange:NSMakeRange(location == -1 ? NSNotFound : location, length)];
}

void UIScriptControllerMac::zoomToScale(double scale, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto* webView = this->webView();
    [webView _setPageScale:scale withOrigin:CGPointZero];

    [webView _doAfterNextPresentationUpdate:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

double UIScriptControllerMac::zoomScale() const
{
    return webView().magnification;
}

double UIScriptControllerMac::minimumZoomScale() const
{
    return webView().minimumMagnification;
}

void UIScriptControllerMac::simulateAccessibilitySettingsChangeNotification(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto* webView = this->webView();
    NSNotificationCenter *center = [[NSWorkspace sharedWorkspace] notificationCenter];
    [center postNotificationName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:webView];

    [webView _doAfterNextPresentationUpdate:makeBlockPtr([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

bool UIScriptControllerMac::isShowingDateTimePicker() const
{
    for (NSWindow *childWindow in webView().window.childWindows) {
        if ([childWindow isKindOfClass:NSClassFromString(@"WKDateTimePickerWindow")])
            return true;
    }
    return false;
}

double UIScriptControllerMac::dateTimePickerValue() const
{
    for (NSWindow *childWindow in webView().window.childWindows) {
        if ([childWindow isKindOfClass:NSClassFromString(@"WKDateTimePickerWindow")]) {
            for (NSView *subview in childWindow.contentView.subviews) {
                if ([subview isKindOfClass:[NSDatePicker class]])
                    return [[(NSDatePicker *)subview dateValue] timeIntervalSince1970] * 1000;
            }
        }
    }
    return 0;
}

void UIScriptControllerMac::chooseDateTimePickerValue()
{
    for (NSWindow *childWindow in webView().window.childWindows) {
        if ([childWindow isKindOfClass:NSClassFromString(@"WKDateTimePickerWindow")]) {
            for (NSView *subview in childWindow.contentView.subviews) {
                if ([subview isKindOfClass:[NSDatePicker class]]) {
                    NSDatePicker *datePicker = (NSDatePicker *)subview;
                    [datePicker.target performSelector:datePicker.action withObject:datePicker];
                    return;
                }
            }
        }
    }
}

bool UIScriptControllerMac::isShowingDataListSuggestions() const
{
    return dataListSuggestionsTableView();
}

void UIScriptControllerMac::activateDataListSuggestion(unsigned index, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    RetainPtr<NSTableView> table;
    do {
        table = dataListSuggestionsTableView();
    } while (index >= [table numberOfRows] && [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]);

    [table selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];

    // Send the action after a short delay to simulate normal user interaction.
    WorkQueue::main().dispatchAfter(50_ms, [this, protectedThis = Ref { *this }, callbackID, table] {
        if ([table window])
            [table sendAction:[table action] to:[table target]];

        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

NSTableView *UIScriptControllerMac::dataListSuggestionsTableView() const
{
    for (NSWindow *childWindow in webView().window.childWindows) {
        if ([childWindow isKindOfClass:NSClassFromString(@"WKDataListSuggestionWindow")])
            return (NSTableView *)[findAllViewsInHierarchyOfType(childWindow.contentView, NSClassFromString(@"WKDataListSuggestionTableView")) firstObject];
    }
    return nil;
}

static void playBackEvents(WKWebView *webView, UIScriptContext *context, NSString *eventStream, JSValueRef callback)
{
    NSError *error = nil;
    NSArray *eventDicts = [NSJSONSerialization JSONObjectWithData:[eventStream dataUsingEncoding:NSUTF8StringEncoding] options:0 error:&error];

    if (error) {
        NSLog(@"ERROR: %@", error);
        return;
    }

    unsigned callbackID = context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [EventStreamPlayer playStream:eventDicts window:webView.window completionHandler:^{
        context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptControllerMac::clearAllCallbacks()
{
    [webView() resetInteractionCallbacks];
}

void UIScriptControllerMac::chooseMenuAction(JSStringRef jsAction, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto action = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, jsAction));
    __block NSUInteger matchIndex = NSNotFound;
    auto activeMenu = retainPtr(webView()._activeMenu);
    [[activeMenu itemArray] enumerateObjectsUsingBlock:^(NSMenuItem *item, NSUInteger index, BOOL *stop) {
        if ([item.title isEqualToString:(__bridge NSString *)action.get()])
            matchIndex = index;
    }];

    if (matchIndex != NSNotFound) {
        [activeMenu performActionForItemAtIndex:matchIndex];
        [activeMenu removeAllItems];
        [activeMenu update];
        [activeMenu cancelTracking];
    }

    WorkQueue::main().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerMac::beginBackSwipe(JSValueRef callback)
{
    playBackEvents(webView(), m_context, beginSwipeBackEventStream(), callback);
}

void UIScriptControllerMac::completeBackSwipe(JSValueRef callback)
{
    playBackEvents(webView(), m_context, completeSwipeBackEventStream(), callback);
}

void UIScriptControllerMac::playBackEventStream(JSStringRef eventStream, JSValueRef callback)
{
    auto stream = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, eventStream));
    playBackEvents(webView(), m_context, (__bridge NSString *)stream.get(), callback);
}

void UIScriptControllerMac::firstResponderSuppressionForWebView(bool shouldSuppress)
{
    [webView() _setShouldSuppressFirstResponderChanges:shouldSuppress];
}

void UIScriptControllerMac::makeWindowContentViewFirstResponder()
{
    NSWindow *window = [webView() window];
    [window makeFirstResponder:[window contentView]];
}

bool UIScriptControllerMac::isWindowContentViewFirstResponder() const
{
    NSWindow *window = [webView() window];
    return [window firstResponder] == [window contentView];
}

void UIScriptControllerMac::becomeFirstResponder()
{
    auto *webView = this->webView();
    [webView.window makeFirstResponder:webView];
}

void UIScriptControllerMac::resignFirstResponder()
{
    auto *webView = this->webView();
    if (webView.window.firstResponder == webView)
        [webView.window makeFirstResponder:nil];
}

void UIScriptControllerMac::toggleCapsLock(JSValueRef callback)
{
    m_capsLockOn = !m_capsLockOn;
    NSWindow *window = [webView() window];
    const auto makeFakeCapsLockKeyEventWithType = [capsLockOn = m_capsLockOn, window] (NSEventType eventType) {
        return [NSEvent keyEventWithType:eventType
            location:NSZeroPoint
            modifierFlags:capsLockOn ? NSEventModifierFlagCapsLock : 0
            timestamp:0
            windowNumber:window.windowNumber
            context:nullptr
            characters:@""
            charactersIgnoringModifiers:@""
            isARepeat:NO
            keyCode:57];
    };
    NSArray<NSEvent *> *fakeEventsToBeSent = @[ makeFakeCapsLockKeyEventWithType(NSEventTypeKeyDown), makeFakeCapsLockKeyEventWithType(NSEventTypeKeyUp), makeFakeCapsLockKeyEventWithType(NSEventTypeFlagsChanged) ];
    for (NSEvent *fakeEvent in fakeEventsToBeSent)
        [window sendEvent:fakeEvent];
    doAsyncTask(callback);
}

NSView *UIScriptControllerMac::platformContentView() const
{
    return webView();
}

void UIScriptControllerMac::activateAtPoint(long x, long y, JSValueRef callback)
{
    auto* eventSender = TestController::singleton().eventSenderProxy();
    if (!eventSender) {
        ASSERT_NOT_REACHED();
        return;
    }

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    eventSender->mouseMoveTo(x, y);
    eventSender->mouseDown(0, 0);
    eventSender->mouseUp(0, 0);

    WorkQueue::main().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

int64_t UIScriptControllerMac::pasteboardChangeCount() const
{
    return NSPasteboard.generalPasteboard.changeCount;
}

void UIScriptControllerMac::copyText(JSStringRef text)
{
    NSPasteboard *pasteboard = NSPasteboard.generalPasteboard;
    [pasteboard declareTypes:@[NSPasteboardTypeString] owner:nil];
    [pasteboard setString:text->string() forType:NSPasteboardTypeString];
}

static NSString *const TopLevelEventInfoKey = @"events";
static NSString *const EventTypeKey = @"type";
static NSString *const ViewRelativeXPositionKey = @"viewX";
static NSString *const ViewRelativeYPositionKey = @"viewY";
static NSString *const DeltaXKey = @"deltaX";
static NSString *const DeltaYKey = @"deltaY";
static NSString *const PhaseKey = @"phase";
static NSString *const MomentumPhaseKey = @"momentumPhase";

static EventSenderProxy::WheelEventPhase eventPhaseFromString(NSString *phaseStr)
{
    if ([phaseStr isEqualToString:@"began"])
        return EventSenderProxy::WheelEventPhase::Began;
    if ([phaseStr isEqualToString:@"changed"] || [phaseStr isEqualToString:@"continue"]) // Allow "continue" for ease of conversion from mouseScrollByWithWheelAndMomentumPhases values.
        return EventSenderProxy::WheelEventPhase::Changed;
    if ([phaseStr isEqualToString:@"ended"])
        return EventSenderProxy::WheelEventPhase::Ended;
    if ([phaseStr isEqualToString:@"cancelled"])
        return EventSenderProxy::WheelEventPhase::Cancelled;
    if ([phaseStr isEqualToString:@"maybegin"])
        return EventSenderProxy::WheelEventPhase::MayBegin;

    ASSERT_NOT_REACHED();
    return EventSenderProxy::WheelEventPhase::None;
}

void UIScriptControllerMac::sendEventStream(JSStringRef eventsJSON, JSValueRef callback)
{
    auto* eventSender = TestController::singleton().eventSenderProxy();
    if (!eventSender) {
        ASSERT_NOT_REACHED();
        return;
    }

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto jsonString = eventsJSON->string();
    auto eventInfo = dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:[(NSString *)jsonString dataUsingEncoding:NSUTF8StringEncoding] options:NSJSONReadingMutableContainers | NSJSONReadingMutableLeaves error:nil]);
    if (!eventInfo || ![eventInfo isKindOfClass:[NSDictionary class]]) {
        WTFLogAlways("JSON is not convertible to a dictionary");
        return;
    }

    auto *webView = this->webView();

    double currentViewRelativeX = 0;
    double currentViewRelativeY = 0;

    constexpr uint64_t nanosecondsPerSecond = 1e9;
    constexpr uint64_t nanosecondsEventInterval = nanosecondsPerSecond / 60;

    auto currentTime = mach_absolute_time();

    for (NSMutableDictionary *event in eventInfo[TopLevelEventInfoKey]) {

        id eventType = event[EventTypeKey];
        if (!event[EventTypeKey]) {
            WTFLogAlways("Missing event type");
            break;
        }
        
        if ([eventType isEqualToString:@"wheel"]) {
            auto phase = EventSenderProxy::WheelEventPhase::None;
            auto momentumPhase = EventSenderProxy::WheelEventPhase::None;

            if (!event[PhaseKey] && !event[MomentumPhaseKey]) {
                WTFLogAlways("Event must specify phase or momentumPhase");
                break;
            }

            if (id phaseString = event[PhaseKey])
                phase = eventPhaseFromString(phaseString);

            if (id phaseString = event[MomentumPhaseKey]) {
                momentumPhase = eventPhaseFromString(phaseString);
                if (momentumPhase == EventSenderProxy::WheelEventPhase::Cancelled || momentumPhase == EventSenderProxy::WheelEventPhase::MayBegin) {
                    WTFLogAlways("Invalid value %@ for momentumPhase", phaseString);
                    break;
                }
            }

            ASSERT_IMPLIES(phase == EventSenderProxy::WheelEventPhase::None, momentumPhase != EventSenderProxy::WheelEventPhase::None);
            ASSERT_IMPLIES(momentumPhase == EventSenderProxy::WheelEventPhase::None, phase != EventSenderProxy::WheelEventPhase::None);

            if (event[ViewRelativeXPositionKey])
                currentViewRelativeX = [event[ViewRelativeXPositionKey] floatValue];

            if (event[ViewRelativeYPositionKey])
                currentViewRelativeY = [event[ViewRelativeYPositionKey] floatValue];

            double deltaX = 0;
            double deltaY = 0;

            if (event[DeltaXKey])
                deltaX = [event[DeltaXKey] floatValue];

            if (event[DeltaYKey])
                deltaY = [event[DeltaYKey] floatValue];

            auto windowPoint = [webView convertPoint:CGPointMake(currentViewRelativeX, currentViewRelativeY) toView:nil];
            eventSender->sendWheelEvent(currentTime, windowPoint.x, windowPoint.y, deltaX, deltaY, phase, momentumPhase);
        }

        currentTime += nanosecondsEventInterval;
    }

    WorkQueue::main().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

JSRetainPtr<JSStringRef> UIScriptControllerMac::scrollbarStateForScrollingNodeID(unsigned long long scrollingNodeID, unsigned long long processID, bool isVertical) const
{
    return adopt(JSStringCreateWithCFString((CFStringRef) [webView() _scrollbarStateForScrollingNodeID:scrollingNodeID processID:processID isVertical:isVertical]));
}

void UIScriptControllerMac::setAppAccentColor(unsigned short red, unsigned short green, unsigned short blue)
{
    NSApp._accentColor = [NSColor colorWithRed:red / 255. green:green / 255. blue:blue / 255. alpha:1];
}

void UIScriptControllerMac::setWebViewAllowsMagnification(bool allowsMagnification)
{
    webView().allowsMagnification = allowsMagnification;
}

void UIScriptControllerMac::setInlinePrediction(JSStringRef jsText, unsigned startIndex)
{
#if HAVE(INLINE_PREDICTIONS)
    if (!webView()._allowsInlinePredictions)
        return;

    auto fullText = jsText->string();
    RetainPtr markedText = adoptNS([[NSMutableAttributedString alloc] initWithString:fullText.left(startIndex)]);
    [markedText appendAttributedString:adoptNS([[NSAttributedString alloc] initWithString:fullText.substring(startIndex) attributes:@{
        NSForegroundColorAttributeName : NSColor.grayColor,
        NSTextCompletionAttributeName : @YES
    }]).get()];
    [static_cast<id<NSTextInputClient>>(webView()) setMarkedText:markedText.get() selectedRange:NSMakeRange(startIndex, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
#else
    UNUSED_PARAM(jsText);
    UNUSED_PARAM(startIndex);
#endif
}

} // namespace WTR
