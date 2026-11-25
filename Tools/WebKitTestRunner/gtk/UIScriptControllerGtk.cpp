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
#include <JavaScriptCore/JSStringRefCPP.h>
#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebKit/WKTextCheckerGLib.h>
#include <WebKit/WKViewPrivate.h>
#include <gtk/gtk.h>
#include <webkit/WebKitWebViewBaseInternal.h>
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
    auto string = utf8(text->string());
#if USE(GTK4)
    gdk_clipboard_set_text(gdk_display_get_clipboard(gdk_display_get_default()), string.legacyCStringPointer());
#else
    auto characters = byteCast<char>(string.span());
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), characters.data(), characters.size());
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
    if (!equal(toWTFString(preference), "WebKitMinimumFontSize"_s))
        return;

    auto preferences = TestController::singleton().platformPreferences();
    WKPreferencesSetMinimumFontSize(preferences, static_cast<uint32_t>(toDouble(toWTFString(value))));
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
            jsonObject->setObject(StringImpl::fromLatin1(key).get(), toJSONObject(value));
        else if (type && g_variant_type_equal(type, G_VARIANT_TYPE_STRING))
            jsonObject->setString(StringImpl::fromLatin1(key).get(), StringImpl::fromLatin1(g_variant_get_string(value, nullptr)).get());
        else if (type && g_variant_type_equal(type, G_VARIANT_TYPE_DOUBLE))
            jsonObject->setDouble(StringImpl::fromLatin1(key).get(), g_variant_get_double(value));
    }
    return jsonObject;
}

JSObjectRef UIScriptControllerGtk::contentsOfUserInterfaceItem(JSStringRef interfaceItem) const
{
    auto* webView = TestController::singleton().mainWebView()->platformView();
    GRefPtr<GVariant> contentDictionary = WKViewContentsOfUserInterfaceItem(webView, utf8(toWTFString(interfaceItem)).legacyCStringPointer());
    auto jsonObject = toJSONObject(contentDictionary.get());

    return JSValueToObject(m_context->jsContext(), contentDictionary ? JSValueMakeFromJSONString(m_context->jsContext(), ::createJSString(jsonObject->toJSONString()).get()) : JSValueMakeUndefined(m_context->jsContext()), nullptr);
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

static WheelEventPhase wheelEventPhaseFromString(StringImpl* phase)
{
    if (equal(phase, "began"_s))
        return WheelEventPhase::Began;
    if (equal(phase, "changed"_s) || equal(phase, "continue"_s)) // Allow "continue" for ease of conversion from mouseScrollByWithWheelAndMomentumPhases values.
        return WheelEventPhase::Changed;
    if (equal(phase, "ended"_s))
        return WheelEventPhase::Ended;
    if (equal(phase, "cancelled"_s))
        return WheelEventPhase::Cancelled;
    if (equal(phase, "maybegin"_s))
        return WheelEventPhase::MayBegin;

    ASSERT_NOT_REACHED();
    return WheelEventPhase::NoPhase;
}

void UIScriptControllerGtk::sendEventStream(JSStringRef eventsJSON, JSValueRef callback)
{
    auto* eventSender = TestController::singleton().eventSenderProxy();
    if (!eventSender) {
        ASSERT_NOT_REACHED();
        return;
    }

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto jsonValue = JSON::Value::parseJSON(eventsJSON->string());
    auto jsonObject = jsonValue ? jsonValue->asObject() : nullptr;
    auto eventsArray = jsonObject ? jsonObject->getArray("events"_si) : nullptr;
    if (!eventsArray) {
        WTFLogAlways("JSON is not convertible to a dictionary with an `events` array");
        return;
    }

    double currentViewRelativeX = 0;
    double currentViewRelativeY = 0;

    for (unsigned i = 0; i < eventsArray->length(); ++i) {
        auto eventObject = eventsArray->get(i)->asObject();
        if (!eventObject) {
            WTFLogAlways("Event is not a dictionary");
            break;
        }

        auto eventTypeString = eventObject->getString("type"_si);
        if (!eventTypeString) {
            SAFE_WTFLOGALWAYS("Failed to find type key in %s", utf8(eventTypeString));
            break;
        }

        if (equal(eventTypeString, "wheel"_s)) {
            auto phase = WheelEventPhase::NoPhase;
            auto momentumPhase = WheelEventPhase::NoPhase;

            auto phaseString = eventObject->getString("phase"_si);
            if (phaseString)
                phase = wheelEventPhaseFromString(phaseString.get());

            auto momentumPhaseString = eventObject->getString("momentumPhase"_si);
            if (momentumPhaseString) {
                momentumPhase = wheelEventPhaseFromString(momentumPhaseString.get());
                if (momentumPhase == WheelEventPhase::Cancelled || momentumPhase == WheelEventPhase::MayBegin) {
                    SAFE_WTFLOGALWAYS("Invalid value %s for momentumPhase", utf8(momentumPhaseString));
                    break;
                }
            }

            ASSERT_IMPLIES(phase == WheelEventPhase::NoPhase, momentumPhase != WheelEventPhase::NoPhase);
            ASSERT_IMPLIES(momentumPhase == WheelEventPhase::NoPhase, phase != WheelEventPhase::NoPhase);

            if (auto x = eventObject->getDouble("viewX"_si))
                currentViewRelativeX = *x;

            if (auto y = eventObject->getDouble("viewY"_si))
                currentViewRelativeY = *y;

            double deltaX = eventObject->getDouble("deltaX"_si).value_or(0);
            double deltaY = eventObject->getDouble("deltaY"_si).value_or(0);

            eventSender->sendWheelEvent(currentViewRelativeX, currentViewRelativeY, deltaX, deltaY, phase, momentumPhase);
        }
    }

    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

} // namespace WTR
