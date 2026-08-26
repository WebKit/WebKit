/*
 * Copyright (C) 2020 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#include "WebKitWebsitePolicies.h"

#include "APIWebsitePolicies.h"
#include "WebKitEnumTypes.h"
#include <WebCore/HTTPSByDefaultMode.h>
#include <glib/gi18n-lib.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

/**
 * WebKitWebsitePolicies:
 * @See_also: #WebKitWebView
 *
 * View specific website policies.
 *
 * WebKitWebsitePolicies allows you to configure per-page policies,
 * currently autoplay, custom user agent and upgrade to HTTPS policies
 * are supported.
 *
 * Since: 2.30
 */

using namespace WebKit;

enum {
    PROP_0,

    PROP_AUTOPLAY_POLICY,
    PROP_CUSTOM_USER_AGENT,
    PROP_UPGRADE_TO_HTTPS_POLICY,
};

struct _WebKitWebsitePoliciesPrivate {
    _WebKitWebsitePoliciesPrivate()
        : websitePolicies(API::WebsitePolicies::create())
    {
    }
    RefPtr<API::WebsitePolicies> websitePolicies;
    CString customUserAgent;
};

WEBKIT_DEFINE_FINAL_TYPE(WebKitWebsitePolicies, webkit_website_policies, G_TYPE_OBJECT, GObject)

API::WebsitePolicies& webkitWebsitePoliciesGetWebsitePolicies(WebKitWebsitePolicies* policies)
{
    return *policies->priv->websitePolicies.get();
}

static void webkitWebsitePoliciesGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebsitePolicies* policies = WEBKIT_WEBSITE_POLICIES(object);

    switch (propID) {
    case PROP_AUTOPLAY_POLICY:
        g_value_set_enum(value, webkit_website_policies_get_autoplay_policy(policies));
        break;
    case PROP_CUSTOM_USER_AGENT:
        g_value_set_string(value, webkit_website_policies_get_custom_user_agent(policies));
        break;
    case PROP_UPGRADE_TO_HTTPS_POLICY:
        g_value_set_enum(value, webkit_website_policies_get_upgrade_to_https_policy(policies));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

void webkitWebsitePoliciesSetAutoplayPolicy(WebKitWebsitePolicies* policies, WebKitAutoplayPolicy policy)
{
    g_return_if_fail(WEBKIT_IS_WEBSITE_POLICIES(policies));

    switch (policy) {
    case WEBKIT_AUTOPLAY_ALLOW:
        policies->priv->websitePolicies->setAutoplayPolicy(WebsiteAutoplayPolicy::Allow);
        break;
    case WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND:
        policies->priv->websitePolicies->setAutoplayPolicy(WebsiteAutoplayPolicy::AllowWithoutSound);
        break;
    case WEBKIT_AUTOPLAY_DENY:
        policies->priv->websitePolicies->setAutoplayPolicy(WebsiteAutoplayPolicy::Deny);
        break;
    }
}

static void webkitWebsitePoliciesSetUpgradeToHTTPSPolicy(WebKitWebsitePolicies* policies, WebKitUpgradeToHTTPSPolicy policy)
{
    switch (policy) {
    case WEBKIT_UPGRADE_TO_HTTPS_POLICY_KEEP_AS_REQUESTED:
        policies->priv->websitePolicies->setHTTPSByDefault(WebCore::HTTPSByDefaultMode::Disabled);
        break;
    case WEBKIT_UPGRADE_TO_HTTPS_POLICY_AUTOMATIC_FALLBACK_TO_HTTP:
        policies->priv->websitePolicies->setHTTPSByDefault(WebCore::HTTPSByDefaultMode::UpgradeWithAutomaticFallback);
        break;
    case WEBKIT_UPGRADE_TO_HTTPS_POLICY_ERROR_ON_FAILURE:
        policies->priv->websitePolicies->setHTTPSByDefault(WebCore::HTTPSByDefaultMode::UpgradeAndNoFallback);
        break;
    }
}

static void webkitWebsitePoliciesSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebsitePolicies* policies = WEBKIT_WEBSITE_POLICIES(object);

    switch (propID) {
    case PROP_AUTOPLAY_POLICY:
        webkitWebsitePoliciesSetAutoplayPolicy(policies, static_cast<WebKitAutoplayPolicy>(g_value_get_enum(value)));
        break;
    case PROP_CUSTOM_USER_AGENT:
        if (const auto* customUserAgent = g_value_get_string(value))
            policies->priv->websitePolicies->setCustomUserAgent(String::fromUTF8(customUserAgent));
        break;
    case PROP_UPGRADE_TO_HTTPS_POLICY:
        webkitWebsitePoliciesSetUpgradeToHTTPSPolicy(policies, static_cast<WebKitUpgradeToHTTPSPolicy>(g_value_get_enum(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void webkit_website_policies_class_init(WebKitWebsitePoliciesClass* findClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(findClass);

    gObjectClass->get_property = webkitWebsitePoliciesGetProperty;
    gObjectClass->set_property = webkitWebsitePoliciesSetProperty;

    /**
     * WebKitWebsitePolicies:autoplay:
     *
     * The #WebKitAutoplayPolicy of #WebKitWebsitePolicies.
     *
     * Since: 2.30
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_AUTOPLAY_POLICY,
        g_param_spec_enum(
            "autoplay",
            nullptr, nullptr,
            WEBKIT_TYPE_AUTOPLAY_POLICY,
            WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebsitePolicies:custom-user-agent: (getter get_custom_user_agent) (attributes org.gtk.Property.get=webkit_website_policies_get_custom_user_agent):
     *
     * The custom user agent string to send for navigations governed by these
     * #WebKitWebsitePolicies, or %NULL to use the default user agent.
     *
     * Since: 2.54
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_CUSTOM_USER_AGENT,
        g_param_spec_string(
            "custom-user-agent",
            nullptr, nullptr,
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebsitePolicies:upgrade-to-https-policy: (getter get_upgrade_to_https_policy):
     *
     * How HTTP navigations governed by these [class@WebsitePolicies] are
     * upgraded to HTTPS.
     *
     * Since: 2.56
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_UPGRADE_TO_HTTPS_POLICY,
        g_param_spec_enum(
            "upgrade-to-https-policy",
            nullptr, nullptr,
            WEBKIT_TYPE_UPGRADE_TO_HTTPS_POLICY,
            WEBKIT_UPGRADE_TO_HTTPS_POLICY_KEEP_AS_REQUESTED,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
}

/**
 * webkit_website_policies_new:
 *
 * Create a new #WebKitWebsitePolicies.
 *
 * Returns: (transfer full): the newly created #WebKitWebsitePolicies
 *
 * Since: 2.30
 */
WebKitWebsitePolicies* webkit_website_policies_new(void)
{
    return WEBKIT_WEBSITE_POLICIES(g_object_new(WEBKIT_TYPE_WEBSITE_POLICIES, nullptr));
}

/**
 * webkit_website_policies_new_with_policies:
 * @first_policy_name: name of the first policy to set
 * @...: value of first policy, followed by more policies, %NULL-terminated
 *
 * Create a new #WebKitWebsitePolicies with given policies.
 *
 * Create a new #WebKitWebsitePolicies with policies given as variadic
 * arguments.
 *
 * Returns: (transfer full): the newly created #WebKitWebsitePolicies
 *
 * ```c
 * WebKitWebsitePolicies *default_website_policies = webkit_website_policies_new_with_policies(
 *     "autoplay", WEBKIT_AUTOPLAY_DENY,
 *     "custom-user-agent", "Foo/1.0 (custom)",
 *     NULL);
 *
 * // ...
 *
 * WebKitWebView *view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
 *     "web-context", ctx,
 *     "settings", settings,
 *     "user-content-manager", content_manager,
 *     "website-policies", default_website_policies,
 *     NULL));
 *
 * // ...
 * ```
 *
 * Since: 2.30
 */
WebKitWebsitePolicies* webkit_website_policies_new_with_policies(const gchar* firstPolicyName, ...)
{
    va_list args;
    va_start(args, firstPolicyName);
    WebKitWebsitePolicies* policies = WEBKIT_WEBSITE_POLICIES(g_object_new_valist(WEBKIT_TYPE_WEBSITE_POLICIES, firstPolicyName, args));
    va_end(args);

    return policies;
}

/**
 * webkit_website_policies_get_autoplay_policy:
 * @policies: a #WebKitWebsitePolicies
 *
 * Get the #WebKitWebsitePolicies:autoplay property.
 *
 * Returns: #WebKitAutoplayPolicy
 *
 * Since: 2.30
 */
WebKitAutoplayPolicy webkit_website_policies_get_autoplay_policy(WebKitWebsitePolicies* policies)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_POLICIES(policies), WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND);

    auto apiAutoplayPolicyType = policies->priv->websitePolicies->autoplayPolicy();
    switch (apiAutoplayPolicyType) {
    case WebsiteAutoplayPolicy::Allow:
        return WEBKIT_AUTOPLAY_ALLOW;
    case WebsiteAutoplayPolicy::AllowWithoutSound:
        return WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND;
    case WebsiteAutoplayPolicy::Deny:
        return WEBKIT_AUTOPLAY_DENY;
    default:
        return WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND;
    }
}

/**
 * webkit_website_policies_get_custom_user_agent: (get-property custom-user-agent):
 * @policies: a #WebKitWebsitePolicies
 *
 * Get the #WebKitWebsitePolicies:custom-user-agent property.
 *
 * Returns: (transfer none) (nullable): the custom user agent string, or %NULL if the default
 *    user agent is used
 *
 * Since: 2.54
 */
const gchar* webkit_website_policies_get_custom_user_agent(WebKitWebsitePolicies* policies)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_POLICIES(policies), nullptr);

    if (policies->priv->customUserAgent.isNull()) {
        const auto& newCustomUserAgent = policies->priv->websitePolicies->customUserAgent();
        policies->priv->customUserAgent = newCustomUserAgent.isEmpty() ? CString() : newCustomUserAgent.utf8();
    }
    return policies->priv->customUserAgent.data();
}

/**
 * webkit_website_policies_get_upgrade_to_https_policy: (get-property upgrade-to-https-policy):
 * @policies: a #WebKitWebsitePolicies
 *
 * Get the [property@WebsitePolicies:upgrade-to-https-policy] property.
 *
 * Returns: a [enum@UpgradeToHTTPSPolicy]
 *
 * Since: 2.56
 */
WebKitUpgradeToHTTPSPolicy webkit_website_policies_get_upgrade_to_https_policy(WebKitWebsitePolicies* policies)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_POLICIES(policies), WEBKIT_UPGRADE_TO_HTTPS_POLICY_KEEP_AS_REQUESTED);

    switch (policies->priv->websitePolicies->httpsByDefaultMode()) {
    case WebCore::HTTPSByDefaultMode::Disabled:
        return WEBKIT_UPGRADE_TO_HTTPS_POLICY_KEEP_AS_REQUESTED;
    case WebCore::HTTPSByDefaultMode::UpgradeWithAutomaticFallback:
        return WEBKIT_UPGRADE_TO_HTTPS_POLICY_AUTOMATIC_FALLBACK_TO_HTTP;
    case WebCore::HTTPSByDefaultMode::UpgradeWithUserMediatedFallback:
    case WebCore::HTTPSByDefaultMode::UpgradeAndNoFallback:
        return WEBKIT_UPGRADE_TO_HTTPS_POLICY_ERROR_ON_FAILURE;
    }

    RELEASE_ASSERT_NOT_REACHED();
}
