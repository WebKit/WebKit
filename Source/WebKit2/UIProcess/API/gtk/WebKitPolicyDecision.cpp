/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitPolicyDecision.h"

#include "WebKitPolicyDecisionPrivate.h"
#include "WebKitPrivate.h"


/**
 * SECTION: WebKitPolicyDecision
 * @Short_description: A pending policy decision
 * @Title: WebKitPolicyDecision
 * @See_also: #WebKitWebView
 *
 * Often WebKit allows the client to decide the policy for certain
 * operations. For instance, a client may want to open a link in a new
 * tab, block a navigation entirely, query the user or trigger a download
 * instead of a navigation. In these cases WebKit will fire the
 * #WebKitWebView::decide-policy signal with a #WebKitPolicyDecision
 * object. If the signal handler does nothing, WebKit will act as if
 * webkit_policy_decision_use() was called as soon as signal handling
 * completes. To make a policy decision asynchronously, simply increment
 * the reference count of the #WebKitPolicyDecision object.
 */
G_DEFINE_ABSTRACT_TYPE(WebKitPolicyDecision, webkit_policy_decision, G_TYPE_OBJECT)

struct _WebKitPolicyDecisionPrivate {
    WKRetainPtr<WKFramePolicyListenerRef> listener;
    bool madePolicyDecision;
};

static void webkit_policy_decision_init(WebKitPolicyDecision* decision)
{
    decision->priv = G_TYPE_INSTANCE_GET_PRIVATE(decision, WEBKIT_TYPE_POLICY_DECISION, WebKitPolicyDecisionPrivate);
    new (decision->priv) WebKitPolicyDecisionPrivate();
    decision->priv->madePolicyDecision = false;
}

static void webkitPolicyDecisionFinalize(GObject* object)
{
     WebKitPolicyDecisionPrivate* priv = WEBKIT_POLICY_DECISION(object)->priv;

    // This is the default choice for all policy decisions in WebPageProxy.cpp.
    if (!priv->madePolicyDecision)
        WKFramePolicyListenerUse(priv->listener.get());

    priv->~WebKitPolicyDecisionPrivate();
    G_OBJECT_CLASS(webkit_policy_decision_parent_class)->finalize(object);
}

void webkitPolicyDecisionSetListener(WebKitPolicyDecision* decision, WKFramePolicyListenerRef listener)
{
     decision->priv->listener = listener;
}

static void webkit_policy_decision_class_init(WebKitPolicyDecisionClass* decisionClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(decisionClass);
    objectClass->finalize = webkitPolicyDecisionFinalize;
    g_type_class_add_private(decisionClass, sizeof(WebKitPolicyDecisionPrivate));
}

/**
 * webkit_policy_decision_use:
 * @decision: a #WebKitPolicyDecision
 *
 * Accept the action which triggerd this decision.
 */
void webkit_policy_decision_use(WebKitPolicyDecision* decision)
{
    g_return_if_fail(WEBKIT_IS_POLICY_DECISION(decision));
    WKFramePolicyListenerUse(decision->priv->listener.get());
    decision->priv->madePolicyDecision = true;
}

/**
 * webkit_policy_decision_ignore:
 * @decision: a #WebKitPolicyDecision
 *
 * Ignore the action which triggerd this decision. For instance, for a
 * #WebKitResponsePolicyDecision, this would cancel the request.
 */
void webkit_policy_decision_ignore(WebKitPolicyDecision* decision)
{
    g_return_if_fail(WEBKIT_IS_POLICY_DECISION(decision));
    WKFramePolicyListenerIgnore(decision->priv->listener.get());
    decision->priv->madePolicyDecision = true;
}

/**
 * webkit_policy_decision_download:
 * @decision: a #WebKitPolicyDecision
 *
 * Spawn a download from this decision.
 */
void webkit_policy_decision_download(WebKitPolicyDecision* decision)
{
    g_return_if_fail(WEBKIT_IS_POLICY_DECISION(decision));
    WKFramePolicyListenerDownload(decision->priv->listener.get());
    decision->priv->madePolicyDecision = true;
}
