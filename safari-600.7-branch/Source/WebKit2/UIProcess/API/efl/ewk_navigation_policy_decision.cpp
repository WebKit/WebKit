/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "ewk_navigation_policy_decision.h"

#include "ewk_navigation_policy_decision_private.h"
#include "ewk_private.h"
#include "ewk_url_request_private.h"

using namespace WebKit;

inline static Ewk_Navigation_Type toEwkNavigationType(WKFrameNavigationType navigationType)
{
    switch (navigationType) {
    case kWKFrameNavigationTypeLinkClicked:
        return EWK_NAVIGATION_TYPE_LINK_ACTIVATED;
    case kWKFrameNavigationTypeFormSubmitted:
        return EWK_NAVIGATION_TYPE_FORM_SUBMITTED;
    case kWKFrameNavigationTypeBackForward:
        return EWK_NAVIGATION_TYPE_BACK_FORWARD;
    case kWKFrameNavigationTypeReload:
        return EWK_NAVIGATION_TYPE_RELOAD;
    case kWKFrameNavigationTypeFormResubmitted:
        return EWK_NAVIGATION_TYPE_FORM_RESUBMITTED;
    case kWKFrameNavigationTypeOther:
        return EWK_NAVIGATION_TYPE_OTHER;
    }
    ASSERT_NOT_REACHED();

    return EWK_NAVIGATION_TYPE_LINK_ACTIVATED;
}

inline static Event_Mouse_Button toEventMouseButton(WKEventMouseButton mouseButton)
{
    switch (mouseButton) {
    case kWKEventMouseButtonNoButton:
        return EVENT_MOUSE_BUTTON_NONE;
    case kWKEventMouseButtonLeftButton:
        return EVENT_MOUSE_BUTTON_LEFT;
    case kWKEventMouseButtonMiddleButton:
        return EVENT_MOUSE_BUTTON_MIDDLE;
    case kWKEventMouseButtonRightButton:
        return EVENT_MOUSE_BUTTON_RIGHT;
    }
    ASSERT_NOT_REACHED();

    return EVENT_MOUSE_BUTTON_NONE;
}

inline static Event_Modifier_Keys toEventModifierKeys(WKEventModifiers modifiers)
{
    unsigned keys = 0;
    if (modifiers & kWKEventModifiersShiftKey)
        keys |= EVENT_MODIFIER_KEY_SHIFT;
    if (modifiers & kWKEventModifiersControlKey)
        keys |= kWKEventModifiersControlKey;
    if (modifiers & kWKEventModifiersAltKey)
        keys |= EVENT_MODIFIER_KEY_ALT;
    if (modifiers & kWKEventModifiersMetaKey)
        keys |= EVENT_MODIFIER_KEY_META;
    return static_cast<Event_Modifier_Keys>(keys);
}

EwkNavigationPolicyDecision::EwkNavigationPolicyDecision(WKFramePolicyListenerRef listener, WKFrameNavigationType navigationType, WKEventMouseButton mouseButton, WKEventModifiers modifiers, PassRefPtr<EwkUrlRequest> request, const char* frameName)
    : m_listener(listener)
    , m_actedUponByClient(false)
    , m_navigationType(navigationType)
    , m_mouseButton(mouseButton)
    , m_modifiers(modifiers)
    , m_request(request)
    , m_frameName(frameName)
{ }

EwkNavigationPolicyDecision::~EwkNavigationPolicyDecision()
{
    // This is the default choice for all policy decisions in WebPageProxy.cpp.
    if (!m_actedUponByClient)
        WKFramePolicyListenerUse(m_listener.get());
}

Ewk_Navigation_Type EwkNavigationPolicyDecision::navigationType() const
{
    return toEwkNavigationType(m_navigationType);
}

Event_Mouse_Button EwkNavigationPolicyDecision::mouseButton() const
{
    return toEventMouseButton(m_mouseButton);
}

Event_Modifier_Keys EwkNavigationPolicyDecision::modifiers() const
{
    return toEventModifierKeys(m_modifiers);
}

const char* EwkNavigationPolicyDecision::frameName() const
{
    return m_frameName;
}

EwkUrlRequest* EwkNavigationPolicyDecision::request() const
{
    return m_request.get();
}

void EwkNavigationPolicyDecision::accept()
{
    WKFramePolicyListenerUse(m_listener.get());
    m_actedUponByClient = true;
}

void EwkNavigationPolicyDecision::reject()
{
    WKFramePolicyListenerIgnore(m_listener.get());
    m_actedUponByClient = true;
}

void EwkNavigationPolicyDecision::download()
{
    WKFramePolicyListenerDownload(m_listener.get());
    m_actedUponByClient = true;
}

Ewk_Navigation_Type ewk_navigation_policy_navigation_type_get(const Ewk_Navigation_Policy_Decision* decision)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkNavigationPolicyDecision, decision, impl, EWK_NAVIGATION_TYPE_OTHER);

    return impl->navigationType();
}

Event_Mouse_Button ewk_navigation_policy_mouse_button_get(const Ewk_Navigation_Policy_Decision* decision)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkNavigationPolicyDecision, decision, impl, EVENT_MOUSE_BUTTON_NONE);

    return impl->mouseButton();
}

Event_Modifier_Keys ewk_navigation_policy_modifiers_get(const Ewk_Navigation_Policy_Decision* decision)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkNavigationPolicyDecision, decision, impl, static_cast<Event_Modifier_Keys>(0));

    return impl->modifiers();
}

const char* ewk_navigation_policy_frame_name_get(const Ewk_Navigation_Policy_Decision* decision)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkNavigationPolicyDecision, decision, impl, nullptr);

    return impl->frameName();
}

Ewk_Url_Request* ewk_navigation_policy_request_get(const Ewk_Navigation_Policy_Decision* decision)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkNavigationPolicyDecision, decision, impl, nullptr);

    return impl->request();
}

void ewk_navigation_policy_decision_accept(Ewk_Navigation_Policy_Decision* decision)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkNavigationPolicyDecision, decision, impl);

    impl->accept();
}

void ewk_navigation_policy_decision_reject(Ewk_Navigation_Policy_Decision* decision)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkNavigationPolicyDecision, decision, impl);

    impl->reject();
}

void ewk_navigation_policy_decision_download(Ewk_Navigation_Policy_Decision* decision)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkNavigationPolicyDecision, decision, impl);

    impl->download();
}
