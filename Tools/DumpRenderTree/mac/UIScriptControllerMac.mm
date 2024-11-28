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

#if PLATFORM(MAC)

#import "DumpRenderTree.h"
#import "EventSendingController.h"
#import "LayoutTestSpellChecker.h"
#import "UIScriptContext.h"
#import <JavaScriptCore/JSContext.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <JavaScriptCore/JSValue.h>
#import <JavaScriptCore/OpaqueJSString.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebViewPrivate.h>
#import <mach/mach_time.h>
#import <pal/spi/mac/NSTextInputContextSPI.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WTR {

Ref<UIScriptController> UIScriptController::create(UIScriptContext& context)
{
    return adoptRef(*new UIScriptControllerMac(context));
}

void UIScriptControllerMac::doAsyncTask(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    WorkQueue::main().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerMac::replaceTextAtRange(JSStringRef text, int location, int length)
{
    auto textToInsert = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, text));
    auto rangeAttribute = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:NSStringFromRange(NSMakeRange(location == -1 ? NSNotFound : location, length)), NSTextInputReplacementRangeAttributeName, nil]);
    auto textAndRange = adoptNS([[NSAttributedString alloc] initWithString:(__bridge NSString *)textToInsert.get() attributes:rangeAttribute.get()]);

    [mainFrame.webView insertText:textAndRange.get()];
}

void UIScriptControllerMac::zoomToScale(double scale, JSValueRef callback)
{
    WebView *webView = [mainFrame webView];
    [webView _scaleWebView:scale atOrigin:NSZeroPoint];

    doAsyncTask(callback);
}

double UIScriptControllerMac::zoomScale() const
{
    return mainFrame.webView._viewScaleFactor;
}

void UIScriptControllerMac::simulateAccessibilitySettingsChangeNotification(JSValueRef callback)
{
    NSNotificationCenter *center = [[NSWorkspace sharedWorkspace] notificationCenter];
    [center postNotificationName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:[mainFrame webView]];

    doAsyncTask(callback);
}

JSObjectRef UIScriptControllerMac::contentsOfUserInterfaceItem(JSStringRef interfaceItem) const
{
#if JSC_OBJC_API_ENABLED
    WebView *webView = [mainFrame webView];
    RetainPtr<CFStringRef> interfaceItemCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, interfaceItem));
    NSDictionary *contentDictionary = [webView _contentsOfUserInterfaceItem:(__bridge NSString *)interfaceItemCF.get()];
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:contentDictionary inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
#else
    UNUSED_PARAM(interfaceItem);
    return nullptr;
#endif
}

void UIScriptControllerMac::activateDataListSuggestion(unsigned index, JSValueRef callback)
{
    // FIXME: Not implemented.
    UNUSED_PARAM(index);

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    WorkQueue::main().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerMac::overridePreference(JSStringRef preferenceRef, JSStringRef valueRef)
{
    WebPreferences *preferences = mainFrame.webView.preferences;

    RetainPtr<CFStringRef> value = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, valueRef));
    if (JSStringIsEqualToUTF8CString(preferenceRef, "WebKitMinimumFontSize"))
        preferences.minimumFontSize = [(__bridge NSString *)value.get() doubleValue];
}

void UIScriptControllerMac::removeViewFromWindow(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    WebView *webView = [mainFrame webView];
    [webView removeFromSuperview];

    WorkQueue::main().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerMac::addViewToWindow(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    WebView *webView = [mainFrame webView];
    [[mainWindow contentView] addSubview:webView];

    WorkQueue::main().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerMac::toggleCapsLock(JSValueRef callback)
{
    doAsyncTask(callback);
}

void UIScriptControllerMac::setWebViewEditable(bool editable)
{
    [mainFrame webView].editable = editable;
}

NSUndoManager *UIScriptControllerMac::platformUndoManager() const
{
    return nil;
}

void UIScriptControllerMac::copyText(JSStringRef text)
{
    NSPasteboard *pasteboard = NSPasteboard.generalPasteboard;
    [pasteboard declareTypes:@[NSPasteboardTypeString] owner:nil];
    [pasteboard setString:text->string() forType:NSPasteboardTypeString];
}

void UIScriptControllerMac::setSpellCheckerResults(JSValueRef results)
{
    [[LayoutTestSpellChecker checker] setResultsFromJSValue:results inContext:m_context->jsContext()];
}

static NSString *const TopLevelEventInfoKey = @"events";
static NSString *const EventTypeKey = @"type";
static NSString *const ViewRelativeXPositionKey = @"viewX";
static NSString *const ViewRelativeYPositionKey = @"viewY";
static NSString *const DeltaXKey = @"deltaX";
static NSString *const DeltaYKey = @"deltaY";
static NSString *const PhaseKey = @"phase";
static NSString *const MomentumPhaseKey = @"momentumPhase";

static CGGesturePhase gesturePhaseFromString(NSString *phaseStr)
{
    if ([phaseStr isEqualToString:@"began"])
        return kCGGesturePhaseBegan;

    if ([phaseStr isEqualToString:@"changed"])
        return kCGGesturePhaseChanged;

    if ([phaseStr isEqualToString:@"ended"])
        return kCGGesturePhaseEnded;

    if ([phaseStr isEqualToString:@"cancelled"])
        return kCGGesturePhaseCancelled;

    if ([phaseStr isEqualToString:@"maybegin"])
        return kCGGesturePhaseMayBegin;

    return kCGGesturePhaseNone;
}

static CGMomentumScrollPhase momentumPhaseFromString(NSString *phaseStr)
{
    if ([phaseStr isEqualToString:@"began"])
        return kCGMomentumScrollPhaseBegin;

    if ([phaseStr isEqualToString:@"changed"] || [phaseStr isEqualToString:@"continue"]) // Allow "continue" for ease of conversion from mouseScrollByWithWheelAndMomentumPhases values.
        return kCGMomentumScrollPhaseContinue;

    if ([phaseStr isEqualToString:@"ended"])
        return kCGMomentumScrollPhaseEnd;

    return kCGMomentumScrollPhaseNone;
}

static EventSendingController *eventSenderFromView(WebView *webView)
{
    auto frame = [webView mainFrame];
    auto windowObject = [frame windowObject];
    return [windowObject valueForKey:@"eventSender"];
}

void UIScriptControllerMac::sendEventStream(JSStringRef eventsJSON, JSValueRef callback)
{
    WebView *webView = [mainFrame webView];

    // didClearWindowObjectInStandardWorldForFrame stashed EventSendingController on this window property.
    EventSendingController* eventSender = eventSenderFromView(webView);
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
            auto phase = kCGGesturePhaseNone;
            auto momentumPhase = kCGMomentumScrollPhaseNone;

            if (!event[PhaseKey] && !event[MomentumPhaseKey]) {
                WTFLogAlways("Event must specify phase or momentumPhase");
                break;
            }

            if (id phaseString = event[PhaseKey])
                phase = gesturePhaseFromString(phaseString);

            if (id phaseString = event[MomentumPhaseKey])
                momentumPhase = momentumPhaseFromString(phaseString);

            ASSERT_IMPLIES(phase == kCGGesturePhaseNone, momentumPhase != kCGMomentumScrollPhaseNone);
            ASSERT_IMPLIES(momentumPhase == kCGMomentumScrollPhaseNone, phase != kCGGesturePhaseNone);

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

            auto windowPoint = [webView convertPoint:CGPointMake(currentViewRelativeX, [webView frame].size.height - currentViewRelativeY) toView:nil];
            [eventSender sendScrollEventAt:windowPoint deltaX:deltaX deltaY:deltaY units:kCGScrollEventUnitPixel wheelPhase:phase momentumPhase:momentumPhase timestamp:currentTime];
        }

        currentTime += nanosecondsEventInterval;
    }

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

} // namespace WTR

#endif // PLATFORM(MAC)
