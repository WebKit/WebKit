/*
 * Copyright (C) 2019 Alexander Mikhaylenko <exalm7659@gmail.com>
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

#include "config.h"
#include "UIScriptControllerGtk.h"

#include "EventSenderProxy.h"
#include "PlatformWebView.h"
#include "TestController.h"
#include "TextChecker.h"
#include "UIScriptContext.h"
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebKit/WKViewPrivate.h>
#include <gtk/gtk.h>
#include <wtf/RunLoop.h>

namespace WTR {

Ref<UIScriptController> UIScriptController::create(UIScriptContext& context)
{
    return adoptRef(*new UIScriptControllerGtk(context));
}

void UIScriptControllerGtk::beginBackSwipe(JSValueRef callback)
{
    auto* webView = TestController::singleton().mainWebView()->platformView();

    WKViewBeginBackSwipeForTesting(webView);
}

void UIScriptControllerGtk::completeBackSwipe(JSValueRef callback)
{
    auto* webView = TestController::singleton().mainWebView()->platformView();

    WKViewCompleteBackSwipeForTesting(webView);
}

bool UIScriptControllerGtk::isShowingDataListSuggestions() const
{
    auto* webView = TestController::singleton().mainWebView()->platformView();
    if (auto* popup = g_object_get_data(G_OBJECT(webView), "wk-datalist-popup"))
        return gtk_widget_get_mapped(GTK_WIDGET(popup));
    return false;
}

void UIScriptControllerGtk::doAsyncTask(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerGtk::setContinuousSpellCheckingEnabled(bool enabled)
{
    WebKit::TextChecker::setContinuousSpellCheckingEnabled(enabled);
}

void UIScriptControllerGtk::copyText(JSStringRef text)
{
    auto string = text->string().utf8();
#if USE(GTK4)
    gdk_clipboard_set_text(gdk_display_get_clipboard(gdk_display_get_default()), string.data());
#else
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), string.data(), string.length());
#endif
}

void UIScriptControllerGtk::dismissMenu()
{
    // FIXME: implement.
}

bool UIScriptControllerGtk::isShowingMenu() const
{
    // FIXME: implement.
    return false;
}

void UIScriptControllerGtk::activateAtPoint(long x, long y, JSValueRef callback)
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

    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerGtk::activateDataListSuggestion(unsigned index, JSValueRef callback)
{
    // FIXME: implement.
    UNUSED_PARAM(index);

    doAsyncTask(callback);
}

void UIScriptControllerGtk::simulateAccessibilitySettingsChangeNotification(JSValueRef callback)
{
    // FIXME: implement.
    doAsyncTask(callback);
}

void UIScriptControllerGtk::removeViewFromWindow(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    auto* mainWebView = TestController::singleton().mainWebView();
    mainWebView->removeFromWindow();

    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerGtk::addViewToWindow(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    auto* mainWebView = TestController::singleton().mainWebView();
    mainWebView->addToWindow();

    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

} // namespace WTR
