/*
 * Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, 
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation and/or 
 * other materials provided with the distribution.
 *
 * Neither the name of Motorola Mobility, Inc. nor the names of its contributors may 
 * be used to endorse or promote products derived from this software without 
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitSettings.h"

#include "WebKitPrivate.h"
#include "WebKitSettingsPrivate.h"
#include <WebKit2/WKRetainPtr.h>
#include <glib/gi18n-lib.h>

struct _WebKitSettingsPrivate {
    WKRetainPtr<WKPreferencesRef> preferences;
};

/**
 * SECTION:WebKitSettings
 * @short_description: Control the behaviour of a #WebKitWebView
 *
 * #WebKitSettings can be applied to a #WebKitWebView to control text encoding, 
 * color, font sizes, printing mode, script support, loading of images and various other things. 
 * After creation, a #WebKitSettings object contains default settings. 
 *
 * <informalexample><programlisting>
 * /<!-- -->* Create a new #WebKitSettings and disable JavaScript. *<!-- -->/
 * WebKitSettings *settings = webkit_settings_new ();
 * g_object_set (G_OBJECT (settings), "enable-javascript", FALSE, NULL);
 *
 * webkit_web_view_set_settings (WEBKIT_WEB_VIEW (my_webview), settings);
 * </programlisting></informalexample>
 */


G_DEFINE_TYPE(WebKitSettings, webkit_settings, G_TYPE_OBJECT)

enum {
    PROP_0,

    PROP_ENABLE_JAVASCRIPT,
    PROP_AUTO_LOAD_IMAGES,
    PROP_LOAD_ICONS_IGNORING_IMAGE_LOAD_SETTING,
    PROP_ENABLE_OFFLINE_WEB_APPLICATION_CACHE,
    PROP_ENABLE_HTML5_LOCAL_STORAGE,
    PROP_ENABLE_HTML5_DATABASE,
    PROP_ENABLE_XSS_AUDITOR,
    PROP_ENABLE_FRAME_FLATTENING,
    PROP_ENABLE_PLUGINS,
    PROP_ENABLE_JAVA,
    PROP_JAVASCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY,
    PROP_ENABLE_HYPERLINK_AUDITING,
};

static void webKitSettingsSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitSettings* settings = WEBKIT_SETTINGS(object);

    switch (propId) {
    case PROP_ENABLE_JAVASCRIPT:
        webkit_settings_set_enable_javascript(settings, g_value_get_boolean(value));
        break;
    case PROP_AUTO_LOAD_IMAGES:
        webkit_settings_set_auto_load_images(settings, g_value_get_boolean(value));
        break;
    case PROP_LOAD_ICONS_IGNORING_IMAGE_LOAD_SETTING:
        webkit_settings_set_load_icons_ignoring_image_load_setting(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_OFFLINE_WEB_APPLICATION_CACHE:
        webkit_settings_set_enable_offline_web_application_cache(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_HTML5_LOCAL_STORAGE:
        webkit_settings_set_enable_html5_local_storage(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_HTML5_DATABASE:
        webkit_settings_set_enable_html5_database(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_XSS_AUDITOR:
        webkit_settings_set_enable_xss_auditor(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_FRAME_FLATTENING:
        webkit_settings_set_enable_frame_flattening(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_PLUGINS:
        webkit_settings_set_enable_plugins(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_JAVA:
        webkit_settings_set_enable_java(settings, g_value_get_boolean(value));
        break;
    case PROP_JAVASCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY:
        webkit_settings_set_javascript_can_open_windows_automatically(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_HYPERLINK_AUDITING:
        webkit_settings_set_enable_hyperlink_auditing(settings, g_value_get_boolean(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
        break;
    }
}

static void webKitSettingsGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitSettings* settings = WEBKIT_SETTINGS(object);

    switch (propId) {
    case PROP_ENABLE_JAVASCRIPT:
        g_value_set_boolean(value, webkit_settings_get_enable_javascript(settings));
        break;
    case PROP_AUTO_LOAD_IMAGES:
        g_value_set_boolean(value, webkit_settings_get_auto_load_images(settings));
        break;
    case PROP_LOAD_ICONS_IGNORING_IMAGE_LOAD_SETTING:
        g_value_set_boolean(value, webkit_settings_get_load_icons_ignoring_image_load_setting(settings));
        break;
    case PROP_ENABLE_OFFLINE_WEB_APPLICATION_CACHE:
        g_value_set_boolean(value, webkit_settings_get_enable_offline_web_application_cache(settings));
        break;
    case PROP_ENABLE_HTML5_LOCAL_STORAGE:
        g_value_set_boolean(value, webkit_settings_get_enable_html5_local_storage(settings));
        break;
    case PROP_ENABLE_HTML5_DATABASE:
        g_value_set_boolean(value, webkit_settings_get_enable_html5_database(settings));
        break;
    case PROP_ENABLE_XSS_AUDITOR:
        g_value_set_boolean(value, webkit_settings_get_enable_xss_auditor(settings));
        break;
    case PROP_ENABLE_FRAME_FLATTENING:
        g_value_set_boolean(value, webkit_settings_get_enable_frame_flattening(settings));
        break;
    case PROP_ENABLE_PLUGINS:
        g_value_set_boolean(value, webkit_settings_get_enable_plugins(settings));
        break;
    case PROP_ENABLE_JAVA:
        g_value_set_boolean(value, webkit_settings_get_enable_java(settings));
        break;
    case PROP_JAVASCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY:
        g_value_set_boolean(value, webkit_settings_get_javascript_can_open_windows_automatically(settings));
        break;
    case PROP_ENABLE_HYPERLINK_AUDITING:
        g_value_set_boolean(value, webkit_settings_get_enable_hyperlink_auditing(settings));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
        break;
    }
}

static void webKitSettingsFinalize(GObject* object)
{
    WEBKIT_SETTINGS(object)->priv->~WebKitSettingsPrivate();
    G_OBJECT_CLASS(webkit_settings_parent_class)->finalize(object);
}

static void webkit_settings_class_init(WebKitSettingsClass* klass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(klass);
    gObjectClass->set_property = webKitSettingsSetProperty;
    gObjectClass->get_property = webKitSettingsGetProperty;
    gObjectClass->finalize = webKitSettingsFinalize;

    GParamFlags readWriteConstructParamFlags = static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    /**
     * WebKitSettings:enable-javascript:
     *
     * Determines whether or not JavaScript executes within a page.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_JAVASCRIPT,
                                    g_param_spec_boolean("enable-javascript",
                                                         _("Enable JavaScript"),
                                                         _("Enable JavaScript."),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:auto-load-images:
     *
     * Determines whether images should be automatically loaded or not.
     * On devices where network bandwidth is of concern, it might be
     * useful to turn this property off.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_AUTO_LOAD_IMAGES,
                                    g_param_spec_boolean("auto-load-images",
                                                         _("Auto load images"),
                                                         _("Load images automatically."),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:load-icons-ignoring-image-load-setting:
     *
     * Determines whether a site can load favicons irrespective
     * of the value of #WebKitSettings:auto-load-images.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_LOAD_ICONS_IGNORING_IMAGE_LOAD_SETTING,
                                    g_param_spec_boolean("load-icons-ignoring-image-load-setting",
                                                         _("Load icons ignoring image load setting"),
                                                         _("Whether to load site icons ignoring image load setting."),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-offline-web-application-cache:
     *
     * Whether to enable HTML5 offline web application cache support. Offline
     * web application cache allows web applications to run even when
     * the user is not connected to the network.
     *
     * HTML5 offline web application specification is available at
     * http://dev.w3.org/html5/spec/offline.html.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_OFFLINE_WEB_APPLICATION_CACHE,
                                    g_param_spec_boolean("enable-offline-web-application-cache",
                                                         _("Enable offline web application cache"),
                                                         _("Whether to enable offline web application cache."),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-html5-local-storage:
     *
     * Whether to enable HTML5 local storage support. Local storage provides
     * simple synchronous storage access.
     *
     * HTML5 local storage specification is available at
     * http://dev.w3.org/html5/webstorage/.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_HTML5_LOCAL_STORAGE,
                                    g_param_spec_boolean("enable-html5-local-storage",
                                                         _("Enable HTML5 local storage"),
                                                         _("Whether to enable HTML5 Local Storage support."),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-html5-database:
     *
     * Whether to enable HTML5 client-side SQL database support. Client-side
     * SQL database allows web pages to store structured data and be able to
     * use SQL to manipulate that data asynchronously.
     *
     * HTML5 database specification is available at
     * http://www.w3.org/TR/webdatabase/.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_HTML5_DATABASE,
                                    g_param_spec_boolean("enable-html5-database",
                                                         _("Enable HTML5 database"),
                                                         _("Whether to enable HTML5 database support."),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-xss-auditor:
     *
     * Whether to enable the XSS auditor. This feature filters some kinds of
     * reflective XSS attacks on vulnerable web sites.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_XSS_AUDITOR,
                                    g_param_spec_boolean("enable-xss-auditor",
                                                         _("Enable XSS auditor"),
                                                         _("Whether to enable the XSS auditor."),
                                                         TRUE,
                                                         readWriteConstructParamFlags));


    /**
     * WebKitSettings:enable-frame-flattening:
     *
     * Whether to enable the frame flattening. With this setting each subframe is expanded
     * to its contents, which will flatten all the frames to become one scrollable page.
     * On touch devices scrollable subframes on a page can result in a confusing user experience.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_FRAME_FLATTENING,
                                    g_param_spec_boolean("enable-frame-flattening",
                                                         _("Enable frame flattening"),
                                                         _("Whether to enable frame flattening."),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-plugins:
     *
     * Determines whether or not plugins on the page are enabled.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_PLUGINS,
                                    g_param_spec_boolean("enable-plugins",
                                                         _("Enable plugins"),
                                                         _("Enable embedded plugin objects."),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-java:
     *
     * Determines whether or not Java is enabled on the page.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_JAVA,
                                    g_param_spec_boolean("enable-java",
                                                         _("Enable Java"),
                                                         _("Whether Java support should be enabled."),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:javascript-can-open-windows-automatically:
     *
     * Whether JavaScript can open popup windows automatically without user
     * intervention.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_JAVASCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY,
                                    g_param_spec_boolean("javascript-can-open-windows-automatically",
                                                         _("JavaScript can open windows automatically"),
                                                         _("Whether JavaScript can open windows automatically."),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-hyperlink-auditing:
     *
     * Determines whether or not hyperlink auditing is enabled.
     *
     * The hyperlink auditing specification is available at
     * http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#hyperlink-auditing.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_HYPERLINK_AUDITING,
                                    g_param_spec_boolean("enable-hyperlink-auditing",
                                                         _("Enable hyperlink auditing"),
                                                         _("Whether <a ping> should be able to send pings."),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    g_type_class_add_private(klass, sizeof(WebKitSettingsPrivate));
}

static void webkit_settings_init(WebKitSettings* settings)
{
    WebKitSettingsPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(settings, WEBKIT_TYPE_SETTINGS, WebKitSettingsPrivate);
    settings->priv = priv;
    new (priv) WebKitSettingsPrivate();

    priv->preferences = adoptWK(WKPreferencesCreate());
}

void webkitSettingsAttachSettingsToPage(WebKitSettings* settings, WKPageRef wkPage)
{
    WKPageGroupSetPreferences(WKPageGetPageGroup(wkPage), settings->priv->preferences.get());
}

/**
 * webkit_settings_new:
 *
 * Creates a new #WebKitSettings instance with default values. It must
 * be manually attached to a WebView.
 *
 * Returns: a new #WebKitSettings instance.
 */
WebKitSettings* webkit_settings_new()
{
    return WEBKIT_SETTINGS(g_object_new(WEBKIT_TYPE_SETTINGS, NULL));
}

/**
 * webkit_settings_get_enable_javascript:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-javascript property.
 *
 * Returns: %TRUE If JavaScript is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_javascript(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetJavaScriptEnabled(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_enable_javascript:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-javascript property.
 */
void webkit_settings_set_enable_javascript(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetJavaScriptEnabled(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetJavaScriptEnabled(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "enable-javascript");
}

/**
 * webkit_settings_get_auto_load_images:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:auto-load-images property.
 *
 * Returns: %TRUE If auto loading of images is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_auto_load_images(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetLoadsImagesAutomatically(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_auto_load_images:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:auto-load-images property.
 */
void webkit_settings_set_auto_load_images(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetLoadsImagesAutomatically(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetLoadsImagesAutomatically(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "auto-load-images");
}

/**
 * webkit_settings_get_load_icons_ignoring_image_load_setting:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:load-icons-ignoring-image-load-setting property.
 *
 * Returns: %TRUE If site icon can be loaded irrespective of image loading preference or %FALSE otherwise.
 */
gboolean webkit_settings_get_load_icons_ignoring_image_load_setting(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetLoadsSiteIconsIgnoringImageLoadingPreference(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_load_icons_ignoring_image_load_setting:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:load-icons-ignoring-image-load-setting property.
 */
void webkit_settings_set_load_icons_ignoring_image_load_setting(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetLoadsSiteIconsIgnoringImageLoadingPreference(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetLoadsSiteIconsIgnoringImageLoadingPreference(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "load-icons-ignoring-image-load-setting");
}

/**
 * webkit_settings_get_enable_offline_web_application_cache:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-offline-web-application-cache property.
 *
 * Returns: %TRUE If HTML5 offline web application cache support is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_offline_web_application_cache(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetOfflineWebApplicationCacheEnabled(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_enable_offline_web_application_cache:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-offline-web-application-cache property.
 */
void webkit_settings_set_enable_offline_web_application_cache(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetOfflineWebApplicationCacheEnabled(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetOfflineWebApplicationCacheEnabled(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "enable-offline-web-application-cache");
}

/**
 * webkit_settings_get_enable_html5_local_storage:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-html5-local-storage property.
 *
 * Returns: %TRUE If HTML5 local storage support is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_html5_local_storage(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetLocalStorageEnabled(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_enable_html5_local_storage:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-html5-local-storage property.
 */
void webkit_settings_set_enable_html5_local_storage(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetLocalStorageEnabled(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetLocalStorageEnabled(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "enable-html5-local-storage");
}

/**
 * webkit_settings_get_enable_html5_database:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-html5-database property.
 *
 * Returns: %TRUE If HTML5 database support is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_html5_database(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetDatabasesEnabled(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_enable_html5_database:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-html5-database property.
 */
void webkit_settings_set_enable_html5_database(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetDatabasesEnabled(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetDatabasesEnabled(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "enable-html5-database");
}

/**
 * webkit_settings_get_enable_xss_auditor:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-xss-auditor property.
 *
 * Returns: %TRUE If XSS auditing is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_xss_auditor(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetXSSAuditorEnabled(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_enable_xss_auditor:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-xss-auditor property.
 */
void webkit_settings_set_enable_xss_auditor(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetXSSAuditorEnabled(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetXSSAuditorEnabled(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "enable-xss-auditor");
}

/**
 * webkit_settings_get_enable_frame_flattening:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-frame-flattening property.
 *
 * Returns: %TRUE If frame flattening is enabled or %FALSE otherwise.
 *
 **/
gboolean webkit_settings_get_enable_frame_flattening(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetFrameFlatteningEnabled(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_enable_frame_flattening:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-frame-flattening property.
 */
void webkit_settings_set_enable_frame_flattening(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetFrameFlatteningEnabled(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetFrameFlatteningEnabled(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "enable-frame-flattening");
}

/**
 * webkit_settings_get_enable_plugins:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-plugins property.
 *
 * Returns: %TRUE If plugins are enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_plugins(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetPluginsEnabled(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_enable_plugins:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-plugins property.
 */
void webkit_settings_set_enable_plugins(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetPluginsEnabled(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetPluginsEnabled(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "enable-plugins");
}

/**
 * webkit_settings_get_enable_java:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-java property.
 *
 * Returns: %TRUE If Java is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_java(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetJavaEnabled(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_enable_java:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-java property.
 */
void webkit_settings_set_enable_java(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetJavaEnabled(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetJavaEnabled(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "enable-java");
}

/**
 * webkit_settings_get_javascript_can_open_windows_automatically:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:javascript-can-open-windows-automatically property.
 *
 * Returns: %TRUE If JavaScript can open window automatically or %FALSE otherwise.
 */
gboolean webkit_settings_get_javascript_can_open_windows_automatically(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetJavaScriptCanOpenWindowsAutomatically(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_javascript_can_open_windows_automatically:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:javascript-can-open-windows-automatically property.
 */
void webkit_settings_set_javascript_can_open_windows_automatically(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetJavaScriptCanOpenWindowsAutomatically(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetJavaScriptCanOpenWindowsAutomatically(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "javascript-can-open-windows-automatically");
}

/**
 * webkit_settings_get_enable_hyperlink_auditing:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-hyperlink-auditing property.
 *
 * Returns: %TRUE If hyper link auditing is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_hyperlink_auditing(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return WKPreferencesGetHyperlinkAuditingEnabled(settings->priv->preferences.get());
}

/**
 * webkit_settings_set_enable_hyperlink_auditing:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-hyperlink-auditing property.
 */
void webkit_settings_set_enable_hyperlink_auditing(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = WKPreferencesGetHyperlinkAuditingEnabled(priv->preferences.get());
    if (currentValue == enabled)
        return;

    WKPreferencesSetHyperlinkAuditingEnabled(priv->preferences.get(), enabled);
    g_object_notify(G_OBJECT(settings), "enable-hyperlink-auditing");
}
