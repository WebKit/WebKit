/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "UIScriptControllerWPE.h"

#include "EventSenderProxy.h"
#include "PlatformWebView.h"
#include "TestController.h"
#include "UIScriptContext.h"
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebKit/WKTextCheckerGLib.h>
#include <wtf/JSONValues.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RunLoop.h>

#if ENABLE(WPE_PLATFORM)
#include <wpe/wpe-platform.h>
#endif

namespace WTR {

Ref<UIScriptController> UIScriptController::create(UIScriptContext& context)
{
    return adoptRef(*new UIScriptControllerWPE(context));
}

void UIScriptControllerWPE::doAsyncTask(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);
    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerWPE::setContinuousSpellCheckingEnabled(bool enabled)
{
    WKTextCheckerSetContinuousSpellCheckingEnabled(enabled);
}

void UIScriptControllerWPE::copyText(JSStringRef text)
{
#if ENABLE(WPE_PLATFORM)
    if (!TestController::singleton().useWPELegacyAPI()) {
        auto* clipboard = wpe_display_get_clipboard(wpe_display_get_primary());
        auto* content = wpe_clipboard_content_new();
        wpe_clipboard_content_set_text(content, text->string().utf8().data());
        wpe_clipboard_set_content(clipboard, content);
        wpe_clipboard_content_unref(content);
    }
#endif
    // FIXME: implement.
}

void UIScriptControllerWPE::paste()
{
    auto page = TestController::singleton().mainWebView()->page();
    WKPageExecuteCommand(page, WKStringCreateWithUTF8CString("Paste"));
}

void UIScriptControllerWPE::dismissMenu()
{
    // FIXME: implement.
}

bool UIScriptControllerWPE::isShowingMenu() const
{
    // FIXME: implement.
    return false;
}

void UIScriptControllerWPE::activateAtPoint(long x, long y, JSValueRef callback)
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

void UIScriptControllerWPE::simulateAccessibilitySettingsChangeNotification(JSValueRef callback)
{
    // FIXME: implement.
    doAsyncTask(callback);
}

void UIScriptControllerWPE::removeViewFromWindow(JSValueRef callback)
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

void UIScriptControllerWPE::addViewToWindow(JSValueRef callback)
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

void UIScriptControllerWPE::zoomToScale(double scale, JSValueRef callback)
{
    auto page = TestController::singleton().mainWebView()->page();
    WKPageSetScaleFactor(page, scale, WKPointMake(0, 0));
    doAsyncTask(callback);
}

double UIScriptControllerWPE::zoomScale() const
{
    auto page = TestController::singleton().mainWebView()->page();
    return WKPageGetScaleFactor(page);
}

static EventSenderProxy::WheelEventPhase wheelEventPhaseFromString(const String& phase)
{
    if (phase == "began"_s)
        return EventSenderProxy::WheelEventPhase::Began;
    if (phase == "changed"_s || phase == "continue"_s) // Allow "continue" for ease of conversion from mouseScrollByWithWheelAndMomentumPhases values.
        return EventSenderProxy::WheelEventPhase::Changed;
    if (phase == "ended"_s)
        return EventSenderProxy::WheelEventPhase::Ended;
    if (phase == "cancelled"_s)
        return EventSenderProxy::WheelEventPhase::Cancelled;
    if (phase == "maybegin"_s)
        return EventSenderProxy::WheelEventPhase::MayBegin;

    ASSERT_NOT_REACHED();
    return EventSenderProxy::WheelEventPhase::None;
}

void UIScriptControllerWPE::sendEventStream(JSStringRef eventsJSON, JSValueRef callback)
{
    auto* eventSender = TestController::singleton().eventSenderProxy();
    if (!eventSender) {
        ASSERT_NOT_REACHED();
        return;
    }

    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    auto jsonValue = JSON::Value::parseJSON(eventsJSON->string());
    auto jsonObject = jsonValue ? jsonValue->asObject() : nullptr;
    auto eventsArray = jsonObject ? jsonObject->getArray("events"_s) : nullptr;
    if (!eventsArray) {
        WTFLogAlways("JSON is not convertible to a dictionary with an `events` array");
        return;
    }

    double currentViewRelativeX = 0;
    double currentViewRelativeY = 0;

    constexpr double secondsPerEvent = 1.0 / 60;
    double currentTime = MonotonicTime::now().secondsSinceEpoch().value();

    for (unsigned i = 0; i < eventsArray->length(); ++i) {
        auto eventObject = eventsArray->get(i)->asObject();
        if (!eventObject) {
            WTFLogAlways("Event is not a dictionary");
            break;
        }

        auto eventTypeString = eventObject->getString("type"_s);
        if (!eventTypeString) {
            WTFLogAlways("Failed to find type key in %s", eventTypeString.utf8().data());
            break;
        }

        if (eventTypeString == "wheel"_s) {
            auto phase = EventSenderProxy::WheelEventPhase::None;
            auto momentumPhase = EventSenderProxy::WheelEventPhase::None;

            auto phaseString = eventObject->getString("phase"_s);
            if (!phaseString.isNull())
                phase = wheelEventPhaseFromString(phaseString);

            auto momentumPhaseString = eventObject->getString("momentumPhase"_s);
            if (!momentumPhaseString.isNull()) {
                momentumPhase = wheelEventPhaseFromString(momentumPhaseString);
                if (momentumPhase == EventSenderProxy::WheelEventPhase::Cancelled || momentumPhase == EventSenderProxy::WheelEventPhase::MayBegin) {
                    WTFLogAlways("Invalid value %s for momentumPhase", momentumPhaseString.utf8().data());
                    break;
                }
            }

            ASSERT_IMPLIES(phase == EventSenderProxy::WheelEventPhase::None, momentumPhase != EventSenderProxy::WheelEventPhase::None);
            ASSERT_IMPLIES(momentumPhase == EventSenderProxy::WheelEventPhase::None, phase != EventSenderProxy::WheelEventPhase::None);

            if (auto x = eventObject->getDouble("viewX"_s))
                currentViewRelativeX = *x;

            if (auto y = eventObject->getDouble("viewY"_s))
                currentViewRelativeY = *y;

            double deltaX = eventObject->getDouble("deltaX"_s).value_or(0);
            double deltaY = eventObject->getDouble("deltaY"_s).value_or(0);

            eventSender->sendWheelEvent(currentTime, currentViewRelativeX, currentViewRelativeY, deltaX, deltaY, phase, momentumPhase);
        }

        currentTime += secondsPerEvent;
    }

    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, callbackID] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

} // namespace WTR
