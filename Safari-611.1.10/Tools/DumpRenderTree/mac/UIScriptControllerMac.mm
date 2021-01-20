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
#import "LayoutTestSpellChecker.h"
#import "UIScriptContext.h"
#import <JavaScriptCore/JSContext.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <JavaScriptCore/JSValue.h>
#import <JavaScriptCore/OpaqueJSString.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebViewPrivate.h>
#import <pal/spi/mac/NSTextInputContextSPI.h>
#import <wtf/BlockPtr.h>

namespace WTR {

Ref<UIScriptController> UIScriptController::create(UIScriptContext& context)
{
    return adoptRef(*new UIScriptControllerMac(context));
}

void UIScriptControllerMac::doAsyncTask(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get());
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
    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get());
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

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get());
}

void UIScriptControllerMac::addViewToWindow(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    WebView *webView = [mainFrame webView];
    [[mainWindow contentView] addSubview:webView];

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, strongThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get());
}

void UIScriptControllerMac::toggleCapsLock(JSValueRef callback)
{
    doAsyncTask(callback);
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

}

#endif // PLATFORM(MAC)
