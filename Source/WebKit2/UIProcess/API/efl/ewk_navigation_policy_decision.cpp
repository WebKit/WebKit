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

#include "WKAPICast.h"
#include "WKEinaSharedString.h"
#include "WKFramePolicyListener.h"
#include "WKRetainPtr.h"
#include "ewk_navigation_policy_decision_private.h"
#include "ewk_private.h"
#include "ewk_url_request_private.h"

using namespace WebKit;

/**
 * \struct  _Ewk_Navigation_Policy_Decision
 * @brief   Contains the navigation policy decision data.
 */
struct _Ewk_Navigation_Policy_Decision {
    unsigned int __ref; /**< the reference count of the object */
    WKRetainPtr<WKFramePolicyListenerRef> listener;
    bool actedUponByClient;
    Ewk_Navigation_Type navigationType;
    Event_Mouse_Button mouseButton;
    Event_Modifier_Keys modifiers;
    Ewk_Url_Request* request;
    WKEinaSharedString frameName;

    _Ewk_Navigation_Policy_Decision(WKFramePolicyListenerRef _listener, Ewk_Navigation_Type _navigationType, Event_Mouse_Button _mouseButton, Event_Modifier_Keys _modifiers, Ewk_Url_Request* _request, const char* _frameName)
        : __ref(1)
        , listener(_listener)
        , actedUponByClient(false)
        , navigationType(_navigationType)
        , mouseButton(_mouseButton)
        , modifiers(_modifiers)
        , request(_request)
        , frameName(_frameName)
    { }

    ~_Ewk_Navigation_Policy_Decision()
    {
        ASSERT(!__ref);

        // This is the default choice for all policy decisions in WebPageProxy.cpp.
        if (!actedUponByClient)
            WKFramePolicyListenerUse(listener.get());

        ewk_url_request_unref(request);
    }
};

void ewk_navigation_policy_decision_ref(Ewk_Navigation_Policy_Decision* decision)
{
    EINA_SAFETY_ON_NULL_RETURN(decision);

    ++decision->__ref;
}

void ewk_navigation_policy_decision_unref(Ewk_Navigation_Policy_Decision* decision)
{
    EINA_SAFETY_ON_NULL_RETURN(decision);

    if (--decision->__ref)
        return;

    delete decision;
}

Ewk_Navigation_Type ewk_navigation_policy_navigation_type_get(const Ewk_Navigation_Policy_Decision* decision)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(decision, EWK_NAVIGATION_TYPE_OTHER);

    return decision->navigationType;
}

Event_Mouse_Button ewk_navigation_policy_mouse_button_get(const Ewk_Navigation_Policy_Decision* decision)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(decision, EVENT_MOUSE_BUTTON_NONE);

    return decision->mouseButton;
}

Event_Modifier_Keys ewk_navigation_policy_modifiers_get(const Ewk_Navigation_Policy_Decision* decision)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(decision, static_cast<Event_Modifier_Keys>(0));

    return decision->modifiers;
}

const char* ewk_navigation_policy_frame_name_get(const Ewk_Navigation_Policy_Decision* decision)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(decision, 0);

    return decision->frameName;
}

Ewk_Url_Request* ewk_navigation_policy_request_get(const Ewk_Navigation_Policy_Decision* decision)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(decision, 0);

    return decision->request;
}

void ewk_navigation_policy_decision_accept(Ewk_Navigation_Policy_Decision* decision)
{
    EINA_SAFETY_ON_NULL_RETURN(decision);

    WKFramePolicyListenerUse(decision->listener.get());
    decision->actedUponByClient = true;
}

void ewk_navigation_policy_decision_reject(Ewk_Navigation_Policy_Decision* decision)
{
    EINA_SAFETY_ON_NULL_RETURN(decision);

    WKFramePolicyListenerIgnore(decision->listener.get());
    decision->actedUponByClient = true;
}

void ewk_navigation_policy_decision_download(Ewk_Navigation_Policy_Decision* decision)
{
    EINA_SAFETY_ON_NULL_RETURN(decision);

    WKFramePolicyListenerDownload(decision->listener.get());
    decision->actedUponByClient = true;
}

// Ewk_Navigation_Type enum validation
COMPILE_ASSERT_MATCHING_ENUM(EWK_NAVIGATION_TYPE_LINK_ACTIVATED, kWKFrameNavigationTypeLinkClicked);
COMPILE_ASSERT_MATCHING_ENUM(EWK_NAVIGATION_TYPE_FORM_SUBMITTED, kWKFrameNavigationTypeFormSubmitted);
COMPILE_ASSERT_MATCHING_ENUM(EWK_NAVIGATION_TYPE_BACK_FORWARD, kWKFrameNavigationTypeBackForward);
COMPILE_ASSERT_MATCHING_ENUM(EWK_NAVIGATION_TYPE_RELOAD, kWKFrameNavigationTypeReload);
COMPILE_ASSERT_MATCHING_ENUM(EWK_NAVIGATION_TYPE_FORM_RESUBMITTED, kWKFrameNavigationTypeFormResubmitted);
COMPILE_ASSERT_MATCHING_ENUM(EWK_NAVIGATION_TYPE_OTHER, kWKFrameNavigationTypeOther);

// Event_Mouse_Button enum validation
COMPILE_ASSERT_MATCHING_ENUM(EVENT_MOUSE_BUTTON_NONE, kWKEventMouseButtonNoButton);
COMPILE_ASSERT_MATCHING_ENUM(EVENT_MOUSE_BUTTON_LEFT, kWKEventMouseButtonLeftButton);
COMPILE_ASSERT_MATCHING_ENUM(EVENT_MOUSE_BUTTON_MIDDLE, kWKEventMouseButtonMiddleButton);
COMPILE_ASSERT_MATCHING_ENUM(EVENT_MOUSE_BUTTON_RIGHT, kWKEventMouseButtonRightButton);

// Event_Modifier_Keys validation
COMPILE_ASSERT_MATCHING_ENUM(EVENT_MODIFIER_KEY_SHIFT, kWKEventModifiersShiftKey);
COMPILE_ASSERT_MATCHING_ENUM(EVENT_MODIFIER_KEY_CTRL, kWKEventModifiersControlKey);
COMPILE_ASSERT_MATCHING_ENUM(EVENT_MODIFIER_KEY_ALT, kWKEventModifiersAltKey);
COMPILE_ASSERT_MATCHING_ENUM(EVENT_MODIFIER_KEY_META, kWKEventModifiersMetaKey);

Ewk_Navigation_Policy_Decision* ewk_navigation_policy_decision_new(WKFrameNavigationType navigationType, WKEventMouseButton mouseButton, WKEventModifiers modifiers, WKURLRequestRef request, const char* frameName, WKFramePolicyListenerRef listener)
{
    return new Ewk_Navigation_Policy_Decision(listener,
                                              static_cast<Ewk_Navigation_Type>(navigationType),
                                              static_cast<Event_Mouse_Button>(mouseButton),
                                              static_cast<Event_Modifier_Keys>(modifiers),
                                              ewk_url_request_new(request),
                                              frameName);
}
