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
#import <wtf/BlockPtr.h>

namespace WTR {

Ref<UIScriptController> UIScriptController::create(UIScriptContext& context)
{
    return adoptRef(*new UIScriptControllerMac(context));
}

static NSString *nsString(JSStringRef string)
{
    return CFBridgingRelease(JSStringCopyCFString(kCFAllocatorDefault, string));
}

void UIScriptControllerMac::replaceTextAtRange(JSStringRef text, int location, int length)
{
    [webView() _insertText:nsString(text) replacementRange:NSMakeRange(location == -1 ? NSNotFound : location, length)];
}

void UIScriptControllerMac::zoomToScale(double scale, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto* webView = this->webView();
    [webView _setPageScale:scale withOrigin:CGPointZero];

    [webView _doAfterNextPresentationUpdate:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

double UIScriptControllerMac::zoomScale() const
{
    return webView().magnification;
}

void UIScriptControllerMac::simulateAccessibilitySettingsChangeNotification(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto* webView = this->webView();
    NSNotificationCenter *center = [[NSWorkspace sharedWorkspace] notificationCenter];
    [center postNotificationName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:webView];

    [webView _doAfterNextPresentationUpdate:makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
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
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.05 * NSEC_PER_SEC)), dispatch_get_main_queue(), [this, protectedThis = makeRefPtr(*this), callbackID, table] {
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

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get());
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
    RetainPtr<CFStringRef> stream = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, eventStream));
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

void UIScriptControllerMac::toggleCapsLock(JSValueRef callback)
{
    m_capsLockOn = !m_capsLockOn;
    NSWindow *window = [webView() window];
    NSEvent *fakeEvent = [NSEvent keyEventWithType:NSEventTypeFlagsChanged
        location:NSZeroPoint
        modifierFlags:m_capsLockOn ? NSEventModifierFlagCapsLock : 0
        timestamp:0
        windowNumber:window.windowNumber
        context:nullptr
        characters:@""
        charactersIgnoringModifiers:@""
        isARepeat:NO
        keyCode:57];
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

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get());
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

} // namespace WTR
