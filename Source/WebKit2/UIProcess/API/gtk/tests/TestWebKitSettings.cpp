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
#include <JavaScriptCore/GRefPtr.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static void testWebKitSettings(Test*, gconstpointer)
{
    /* Skip running this test for now till a bug in WKPreferencesCreate
       https://bugs.webkit.org/show_bug.cgi?id=70127 gets fixed. */
#if 0
    WebKitSettings* settings = webkit_settings_new();

    // JavaScript is enabled by default.
    g_assert(webkit_settings_get_enable_javascript(settings));
    webkit_settings_set_enable_javascript(settings, FALSE);
    g_assert(!webkit_settings_get_enable_javascript(settings));

    // By default auto-load-image is true.
    g_assert(webkit_settings_get_auto_load_images(settings));
    webkit_settings_set_auto_load_images(settings, FALSE);
    g_assert(!webkit_settings_get_auto_load_images(settings));

    // load-icons-ignoring-image-load-setting is false by default.
    g_assert(!webkit_settings_get_load_icons_ignoring_image_load_setting(settings));
    webkit_settings_set_load_icons_ignoring_image_load_setting(settings, TRUE);
    g_assert(webkit_settings_get_load_icons_ignoring_image_load_setting(settings));
    
    // Offline application cache is true by default.
    g_assert(webkit_settings_get_enable_offline_web_application_cache(settings));
    webkit_settings_set_enable_offline_web_application_cache(settings, FALSE);
    g_assert(!webkit_settings_get_enable_offline_web_application_cache(settings));

    // Local storage is enable by default.
    g_assert(webkit_settings_get_enable_html5_local_storage(settings));
    webkit_settings_set_enable_html5_local_storage(settings, FALSE);
    g_assert(!webkit_settings_get_enable_html5_local_storage(settings));

    // HTML5 database is enabled by default.
    g_assert(webkit_settings_get_enable_html5_database(settings));
    webkit_settings_set_enable_html5_database(settings, FALSE);
    g_assert(!webkit_settings_get_enable_html5_database(settings));

    // XSS Auditor is enabled by default.
    g_assert(webkit_settings_get_enable_xss_auditor(settings));
    webkit_settings_set_enable_xss_auditor(settings, FALSE);
    g_assert(!webkit_settings_get_enable_xss_auditor(settings));

    // Frame flattening is disabled by default.
    g_assert(!webkit_settings_get_enable_frame_flattening(settings));
    webkit_settings_set_enable_frame_flattening(settings, TRUE);
    g_assert(webkit_settings_get_enable_frame_flattening(settings));

    // Plugins are enabled by default.
    g_assert(webkit_settings_get_enable_plugins(settings));
    webkit_settings_set_enable_plugins(settings, FALSE);
    g_assert(!webkit_settings_get_enable_plugins(settings));

    // Java is enabled by default.
    g_assert(webkit_settings_get_enable_java(settings));
    webkit_settings_set_enable_java(settings, FALSE);
    g_assert(!webkit_settings_get_enable_java(settings));

    // By default, JavaScript can open windows automatically is disabled.
    g_assert(!webkit_settings_get_javascript_can_open_windows_automatically(settings));
    webkit_settings_set_javascript_can_open_windows_automatically(settings, TRUE);
    g_assert(webkit_settings_get_javascript_can_open_windows_automatically(settings));

    // By default hyper link auditing is disabled.
    g_assert(!webkit_settings_get_enable_hyperlink_auditing(settings));
    webkit_settings_set_enable_hyperlink_auditing(settings, TRUE);
    g_assert(webkit_settings_get_enable_hyperlink_auditing(settings));

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

    // Default charset is "iso-8859-1".
    g_assert_cmpstr(webkit_settings_get_default_charset(settings), ==, "iso-8859-1");
    webkit_settings_set_default_charset(settings, "utf8");
    g_assert_cmpstr(webkit_settings_get_default_charset(settings), ==, "utf8");

    g_assert(!webkit_settings_get_enable_private_browsing(settings));
    webkit_settings_set_enable_private_browsing(settings, TRUE);
    g_assert(webkit_settings_get_enable_private_browsing(settings));

    g_assert(!webkit_settings_get_enable_developer_extras(settings));
    webkit_settings_set_enable_developer_extras(settings, TRUE);
    g_assert(webkit_settings_get_enable_developer_extras(settings));

    g_assert(webkit_settings_get_enable_resizable_text_areas(settings));
    webkit_settings_set_enable_resizable_text_areas(settings, FALSE);
    g_assert(!webkit_settings_get_enable_resizable_text_areas(settings));

    g_assert(webkit_settings_get_enable_tabs_to_links(settings));
    webkit_settings_set_enable_tabs_to_links(settings, FALSE);
    g_assert(!webkit_settings_get_enable_tabs_to_links(settings));

    g_assert(!webkit_settings_get_enable_dns_prefetching(settings));
    webkit_settings_set_enable_dns_prefetching(settings, TRUE);
    g_assert(webkit_settings_get_enable_dns_prefetching(settings));

    // Caret browsing is disabled by default.
    g_assert(!webkit_settings_get_enable_caret_browsing(settings));
    webkit_settings_set_enable_caret_browsing(settings, TRUE);
    g_assert(webkit_settings_get_enable_caret_browsing(settings));

    g_object_unref(G_OBJECT(settings));
#endif
}

void testWebKitSettingsNewWithSettings(Test* test, gconstpointer)
{
    /* Skip running this test for now till a bug in WKPreferencesCreate
       https://bugs.webkit.org/show_bug.cgi?id=70127 gets fixed. */
#if 0
    GRefPtr<WebKitSettings> settings = adoptGRef(webkit_settings_new_with_settings("enable-javascript", FALSE,
                                                                                   "auto-load-images", FALSE,
                                                                                   "load-icons-ignoring-image-load-setting", TRUE,
                                                                                   NULL));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(settings.get()));
    g_assert(!webkit_settings_get_enable_javascript(settings.get()));
    g_assert(!webkit_settings_get_auto_load_images(settings.get()));
    g_assert(webkit_settings_get_load_icons_ignoring_image_load_setting(settings.get()));
#endif
}

void beforeAll()
{
    Test::add("WebKitSettings", "webkit-settings", testWebKitSettings);
    Test::add("WebKitSettings", "new-with-settings", testWebKitSettingsNewWithSettings);
}

void afterAll()
{
}

