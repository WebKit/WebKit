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

#include "TestMain.h"
#include "WebKitTestServer.h"
#include "WebViewTest.h"
#include <WebCore/SoupVersioning.h>
#include <wtf/glib/GRefPtr.h>

static WebKitTestServer* gServer;

static void testWebKitSettings(Test*, gconstpointer)
{
    WebKitSettings* settings = webkit_settings_new();

    // JavaScript is enabled by default.
    g_assert_true(webkit_settings_get_enable_javascript(settings));
    webkit_settings_set_enable_javascript(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_javascript(settings));

    // By default auto-load-image is true.
    g_assert_true(webkit_settings_get_auto_load_images(settings));
    webkit_settings_set_auto_load_images(settings, FALSE);
    g_assert_false(webkit_settings_get_auto_load_images(settings));

    // load-icons-ignoring-image-load-setting is false by default.
    g_assert_false(webkit_settings_get_load_icons_ignoring_image_load_setting(settings));
    webkit_settings_set_load_icons_ignoring_image_load_setting(settings, TRUE);
    g_assert_true(webkit_settings_get_load_icons_ignoring_image_load_setting(settings));
    
    // Offline application cache is true by default.
    g_assert_true(webkit_settings_get_enable_offline_web_application_cache(settings));
    webkit_settings_set_enable_offline_web_application_cache(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_offline_web_application_cache(settings));

    // Local storage is enable by default.
    g_assert_true(webkit_settings_get_enable_html5_local_storage(settings));
    webkit_settings_set_enable_html5_local_storage(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_html5_local_storage(settings));

    // HTML5 database is enabled by default.
    g_assert_true(webkit_settings_get_enable_html5_database(settings));
    webkit_settings_set_enable_html5_database(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_html5_database(settings));

    // By default, JavaScript can open windows automatically is disabled.
    g_assert_false(webkit_settings_get_javascript_can_open_windows_automatically(settings));
    webkit_settings_set_javascript_can_open_windows_automatically(settings, TRUE);
    g_assert_true(webkit_settings_get_javascript_can_open_windows_automatically(settings));

    // By default hyper link auditing is enabled.
    g_assert_true(webkit_settings_get_enable_hyperlink_auditing(settings));
    webkit_settings_set_enable_hyperlink_auditing(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_hyperlink_auditing(settings));

    // Default font family is "sans-serif".
    g_assert_cmpstr(webkit_settings_get_default_font_family(settings), ==, "sans-serif");
    webkit_settings_set_default_font_family(settings, "monospace");
    g_assert_cmpstr(webkit_settings_get_default_font_family(settings), ==, "monospace");

    // Default monospace font family font family is "monospace".
    g_assert_cmpstr(webkit_settings_get_monospace_font_family(settings), ==, "monospace");
    webkit_settings_set_monospace_font_family(settings, "sans-serif");
    g_assert_cmpstr(webkit_settings_get_monospace_font_family(settings), ==, "sans-serif");

    // Default serif font family is "serif".
    g_assert_cmpstr(webkit_settings_get_serif_font_family(settings), ==, "serif");
    webkit_settings_set_serif_font_family(settings, "sans-serif");
    g_assert_cmpstr(webkit_settings_get_serif_font_family(settings), ==, "sans-serif");

    // Default sans serif font family is "sans-serif".
    g_assert_cmpstr(webkit_settings_get_sans_serif_font_family(settings), ==, "sans-serif");
    webkit_settings_set_sans_serif_font_family(settings, "serif");
    g_assert_cmpstr(webkit_settings_get_sans_serif_font_family(settings), ==, "serif");

    // Default cursive font family "serif".
    g_assert_cmpstr(webkit_settings_get_cursive_font_family(settings), ==, "serif");
    webkit_settings_set_cursive_font_family(settings, "sans-serif");
    g_assert_cmpstr(webkit_settings_get_cursive_font_family(settings), ==, "sans-serif");

    // Default fantasy font family is "serif".
    g_assert_cmpstr(webkit_settings_get_fantasy_font_family(settings), ==, "serif");
    webkit_settings_set_fantasy_font_family(settings, "sans-serif");
    g_assert_cmpstr(webkit_settings_get_fantasy_font_family(settings), ==, "sans-serif");

    // Default pictograph font family is "serif".
    g_assert_cmpstr(webkit_settings_get_pictograph_font_family(settings), ==, "serif");
    webkit_settings_set_pictograph_font_family(settings, "sans-serif");
    g_assert_cmpstr(webkit_settings_get_pictograph_font_family(settings), ==, "sans-serif");

    // Default font size is 16.
    g_assert_cmpuint(webkit_settings_get_default_font_size(settings), ==, 16);
    webkit_settings_set_default_font_size(settings, 14);
    g_assert_cmpuint(webkit_settings_get_default_font_size(settings), ==, 14);

    // Default monospace font size is 13.
    g_assert_cmpuint(webkit_settings_get_default_monospace_font_size(settings), ==, 13);
    webkit_settings_set_default_monospace_font_size(settings, 10);
    g_assert_cmpuint(webkit_settings_get_default_monospace_font_size(settings), ==, 10);

    // Default minimum font size is 0.
    g_assert_cmpuint(webkit_settings_get_minimum_font_size(settings), ==, 0);
    webkit_settings_set_minimum_font_size(settings, 7);
    g_assert_cmpuint(webkit_settings_get_minimum_font_size(settings), ==, 7);

    // Test conversion between pixels and points. Use a standard DPI of 96.
    // Set DPI explicitly to avoid the tests failing for users that use a
    // different default DPI. This doesn't affect the system's DPI outside
    // of the tests scope, so we don't need to change it back to the original
    // value. We can control DPI only on GTK. On WPE, WebCore defaults it to 96.
#if PLATFORM(GTK)
    GtkSettings* gtkSettings = gtk_settings_get_default();
    if (gtkSettings)
        g_object_set(gtkSettings, "gtk-xft-dpi", 96 * 1024, nullptr);

    // At 96 DPI, 9 points is 12 pixels and 24 points is 32 pixels.
    g_assert_cmpuint(webkit_settings_font_size_to_pixels(9), ==, 12);
    g_assert_cmpuint(webkit_settings_font_size_to_pixels(24), ==, 32);

    // At 96 DPI, 8 pixels is 6 points and 24 pixels is 18 points.
    g_assert_cmpuint(webkit_settings_font_size_to_points(8), ==, 6);
    g_assert_cmpuint(webkit_settings_font_size_to_points(24), ==, 18);

    // Test font size on DPI change.
    if (gtkSettings) {
        // At 96 DPI, 20 pixels is 15 points.
        webkit_settings_set_default_font_size(settings, 20);
        g_assert_cmpuint(webkit_settings_font_size_to_points(webkit_settings_get_default_font_size(settings)), ==, 15);
        // At 96 DPI, 16 pixels is 12 points.
        webkit_settings_set_default_monospace_font_size(settings, 16);
        g_assert_cmpuint(webkit_settings_font_size_to_points(webkit_settings_get_default_monospace_font_size(settings)), ==, 12);

        // Set DPI to 120. The scaling factor is 120 / 96 == 1.25.
        g_object_set(gtkSettings, "gtk-xft-dpi", 120 * 1024, nullptr);
        g_assert_cmpuint(webkit_settings_get_default_font_size(settings), ==, 20);
        g_assert_cmpuint(webkit_settings_font_size_to_points(webkit_settings_get_default_font_size(settings) * 1.25), ==, 15);
        g_assert_cmpuint(webkit_settings_get_default_monospace_font_size(settings), ==, 16);
        g_assert_cmpuint(webkit_settings_font_size_to_points(webkit_settings_get_default_monospace_font_size(settings) * 1.25), ==, 12);

        // Set DPI back to 96.
        g_object_set(gtkSettings, "gtk-xft-dpi", 96 * 1024, nullptr);
        g_assert_cmpuint(webkit_settings_get_default_font_size(settings), ==, 20);
        g_assert_cmpuint(webkit_settings_font_size_to_points(webkit_settings_get_default_font_size(settings)), ==, 15);
        g_assert_cmpuint(webkit_settings_get_default_monospace_font_size(settings), ==, 16);
        g_assert_cmpuint(webkit_settings_font_size_to_points(webkit_settings_get_default_monospace_font_size(settings)), ==, 12);
    }
#endif

    // Default charset is "iso-8859-1".
    g_assert_cmpstr(webkit_settings_get_default_charset(settings), ==, "iso-8859-1");
    webkit_settings_set_default_charset(settings, "utf8");
    g_assert_cmpstr(webkit_settings_get_default_charset(settings), ==, "utf8");

    g_assert_false(webkit_settings_get_enable_developer_extras(settings));
    webkit_settings_set_enable_developer_extras(settings, TRUE);
    g_assert_true(webkit_settings_get_enable_developer_extras(settings));

    g_assert_true(webkit_settings_get_enable_resizable_text_areas(settings));
    webkit_settings_set_enable_resizable_text_areas(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_resizable_text_areas(settings));

    g_assert_true(webkit_settings_get_enable_tabs_to_links(settings));
    webkit_settings_set_enable_tabs_to_links(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_tabs_to_links(settings));

    g_assert_false(webkit_settings_get_enable_dns_prefetching(settings));
    webkit_settings_set_enable_dns_prefetching(settings, TRUE);
    g_assert_true(webkit_settings_get_enable_dns_prefetching(settings));

    // Caret browsing is disabled by default.
    g_assert_false(webkit_settings_get_enable_caret_browsing(settings));
    webkit_settings_set_enable_caret_browsing(settings, TRUE);
    g_assert_true(webkit_settings_get_enable_caret_browsing(settings));

    // Fullscreen JavaScript API is enabled by default.
    g_assert_true(webkit_settings_get_enable_fullscreen(settings));
    webkit_settings_set_enable_fullscreen(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_fullscreen(settings));

    // Print backgrounds is enabled by default
    g_assert_true(webkit_settings_get_print_backgrounds(settings));
    webkit_settings_set_print_backgrounds(settings, FALSE);
    g_assert_false(webkit_settings_get_print_backgrounds(settings));

    // WebAudio is enabled by default.
    g_assert_true(webkit_settings_get_enable_webaudio(settings));
    webkit_settings_set_enable_webaudio(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_webaudio(settings));

    // WebGL is enabled by default.
    g_assert_true(webkit_settings_get_enable_webgl(settings));
    webkit_settings_set_enable_webgl(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_webgl(settings));

    // Allow Modal Dialogs is disabled by default.
    g_assert_false(webkit_settings_get_allow_modal_dialogs(settings));
    webkit_settings_set_allow_modal_dialogs(settings, TRUE);
    g_assert_true(webkit_settings_get_allow_modal_dialogs(settings));

    // Zoom text only is disabled by default.
    g_assert_false(webkit_settings_get_zoom_text_only(settings));
    webkit_settings_set_zoom_text_only(settings, TRUE);
    g_assert_true(webkit_settings_get_zoom_text_only(settings));

    // By default, JavaScript cannot access the clipboard.
    g_assert_false(webkit_settings_get_javascript_can_access_clipboard(settings));
    webkit_settings_set_javascript_can_access_clipboard(settings, TRUE);
    g_assert_true(webkit_settings_get_javascript_can_access_clipboard(settings));

    // By default, media playback doesn't require user gestures.
    g_assert_false(webkit_settings_get_media_playback_requires_user_gesture(settings));
    webkit_settings_set_media_playback_requires_user_gesture(settings, TRUE);
    g_assert_true(webkit_settings_get_media_playback_requires_user_gesture(settings));

    // By default, inline media playback is allowed
    g_assert_true(webkit_settings_get_media_playback_allows_inline(settings));
    webkit_settings_set_media_playback_allows_inline(settings, FALSE);
    g_assert_false(webkit_settings_get_media_playback_allows_inline(settings));

    // By default, debug indicators are disabled.
    g_assert_false(webkit_settings_get_draw_compositing_indicators(settings));
    webkit_settings_set_draw_compositing_indicators(settings, TRUE);
    g_assert_true(webkit_settings_get_draw_compositing_indicators(settings));

    // By default, site specific quirks are enabled.
    g_assert_true(webkit_settings_get_enable_site_specific_quirks(settings));
    webkit_settings_set_enable_site_specific_quirks(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_site_specific_quirks(settings));

    // By default, the back/forward cache is enabled.
    g_assert_true(webkit_settings_get_enable_page_cache(settings));
    webkit_settings_set_enable_page_cache(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_page_cache(settings));

    // By default, smooth scrolling is enabled.
    g_assert_true(webkit_settings_get_enable_smooth_scrolling(settings));
    webkit_settings_set_enable_smooth_scrolling(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_smooth_scrolling(settings));

    // By default, writing of console messages to stdout is disabled.
    g_assert_false(webkit_settings_get_enable_write_console_messages_to_stdout(settings));
    webkit_settings_set_enable_write_console_messages_to_stdout(settings, TRUE);
    g_assert_true(webkit_settings_get_enable_write_console_messages_to_stdout(settings));

    // MediaStream is enabled by default as experimental feature.
    g_assert_true(webkit_settings_get_enable_media_stream(settings));
    webkit_settings_set_enable_media_stream(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_media_stream(settings));

    // By default, WebRTC is disabled
    g_assert_false(webkit_settings_get_enable_webrtc(settings));
    webkit_settings_set_enable_webrtc(settings, TRUE);
    g_assert_true(webkit_settings_get_enable_webrtc(settings));

    // By default, SpatialNavigation is disabled
    g_assert_false(webkit_settings_get_enable_spatial_navigation(settings));
    webkit_settings_set_enable_spatial_navigation(settings, TRUE);
    g_assert_true(webkit_settings_get_enable_spatial_navigation(settings));

    // MediaSource is enabled by default
    g_assert_true(webkit_settings_get_enable_mediasource(settings));
    webkit_settings_set_enable_mediasource(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_mediasource(settings));

    // EncryptedMedia is disabled by default
    g_assert_false(webkit_settings_get_enable_encrypted_media(settings));
    webkit_settings_set_enable_encrypted_media(settings, TRUE);
    g_assert_true(webkit_settings_get_enable_encrypted_media(settings));

    // MediaCapabilities is disabled by default
    g_assert_false(webkit_settings_get_enable_media_capabilities(settings));
    webkit_settings_set_enable_media_capabilities(settings, TRUE);
    g_assert_true(webkit_settings_get_enable_media_capabilities(settings));

    // File access from file URLs is not allowed by default.
    g_assert_false(webkit_settings_get_allow_file_access_from_file_urls(settings));
    webkit_settings_set_allow_file_access_from_file_urls(settings, TRUE);
    g_assert_true(webkit_settings_get_allow_file_access_from_file_urls(settings));

    // Universal access from file URLs is not allowed by default.
    g_assert_false(webkit_settings_get_allow_universal_access_from_file_urls(settings));
    webkit_settings_set_allow_universal_access_from_file_urls(settings, TRUE);
    g_assert_true(webkit_settings_get_allow_universal_access_from_file_urls(settings));

    // Top frame navigation to data url is not allowed by default.
    g_assert_false(webkit_settings_get_allow_top_navigation_to_data_urls(settings));
    webkit_settings_set_allow_top_navigation_to_data_urls(settings, TRUE);
    g_assert_true(webkit_settings_get_allow_top_navigation_to_data_urls(settings));

    // Media is enabled by default.
    g_assert_true(webkit_settings_get_enable_media(settings));
    webkit_settings_set_enable_media(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_media(settings));

    // Default media content types requiring hardware support is nullptr.
    g_assert_cmpstr(nullptr, ==, webkit_settings_get_media_content_types_requiring_hardware_support(settings));
    webkit_settings_set_media_content_types_requiring_hardware_support(settings, "video/*; codecs=\"*\"");
    g_assert_cmpstr("video/*; codecs=\"*\"", ==, webkit_settings_get_media_content_types_requiring_hardware_support(settings));
    webkit_settings_set_media_content_types_requiring_hardware_support(settings, nullptr);
    g_assert_cmpstr(nullptr, ==, webkit_settings_get_media_content_types_requiring_hardware_support(settings));

#if PLATFORM(GTK)
    // Always is the default hardware acceleration policy.
    g_assert_cmpuint(webkit_settings_get_hardware_acceleration_policy(settings), ==, WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS);
    webkit_settings_set_hardware_acceleration_policy(settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
    g_assert_cmpuint(webkit_settings_get_hardware_acceleration_policy(settings), ==, WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
#if !USE(GTK4)
    webkit_settings_set_hardware_acceleration_policy(settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND);
    g_assert_cmpuint(webkit_settings_get_hardware_acceleration_policy(settings), ==, WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND);
#endif
    webkit_settings_set_hardware_acceleration_policy(settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS);
    g_assert_cmpuint(webkit_settings_get_hardware_acceleration_policy(settings), ==, WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS);

    // Back-forward navigation gesture is disabled by default
    g_assert_false(webkit_settings_get_enable_back_forward_navigation_gestures(settings));
    webkit_settings_set_enable_back_forward_navigation_gestures(settings, TRUE);
    g_assert_true(webkit_settings_get_enable_back_forward_navigation_gestures(settings));
#endif

    // JavaScript markup is enabled by default.
    g_assert_true(webkit_settings_get_enable_javascript_markup(settings));
    webkit_settings_set_enable_javascript_markup(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_javascript_markup(settings));

#if !ENABLE(2022_GLIB_API)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // Accelerated 2D canvas is deprecated and always disabled.
    g_assert_false(webkit_settings_get_enable_accelerated_2d_canvas(settings));
    webkit_settings_set_enable_accelerated_2d_canvas(settings, TRUE);
    g_assert_false(webkit_settings_get_enable_accelerated_2d_canvas(settings));

    // XSS Auditor is deprecated and always disabled.
    g_assert_false(webkit_settings_get_enable_xss_auditor(settings));
    webkit_settings_set_enable_xss_auditor(settings, TRUE);
    g_assert_false(webkit_settings_get_enable_xss_auditor(settings));

    // Frame flattening is deprecated and always disabled.
    g_assert_false(webkit_settings_get_enable_frame_flattening(settings));
    webkit_settings_set_enable_frame_flattening(settings, TRUE);
    g_assert_false(webkit_settings_get_enable_frame_flattening(settings));

    // Java is not supported, and always disabled.
    // Make warnings non-fatal for this test to make it pass.
    Test::removeLogFatalFlag(G_LOG_LEVEL_WARNING);
    g_assert_false(webkit_settings_get_enable_java(settings));
    webkit_settings_set_enable_java(settings, FALSE);
    g_assert_false(webkit_settings_get_enable_java(settings));
    Test::addLogFatalFlag(G_LOG_LEVEL_WARNING);
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

    // WebSecurity is enabled by default.
    g_assert_false(webkit_settings_get_disable_web_security(settings));
    webkit_settings_set_disable_web_security(settings, TRUE);
    g_assert_true(webkit_settings_get_disable_web_security(settings));

    g_object_unref(G_OBJECT(settings));
}

void testWebKitSettingsNewWithSettings(Test* test, gconstpointer)
{
    GRefPtr<WebKitSettings> settings = adoptGRef(webkit_settings_new_with_settings(
        "enable-javascript", FALSE,
        "auto-load-images", FALSE,
        "load-icons-ignoring-image-load-setting", TRUE,
        nullptr));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(settings.get()));
    g_assert_false(webkit_settings_get_enable_javascript(settings.get()));
    g_assert_false(webkit_settings_get_auto_load_images(settings.get()));
    g_assert_true(webkit_settings_get_load_icons_ignoring_image_load_setting(settings.get()));
}

#if PLATFORM(GTK)
static CString convertWebViewMainResourceDataToCString(WebViewTest* test)
{
    size_t mainResourceDataSize = 0;
    const char* mainResourceData = test->mainResourceData(mainResourceDataSize);
    return CString(mainResourceData, mainResourceDataSize);
}

static void assertThatUserAgentIsSentInHeaders(WebViewTest* test, const CString& userAgent)
{
    test->loadURI(gServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    ASSERT_CMP_CSTRING(convertWebViewMainResourceDataToCString(test), ==, userAgent);
}

static void testWebKitSettingsUserAgent(WebViewTest* test, gconstpointer)
{
    GRefPtr<WebKitSettings> settings = adoptGRef(webkit_settings_new());
    CString defaultUserAgent = webkit_settings_get_user_agent(settings.get());
    webkit_web_view_set_settings(test->m_webView, settings.get());

    g_assert_nonnull(g_strstr_len(defaultUserAgent.data(), -1, "AppleWebKit"));
    g_assert_nonnull(g_strstr_len(defaultUserAgent.data(), -1, "Safari"));

    webkit_settings_set_user_agent(settings.get(), 0);
    g_assert_cmpstr(defaultUserAgent.data(), ==, webkit_settings_get_user_agent(settings.get()));
    assertThatUserAgentIsSentInHeaders(test, defaultUserAgent.data());

    webkit_settings_set_user_agent(settings.get(), "");
    g_assert_cmpstr(defaultUserAgent.data(), ==, webkit_settings_get_user_agent(settings.get()));

    const char* funkyUserAgent = "Funky!";
    webkit_settings_set_user_agent(settings.get(), funkyUserAgent);
    g_assert_cmpstr(funkyUserAgent, ==, webkit_settings_get_user_agent(settings.get()));
    assertThatUserAgentIsSentInHeaders(test, funkyUserAgent);

    webkit_settings_set_user_agent_with_application_details(settings.get(), "WebKitGTK", 0);
    const char* userAgentWithNullVersion = webkit_settings_get_user_agent(settings.get());
    g_assert_cmpstr(g_strstr_len(userAgentWithNullVersion, -1, defaultUserAgent.data()), ==, userAgentWithNullVersion);
    g_assert_nonnull(g_strstr_len(userAgentWithNullVersion, -1, "WebKitGTK"));

    webkit_settings_set_user_agent_with_application_details(settings.get(), "WebKitGTK", "");
    g_assert_cmpstr(webkit_settings_get_user_agent(settings.get()), ==, userAgentWithNullVersion);

    webkit_settings_set_user_agent_with_application_details(settings.get(), "WebCatGTK+", "3.4.5");
    const char* newUserAgent = webkit_settings_get_user_agent(settings.get());
    g_assert_nonnull(g_strstr_len(newUserAgent, -1, "3.4.5"));
    g_assert_nonnull(g_strstr_len(newUserAgent, -1, "WebCatGTK+"));

    GUniquePtr<char> applicationUserAgent(g_strdup_printf("%s %s", defaultUserAgent.data(), "WebCatGTK+/3.4.5"));
    g_assert_cmpstr(applicationUserAgent.get(), ==, webkit_settings_get_user_agent(settings.get()));
}
#endif // PLATFORM(GTK)

static void testWebKitSettingsJavaScriptMarkup(WebViewTest* test, gconstpointer)
{
    webkit_settings_set_enable_javascript_markup(webkit_web_view_get_settings(test->m_webView), FALSE);
    static const char* html =
        "<html>"
        " <head>"
        "  <title>No JavaScript allowed</title>"
        "  <script>document.title = 'JavaScript allowed from head script'</script>"
        " </head>"
        " <body onload='document.title = \"JavaScript allowed from body onload attribute\"'>"
        "  <p>No JavaScript markup should be allowed</p>"
        "  <script>document.title = 'JavaScript allowed from body script'</script>"
        " </body>"
        "</html>";
    test->loadHtml(html, nullptr);
    test->waitUntilTitleChanged();

    g_assert_cmpstr(webkit_web_view_get_title(test->m_webView), ==, "No JavaScript allowed");
    auto* jsResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('script').length", nullptr);
    g_assert(jsResult);
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(jsResult), ==, 0);

    webkit_settings_set_enable_javascript_markup(webkit_web_view_get_settings(test->m_webView), TRUE);
}

#if USE(SOUP2)
static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
#else
static void serverCallback(SoupServer* server, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
#endif
{
    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    if (g_str_equal(path, "/")) {
        const char* userAgent = soup_message_headers_get_one(soup_server_message_get_request_headers(message), "User-Agent");
        auto* responseBody = soup_server_message_get_response_body(message);
        soup_message_body_append(responseBody, SOUP_MEMORY_COPY, userAgent, strlen(userAgent));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
}

void beforeAll()
{
    gServer = new WebKitTestServer();
    gServer->run(serverCallback);

    Test::add("WebKitSettings", "webkit-settings", testWebKitSettings);
    Test::add("WebKitSettings", "new-with-settings", testWebKitSettingsNewWithSettings);
#if PLATFORM(GTK)
    WebViewTest::add("WebKitSettings", "user-agent", testWebKitSettingsUserAgent);
#endif
    WebViewTest::add("WebKitSettings", "javascript-markup", testWebKitSettingsJavaScriptMarkup);
}

void afterAll()
{
    delete gServer;
}

