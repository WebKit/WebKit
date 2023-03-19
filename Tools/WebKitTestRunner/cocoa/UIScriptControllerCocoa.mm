/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "UIScriptControllerCocoa.h"

#import "PlatformWebView.h"
#import "StringFunctions.h"
#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import "UIScriptContext.h"
#import <JavaScriptCore/JavaScriptCore.h>
#import <WebKit/WKURLCF.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/BlockPtr.h>

@interface WKWebView (WKWebViewInternal)
- (void)paste:(id)sender;
@end

namespace WTR {

UIScriptControllerCocoa::UIScriptControllerCocoa(UIScriptContext& context)
    : UIScriptControllerCommon(context)
{
}

TestRunnerWKWebView *UIScriptControllerCocoa::webView() const
{
    return TestController::singleton().mainWebView()->platformView();
}

void UIScriptControllerCocoa::setViewScale(double scale)
{
    webView()._viewScale = scale;
}

void UIScriptControllerCocoa::setMinimumEffectiveWidth(double effectiveWidth)
{
    webView()._minimumEffectiveDeviceWidth = effectiveWidth;
}

void UIScriptControllerCocoa::setWebViewEditable(bool editable)
{
    webView()._editable = editable;
}

void UIScriptControllerCocoa::doAsyncTask(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerCocoa::doAfterPresentationUpdate(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    [webView() _doAfterNextPresentationUpdate:makeBlockPtr([this, strongThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }).get()];
}

void UIScriptControllerCocoa::completeTaskAsynchronouslyAfterActivityStateUpdate(unsigned callbackID)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        auto* mainWebView = TestController::singleton().mainWebView();
        ASSERT(mainWebView);

        [mainWebView->platformView() _doAfterActivityStateUpdate: ^{
            if (!m_context)
                return;

            m_context->asyncTaskComplete(callbackID);
        }];
    });
}

JSRetainPtr<JSStringRef> UIScriptControllerCocoa::scrollingTreeAsText() const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)[webView() _scrollingTreeAsText]));
}

void UIScriptControllerCocoa::removeViewFromWindow(JSValueRef callback)
{
    // FIXME: On iOS, we never invoke the completion callback that's passed in. Fixing this causes the layout
    // test pageoverlay/overlay-remove-reinsert-view.html to start failing consistently on iOS. It seems like
    // this warrants some more investigation.
#if PLATFORM(MAC)
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
#else
    UNUSED_PARAM(callback);
#endif

    auto* mainWebView = TestController::singleton().mainWebView();
    mainWebView->removeFromWindow();

#if PLATFORM(MAC)
    completeTaskAsynchronouslyAfterActivityStateUpdate(callbackID);
#endif // PLATFORM(MAC)
}

void UIScriptControllerCocoa::addViewToWindow(JSValueRef callback)
{
#if PLATFORM(MAC)
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
#else
    UNUSED_PARAM(callback);
#endif

    auto* mainWebView = TestController::singleton().mainWebView();
    mainWebView->addToWindow();

#if PLATFORM(MAC)
    completeTaskAsynchronouslyAfterActivityStateUpdate(callbackID);
#endif // PLATFORM(MAC)
}

void UIScriptControllerCocoa::overridePreference(JSStringRef preferenceRef, JSStringRef valueRef)
{
    if (toWTFString(preferenceRef) == "WebKitMinimumFontSize"_s)
        webView().configuration.preferences.minimumFontSize = toWTFString(valueRef).toDouble();
}

void UIScriptControllerCocoa::findString(JSStringRef string, unsigned long options, unsigned long maxCount)
{
    [webView() _findString:toWTFString(string) options:options maxCount:maxCount];
}

JSObjectRef UIScriptControllerCocoa::contentsOfUserInterfaceItem(JSStringRef interfaceItem) const
{
    NSDictionary *contentDictionary = [webView() _contentsOfUserInterfaceItem:toWTFString(interfaceItem)];
    return JSValueToObject(m_context->jsContext(), [JSValue valueWithObject:contentDictionary inContext:[JSContext contextWithJSGlobalContextRef:m_context->jsContext()]].JSValueRef, nullptr);
}

void UIScriptControllerCocoa::setDefaultCalendarType(JSStringRef calendarIdentifier, JSStringRef localeIdentifier)
{
    auto cfCalendarIdentifier = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, calendarIdentifier));
    auto cfLocaleIdentifier = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, localeIdentifier));
    TestController::singleton().setDefaultCalendarType((__bridge NSString *)cfCalendarIdentifier.get(), (__bridge NSString *)cfLocaleIdentifier.get());
}

JSRetainPtr<JSStringRef> UIScriptControllerCocoa::lastUndoLabel() const
{
    return adopt(JSStringCreateWithCFString((__bridge CFStringRef)platformUndoManager().undoActionName));
}

JSRetainPtr<JSStringRef> UIScriptControllerCocoa::caLayerTreeAsText() const
{
    return adopt(JSStringCreateWithCFString((CFStringRef)[webView() _caLayerTreeAsText]));
}

JSRetainPtr<JSStringRef> UIScriptControllerCocoa::firstRedoLabel() const
{
    return adopt(JSStringCreateWithCFString((__bridge CFStringRef)platformUndoManager().redoActionName));
}

NSUndoManager *UIScriptControllerCocoa::platformUndoManager() const
{
    return platformContentView().undoManager;
}

void UIScriptControllerCocoa::setDidShowContextMenuCallback(JSValueRef callback)
{
    UIScriptController::setDidShowContextMenuCallback(callback);
    webView().didShowContextMenuCallback = makeBlockPtr([this, strongThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidShowContextMenu);
    }).get();
}

void UIScriptControllerCocoa::setDidDismissContextMenuCallback(JSValueRef callback)
{
    UIScriptController::setDidDismissContextMenuCallback(callback);
    webView().didDismissContextMenuCallback = makeBlockPtr([this, strongThis = Ref { *this }] {
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidDismissContextMenu);
    }).get();
}

bool UIScriptControllerCocoa::isShowingContextMenu() const
{
    return webView().isShowingContextMenu;
}

void UIScriptControllerCocoa::setDidShowMenuCallback(JSValueRef callback)
{
    UIScriptController::setDidShowMenuCallback(callback);
    webView().didShowMenuCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidShowMenu);
    };
}

void UIScriptControllerCocoa::setDidHideMenuCallback(JSValueRef callback)
{
    UIScriptController::setDidHideMenuCallback(callback);
    webView().didHideMenuCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidHideMenu);
    };
}

void UIScriptControllerCocoa::dismissMenu()
{
    [webView() dismissActiveMenu];
}

bool UIScriptControllerCocoa::isShowingMenu() const
{
    return webView().showingMenu;
}

void UIScriptControllerCocoa::setContinuousSpellCheckingEnabled(bool enabled)
{
    [webView() _setContinuousSpellCheckingEnabledForTesting:enabled];
}

void UIScriptControllerCocoa::paste()
{
    [webView() paste:nil];
}

void UIScriptControllerCocoa::insertAttachmentForFilePath(JSStringRef filePath, JSStringRef contentType, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    auto testURL = adoptCF(WKURLCopyCFURL(kCFAllocatorDefault, TestController::singleton().currentTestURL()));
    auto attachmentURL = [NSURL fileURLWithPath:toWTFString(filePath) relativeToURL:(__bridge NSURL *)testURL.get()];
    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initWithURL:attachmentURL options:0 error:nil]);
    [webView() _insertAttachmentWithFileWrapper:fileWrapper.get() contentType:toWTFString(contentType) completion:^(BOOL success) {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    }];
}

void UIScriptControllerCocoa::setDidShowContactPickerCallback(JSValueRef callback)
{
    UIScriptController::setDidShowContactPickerCallback(callback);
    webView().didShowContactPickerCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidShowContactPicker);
    };
}

void UIScriptControllerCocoa::setDidHideContactPickerCallback(JSValueRef callback)
{
    UIScriptController::setDidHideContactPickerCallback(callback);
    webView().didHideContactPickerCallback = ^{
        if (!m_context)
            return;
        m_context->fireCallback(CallbackTypeDidHideContactPicker);
    };
}

bool UIScriptControllerCocoa::isShowingContactPicker() const
{
    return webView().showingContactPicker;
}

void UIScriptControllerCocoa::dismissContactPickerWithContacts(JSValueRef contacts)
{
    JSContext *context = [JSContext contextWithJSGlobalContextRef:m_context->jsContext()];
    JSValue *value = [JSValue valueWithJSValueRef:contacts inContext:context];
    [webView() _dismissContactPickerWithContacts:[value toArray]];
}

unsigned long UIScriptControllerCocoa::countOfUpdatesWithLayerChanges() const
{
    return webView()._countOfUpdatesWithLayerChanges;
}

} // namespace WTR
