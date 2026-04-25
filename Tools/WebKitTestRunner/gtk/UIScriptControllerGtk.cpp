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
#include "StringFunctions.h"
#include "TestController.h"
#include "UIScriptContext.h"
#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebKit/WKTextCheckerGLib.h>
#include <WebKit/WKViewPrivate.h>
#include <gtk/gtk.h>
#include <wtf/JSONValues.h>
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
    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerGtk::setContinuousSpellCheckingEnabled(bool enabled)
{
    WKTextCheckerSetContinuousSpellCheckingEnabled(enabled);
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

void UIScriptControllerGtk::paste()
{
    auto page = TestController::singleton().mainWebView()->page();
    WKPageExecuteCommand(page, WKStringCreateWithUTF8CString("Paste"));
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

    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, callbackID] {
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

    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, callbackID] {
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

    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerGtk::overridePreference(JSStringRef preference, JSStringRef value)
{
    if (toWTFString(preference) != "WebKitMinimumFontSize"_s)
        return;

    auto preferences = TestController::singleton().platformPreferences();
    WKPreferencesSetMinimumFontSize(preferences, static_cast<uint32_t>(toWTFString(value).toDouble()));
}

static Ref<JSON::Object> toJSONObject(GVariant* variant)
{
    Ref<JSON::Object> jsonObject = JSON::Object::create();

    const char* key;
    GVariant* value;
    GVariantIter iter;
    g_variant_iter_init(&iter, variant);
    while (g_variant_iter_loop(&iter, "{&sv}", &key, &value)) {
        const GVariantType* type = g_variant_get_type(value);
        if (type && g_variant_type_equal(type, G_VARIANT_TYPE_VARDICT))
            jsonObject->setObject(String::fromLatin1(key), toJSONObject(value));
        else if (type && g_variant_type_equal(type, G_VARIANT_TYPE_STRING))
            jsonObject->setString(String::fromLatin1(key), String::fromLatin1(g_variant_get_string(value, nullptr)));
        else if (type && g_variant_type_equal(type, G_VARIANT_TYPE_DOUBLE))
            jsonObject->setDouble(String::fromLatin1(key), g_variant_get_double(value));
    }
    return jsonObject;
}

JSObjectRef UIScriptControllerGtk::contentsOfUserInterfaceItem(JSStringRef interfaceItem) const
{
    auto* webView = TestController::singleton().mainWebView()->platformView();
    GRefPtr<GVariant> contentDictionary = WKViewContentsOfUserInterfaceItem(webView, toWTFString(interfaceItem).utf8().data());
    auto jsonObject = toJSONObject(contentDictionary.get());

    return JSValueToObject(m_context->jsContext(), contentDictionary ? JSValueMakeFromJSONString(m_context->jsContext(), createJSString(jsonObject->toJSONString().utf8().data()).get()) : JSValueMakeUndefined(m_context->jsContext()), nullptr);
}

void UIScriptControllerGtk::setWebViewEditable(bool editable)
{
    auto* webView = TestController::singleton().mainWebView()->platformView();
    WKViewSetEditable(webView, editable);
}

void UIScriptControllerGtk::zoomToScale(double scale, JSValueRef callback)
{
    auto page = TestController::singleton().mainWebView()->page();
    WKPageSetScaleFactor(page, scale, WKPointMake(0, 0));
    doAsyncTask(callback);
}

double UIScriptControllerGtk::zoomScale() const
{
    auto page = TestController::singleton().mainWebView()->page();
    return WKPageGetScaleFactor(page);
}

} // namespace WTR
