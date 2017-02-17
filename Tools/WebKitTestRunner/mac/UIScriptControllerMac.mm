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
    return (NSString *)adoptCF(JSStringCopyCFString(kCFAllocatorDefault, string)).autorelease();
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

void UIScriptController::insertText(JSStringRef text, int location, int length)
{
#if WK_API_ENABLED
    if (location == -1)
        location = NSNotFound;

    auto* webView = TestController::singleton().mainWebView()->platformView();
    [webView _insertText:nsString(text) replacementRange:NSMakeRange(location, length)];
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

JSObjectRef UIScriptController::contentsOfUserInterfaceItem(JSStringRef interfaceItem) const
{
#if WK_API_ENABLED
    TestRunnerWKWebView *webView = TestController::singleton().mainWebView()->platformView();
    NSDictionary *contentDictionary = [webView _contentsOfUserInterfaceItem:toWTFString(toWK(interfaceItem))];
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:contentDictionary inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
#else
    UNUSED_PARAM(interfaceItem);
    return nullptr;
#endif
}

void UIScriptController::removeViewFromWindow(JSValueRef callback)
{
#if WK_API_ENABLED
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto* mainWebView = TestController::singleton().mainWebView();
    mainWebView->removeFromWindow();

    [mainWebView->platformView() _doAfterNextPresentationUpdate: ^ {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
#else
    UNUSED_PARAM(callback);
#endif
}

void UIScriptController::addViewToWindow(JSValueRef callback)
{
#if WK_API_ENABLED
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto* mainWebView = TestController::singleton().mainWebView();
    mainWebView->addToWindow();

    [mainWebView->platformView() _doAfterNextPresentationUpdate: ^ {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
#else
    UNUSED_PARAM(callback);
#endif
}

} // namespace WTR
