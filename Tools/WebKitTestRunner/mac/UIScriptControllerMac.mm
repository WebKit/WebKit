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

#import "EventSerializerMac.h"
#import "PlatformWebView.h"
#import "SharedEventStreamsMac.h"
#import "TestController.h"
#import "PlatformWebView.h"
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

namespace WTR {

NSString *nsString(JSStringRef string)
{
    return CFBridgingRelease(JSStringCopyCFString(kCFAllocatorDefault, string));
}

void UIScriptController::doAfterPresentationUpdate(JSValueRef callback)
{
    return doAsyncTask(callback);
}

void UIScriptController::doAfterNextStablePresentationUpdate(JSValueRef callback)
{
    doAsyncTask(callback);
}

void UIScriptController::doAfterVisibleContentRectUpdate(JSValueRef callback)
{
    doAsyncTask(callback);
}

void UIScriptController::replaceTextAtRange(JSStringRef text, int location, int length)
{
#if WK_API_ENABLED
    auto* webView = TestController::singleton().mainWebView()->platformView();
    [webView _insertText:nsString(text) replacementRange:NSMakeRange(location == -1 ? NSNotFound : location, length)];
#else
    UNUSED_PARAM(text);
    UNUSED_PARAM(location);
    UNUSED_PARAM(length);
#endif
}

void UIScriptController::zoomToScale(double scale, JSValueRef callback)
{
#if WK_API_ENABLED
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto* webView = TestController::singleton().mainWebView()->platformView();
    [webView _setPageScale:scale withOrigin:CGPointZero];

    [webView _doAfterNextPresentationUpdate: ^ {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
#else
    UNUSED_PARAM(scale);
    UNUSED_PARAM(callback);
#endif
}

void UIScriptController::simulateAccessibilitySettingsChangeNotification(JSValueRef callback)
{
#if WK_API_ENABLED
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto* webView = TestController::singleton().mainWebView()->platformView();
    NSNotificationCenter *center = [[NSWorkspace sharedWorkspace] notificationCenter];
    [center postNotificationName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:webView];

    [webView _doAfterNextPresentationUpdate: ^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
#else
    UNUSED_PARAM(callback);
#endif
}

void UIScriptController::simulateRotation(DeviceOrientation*, JSValueRef)
{
}

void UIScriptController::simulateRotationLikeSafari(DeviceOrientation*, JSValueRef)
{
}

bool UIScriptController::isShowingDataListSuggestions() const
{
#if WK_API_ENABLED
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    for (NSWindow *childWindow in webView.window.childWindows) {
        if ([childWindow isKindOfClass:NSClassFromString(@"WKDataListSuggestionWindow")])
            return true;
    }
#endif
    return false;
}

static void playBackEvents(UIScriptContext *context, NSString *eventStream, JSValueRef callback)
{
    NSError *error = nil;
    NSArray *eventDicts = [NSJSONSerialization JSONObjectWithData:[eventStream dataUsingEncoding:NSUTF8StringEncoding] options:0 error:&error];

    if (error) {
        NSLog(@"ERROR: %@", error);
        return;
    }

    unsigned callbackID = context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    NSWindow *window = [TestController::singleton().mainWebView()->platformView() window];

    [EventStreamPlayer playStream:eventDicts window:window completionHandler:^ {
        context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptController::beginBackSwipe(JSValueRef callback)
{
    playBackEvents(m_context, beginSwipeBackEventStream(), callback);
}

void UIScriptController::completeBackSwipe(JSValueRef callback)
{
    playBackEvents(m_context, completeSwipeBackEventStream(), callback);
}

void UIScriptController::platformPlayBackEventStream(JSStringRef eventStream, JSValueRef callback)
{
    RetainPtr<CFStringRef> stream = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, eventStream));
    playBackEvents(m_context, (__bridge NSString *)stream.get(), callback);
}

void UIScriptController::firstResponderSuppressionForWebView(bool shouldSuppress)
{
#if WK_API_ENABLED
    auto* webView = TestController::singleton().mainWebView()->platformView();
    [webView _setShouldSuppressFirstResponderChanges:shouldSuppress];
#else
    UNUSED_PARAM(shouldSuppress);
#endif
}

void UIScriptController::makeWindowContentViewFirstResponder()
{
    NSWindow *window = [TestController::singleton().mainWebView()->platformView() window];
    [window makeFirstResponder:[window contentView]];
}

bool UIScriptController::isWindowContentViewFirstResponder() const
{
    NSWindow *window = [TestController::singleton().mainWebView()->platformView() window];
    return [window firstResponder] == [window contentView];
}

void UIScriptController::toggleCapsLock(JSValueRef callback)
{
    m_capsLockOn = !m_capsLockOn;
    NSWindow *window = [TestController::singleton().mainWebView()->platformView() window];
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

NSView *UIScriptController::platformContentView() const
{
    return TestController::singleton().mainWebView()->platformView();
}

JSObjectRef UIScriptController::calendarType() const
{
    return nullptr;
}

} // namespace WTR
