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

#include "WebKitEnumTypes.h"
#include "WebKitSettingsPrivate.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include <WebCore/HTTPParsers.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/TextEncodingRegistry.h>
#include <WebCore/UserAgent.h>
#include <cmath>
#include <glib/gi18n-lib.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

#if PLATFORM(GTK)
#include "HardwareAccelerationManager.h"
#endif

#if PLATFORM(WAYLAND)
#include <WebCore/PlatformDisplay.h>
#endif

using namespace WebKit;

struct _WebKitSettingsPrivate {
    _WebKitSettingsPrivate()
        : preferences(WebPreferences::create(String(), "WebKit2.", "WebKit2."))
    {
        defaultFontFamily = preferences->standardFontFamily().utf8();
        monospaceFontFamily = preferences->fixedFontFamily().utf8();
        serifFontFamily = preferences->serifFontFamily().utf8();
        sansSerifFontFamily = preferences->sansSerifFontFamily().utf8();
        cursiveFontFamily = preferences->cursiveFontFamily().utf8();
        fantasyFontFamily = preferences->fantasyFontFamily().utf8();
        pictographFontFamily = preferences->pictographFontFamily().utf8();
        defaultCharset = preferences->defaultTextEncodingName().utf8();
    }

    RefPtr<WebPreferences> preferences;
    CString defaultFontFamily;
    CString monospaceFontFamily;
    CString serifFontFamily;
    CString sansSerifFontFamily;
    CString cursiveFontFamily;
    CString fantasyFontFamily;
    CString pictographFontFamily;
    CString defaultCharset;
    CString userAgent;
    CString mediaContentTypesRequiringHardwareSupport;
    bool allowModalDialogs { false };
    bool zoomTextOnly { false };
    double screenDpi { 96 };
#if PLATFORM(GTK)
    bool enableBackForwardNavigationGestures { false };
#endif
};

/**
 * SECTION:WebKitSettings
 * @short_description: Control the behaviour of a #WebKitWebView
 *
 * #WebKitSettings can be applied to a #WebKitWebView to control text charset,
 * color, font sizes, printing mode, script support, loading of images and various
 * other things on a #WebKitWebView. After creation, a #WebKitSettings object
 * contains default settings.
 *
 * <informalexample><programlisting>
 * /<!-- -->* Disable JavaScript. *<!-- -->/
 * WebKitSettings *settings = webkit_web_view_group_get_settings (my_view_group);
 * webkit_settings_set_enable_javascript (settings, FALSE);
 *
 * </programlisting></informalexample>
 */

WEBKIT_DEFINE_TYPE(WebKitSettings, webkit_settings, G_TYPE_OBJECT)

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
    PROP_DEFAULT_FONT_FAMILY,
    PROP_MONOSPACE_FONT_FAMILY,
    PROP_SERIF_FONT_FAMILY,
    PROP_SANS_SERIF_FONT_FAMILY,
    PROP_CURSIVE_FONT_FAMILY,
    PROP_FANTASY_FONT_FAMILY,
    PROP_PICTOGRAPH_FONT_FAMILY,
    PROP_DEFAULT_FONT_SIZE,
    PROP_DEFAULT_MONOSPACE_FONT_SIZE,
    PROP_MINIMUM_FONT_SIZE,
    PROP_DEFAULT_CHARSET,
#if PLATFORM(GTK)
    PROP_ENABLE_PRIVATE_BROWSING,
#endif
    PROP_ENABLE_DEVELOPER_EXTRAS,
    PROP_ENABLE_RESIZABLE_TEXT_AREAS,
    PROP_ENABLE_TABS_TO_LINKS,
    PROP_ENABLE_DNS_PREFETCHING,
    PROP_ENABLE_CARET_BROWSING,
    PROP_ENABLE_FULLSCREEN,
    PROP_PRINT_BACKGROUNDS,
    PROP_ENABLE_WEBAUDIO,
    PROP_ENABLE_WEBGL,
    PROP_ALLOW_MODAL_DIALOGS,
    PROP_ZOOM_TEXT_ONLY,
    PROP_JAVASCRIPT_CAN_ACCESS_CLIPBOARD,
    PROP_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE,
    PROP_MEDIA_PLAYBACK_ALLOWS_INLINE,
    PROP_DRAW_COMPOSITING_INDICATORS,
    PROP_ENABLE_SITE_SPECIFIC_QUIRKS,
    PROP_ENABLE_PAGE_CACHE,
    PROP_USER_AGENT,
    PROP_ENABLE_SMOOTH_SCROLLING,
    PROP_ENABLE_ACCELERATED_2D_CANVAS,
    PROP_ENABLE_WRITE_CONSOLE_MESSAGES_TO_STDOUT,
    PROP_ENABLE_MEDIA_STREAM,
    PROP_ENABLE_MOCK_CAPTURE_DEVICES,
    PROP_ENABLE_SPATIAL_NAVIGATION,
    PROP_ENABLE_MEDIASOURCE,
    PROP_ENABLE_ENCRYPTED_MEDIA,
    PROP_ENABLE_MEDIA_CAPABILITIES,
    PROP_ALLOW_FILE_ACCESS_FROM_FILE_URLS,
    PROP_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS,
    PROP_ALLOW_TOP_NAVIGATION_TO_DATA_URLS,
#if PLATFORM(GTK)
    PROP_HARDWARE_ACCELERATION_POLICY,
    PROP_ENABLE_BACK_FORWARD_NAVIGATION_GESTURES,
#endif
    PROP_ENABLE_JAVASCRIPT_MARKUP,
    PROP_ENABLE_MEDIA,
    PROP_MEDIA_CONTENT_TYPES_REQUIRING_HARDWARE_SUPPORT,
};

static void webKitSettingsDispose(GObject* object)
{
    WebCore::setScreenDPIObserverHandler(nullptr, object);
    G_OBJECT_CLASS(webkit_settings_parent_class)->dispose(object);
}

static void webKitSettingsConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_settings_parent_class)->constructed(object);

    WebKitSettings* settings = WEBKIT_SETTINGS(object);
    WebPreferences* prefs = settings->priv->preferences.get();
    prefs->setShouldRespectImageOrientation(true);

    bool mediaStreamEnabled = prefs->mediaStreamEnabled();
    prefs->setMediaDevicesEnabled(mediaStreamEnabled);
    prefs->setPeerConnectionEnabled(mediaStreamEnabled);

    settings->priv->screenDpi = WebCore::screenDPI();
    WebCore::setScreenDPIObserverHandler([settings]() {
        auto newScreenDpi = WebCore::screenDPI();
        if (newScreenDpi == settings->priv->screenDpi)
            return;

        auto scalingFactor = newScreenDpi / settings->priv->screenDpi;
        auto fontSize = settings->priv->preferences->defaultFontSize();
        auto monospaceFontSize = settings->priv->preferences->defaultFixedFontSize();
        settings->priv->screenDpi = newScreenDpi;

        g_object_freeze_notify(G_OBJECT(settings));
        webkit_settings_set_default_font_size(settings, std::round(fontSize * scalingFactor));
        webkit_settings_set_default_monospace_font_size(settings, std::round(monospaceFontSize * scalingFactor));
        g_object_thaw_notify(G_OBJECT(settings));
    }, object);
}

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
    case PROP_DEFAULT_FONT_FAMILY:
        webkit_settings_set_default_font_family(settings, g_value_get_string(value));
        break;
    case PROP_MONOSPACE_FONT_FAMILY:
        webkit_settings_set_monospace_font_family(settings, g_value_get_string(value));
        break;
    case PROP_SERIF_FONT_FAMILY:
        webkit_settings_set_serif_font_family(settings, g_value_get_string(value));
        break;
    case PROP_SANS_SERIF_FONT_FAMILY:
        webkit_settings_set_sans_serif_font_family(settings, g_value_get_string(value));
        break;
    case PROP_CURSIVE_FONT_FAMILY:
        webkit_settings_set_cursive_font_family(settings, g_value_get_string(value));
        break;
    case PROP_FANTASY_FONT_FAMILY:
        webkit_settings_set_fantasy_font_family(settings, g_value_get_string(value));
        break;
    case PROP_PICTOGRAPH_FONT_FAMILY:
        webkit_settings_set_pictograph_font_family(settings, g_value_get_string(value));
        break;
    case PROP_DEFAULT_FONT_SIZE:
        webkit_settings_set_default_font_size(settings, g_value_get_uint(value));
        break;
    case PROP_DEFAULT_MONOSPACE_FONT_SIZE:
        webkit_settings_set_default_monospace_font_size(settings, g_value_get_uint(value));
        break;
    case PROP_MINIMUM_FONT_SIZE:
        webkit_settings_set_minimum_font_size(settings, g_value_get_uint(value));
        break;
    case PROP_DEFAULT_CHARSET:
        webkit_settings_set_default_charset(settings, g_value_get_string(value));
        break;
#if PLATFORM(GTK)
    case PROP_ENABLE_PRIVATE_BROWSING:
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        if (g_value_get_boolean(value))
            webkit_settings_set_enable_private_browsing(settings, TRUE);
        G_GNUC_END_IGNORE_DEPRECATIONS;
        break;
#endif
    case PROP_ENABLE_DEVELOPER_EXTRAS:
        webkit_settings_set_enable_developer_extras(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_RESIZABLE_TEXT_AREAS:
        webkit_settings_set_enable_resizable_text_areas(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_TABS_TO_LINKS:
        webkit_settings_set_enable_tabs_to_links(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_DNS_PREFETCHING:
        webkit_settings_set_enable_dns_prefetching(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_CARET_BROWSING:
        webkit_settings_set_enable_caret_browsing(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_FULLSCREEN:
        webkit_settings_set_enable_fullscreen(settings, g_value_get_boolean(value));
        break;
    case PROP_PRINT_BACKGROUNDS:
        webkit_settings_set_print_backgrounds(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_WEBAUDIO:
        webkit_settings_set_enable_webaudio(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_WEBGL:
        webkit_settings_set_enable_webgl(settings, g_value_get_boolean(value));
        break;
    case PROP_ALLOW_MODAL_DIALOGS:
        webkit_settings_set_allow_modal_dialogs(settings, g_value_get_boolean(value));
        break;
    case PROP_ZOOM_TEXT_ONLY:
        webkit_settings_set_zoom_text_only(settings, g_value_get_boolean(value));
        break;
    case PROP_JAVASCRIPT_CAN_ACCESS_CLIPBOARD:
        webkit_settings_set_javascript_can_access_clipboard(settings, g_value_get_boolean(value));
        break;
    case PROP_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE:
        webkit_settings_set_media_playback_requires_user_gesture(settings, g_value_get_boolean(value));
        break;
    case PROP_MEDIA_PLAYBACK_ALLOWS_INLINE:
        webkit_settings_set_media_playback_allows_inline(settings, g_value_get_boolean(value));
        break;
    case PROP_DRAW_COMPOSITING_INDICATORS:
        if (g_value_get_boolean(value))
            webkit_settings_set_draw_compositing_indicators(settings, g_value_get_boolean(value));
        else {
            char* debugVisualsEnvironment = getenv("WEBKIT_SHOW_COMPOSITING_DEBUG_VISUALS");
            bool showDebugVisuals = debugVisualsEnvironment && !strcmp(debugVisualsEnvironment, "1");
            webkit_settings_set_draw_compositing_indicators(settings, showDebugVisuals);
        }
        break;
    case PROP_ENABLE_SITE_SPECIFIC_QUIRKS:
        webkit_settings_set_enable_site_specific_quirks(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_PAGE_CACHE:
        webkit_settings_set_enable_page_cache(settings, g_value_get_boolean(value));
        break;
    case PROP_USER_AGENT:
        webkit_settings_set_user_agent(settings, g_value_get_string(value));
        break;
    case PROP_ENABLE_SMOOTH_SCROLLING:
        webkit_settings_set_enable_smooth_scrolling(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_ACCELERATED_2D_CANVAS:
        webkit_settings_set_enable_accelerated_2d_canvas(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_WRITE_CONSOLE_MESSAGES_TO_STDOUT:
        webkit_settings_set_enable_write_console_messages_to_stdout(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_MEDIA_STREAM:
        webkit_settings_set_enable_media_stream(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_MOCK_CAPTURE_DEVICES:
        webkit_settings_set_enable_mock_capture_devices(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_SPATIAL_NAVIGATION:
        webkit_settings_set_enable_spatial_navigation(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_MEDIASOURCE:
        webkit_settings_set_enable_mediasource(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_ENCRYPTED_MEDIA:
        webkit_settings_set_enable_encrypted_media(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_MEDIA_CAPABILITIES:
        webkit_settings_set_enable_media_capabilities(settings, g_value_get_boolean(value));
        break;
    case PROP_ALLOW_FILE_ACCESS_FROM_FILE_URLS:
        webkit_settings_set_allow_file_access_from_file_urls(settings, g_value_get_boolean(value));
        break;
    case PROP_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS:
        webkit_settings_set_allow_universal_access_from_file_urls(settings, g_value_get_boolean(value));
        break;
    case PROP_ALLOW_TOP_NAVIGATION_TO_DATA_URLS:
        webkit_settings_set_allow_top_navigation_to_data_urls(settings, g_value_get_boolean(value));
        break;
#if PLATFORM(GTK)
    case PROP_HARDWARE_ACCELERATION_POLICY:
        webkit_settings_set_hardware_acceleration_policy(settings, static_cast<WebKitHardwareAccelerationPolicy>(g_value_get_enum(value)));
        break;
    case PROP_ENABLE_BACK_FORWARD_NAVIGATION_GESTURES:
        webkit_settings_set_enable_back_forward_navigation_gestures(settings, g_value_get_boolean(value));
        break;
#endif
    case PROP_ENABLE_JAVASCRIPT_MARKUP:
        webkit_settings_set_enable_javascript_markup(settings, g_value_get_boolean(value));
        break;
    case PROP_ENABLE_MEDIA:
        webkit_settings_set_enable_media(settings, g_value_get_boolean(value));
        break;
    case PROP_MEDIA_CONTENT_TYPES_REQUIRING_HARDWARE_SUPPORT:
        webkit_settings_set_media_content_types_requiring_hardware_support(settings, g_value_get_string(value));
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
    case PROP_DEFAULT_FONT_FAMILY:
        g_value_set_string(value, webkit_settings_get_default_font_family(settings));
        break;
    case PROP_MONOSPACE_FONT_FAMILY:
        g_value_set_string(value, webkit_settings_get_monospace_font_family(settings));
        break;
    case PROP_SERIF_FONT_FAMILY:
        g_value_set_string(value, webkit_settings_get_serif_font_family(settings));
        break;
    case PROP_SANS_SERIF_FONT_FAMILY:
        g_value_set_string(value, webkit_settings_get_sans_serif_font_family(settings));
        break;
    case PROP_CURSIVE_FONT_FAMILY:
        g_value_set_string(value, webkit_settings_get_cursive_font_family(settings));
        break;
    case PROP_FANTASY_FONT_FAMILY:
        g_value_set_string(value, webkit_settings_get_fantasy_font_family(settings));
        break;
    case PROP_PICTOGRAPH_FONT_FAMILY:
        g_value_set_string(value, webkit_settings_get_pictograph_font_family(settings));
        break;
    case PROP_DEFAULT_FONT_SIZE:
        g_value_set_uint(value, webkit_settings_get_default_font_size(settings));
        break;
    case PROP_DEFAULT_MONOSPACE_FONT_SIZE:
        g_value_set_uint(value, webkit_settings_get_default_monospace_font_size(settings));
        break;
    case PROP_MINIMUM_FONT_SIZE:
        g_value_set_uint(value, webkit_settings_get_minimum_font_size(settings));
        break;
    case PROP_DEFAULT_CHARSET:
        g_value_set_string(value, webkit_settings_get_default_charset(settings));
        break;
#if PLATFORM(GTK)
    case PROP_ENABLE_PRIVATE_BROWSING:
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        g_value_set_boolean(value, webkit_settings_get_enable_private_browsing(settings));
        G_GNUC_END_IGNORE_DEPRECATIONS;
        break;
#endif
    case PROP_ENABLE_DEVELOPER_EXTRAS:
        g_value_set_boolean(value, webkit_settings_get_enable_developer_extras(settings));
        break;
    case PROP_ENABLE_RESIZABLE_TEXT_AREAS:
        g_value_set_boolean(value, webkit_settings_get_enable_resizable_text_areas(settings));
        break;
    case PROP_ENABLE_TABS_TO_LINKS:
        g_value_set_boolean(value, webkit_settings_get_enable_tabs_to_links(settings));
        break;
    case PROP_ENABLE_DNS_PREFETCHING:
        g_value_set_boolean(value, webkit_settings_get_enable_dns_prefetching(settings));
        break;
    case PROP_ENABLE_CARET_BROWSING:
        g_value_set_boolean(value, webkit_settings_get_enable_caret_browsing(settings));
        break;
    case PROP_ENABLE_FULLSCREEN:
        g_value_set_boolean(value, webkit_settings_get_enable_fullscreen(settings));
        break;
    case PROP_PRINT_BACKGROUNDS:
        g_value_set_boolean(value, webkit_settings_get_print_backgrounds(settings));
        break;
    case PROP_ENABLE_WEBAUDIO:
        g_value_set_boolean(value, webkit_settings_get_enable_webaudio(settings));
        break;
    case PROP_ENABLE_WEBGL:
        g_value_set_boolean(value, webkit_settings_get_enable_webgl(settings));
        break;
    case PROP_ALLOW_MODAL_DIALOGS:
        g_value_set_boolean(value, webkit_settings_get_allow_modal_dialogs(settings));
        break;
    case PROP_ZOOM_TEXT_ONLY:
        g_value_set_boolean(value, webkit_settings_get_zoom_text_only(settings));
        break;
    case PROP_JAVASCRIPT_CAN_ACCESS_CLIPBOARD:
        g_value_set_boolean(value, webkit_settings_get_javascript_can_access_clipboard(settings));
        break;
    case PROP_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE:
        g_value_set_boolean(value, webkit_settings_get_media_playback_requires_user_gesture(settings));
        break;
    case PROP_MEDIA_PLAYBACK_ALLOWS_INLINE:
        g_value_set_boolean(value, webkit_settings_get_media_playback_allows_inline(settings));
        break;
    case PROP_DRAW_COMPOSITING_INDICATORS:
        g_value_set_boolean(value, webkit_settings_get_draw_compositing_indicators(settings));
        break;
    case PROP_ENABLE_SITE_SPECIFIC_QUIRKS:
        g_value_set_boolean(value, webkit_settings_get_enable_site_specific_quirks(settings));
        break;
    case PROP_ENABLE_PAGE_CACHE:
        g_value_set_boolean(value, webkit_settings_get_enable_page_cache(settings));
        break;
    case PROP_USER_AGENT:
        g_value_set_string(value, webkit_settings_get_user_agent(settings));
        break;
    case PROP_ENABLE_SMOOTH_SCROLLING:
        g_value_set_boolean(value, webkit_settings_get_enable_smooth_scrolling(settings));
        break;
    case PROP_ENABLE_ACCELERATED_2D_CANVAS:
        g_value_set_boolean(value, webkit_settings_get_enable_accelerated_2d_canvas(settings));
        break;
    case PROP_ENABLE_WRITE_CONSOLE_MESSAGES_TO_STDOUT:
        g_value_set_boolean(value, webkit_settings_get_enable_write_console_messages_to_stdout(settings));
        break;
    case PROP_ENABLE_MEDIA_STREAM:
        g_value_set_boolean(value, webkit_settings_get_enable_media_stream(settings));
        break;
    case PROP_ENABLE_MOCK_CAPTURE_DEVICES:
        g_value_set_boolean(value, webkit_settings_get_enable_mock_capture_devices(settings));
        break;
    case PROP_ENABLE_SPATIAL_NAVIGATION:
        g_value_set_boolean(value, webkit_settings_get_enable_spatial_navigation(settings));
        break;
    case PROP_ENABLE_MEDIASOURCE:
        g_value_set_boolean(value, webkit_settings_get_enable_mediasource(settings));
        break;
    case PROP_ENABLE_ENCRYPTED_MEDIA:
        g_value_set_boolean(value, webkit_settings_get_enable_encrypted_media(settings));
        break;
    case PROP_ENABLE_MEDIA_CAPABILITIES:
        g_value_set_boolean(value, webkit_settings_get_enable_media_capabilities(settings));
        break;
    case PROP_ALLOW_FILE_ACCESS_FROM_FILE_URLS:
        g_value_set_boolean(value, webkit_settings_get_allow_file_access_from_file_urls(settings));
        break;
    case PROP_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS:
        g_value_set_boolean(value, webkit_settings_get_allow_universal_access_from_file_urls(settings));
        break;
    case PROP_ALLOW_TOP_NAVIGATION_TO_DATA_URLS:
        g_value_set_boolean(value, webkit_settings_get_allow_top_navigation_to_data_urls(settings));
        break;
#if PLATFORM(GTK)
    case PROP_HARDWARE_ACCELERATION_POLICY:
        g_value_set_enum(value, webkit_settings_get_hardware_acceleration_policy(settings));
        break;
    case PROP_ENABLE_BACK_FORWARD_NAVIGATION_GESTURES:
        g_value_set_boolean(value, webkit_settings_get_enable_back_forward_navigation_gestures(settings));
        break;
#endif
    case PROP_ENABLE_JAVASCRIPT_MARKUP:
        g_value_set_boolean(value, webkit_settings_get_enable_javascript_markup(settings));
        break;
    case PROP_ENABLE_MEDIA:
        g_value_set_boolean(value, webkit_settings_get_enable_media(settings));
        break;
    case PROP_MEDIA_CONTENT_TYPES_REQUIRING_HARDWARE_SUPPORT:
        g_value_set_string(value, webkit_settings_get_media_content_types_requiring_hardware_support(settings));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
        break;
    }
}

static void webkit_settings_class_init(WebKitSettingsClass* klass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(klass);
    gObjectClass->constructed = webKitSettingsConstructed;
    gObjectClass->dispose = webKitSettingsDispose;
    gObjectClass->set_property = webKitSettingsSetProperty;
    gObjectClass->get_property = webKitSettingsGetProperty;

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
     * Whether to enable HTML5 client-side SQL database support (IndexedDB).
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
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:default-font-family:
     *
     * The font family to use as the default for content that does not specify a font.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_DEFAULT_FONT_FAMILY,
                                    g_param_spec_string("default-font-family",
                                                        _("Default font family"),
                                                        _("The font family to use as the default for content that does not specify a font."),
                                                        "sans-serif",
                                                        readWriteConstructParamFlags));

    /**
     * WebKitSettings:monospace-font-family:
     *
     * The font family used as the default for content using a monospace font.
     *
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_MONOSPACE_FONT_FAMILY,
                                    g_param_spec_string("monospace-font-family",
                                                        _("Monospace font family"),
                                                        _("The font family used as the default for content using monospace font."),
                                                        "monospace",
                                                        readWriteConstructParamFlags));

    /**
     * WebKitSettings:serif-font-family:
     *
     * The font family used as the default for content using a serif font.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_SERIF_FONT_FAMILY,
                                    g_param_spec_string("serif-font-family",
                                                        _("Serif font family"),
                                                        _("The font family used as the default for content using serif font."),
                                                        "serif",
                                                        readWriteConstructParamFlags));

    /**
     * WebKitSettings:sans-serif-font-family:
     *
     * The font family used as the default for content using a sans-serif font.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_SANS_SERIF_FONT_FAMILY,
                                    g_param_spec_string("sans-serif-font-family",
                                                        _("Sans-serif font family"),
                                                        _("The font family used as the default for content using sans-serif font."),
                                                        "sans-serif",
                                                        readWriteConstructParamFlags));

    /**
     * WebKitSettings:cursive-font-family:
     *
     * The font family used as the default for content using a cursive font.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_CURSIVE_FONT_FAMILY,
                                    g_param_spec_string("cursive-font-family",
                                                        _("Cursive font family"),
                                                        _("The font family used as the default for content using cursive font."),
                                                        "serif",
                                                        readWriteConstructParamFlags));

    /**
     * WebKitSettings:fantasy-font-family:
     *
     * The font family used as the default for content using a fantasy font.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_FANTASY_FONT_FAMILY,
                                    g_param_spec_string("fantasy-font-family",
                                                        _("Fantasy font family"),
                                                        _("The font family used as the default for content using fantasy font."),
                                                        "serif",
                                                        readWriteConstructParamFlags));

    /**
     * WebKitSettings:pictograph-font-family:
     *
     * The font family used as the default for content using a pictograph font.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_PICTOGRAPH_FONT_FAMILY,
                                    g_param_spec_string("pictograph-font-family",
                                                        _("Pictograph font family"),
                                                        _("The font family used as the default for content using pictograph font."),
                                                        "serif",
                                                        readWriteConstructParamFlags));

    /**
     * WebKitSettings:default-font-size:
     *
     * The default font size in pixels to use for content displayed if
     * no font size is specified.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_DEFAULT_FONT_SIZE,
                                    g_param_spec_uint("default-font-size",
                                                      _("Default font size"),
                                                      _("The default font size used to display text."),
                                                      0, G_MAXUINT, 16,
                                                      readWriteConstructParamFlags));

    /**
     * WebKitSettings:default-monospace-font-size:
     *
     * The default font size in pixels to use for content displayed in
     * monospace font if no font size is specified.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_DEFAULT_MONOSPACE_FONT_SIZE,
                                    g_param_spec_uint("default-monospace-font-size",
                                                      _("Default monospace font size"),
                                                      _("The default font size used to display monospace text."),
                                                      0, G_MAXUINT, 13,
                                                      readWriteConstructParamFlags));

    /**
     * WebKitSettings:minimum-font-size:
     *
     * The minimum font size in pixels used to display text. This setting
     * controls the absolute smallest size. Values other than 0 can
     * potentially break page layouts.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_MINIMUM_FONT_SIZE,
                                    g_param_spec_uint("minimum-font-size",
                                                      _("Minimum font size"),
                                                      _("The minimum font size used to display text."),
                                                      0, G_MAXUINT, 0,
                                                      readWriteConstructParamFlags));

    /**
     * WebKitSettings:default-charset:
     *
     * The default text charset used when interpreting content with an unspecified charset.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_DEFAULT_CHARSET,
                                    g_param_spec_string("default-charset",
                                                        _("Default charset"),
                                                        _("The default text charset used when interpreting content with unspecified charset."),
                                                        "iso-8859-1",
                                                        readWriteConstructParamFlags));

#if PLATFORM(GTK)
    /**
     * WebKitSettings:enable-private-browsing:
     *
     * Determines whether or not private browsing is enabled. Private browsing
     * will disable history, cache and form auto-fill for any pages visited.
     *
     * Deprecated: 2.16. Use #WebKitWebView:is-ephemeral or #WebKitWebsiteDataManager:is-ephemeral instead.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_PRIVATE_BROWSING,
                                    g_param_spec_boolean("enable-private-browsing",
                                                         _("Enable private browsing"),
                                                         _("Whether to enable private browsing"),
                                                         FALSE,
                                                         static_cast<GParamFlags>(readWriteConstructParamFlags | G_PARAM_DEPRECATED)));
#endif

    /**
     * WebKitSettings:enable-developer-extras:
     *
     * Determines whether or not developer tools, such as the Web Inspector, are enabled.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_DEVELOPER_EXTRAS,
                                    g_param_spec_boolean("enable-developer-extras",
                                                         _("Enable developer extras"),
                                                         _("Whether to enable developer extras"),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-resizable-text-areas:
     *
     * Determines whether or not text areas can be resized.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_RESIZABLE_TEXT_AREAS,
                                    g_param_spec_boolean("enable-resizable-text-areas",
                                                         _("Enable resizable text areas"),
                                                         _("Whether to enable resizable text areas"),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-tabs-to-links:
     *
     * Determines whether the tab key cycles through the elements on the page.
     * When this setting is enabled, users will be able to focus the next element
     * in the page by pressing the tab key. If the selected element is editable,
     * then pressing tab key will insert the tab character.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_TABS_TO_LINKS,
                                    g_param_spec_boolean("enable-tabs-to-links",
                                                         _("Enable tabs to links"),
                                                         _("Whether to enable tabs to links"),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-dns-prefetching:
     *
     * Determines whether or not to prefetch domain names. DNS prefetching attempts
     * to resolve domain names before a user tries to follow a link.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_DNS_PREFETCHING,
                                    g_param_spec_boolean("enable-dns-prefetching",
                                                         _("Enable DNS prefetching"),
                                                         _("Whether to enable DNS prefetching"),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-caret-browsing:
     *
     * Whether to enable accessibility enhanced keyboard navigation.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_CARET_BROWSING,
                                    g_param_spec_boolean("enable-caret-browsing",
                                                         _("Enable Caret Browsing"),
                                                         _("Whether to enable accessibility enhanced keyboard navigation"),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-fullscreen:
     *
     * Whether to enable the Javascript Fullscreen API. The API
     * allows any HTML element to request fullscreen display. See also
     * the current draft of the spec:
     * http://www.w3.org/TR/fullscreen/
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_FULLSCREEN,
        g_param_spec_boolean("enable-fullscreen",
            _("Enable Fullscreen"),
            _("Whether to enable the Javascript Fullscreen API"),
            TRUE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:print-backgrounds:
     *
     * Whether background images should be drawn during printing.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_PRINT_BACKGROUNDS,
                                    g_param_spec_boolean("print-backgrounds",
                                                         _("Print Backgrounds"),
                                                         _("Whether background images should be drawn during printing"),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-webaudio:
     *
     *
     * Enable or disable support for WebAudio on pages. WebAudio is an
     * API for processing and synthesizing audio in web applications
     *
     * See also https://webaudio.github.io/web-audio-api
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_ENABLE_WEBAUDIO,
        g_param_spec_boolean(
            "enable-webaudio",
            _("Enable WebAudio"),
            _("Whether WebAudio content should be handled"),
            TRUE,
            readWriteConstructParamFlags));

    /**
    * WebKitSettings:enable-webgl:
    *
    * Enable or disable support for WebGL on pages. WebGL enables web
    * content to use an API based on OpenGL ES 2.0.
    */
    g_object_class_install_property(
        gObjectClass,
        PROP_ENABLE_WEBGL,
        g_param_spec_boolean(
            "enable-webgl",
            _("Enable WebGL"),
            _("Whether WebGL content should be rendered"),
            TRUE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:allow-modal-dialogs:
     *
     * Determine whether it's allowed to create and run modal dialogs
     * from a #WebKitWebView through JavaScript with
     * <function>window.showModalDialog</function>. If it's set to
     * %FALSE, the associated #WebKitWebView won't be able to create
     * new modal dialogs, so not even the #WebKitWebView::create
     * signal will be emitted.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ALLOW_MODAL_DIALOGS,
                                    g_param_spec_boolean("allow-modal-dialogs",
                                                         _("Allow modal dialogs"),
                                                         _("Whether it is possible to create modal dialogs"),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:zoom-text-only:
     *
     * Whether #WebKitWebView:zoom-level affects only the
     * text of the page or all the contents. Other contents containing text
     * like form controls will be also affected by zoom factor when
     * this property is enabled.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ZOOM_TEXT_ONLY,
                                    g_param_spec_boolean("zoom-text-only",
                                                         _("Zoom Text Only"),
                                                         _("Whether zoom level of web view changes only the text size"),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:javascript-can-access-clipboard:
     *
     * Whether JavaScript can access the clipboard. The default value is %FALSE. If
     * set to %TRUE, document.execCommand() allows cut, copy and paste commands.
     *
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_JAVASCRIPT_CAN_ACCESS_CLIPBOARD,
                                    g_param_spec_boolean("javascript-can-access-clipboard",
                                                         _("JavaScript can access clipboard"),
                                                         _("Whether JavaScript can access Clipboard"),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:media-playback-requires-user-gesture:
     *
     * Whether a user gesture (such as clicking the play button)
     * would be required to start media playback or load media. This is off
     * by default, so media playback could start automatically.
     * Setting it on requires a gesture by the user to start playback, or to
     * load the media.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE,
                                    g_param_spec_boolean("media-playback-requires-user-gesture",
                                                         _("Media playback requires user gesture"),
                                                         _("Whether media playback requires user gesture"),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:media-playback-allows-inline:
     *
     * Whether media playback is full-screen only or inline playback is allowed.
     * This is %TRUE by default, so media playback can be inline. Setting it to
     * %FALSE allows specifying that media playback should be always fullscreen.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_MEDIA_PLAYBACK_ALLOWS_INLINE,
                                    g_param_spec_boolean("media-playback-allows-inline",
                                                         _("Media playback allows inline"),
                                                         _("Whether media playback allows inline"),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:draw-compositing-indicators:
     *
     * Whether to draw compositing borders and repaint counters on layers drawn
     * with accelerated compositing. This is useful for debugging issues related
     * to web content that is composited with the GPU.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_DRAW_COMPOSITING_INDICATORS,
                                    g_param_spec_boolean("draw-compositing-indicators",
                                                         _("Draw compositing indicators"),
                                                         _("Whether to draw compositing borders and repaint counters"),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-site-specific-quirks:
     *
     * Whether to turn on site-specific quirks. Turning this on will
     * tell WebKit to use some site-specific workarounds for
     * better web compatibility. For example, older versions of
     * MediaWiki will incorrectly send to WebKit a CSS file with KHTML
     * workarounds. By turning on site-specific quirks, WebKit will
     * special-case this and other cases to make some specific sites work.
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_ENABLE_SITE_SPECIFIC_QUIRKS,
        g_param_spec_boolean(
            "enable-site-specific-quirks",
            _("Enable Site Specific Quirks"),
            _("Enables the site-specific compatibility workarounds"),
            TRUE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-page-cache:
     *
     * Enable or disable the page cache. Disabling the page cache is
     * generally only useful for special circumstances like low-memory
     * scenarios or special purpose applications like static HTML
     * viewers. This setting only controls the Page Cache, this cache
     * is different than the disk-based or memory-based traditional
     * resource caches, its point is to make going back and forth
     * between pages much faster. For details about the different types
     * of caches and their purposes see:
     * http://webkit.org/blog/427/webkit-page-cache-i-the-basics/
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_PAGE_CACHE,
                                    g_param_spec_boolean("enable-page-cache",
                                                         _("Enable page cache"),
                                                         _("Whether the page cache should be used"),
                                                         TRUE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:user-agent:
     *
     * The user-agent string used by WebKit. Unusual user-agent strings may cause web
     * content to render incorrectly or fail to run, as many web pages are written to
     * parse the user-agent strings of only the most popular browsers. Therefore, it's
     * typically better to not completely override the standard user-agent, but to use
     * webkit_settings_set_user_agent_with_application_details() instead.
     *
     * If this property is set to the empty string or %NULL, it will revert to the standard
     * user-agent.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_USER_AGENT,
                                    g_param_spec_string("user-agent",
                                                        _("User agent string"),
                                                        _("The user agent string"),
                                                        0, // A null string forces the standard user agent.
                                                        readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-smooth-scrolling:
     *
     * Enable or disable smooth scrolling.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ENABLE_SMOOTH_SCROLLING,
                                    g_param_spec_boolean("enable-smooth-scrolling",
                                                         _("Enable smooth scrolling"),
                                                         _("Whether to enable smooth scrolling"),
                                                         FALSE,
                                                         readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-accelerated-2d-canvas:
     *
     * Enable or disable accelerated 2D canvas. Accelerated 2D canvas is only available
     * if WebKit was compiled with a version of Cairo including the unstable CairoGL API.
     * When accelerated 2D canvas is enabled, WebKit may render some 2D canvas content
     * using hardware accelerated drawing operations.
     *
     * Since: 2.2
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_ACCELERATED_2D_CANVAS,
        g_param_spec_boolean("enable-accelerated-2d-canvas",
            _("Enable accelerated 2D canvas"),
            _("Whether to enable accelerated 2D canvas"),
            FALSE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-write-console-messages-to-stdout:
     *
     * Enable or disable writing console messages to stdout. These are messages
     * sent to the console with console.log and related methods.
     *
     * Since: 2.2
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_WRITE_CONSOLE_MESSAGES_TO_STDOUT,
        g_param_spec_boolean("enable-write-console-messages-to-stdout",
            _("Write console messages on stdout"),
            _("Whether to write console messages on stdout"),
            FALSE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-media-stream:
     *
     * Enable or disable support for MediaStream on pages. MediaStream
     * is an experimental proposal for allowing web pages to access
     * audio and video devices for capture.
     *
     * See also http://dev.w3.org/2011/webrtc/editor/getusermedia.html
     *
     * Since: 2.4
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_MEDIA_STREAM,
        g_param_spec_boolean("enable-media-stream",
            _("Enable MediaStream"),
            _("Whether MediaStream content should be handled"),
            FALSE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-mock-capture-devices:
     *
     * Enable or disable the Mock Capture Devices. Those are fake
     * Microphone and Camera devices to be used as MediaStream
     * sources.
     *
     * Since: 2.24
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_MOCK_CAPTURE_DEVICES,
        g_param_spec_boolean("enable-mock-capture-devices",
            _("Enable mock capture devices"),
            _("Whether we expose mock capture devices or not"),
            FALSE,
            readWriteConstructParamFlags));

   /**
     * WebKitSettings:enable-spatial-navigation:
     *
     * Whether to enable Spatial Navigation. This feature consists in the ability
     * to navigate between focusable elements in a Web page, such as hyperlinks
     * and form controls, by using Left, Right, Up and Down arrow keys.
     * For example, if an user presses the Right key, heuristics determine whether
     * there is an element they might be trying to reach towards the right, and if
     * there are multiple elements, which element they probably wants.
     *
     * Since: 2.4
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_SPATIAL_NAVIGATION,
        g_param_spec_boolean("enable-spatial-navigation",
            _("Enable Spatial Navigation"),
            _("Whether to enable Spatial Navigation support."),
            FALSE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-mediasource:
     *
     * Enable or disable support for MediaSource on pages. MediaSource
     * extends HTMLMediaElement to allow JavaScript to generate media
     * streams for playback.
     *
     * See also http://www.w3.org/TR/media-source/
     *
     * Since: 2.4
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_MEDIASOURCE,
        g_param_spec_boolean("enable-mediasource",
            _("Enable MediaSource"),
            _("Whether MediaSource should be enabled."),
            TRUE,
            readWriteConstructParamFlags));


   /**
     * WebKitSettings:enable-encrypted-media:
     *
     * Enable or disable support for Encrypted Media API on pages.
     * EncryptedMedia is an experimental JavaScript API for playing encrypted media in HTML.
     * This property will only work as intended if the EncryptedMedia feature is enabled at build time
     * with the ENABLE_ENCRYPTED_MEDIA flag.
     *
     * See https://www.w3.org/TR/encrypted-media/
     *
     * Since: 2.20
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_ENCRYPTED_MEDIA,
        g_param_spec_boolean("enable-encrypted-media",
            _("Enable EncryptedMedia"),
            _("Whether EncryptedMedia should be enabled."),
            FALSE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-media-capabilities:
     *
     * Enable or disable support for MediaCapabilities on pages. This
     * specification intends to provide APIs to allow websites to make an optimal
     * decision when picking media content for the user. The APIs will expose
     * information about the decoding and encoding capabilities for a given format
     * but also output capabilities to find the best match based on the device’s
     * display.
     *
     * See also https://wicg.github.io/media-capabilities/
     *
     * Since: 2.22
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_MEDIA_CAPABILITIES,
        g_param_spec_boolean("enable-media-capabilities",
            _("Enable MediaCapabilities"),
            _("Whether MediaCapabilities should be enabled."),
            FALSE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:allow-file-access-from-file-urls:
     *
     * Whether file access is allowed from file URLs. By default, when
     * something is loaded in a #WebKitWebView using a file URI, cross
     * origin requests to other file resources are not allowed. This
     * setting allows you to change that behaviour, so that it would be
     * possible to do a XMLHttpRequest of a local file, for example.
     *
     * Since: 2.10
     */
    g_object_class_install_property(gObjectClass,
        PROP_ALLOW_FILE_ACCESS_FROM_FILE_URLS,
        g_param_spec_boolean("allow-file-access-from-file-urls",
            _("Allow file access from file URLs"),
            _("Whether file access is allowed from file URLs."),
            FALSE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:allow-universal-access-from-file-urls:
     *
     * Whether or not JavaScript running in the context of a file scheme URL
     * should be allowed to access content from any origin.  By default, when
     * something is loaded in a #WebKitWebView using a file scheme URL,
     * access to the local file system and arbitrary local storage is not
     * allowed. This setting allows you to change that behaviour, so that
     * it would be possible to use local storage, for example.
     *
     * Since: 2.14
     */
    g_object_class_install_property(gObjectClass,
        PROP_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS,
        g_param_spec_boolean("allow-universal-access-from-file-urls",
            _("Allow universal access from the context of file scheme URLs"),
            _("Whether or not universal access is allowed from the context of file scheme URLs"),
            FALSE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:allow-top-navigation-to-data-urls:
     *
     * Whether or not the top frame is allowed to navigate to data URLs. It is disabled by default
     * due to the risk it poses when loading untrusted URLs, with data URLs being used in scamming
     * and phishing attacks. In contrast, a scenario where it could be enabled could be an app that
     * embeds a WebView and you have control of the pages being show instead of a generic browser.
     *
     * Since: 2.28
     */
    g_object_class_install_property(gObjectClass,
        PROP_ALLOW_TOP_NAVIGATION_TO_DATA_URLS,
        g_param_spec_boolean("allow-top-navigation-to-data-urls",
            _("Allow top frame navigation to data URLs"),
            _("Whether or not top frame navigation is allowed to data URLs"),
            FALSE,
            readWriteConstructParamFlags));

#if PLATFORM(GTK)
    /**
     * WebKitSettings:hardware-acceleration-policy:
     *
     * The #WebKitHardwareAccelerationPolicy to decide how to enable and disable
     * hardware acceleration. The default value %WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND
     * enables the hardware acceleration when the web contents request it.
     * It's possible to enforce hardware acceleration to be always enabled
     * by using %WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS. And it's also possible to disable it
     * completely using %WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER. Note that disabling hardware
     * acceleration might cause some websites to not render correctly or consume more CPU.
     *
     * Note that changing this setting might not be possible if hardware acceleration is not
     * supported by the hardware or the system. In that case you can get the value to know the
     * actual policy being used, but changing the setting will not have any effect.
     *
     * Since: 2.16
     */
    g_object_class_install_property(gObjectClass,
        PROP_HARDWARE_ACCELERATION_POLICY,
        g_param_spec_enum("hardware-acceleration-policy",
            _("Hardware Acceleration Policy"),
            _("The policy to decide how to enable and disable hardware acceleration"),
            WEBKIT_TYPE_HARDWARE_ACCELERATION_POLICY,
            WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-back-forward-navigation-gestures:
     *
     * Enable or disable horizontal swipe gesture for back-forward navigation.
     *
     * Since: 2.24
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_BACK_FORWARD_NAVIGATION_GESTURES,
        g_param_spec_boolean("enable-back-forward-navigation-gestures",
            _("Enable back-forward navigation gestures"),
            _("Whether horizontal swipe gesture will trigger back-forward navigation"),
            FALSE,
            readWriteConstructParamFlags));
#endif // PLATFOTM(GTK)

    /**
     * WebKitSettings:enable-javascript-markup:
     *
     * Determines whether or not JavaScript markup is allowed in document. When this setting is disabled,
     * all JavaScript-related elements and attributes are removed from the document during parsing. Note that
     * executing JavaScript is still allowed if #WebKitSettings:enable-javascript is %TRUE.
     *
     * Since: 2.24
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_JAVASCRIPT_MARKUP,
        g_param_spec_boolean("enable-javascript-markup",
            _("Enable JavaScript Markup"),
            _("Enable JavaScript in document markup."),
            TRUE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:enable-media:
     *
     * Enable or disable support for media playback on pages. This setting is enabled by
     * default. Disabling it means `<audio>`, `<track>` and `<video>` elements will have
     * playback support disabled.
     *
     * Since: 2.26
     */
    g_object_class_install_property(gObjectClass,
        PROP_ENABLE_MEDIA,
        g_param_spec_boolean("enable-media",
            _("Enable media"),
            _("Whether media content should be handled"),
            TRUE,
            readWriteConstructParamFlags));

    /**
     * WebKitSettings:media-content-types-requiring-hardware-support:
     *
     * List of media content types requiring hardware support, split by semicolons (:).
     * For example: 'video/webm; codecs="vp*":video/mp4; codecs="avc*":video/&ast; codecs="av1*"'.
     *
     * Since: 2.30
     */
    g_object_class_install_property(gObjectClass,
        PROP_MEDIA_CONTENT_TYPES_REQUIRING_HARDWARE_SUPPORT,
        g_param_spec_string("media-content-types-requiring-hardware-support",
            _("Media content types requiring hardware support"),
            _("List of media content types requiring hardware support."),
            nullptr, // A null string forces the default value.
            readWriteConstructParamFlags));
}

WebPreferences* webkitSettingsGetPreferences(WebKitSettings* settings)
{
    return settings->priv->preferences.get();
}

/**
 * webkit_settings_new:
 *
 * Creates a new #WebKitSettings instance with default values. It must
 * be manually attached to a #WebKitWebView.
 * See also webkit_settings_new_with_settings().
 *
 * Returns: a new #WebKitSettings instance.
 */
WebKitSettings* webkit_settings_new()
{
    return WEBKIT_SETTINGS(g_object_new(WEBKIT_TYPE_SETTINGS, NULL));
}

/**
 * webkit_settings_new_with_settings:
 * @first_setting_name: name of first setting to set
 * @...: value of first setting, followed by more settings,
 *    %NULL-terminated
 *
 * Creates a new #WebKitSettings instance with the given settings. It must
 * be manually attached to a #WebKitWebView.
 *
 * Returns: a new #WebKitSettings instance.
 */
WebKitSettings* webkit_settings_new_with_settings(const gchar* firstSettingName, ...)
{
    va_list args;
    va_start(args, firstSettingName);
    WebKitSettings* settings = WEBKIT_SETTINGS(g_object_new_valist(WEBKIT_TYPE_SETTINGS, firstSettingName, args));
    va_end(args);
    return settings;
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

    return settings->priv->preferences->javaScriptEnabled();
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
    bool currentValue = priv->preferences->javaScriptEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setJavaScriptEnabled(enabled);
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

    return settings->priv->preferences->loadsImagesAutomatically();
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
    bool currentValue = priv->preferences->loadsImagesAutomatically();
    if (currentValue == enabled)
        return;

    priv->preferences->setLoadsImagesAutomatically(enabled);
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

    return settings->priv->preferences->loadsSiteIconsIgnoringImageLoadingPreference();
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
    bool currentValue = priv->preferences->loadsSiteIconsIgnoringImageLoadingPreference();
    if (currentValue == enabled)
        return;

    priv->preferences->setLoadsSiteIconsIgnoringImageLoadingPreference(enabled);
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

    return settings->priv->preferences->offlineWebApplicationCacheEnabled();
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
    bool currentValue = priv->preferences->offlineWebApplicationCacheEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setOfflineWebApplicationCacheEnabled(enabled);
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

    return settings->priv->preferences->localStorageEnabled();
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
    bool currentValue = priv->preferences->localStorageEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setLocalStorageEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-html5-local-storage");
}

/**
 * webkit_settings_get_enable_html5_database:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-html5-database property.
 *
 * Returns: %TRUE if IndexedDB support is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_html5_database(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->databasesEnabled();
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
    bool currentValue = priv->preferences->databasesEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setDatabasesEnabled(enabled);
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

    return settings->priv->preferences->xssAuditorEnabled();
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
    bool currentValue = priv->preferences->xssAuditorEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setXSSAuditorEnabled(enabled);
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

    return settings->priv->preferences->frameFlatteningEnabled();
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
    if (priv->preferences->frameFlatteningEnabled() == enabled)
        return;

    priv->preferences->setFrameFlatteningEnabled(enabled);
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

    return settings->priv->preferences->pluginsEnabled();
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
    bool currentValue = priv->preferences->pluginsEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setPluginsEnabled(enabled);
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

    return settings->priv->preferences->javaEnabled();
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
    bool currentValue = priv->preferences->javaEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setJavaEnabled(enabled);
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

    return settings->priv->preferences->javaScriptCanOpenWindowsAutomatically();
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
    bool currentValue = priv->preferences->javaScriptCanOpenWindowsAutomatically();
    if (currentValue == enabled)
        return;

    priv->preferences->setJavaScriptCanOpenWindowsAutomatically(enabled);
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

    return settings->priv->preferences->hyperlinkAuditingEnabled();
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
    bool currentValue = priv->preferences->hyperlinkAuditingEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setHyperlinkAuditingEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-hyperlink-auditing");
}

/**
 * webkit_web_settings_get_default_font_family:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:default-font-family property.
 *
 * Returns: The default font family used to display content that does not specify a font.
 */
const gchar* webkit_settings_get_default_font_family(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    return settings->priv->defaultFontFamily.data();
}

/**
 * webkit_settings_set_default_font_family:
 * @settings: a #WebKitSettings
 * @default_font_family: the new default font family
 *
 * Set the #WebKitSettings:default-font-family property.
 */
void webkit_settings_set_default_font_family(WebKitSettings* settings, const gchar* defaultFontFamily)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));
    g_return_if_fail(defaultFontFamily);

    WebKitSettingsPrivate* priv = settings->priv;
    if (!g_strcmp0(priv->defaultFontFamily.data(), defaultFontFamily))
        return;

    String standardFontFamily = String::fromUTF8(defaultFontFamily);
    priv->preferences->setStandardFontFamily(standardFontFamily);
    priv->defaultFontFamily = standardFontFamily.utf8();
    g_object_notify(G_OBJECT(settings), "default-font-family");
}

/**
 * webkit_settings_get_monospace_font_family:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:monospace-font-family property.
 *
 * Returns: Default font family used to display content marked with monospace font.
 */
const gchar* webkit_settings_get_monospace_font_family(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    return settings->priv->monospaceFontFamily.data();
}

/**
 * webkit_settings_set_monospace_font_family:
 * @settings: a #WebKitSettings
 * @monospace_font_family: the new default monospace font family
 *
 * Set the #WebKitSettings:monospace-font-family property.
 */
void webkit_settings_set_monospace_font_family(WebKitSettings* settings, const gchar* monospaceFontFamily)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));
    g_return_if_fail(monospaceFontFamily);

    WebKitSettingsPrivate* priv = settings->priv;
    if (!g_strcmp0(priv->monospaceFontFamily.data(), monospaceFontFamily))
        return;

    String fixedFontFamily = String::fromUTF8(monospaceFontFamily);
    priv->preferences->setFixedFontFamily(fixedFontFamily);
    priv->monospaceFontFamily = fixedFontFamily.utf8();
    g_object_notify(G_OBJECT(settings), "monospace-font-family");
}

/**
 * webkit_settings_get_serif_font_family:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:serif-font-family property.
 *
 * Returns: The default font family used to display content marked with serif font.
 */
const gchar* webkit_settings_get_serif_font_family(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    return settings->priv->serifFontFamily.data();
}

/**
 * webkit_settings_set_serif_font_family:
 * @settings: a #WebKitSettings
 * @serif_font_family: the new default serif font family
 *
 * Set the #WebKitSettings:serif-font-family property.
 */
void webkit_settings_set_serif_font_family(WebKitSettings* settings, const gchar* serifFontFamily)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));
    g_return_if_fail(serifFontFamily);

    WebKitSettingsPrivate* priv = settings->priv;
    if (!g_strcmp0(priv->serifFontFamily.data(), serifFontFamily))
        return;

    String serifFontFamilyString = String::fromUTF8(serifFontFamily);
    priv->preferences->setSerifFontFamily(serifFontFamilyString);
    priv->serifFontFamily = serifFontFamilyString.utf8();
    g_object_notify(G_OBJECT(settings), "serif-font-family");
}

/**
 * webkit_settings_get_sans_serif_font_family:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:sans-serif-font-family property.
 *
 * Returns: The default font family used to display content marked with sans-serif font.
 */
const gchar* webkit_settings_get_sans_serif_font_family(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    return settings->priv->sansSerifFontFamily.data();
}

/**
 * webkit_settings_set_sans_serif_font_family:
 * @settings: a #WebKitSettings
 * @sans_serif_font_family: the new default sans-serif font family
 *
 * Set the #WebKitSettings:sans-serif-font-family property.
 */
void webkit_settings_set_sans_serif_font_family(WebKitSettings* settings, const gchar* sansSerifFontFamily)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));
    g_return_if_fail(sansSerifFontFamily);

    WebKitSettingsPrivate* priv = settings->priv;
    if (!g_strcmp0(priv->sansSerifFontFamily.data(), sansSerifFontFamily))
        return;

    String sansSerifFontFamilyString = String::fromUTF8(sansSerifFontFamily);
    priv->preferences->setSansSerifFontFamily(sansSerifFontFamilyString);
    priv->sansSerifFontFamily = sansSerifFontFamilyString.utf8();
    g_object_notify(G_OBJECT(settings), "sans-serif-font-family");
}

/**
 * webkit_settings_get_cursive_font_family:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:cursive-font-family property.
 *
 * Returns: The default font family used to display content marked with cursive font.
 */
const gchar* webkit_settings_get_cursive_font_family(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    return settings->priv->cursiveFontFamily.data();
}

/**
 * webkit_settings_set_cursive_font_family:
 * @settings: a #WebKitSettings
 * @cursive_font_family: the new default cursive font family
 *
 * Set the #WebKitSettings:cursive-font-family property.
 */
void webkit_settings_set_cursive_font_family(WebKitSettings* settings, const gchar* cursiveFontFamily)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));
    g_return_if_fail(cursiveFontFamily);

    WebKitSettingsPrivate* priv = settings->priv;
    if (!g_strcmp0(priv->cursiveFontFamily.data(), cursiveFontFamily))
        return;

    String cursiveFontFamilyString = String::fromUTF8(cursiveFontFamily);
    priv->preferences->setCursiveFontFamily(cursiveFontFamilyString);
    priv->cursiveFontFamily = cursiveFontFamilyString.utf8();
    g_object_notify(G_OBJECT(settings), "cursive-font-family");
}

/**
 * webkit_settings_get_fantasy_font_family:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:fantasy-font-family property.
 *
 * Returns: The default font family used to display content marked with fantasy font.
 */
const gchar* webkit_settings_get_fantasy_font_family(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    return settings->priv->fantasyFontFamily.data();
}

/**
 * webkit_settings_set_fantasy_font_family:
 * @settings: a #WebKitSettings
 * @fantasy_font_family: the new default fantasy font family
 *
 * Set the #WebKitSettings:fantasy-font-family property.
 */
void webkit_settings_set_fantasy_font_family(WebKitSettings* settings, const gchar* fantasyFontFamily)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));
    g_return_if_fail(fantasyFontFamily);

    WebKitSettingsPrivate* priv = settings->priv;
    if (!g_strcmp0(priv->fantasyFontFamily.data(), fantasyFontFamily))
        return;

    String fantasyFontFamilyString = String::fromUTF8(fantasyFontFamily);
    priv->preferences->setFantasyFontFamily(fantasyFontFamilyString);
    priv->fantasyFontFamily = fantasyFontFamilyString.utf8();
    g_object_notify(G_OBJECT(settings), "fantasy-font-family");
}

/**
 * webkit_settings_get_pictograph_font_family:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:pictograph-font-family property.
 *
 * Returns: The default font family used to display content marked with pictograph font.
 */
const gchar* webkit_settings_get_pictograph_font_family(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    return settings->priv->pictographFontFamily.data();
}

/**
 * webkit_settings_set_pictograph_font_family:
 * @settings: a #WebKitSettings
 * @pictograph_font_family: the new default pictograph font family
 *
 * Set the #WebKitSettings:pictograph-font-family property.
 */
void webkit_settings_set_pictograph_font_family(WebKitSettings* settings, const gchar* pictographFontFamily)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));
    g_return_if_fail(pictographFontFamily);

    WebKitSettingsPrivate* priv = settings->priv;
    if (!g_strcmp0(priv->pictographFontFamily.data(), pictographFontFamily))
        return;

    String pictographFontFamilyString = String::fromUTF8(pictographFontFamily);
    priv->preferences->setPictographFontFamily(pictographFontFamilyString);
    priv->pictographFontFamily = pictographFontFamilyString.utf8();
    g_object_notify(G_OBJECT(settings), "pictograph-font-family");
}

/**
 * webkit_settings_get_default_font_size:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:default-font-size property.
 *
 * Returns: The default font size, in pixels.
 */
guint32 webkit_settings_get_default_font_size(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    return settings->priv->preferences->defaultFontSize();
}

/**
 * webkit_settings_set_default_font_size:
 * @settings: a #WebKitSettings
 * @font_size: default font size to be set in pixels
 *
 * Set the #WebKitSettings:default-font-size property.
 */
void webkit_settings_set_default_font_size(WebKitSettings* settings, guint32 fontSize)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    uint32_t currentSize = priv->preferences->defaultFontSize();
    if (currentSize == fontSize)
        return;

    priv->preferences->setDefaultFontSize(fontSize);
    g_object_notify(G_OBJECT(settings), "default-font-size");
}

/**
 * webkit_settings_get_default_monospace_font_size:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:default-monospace-font-size property.
 *
 * Returns: Default monospace font size, in pixels.
 */
guint32 webkit_settings_get_default_monospace_font_size(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    return settings->priv->preferences->defaultFixedFontSize();
}

/**
 * webkit_settings_set_default_monospace_font_size:
 * @settings: a #WebKitSettings
 * @font_size: default monospace font size to be set in pixels
 *
 * Set the #WebKitSettings:default-monospace-font-size property.
 */
void webkit_settings_set_default_monospace_font_size(WebKitSettings* settings, guint32 fontSize)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    uint32_t currentSize = priv->preferences->defaultFixedFontSize();
    if (currentSize == fontSize)
        return;

    priv->preferences->setDefaultFixedFontSize(fontSize);
    g_object_notify(G_OBJECT(settings), "default-monospace-font-size");
}

/**
 * webkit_settings_get_minimum_font_size:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:minimum-font-size property.
 *
 * Returns: Minimum font size, in pixels.
 */
guint32 webkit_settings_get_minimum_font_size(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    return settings->priv->preferences->minimumFontSize();
}

/**
 * webkit_settings_set_minimum_font_size:
 * @settings: a #WebKitSettings
 * @font_size: minimum font size to be set in pixels
 *
 * Set the #WebKitSettings:minimum-font-size property.
 */
void webkit_settings_set_minimum_font_size(WebKitSettings* settings, guint32 fontSize)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    uint32_t currentSize = priv->preferences->minimumFontSize();
    if (currentSize == fontSize)
        return;

    priv->preferences->setMinimumFontSize(fontSize);
    g_object_notify(G_OBJECT(settings), "minimum-font-size");
}

/**
 * webkit_settings_get_default_charset:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:default-charset property.
 *
 * Returns: Default charset.
 */
const gchar* webkit_settings_get_default_charset(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    return settings->priv->defaultCharset.data();
}

/**
 * webkit_settings_set_default_charset:
 * @settings: a #WebKitSettings
 * @default_charset: default charset to be set
 *
 * Set the #WebKitSettings:default-charset property.
 */
void webkit_settings_set_default_charset(WebKitSettings* settings, const gchar* defaultCharset)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));
    g_return_if_fail(defaultCharset);

    WebKitSettingsPrivate* priv = settings->priv;
    if (!g_strcmp0(priv->defaultCharset.data(), defaultCharset))
        return;

    String defaultCharsetString = String::fromUTF8(defaultCharset);
    priv->preferences->setDefaultTextEncodingName(defaultCharsetString);
    priv->defaultCharset = defaultCharsetString.utf8();
    g_object_notify(G_OBJECT(settings), "default-charset");
}

#if PLATFORM(GTK)
/**
 * webkit_settings_get_enable_private_browsing:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-private-browsing property.
 *
 * Returns: %TRUE If private browsing is enabled or %FALSE otherwise.
 *
 * Deprecated: 2.16. Use #WebKitWebView:is-ephemeral or #WebKitWebContext:is-ephemeral instead.
 */
gboolean webkit_settings_get_enable_private_browsing(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return FALSE;
}

/**
 * webkit_settings_set_enable_private_browsing:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-private-browsing property.
 *
 * Deprecated: 2.16. Use #WebKitWebView:is-ephemeral or #WebKitWebContext:is-ephemeral instead.
 */
void webkit_settings_set_enable_private_browsing(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    g_warning("webkit_settings_set_enable_private_browsing is deprecated and does nothing, use #WebKitWebView:is-ephemeral or #WebKitWebContext:is-ephemeral instead");
}
#endif

/**
 * webkit_settings_get_enable_developer_extras:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-developer-extras property.
 *
 * Returns: %TRUE If developer extras is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_developer_extras(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->developerExtrasEnabled();
}

/**
 * webkit_settings_set_enable_developer_extras:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-developer-extras property.
 */
void webkit_settings_set_enable_developer_extras(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->developerExtrasEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setDeveloperExtrasEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-developer-extras");
}

/**
 * webkit_settings_get_enable_resizable_text_areas:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-resizable-text-areas property.
 *
 * Returns: %TRUE If text areas can be resized or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_resizable_text_areas(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->textAreasAreResizable();
}

/**
 * webkit_settings_set_enable_resizable_text_areas:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-resizable-text-areas property.
 */
void webkit_settings_set_enable_resizable_text_areas(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->textAreasAreResizable();
    if (currentValue == enabled)
        return;

    priv->preferences->setTextAreasAreResizable(enabled);
    g_object_notify(G_OBJECT(settings), "enable-resizable-text-areas");
}

/**
 * webkit_settings_get_enable_tabs_to_links:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-tabs-to-links property.
 *
 * Returns: %TRUE If tabs to link is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_tabs_to_links(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->tabsToLinks();
}

/**
 * webkit_settings_set_enable_tabs_to_links:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-tabs-to-links property.
 */
void webkit_settings_set_enable_tabs_to_links(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->tabsToLinks();
    if (currentValue == enabled)
        return;

    priv->preferences->setTabsToLinks(enabled);
    g_object_notify(G_OBJECT(settings), "enable-tabs-to-links");
}

/**
 * webkit_settings_get_enable_dns_prefetching:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-dns-prefetching property.
 *
 * Returns: %TRUE If DNS prefetching is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_dns_prefetching(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->dnsPrefetchingEnabled();
}

/**
 * webkit_settings_set_enable_dns_prefetching:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-dns-prefetching property.
 */
void webkit_settings_set_enable_dns_prefetching(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->dnsPrefetchingEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setDNSPrefetchingEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-dns-prefetching");
}

/**
 * webkit_settings_get_enable_caret_browsing:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-caret-browsing property.
 *
 * Returns: %TRUE If caret browsing is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_caret_browsing(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->caretBrowsingEnabled();
}

/**
 * webkit_settings_set_enable_caret_browsing:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-caret-browsing property.
 */
void webkit_settings_set_enable_caret_browsing(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->caretBrowsingEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setCaretBrowsingEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-caret-browsing");
}

/**
 * webkit_settings_get_enable_fullscreen:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-fullscreen property.
 *
 * Returns: %TRUE If fullscreen support is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_fullscreen(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->fullScreenEnabled();
}

/**
 * webkit_settings_set_enable_fullscreen:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-fullscreen property.
 */
void webkit_settings_set_enable_fullscreen(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->fullScreenEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setFullScreenEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-fullscreen");
}

/**
 * webkit_settings_get_print_backgrounds:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:print-backgrounds property.
 *
 * Returns: %TRUE If background images should be printed or %FALSE otherwise.
 */
gboolean webkit_settings_get_print_backgrounds(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->shouldPrintBackgrounds();
}

/**
 * webkit_settings_set_print_backgrounds:
 * @settings: a #WebKitSettings
 * @print_backgrounds: Value to be set
 *
 * Set the #WebKitSettings:print-backgrounds property.
 */
void webkit_settings_set_print_backgrounds(WebKitSettings* settings, gboolean printBackgrounds)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->shouldPrintBackgrounds();
    if (currentValue == printBackgrounds)
        return;

    priv->preferences->setShouldPrintBackgrounds(printBackgrounds);
    g_object_notify(G_OBJECT(settings), "print-backgrounds");
}

/**
 * webkit_settings_get_enable_webaudio:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-webaudio property.
 *
 * Returns: %TRUE If webaudio support is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_webaudio(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->webAudioEnabled();
}

/**
 * webkit_settings_set_enable_webaudio:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-webaudio property.
 */
void webkit_settings_set_enable_webaudio(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->webAudioEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setWebAudioEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-webaudio");
}

/**
 * webkit_settings_get_enable_webgl:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-webgl property.
 *
 * Returns: %TRUE If WebGL support is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_webgl(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->webGLEnabled();
}

/**
 * webkit_settings_set_enable_webgl:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-webgl property.
 */
void webkit_settings_set_enable_webgl(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->webGLEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setWebGLEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-webgl");
}

/**
 * webkit_settings_get_allow_modal_dialogs:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:allow-modal-dialogs property.
 *
 * Returns: %TRUE if it's allowed to create and run modal dialogs or %FALSE otherwise.
 */
gboolean webkit_settings_get_allow_modal_dialogs(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);
    return settings->priv->allowModalDialogs;
}

/**
 * webkit_settings_set_allow_modal_dialogs:
 * @settings: a #WebKitSettings
 * @allowed: Value to be set
 *
 * Set the #WebKitSettings:allow-modal-dialogs property.
 */
void webkit_settings_set_allow_modal_dialogs(WebKitSettings* settings, gboolean allowed)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    if (priv->allowModalDialogs == allowed)
        return;

    priv->allowModalDialogs = allowed;
    g_object_notify(G_OBJECT(settings), "allow-modal-dialogs");
}

/**
 * webkit_settings_get_zoom_text_only:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:zoom-text-only property.
 *
 * Returns: %TRUE If zoom level of the view should only affect the text
 *    or %FALSE if all view contents should be scaled.
 */
gboolean webkit_settings_get_zoom_text_only(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->zoomTextOnly;
}

/**
 * webkit_settings_set_zoom_text_only:
 * @settings: a #WebKitSettings
 * @zoom_text_only: Value to be set
 *
 * Set the #WebKitSettings:zoom-text-only property.
 */
void webkit_settings_set_zoom_text_only(WebKitSettings* settings, gboolean zoomTextOnly)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    if (priv->zoomTextOnly == zoomTextOnly)
        return;

    priv->zoomTextOnly = zoomTextOnly;
    g_object_notify(G_OBJECT(settings), "zoom-text-only");
}

/**
 * webkit_settings_get_javascript_can_access_clipboard:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:javascript-can-access-clipboard property.
 *
 * Returns: %TRUE If javascript-can-access-clipboard is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_javascript_can_access_clipboard(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->javaScriptCanAccessClipboard()
        && settings->priv->preferences->domPasteAllowed();
}

/**
 * webkit_settings_set_javascript_can_access_clipboard:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:javascript-can-access-clipboard property.
 */
void webkit_settings_set_javascript_can_access_clipboard(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->javaScriptCanAccessClipboard() && priv->preferences->domPasteAllowed();
    if (currentValue == enabled)
        return;

    priv->preferences->setJavaScriptCanAccessClipboard(enabled);
    priv->preferences->setDOMPasteAllowed(enabled);
    g_object_notify(G_OBJECT(settings), "javascript-can-access-clipboard");
}

/**
 * webkit_settings_get_media_playback_requires_user_gesture:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:media-playback-requires-user-gesture property.
 *
 * Returns: %TRUE If an user gesture is needed to play or load media
 *    or %FALSE if no user gesture is needed.
 */
gboolean webkit_settings_get_media_playback_requires_user_gesture(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->requiresUserGestureForMediaPlayback();
}

/**
 * webkit_settings_set_media_playback_requires_user_gesture:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:media-playback-requires-user-gesture property.
 */
void webkit_settings_set_media_playback_requires_user_gesture(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->requiresUserGestureForMediaPlayback();
    if (currentValue == enabled)
        return;

    priv->preferences->setRequiresUserGestureForMediaPlayback(enabled);
    g_object_notify(G_OBJECT(settings), "media-playback-requires-user-gesture");
}

/**
 * webkit_settings_get_media_playback_allows_inline:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:media-playback-allows-inline property.
 *
 * Returns: %TRUE If inline playback is allowed for media
 *    or %FALSE if only fullscreen playback is allowed.
 */
gboolean webkit_settings_get_media_playback_allows_inline(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), TRUE);

    return settings->priv->preferences->allowsInlineMediaPlayback();
}

/**
 * webkit_settings_set_media_playback_allows_inline:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:media-playback-allows-inline property.
 */
void webkit_settings_set_media_playback_allows_inline(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->allowsInlineMediaPlayback();
    if (currentValue == enabled)
        return;

    priv->preferences->setAllowsInlineMediaPlayback(enabled);
    g_object_notify(G_OBJECT(settings), "media-playback-allows-inline");
}

/**
 * webkit_settings_get_draw_compositing_indicators:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:draw-compositing-indicators property.
 *
 * Returns: %TRUE If compositing borders are drawn or %FALSE otherwise.
 */
gboolean webkit_settings_get_draw_compositing_indicators(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);
    return settings->priv->preferences->compositingBordersVisible()
        && settings->priv->preferences->compositingRepaintCountersVisible();
}

/**
 * webkit_settings_set_draw_compositing_indicators:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:draw-compositing-indicators property.
 */
void webkit_settings_set_draw_compositing_indicators(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    if (priv->preferences->compositingBordersVisible() == enabled
        && priv->preferences->compositingRepaintCountersVisible() == enabled)
        return;

    priv->preferences->setCompositingBordersVisible(enabled);
    priv->preferences->setCompositingRepaintCountersVisible(enabled);
    g_object_notify(G_OBJECT(settings), "draw-compositing-indicators");
}

/**
 * webkit_settings_get_enable_site_specific_quirks:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-site-specific-quirks property.
 *
 * Returns: %TRUE if site specific quirks are enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_site_specific_quirks(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->needsSiteSpecificQuirks();
}

/**
 * webkit_settings_set_enable_site_specific_quirks:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-site-specific-quirks property.
 */
void webkit_settings_set_enable_site_specific_quirks(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->needsSiteSpecificQuirks();
    if (currentValue == enabled)
        return;

    priv->preferences->setNeedsSiteSpecificQuirks(enabled);
    g_object_notify(G_OBJECT(settings), "enable-site-specific-quirks");
}

/**
 * webkit_settings_get_enable_page_cache:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-page-cache property.
 *
 * Returns: %TRUE if page cache enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_page_cache(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->usesBackForwardCache();
}

/**
 * webkit_settings_set_enable_page_cache:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-page-cache property.
 */
void webkit_settings_set_enable_page_cache(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->usesBackForwardCache();
    if (currentValue == enabled)
        return;

    priv->preferences->setUsesBackForwardCache(enabled);
    g_object_notify(G_OBJECT(settings), "enable-page-cache");
}

/**
 * webkit_settings_get_user_agent:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:user-agent property.
 *
 * Returns: The current value of the user-agent property.
 */
const char* webkit_settings_get_user_agent(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    WebKitSettingsPrivate* priv = settings->priv;
    ASSERT(!priv->userAgent.isNull());
    return priv->userAgent.data();
}

/**
 * webkit_settings_set_user_agent:
 * @settings: a #WebKitSettings
 * @user_agent: (allow-none): The new custom user agent string or %NULL to use the default user agent
 *
 * Set the #WebKitSettings:user-agent property.
 */
void webkit_settings_set_user_agent(WebKitSettings* settings, const char* userAgent)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;

    String userAgentString;
    if (userAgent && *userAgent) {
        userAgentString = String::fromUTF8(userAgent);
        g_return_if_fail(WebCore::isValidUserAgentHeaderValue(userAgentString));
    } else
        userAgentString = WebCore::standardUserAgent("");

    CString newUserAgent = userAgentString.utf8();
    if (newUserAgent == priv->userAgent)
        return;

    priv->userAgent = newUserAgent;
    g_object_notify(G_OBJECT(settings), "user-agent");
}

/**
 * webkit_settings_set_user_agent_with_application_details:
 * @settings: a #WebKitSettings
 * @application_name: (allow-none): The application name used for the user agent or %NULL to use the default user agent.
 * @application_version: (allow-none): The application version for the user agent or %NULL to user the default version.
 *
 * Set the #WebKitSettings:user-agent property by appending the application details to the default user
 * agent. If no application name or version is given, the default user agent used will be used. If only
 * the version is given, the default engine version is used with the given application name.
 */
void webkit_settings_set_user_agent_with_application_details(WebKitSettings* settings, const char* applicationName, const char* applicationVersion)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    CString newUserAgent = WebCore::standardUserAgent(String::fromUTF8(applicationName), String::fromUTF8(applicationVersion)).utf8();
    webkit_settings_set_user_agent(settings, newUserAgent.data());
}

/**
 * webkit_settings_get_enable_smooth_scrolling:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-smooth-scrolling property.
 *
 * Returns: %TRUE if smooth scrolling is enabled or %FALSE otherwise.
 */
gboolean webkit_settings_get_enable_smooth_scrolling(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->scrollAnimatorEnabled();
}

/**
 * webkit_settings_set_enable_smooth_scrolling:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-smooth-scrolling property.
 */
void webkit_settings_set_enable_smooth_scrolling(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->scrollAnimatorEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setScrollAnimatorEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-smooth-scrolling");
}

/**
 * webkit_settings_get_enable_accelerated_2d_canvas:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-accelerated-2d-canvas property.
 *
 * Returns: %TRUE if accelerated 2D canvas is enabled or %FALSE otherwise.
 *
 * Since: 2.2
 */
gboolean webkit_settings_get_enable_accelerated_2d_canvas(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->accelerated2dCanvasEnabled();
}

/**
 * webkit_settings_set_enable_accelerated_2d_canvas:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-accelerated-2d-canvas property.
 *
 * Since: 2.2
 */
void webkit_settings_set_enable_accelerated_2d_canvas(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    if (priv->preferences->accelerated2dCanvasEnabled() == enabled)
        return;

    priv->preferences->setAccelerated2dCanvasEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-accelerated-2d-canvas");
}

/**
 * webkit_settings_get_enable_write_console_messages_to_stdout:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-write-console-messages-to-stdout property.
 *
 * Returns: %TRUE if writing console messages to stdout is enabled or %FALSE
 * otherwise.
 *
 * Since: 2.2
 */
gboolean webkit_settings_get_enable_write_console_messages_to_stdout(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->logsPageMessagesToSystemConsoleEnabled();
}

/**
 * webkit_settings_set_enable_write_console_messages_to_stdout:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-write-console-messages-to-stdout property.
 *
 * Since: 2.2
 */
void webkit_settings_set_enable_write_console_messages_to_stdout(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->logsPageMessagesToSystemConsoleEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setLogsPageMessagesToSystemConsoleEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-write-console-messages-to-stdout");
}

/**
 * webkit_settings_get_enable_media_stream:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-media-stream property.
 *
 * Returns: %TRUE If mediastream support is enabled or %FALSE otherwise.
 *
 * Since: 2.4
 */
gboolean webkit_settings_get_enable_media_stream(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->mediaStreamEnabled();
}

/**
 * webkit_settings_set_enable_media_stream:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-media-stream property.
 *
 * Since: 2.4
 */
void webkit_settings_set_enable_media_stream(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->mediaStreamEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setMediaDevicesEnabled(enabled);
    priv->preferences->setMediaStreamEnabled(enabled);
    priv->preferences->setPeerConnectionEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-media-stream");
}

/**
 * webkit_settings_get_enable_mock_capture_devices:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-mock-capture-devices property.
 *
 * Returns: %TRUE If mock capture devices is enabled or %FALSE otherwise.
 *
 * Since: 2.24
 */
gboolean webkit_settings_get_enable_mock_capture_devices(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->mockCaptureDevicesEnabled();
}

/**
 * webkit_settings_set_enable_mock_capture_devices:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-mock-capture-devices property.
 *
 * Since: 2.4
 */
void webkit_settings_set_enable_mock_capture_devices(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->mockCaptureDevicesEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setMockCaptureDevicesEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-mock-capture-devices");
}

/**
 * webkit_settings_set_enable_spatial_navigation:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-spatial-navigation property.
 *
 * Since: 2.2
 */
void webkit_settings_set_enable_spatial_navigation(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->spatialNavigationEnabled();

    if (currentValue == enabled)
        return;

    priv->preferences->setSpatialNavigationEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-spatial-navigation");
}


/**
 * webkit_settings_get_enable_spatial_navigation:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-spatial-navigation property.
 *
 * Returns: %TRUE If HTML5 spatial navigation support is enabled or %FALSE otherwise.
 *
 * Since: 2.2
 */
gboolean webkit_settings_get_enable_spatial_navigation(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->spatialNavigationEnabled();
}

/**
 * webkit_settings_get_enable_mediasource:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-mediasource property.
 *
 * Returns: %TRUE If MediaSource support is enabled or %FALSE otherwise.
 *
 * Since: 2.4
 */
gboolean webkit_settings_get_enable_mediasource(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->mediaSourceEnabled();
}

/**
 * webkit_settings_set_enable_mediasource:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-mediasource property.
 *
 * Since: 2.4
 */
void webkit_settings_set_enable_mediasource(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->mediaSourceEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setMediaSourceEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-mediasource");
}

/**
 * webkit_settings_get_enable_encrypted_media:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-encrypted-media property.
 *
 * Returns: %TRUE if EncryptedMedia support is enabled or %FALSE otherwise.
 *
 * Since: 2.20
 */
gboolean webkit_settings_get_enable_encrypted_media(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->encryptedMediaAPIEnabled();
}


/**
 * webkit_settings_set_enable_encrypted_media:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-encrypted-media property.
 *
 * Since: 2.20
 */
void webkit_settings_set_enable_encrypted_media(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->encryptedMediaAPIEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setEncryptedMediaAPIEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-encrypted-media");
}

/**
 * webkit_settings_get_enable_media_capabilities:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-media-capabilities property.
 *
 * Returns: %TRUE if MediaCapabilities support is enabled or %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean webkit_settings_get_enable_media_capabilities(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->mediaCapabilitiesEnabled();
}


/**
 * webkit_settings_set_enable_media_capabilities:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-media-capabilities property.
 *
 * Since: 2.22
 */
void webkit_settings_set_enable_media_capabilities(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->mediaCapabilitiesEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setMediaCapabilitiesEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-media-capabilities");
}

/**
 * webkit_settings_get_allow_file_access_from_file_urls:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:allow-file-access-from-file-urls property.
 *
 * Returns: %TRUE If file access from file URLs is allowed or %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean webkit_settings_get_allow_file_access_from_file_urls(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->allowFileAccessFromFileURLs();
}

/**
 * webkit_settings_set_allow_file_access_from_file_urls:
 * @settings: a #WebKitSettings
 * @allowed: Value to be set
 *
 * Set the #WebKitSettings:allow-file-access-from-file-urls property.
 *
 * Since: 2.10
 */
void webkit_settings_set_allow_file_access_from_file_urls(WebKitSettings* settings, gboolean allowed)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    if (priv->preferences->allowFileAccessFromFileURLs() == allowed)
        return;

    priv->preferences->setAllowFileAccessFromFileURLs(allowed);
    g_object_notify(G_OBJECT(settings), "allow-file-access-from-file-urls");
}

/**
 * webkit_settings_get_allow_universal_access_from_file_urls:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:allow-universal-access-from-file-urls property.
 *
 * Returns: %TRUE If universal access from file URLs is allowed or %FALSE otherwise.
 *
 * Since: 2.14
 */
gboolean webkit_settings_get_allow_universal_access_from_file_urls(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->allowUniversalAccessFromFileURLs();
}

/**
 * webkit_settings_set_allow_universal_access_from_file_urls:
 * @settings: a #WebKitSettings
 * @allowed: Value to be set
 *
 * Set the #WebKitSettings:allow-universal-access-from-file-urls property.
 *
 * Since: 2.14
 */
void webkit_settings_set_allow_universal_access_from_file_urls(WebKitSettings* settings, gboolean allowed)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    if (priv->preferences->allowUniversalAccessFromFileURLs() == allowed)
        return;

    priv->preferences->setAllowUniversalAccessFromFileURLs(allowed);
    g_object_notify(G_OBJECT(settings), "allow-universal-access-from-file-urls");
}

/**
 * webkit_settings_get_allow_top_navigation_to_data_urls:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:allow-top-navigation-to-data-urls property.
 *
 * Returns: %TRUE If navigation to data URLs from the top frame is allowed or %FALSE\
 * otherwise.
 *
 * Since: 2.28
 */
gboolean webkit_settings_get_allow_top_navigation_to_data_urls(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->allowTopNavigationToDataURLs();
}

/**
 * webkit_settings_set_allow_top_navigation_to_data_urls:
 * @settings: a #WebKitSettings
 * @allowed: Value to be set
 *
 * Set the #WebKitSettings:allow-top-navigation-to-data-urls property.
 *
 * Since: 2.28
 */
void webkit_settings_set_allow_top_navigation_to_data_urls(WebKitSettings* settings, gboolean allowed)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    if (priv->preferences->allowTopNavigationToDataURLs() == allowed)
        return;

    priv->preferences->setAllowTopNavigationToDataURLs(allowed);
    g_object_notify(G_OBJECT(settings), "allow-top-navigation-to-data-urls");
}

#if PLATFORM(GTK)
/**
 * webkit_settings_get_hardware_acceleration_policy:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:hardware-acceleration-policy property.
 *
 * Return: a #WebKitHardwareAccelerationPolicy
 *
 * Since: 2.16
 */
WebKitHardwareAccelerationPolicy webkit_settings_get_hardware_acceleration_policy(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND);

    WebKitSettingsPrivate* priv = settings->priv;
    if (!priv->preferences->acceleratedCompositingEnabled())
        return WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER;

    if (priv->preferences->forceCompositingMode())
        return WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS;

    return WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND;
}

/**
 * webkit_settings_set_hardware_acceleration_policy:
 * @settings: a #WebKitSettings
 * @policy: a #WebKitHardwareAccelerationPolicy
 *
 * Set the #WebKitSettings:hardware-acceleration-policy property.
 *
 * Since: 2.16
 */
void webkit_settings_set_hardware_acceleration_policy(WebKitSettings* settings, WebKitHardwareAccelerationPolicy policy)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool changed = false;
    switch (policy) {
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS:
        if (!HardwareAccelerationManager::singleton().canUseHardwareAcceleration())
            return;
        if (!priv->preferences->acceleratedCompositingEnabled()) {
            priv->preferences->setAcceleratedCompositingEnabled(true);
            changed = true;
        }
        if (!priv->preferences->forceCompositingMode()) {
            priv->preferences->setForceCompositingMode(true);
            priv->preferences->setThreadedScrollingEnabled(true);
            changed = true;
        }
        break;
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER:
        if (HardwareAccelerationManager::singleton().forceHardwareAcceleration())
            return;
        if (priv->preferences->acceleratedCompositingEnabled()) {
            priv->preferences->setAcceleratedCompositingEnabled(false);
            changed = true;
        }

        if (priv->preferences->forceCompositingMode()) {
            priv->preferences->setForceCompositingMode(false);
            priv->preferences->setThreadedScrollingEnabled(false);
            changed = true;
        }
        break;
    case WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND:
        if (!priv->preferences->acceleratedCompositingEnabled() && HardwareAccelerationManager::singleton().canUseHardwareAcceleration()) {
            priv->preferences->setAcceleratedCompositingEnabled(true);
            changed = true;
        }

        if (priv->preferences->forceCompositingMode() && !HardwareAccelerationManager::singleton().forceHardwareAcceleration()) {
            priv->preferences->setForceCompositingMode(false);
            priv->preferences->setThreadedScrollingEnabled(false);
            changed = true;
        }
        break;
    }

    if (changed)
        g_object_notify(G_OBJECT(settings), "hardware-acceleration-policy");
}

/**
 * webkit_settings_get_enable_back_forward_navigation_gestures:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-back-forward-navigation-gestures property.
 *
 * Returns: %TRUE if horizontal swipe gesture will trigger back-forward navigaiton or %FALSE otherwise.
 *
 * Since: 2.24
 */
gboolean webkit_settings_get_enable_back_forward_navigation_gestures(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->enableBackForwardNavigationGestures;
}

/**
 * webkit_settings_set_enable_back_forward_navigation_gestures:
 * @settings: a #WebKitSettings
 * @enabled: value to be set
 *
 * Set the #WebKitSettings:enable-back-forward-navigation-gestures property.
 *
 * Since: 2.24
 */
void webkit_settings_set_enable_back_forward_navigation_gestures(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    if (priv->enableBackForwardNavigationGestures == enabled)
        return;

    priv->enableBackForwardNavigationGestures = enabled;
    g_object_notify(G_OBJECT(settings), "enable-back-forward-navigation-gestures");
}

/**
 * webkit_settings_font_size_to_points:
 * @pixels: the font size in pixels to convert to points
 *
 * Convert @pixels to the equivalent value in points, based on the current
 * screen DPI. Applications can use this function to convert font size values
 * in pixels to font size values in points when getting the font size properties
 * of #WebKitSettings.
 *
 * Returns: the equivalent font size in points.
 *
 * Since: 2.20
 */
guint32 webkit_settings_font_size_to_points(guint32 pixels)
{
    return std::round(pixels * 72 / WebCore::screenDPI());
}

/**
 * webkit_settings_font_size_to_pixels:
 * @points: the font size in points to convert to pixels
 *
 * Convert @points to the equivalent value in pixels, based on the current
 * screen DPI. Applications can use this function to convert font size values
 * in points to font size values in pixels when setting the font size properties
 * of #WebKitSettings.
 *
 * Returns: the equivalent font size in pixels.
 *
 * Since: 2.20
 */
guint32 webkit_settings_font_size_to_pixels(guint32 points)
{
    return std::round(points * WebCore::screenDPI() / 72);
}
#endif // PLATFORM(GTK)

/**
 * webkit_settings_get_enable_javascript_markup:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-javascript-markup property.
 *
 * Returns: %TRUE if JavaScript markup is enabled or %FALSE otherwise.
 *
 * Since: 2.24
 */
gboolean webkit_settings_get_enable_javascript_markup(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->javaScriptMarkupEnabled();
}

/**
 * webkit_settings_set_enable_javascript_markup:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-javascript-markup property.
 *
 * Since: 2.24
 */
void webkit_settings_set_enable_javascript_markup(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->javaScriptMarkupEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setJavaScriptMarkupEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-javascript-markup");
}

/**
 * webkit_settings_get_enable_media:
 * @settings: a #WebKitSettings
 *
 * Get the #WebKitSettings:enable-media property.
 *
 * Returns: %TRUE if media support is enabled or %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean webkit_settings_get_enable_media(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), FALSE);

    return settings->priv->preferences->mediaEnabled();
}

/**
 * webkit_settings_set_enable_media:
 * @settings: a #WebKitSettings
 * @enabled: Value to be set
 *
 * Set the #WebKitSettings:enable-media property.
 *
 * Since: 2.26
 */
void webkit_settings_set_enable_media(WebKitSettings* settings, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    bool currentValue = priv->preferences->mediaEnabled();
    if (currentValue == enabled)
        return;

    priv->preferences->setMediaEnabled(enabled);
    g_object_notify(G_OBJECT(settings), "enable-media");
}

void webkitSettingsSetMediaCaptureRequiresSecureConnection(WebKitSettings* settings, bool required)
{
    WebKitSettingsPrivate* priv = settings->priv;
    priv->preferences->setMediaCaptureRequiresSecureConnection(required);
}

/**
 * webkit_settings_get_media_content_types_requiring_hardware_support:
 * @settings: a #WebKitSettings
 *
 * Gets the #WebKitSettings:media-content-types-requiring-hardware-support property.
 *
 * Returns: Media content types requiring hardware support, or %NULL.
 *
 * Since: 2.30
 */
const gchar* webkit_settings_get_media_content_types_requiring_hardware_support(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), 0);

    const auto& mediaContentTypesRequiringHardwareSupport = settings->priv->mediaContentTypesRequiringHardwareSupport;
    if (!mediaContentTypesRequiringHardwareSupport.length())
        return nullptr;
    return mediaContentTypesRequiringHardwareSupport.data();
}

/**
 * webkit_settings_set_media_content_types_requiring_hardware_support:
 * @settings: a #WebKitSettings
 * @content_types: (allow-none): list of media content types requiring hardware support split by semicolons (:) or %NULL to use the default value.
 *
 * Set the #WebKitSettings:media-content-types-requiring-hardware-support property.
 *
 * Since: 2.30
 */
void webkit_settings_set_media_content_types_requiring_hardware_support(WebKitSettings* settings, const gchar* mediaContentTypesRequiringHardwareSupport)
{
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitSettingsPrivate* priv = settings->priv;
    if (!g_strcmp0(priv->mediaContentTypesRequiringHardwareSupport.data(), mediaContentTypesRequiringHardwareSupport))
        return;

    String mediaContentTypesRequiringHardwareSupportString = String::fromUTF8(mediaContentTypesRequiringHardwareSupport);
    priv->preferences->setMediaContentTypesRequiringHardwareSupport(mediaContentTypesRequiringHardwareSupportString);
    priv->mediaContentTypesRequiringHardwareSupport = mediaContentTypesRequiringHardwareSupportString.utf8();
    g_object_notify(G_OBJECT(settings), "media-content-types-requiring-hardware-support");
}
