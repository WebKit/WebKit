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
#import "UIScriptContext.h"
#import <UIKit/UIKit.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>

namespace WTR {

void UIScriptController::doAsyncTask(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
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

JSObjectRef UIScriptController::contentVisibleRect() const
{
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();

    CGRect contentVisibleRect = webView._contentVisibleRect;
    
    WKRect wkRect = WKRectMake(contentVisibleRect.origin.x, contentVisibleRect.origin.y, contentVisibleRect.size.width, contentVisibleRect.size.height);
    return m_context->objectFromRect(wkRect);
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
    webView.didEndZoomingCallback = nil;
    webView.willBeginZoomingCallback = nil;
    webView.didHideKeyboardCallback = nil;
    webView.didShowKeyboardCallback = nil;
    webView.didEndScrollingCallback = nil;
}

}

#endif // PLATFORM(IOS)
