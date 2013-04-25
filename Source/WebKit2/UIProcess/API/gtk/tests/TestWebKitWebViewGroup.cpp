/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#include "WebViewTest.h"
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <wtf/gobject/GRefPtr.h>

static void testWebViewGroupDefault(Test* test, gconstpointer)
{
    // Default group is shared by all WebViews by default.
    GRefPtr<WebKitWebView> webView1 = WEBKIT_WEB_VIEW(webkit_web_view_new());
    GRefPtr<WebKitWebView> webView2 = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_assert(webkit_web_view_get_group(webView1.get()) == webkit_web_view_get_group(webView2.get()));

    // Settings are shared by all web view in the same group.
    g_assert(webkit_web_view_get_settings(webView1.get()) == webkit_web_view_get_settings(webView2.get()));
    g_assert(webkit_web_view_get_settings(webView1.get()) == webkit_web_view_group_get_settings(webkit_web_view_get_group(webView2.get())));
}

static void testWebViewGroupNewGroup(Test* test, gconstpointer)
{
    // Passing 0 as group name generates the name automatically.
    GRefPtr<WebKitWebViewGroup> viewGroup1 = adoptGRef(webkit_web_view_group_new(0));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(viewGroup1.get()));
    g_assert(webkit_web_view_group_get_name(viewGroup1.get()));

    // New group with a given name.
    GRefPtr<WebKitWebViewGroup> viewGroup2 = adoptGRef(webkit_web_view_group_new("TestGroup2"));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(viewGroup2.get()));
    g_assert_cmpstr(webkit_web_view_group_get_name(viewGroup2.get()), ==, "TestGroup2");
    g_assert_cmpstr(webkit_web_view_group_get_name(viewGroup2.get()), !=, webkit_web_view_group_get_name(viewGroup1.get()));

    // Every group has its own settings.
    g_assert(webkit_web_view_group_get_settings(viewGroup1.get()) != webkit_web_view_group_get_settings(viewGroup2.get()));
}

static void testWebViewNewWithGroup(Test* test, gconstpointer)
{
    GRefPtr<WebKitWebViewGroup> viewGroup1 = adoptGRef(webkit_web_view_group_new("TestGroup1"));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(viewGroup1.get()));
    GRefPtr<WebKitWebView> webView1 = WEBKIT_WEB_VIEW(webkit_web_view_new_with_group(viewGroup1.get()));
    g_assert(webkit_web_view_get_group(webView1.get()) == viewGroup1.get());

    GRefPtr<WebKitWebView> webView2 = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_assert(webkit_web_view_get_group(webView2.get()) != viewGroup1.get());

    // Settings should be different for views in different groups.
    g_assert(webkit_web_view_get_settings(webView1.get()) != webkit_web_view_get_settings(webView2.get()));
}

static void testWebViewGroupSettings(Test* test, gconstpointer)
{
    GRefPtr<WebKitWebViewGroup> viewGroup1 = adoptGRef(webkit_web_view_group_new("TestGroup1"));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(viewGroup1.get()));
    GRefPtr<WebKitSettings> newSettings = adoptGRef(webkit_settings_new_with_settings("enable-javascript", FALSE, NULL));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(newSettings.get()));
    webkit_web_view_group_set_settings(viewGroup1.get(), newSettings.get());
    g_assert(webkit_web_view_group_get_settings(viewGroup1.get()) == newSettings.get());

    GRefPtr<WebKitWebView> webView1 = WEBKIT_WEB_VIEW(webkit_web_view_new_with_group(viewGroup1.get()));
    GRefPtr<WebKitWebView> webView2 = WEBKIT_WEB_VIEW(webkit_web_view_new());
    WebKitSettings* webView1Settings = webkit_web_view_get_settings(webView1.get());
    WebKitSettings* webView2Settings = webkit_web_view_get_settings(webView2.get());
    g_assert(webView1Settings != webView2Settings);
    g_assert(webkit_settings_get_enable_javascript(webView1Settings) != webkit_settings_get_enable_javascript(webView2Settings));

    webkit_web_view_set_settings(webView1.get(), webView2Settings);
    g_assert(webkit_web_view_get_settings(webView1.get()) == webView2Settings);
    g_assert(webkit_web_view_group_get_settings(webkit_web_view_get_group(webView1.get())) == webView2Settings);
}

void beforeAll()
{
    Test::add("WebKitWebViewGroup", "default-group", testWebViewGroupDefault);
    Test::add("WebKitWebViewGroup", "new-group", testWebViewGroupNewGroup);
    Test::add("WebKitWebView", "new-with-group", testWebViewNewWithGroup);
    Test::add("WebKitWebViewGroup", "settings", testWebViewGroupSettings);
}

void afterAll()
{

}
