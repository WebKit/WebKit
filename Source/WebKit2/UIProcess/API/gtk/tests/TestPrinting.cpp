/*
 * Copyright (C) 2012 Igalia S.L.
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
#include <wtf/gobject/GRefPtr.h>

static void testPrintOperationPrintSettings(WebViewTest* test, gconstpointer)
{
    GRefPtr<WebKitPrintOperation> printOperation = adoptGRef(webkit_print_operation_new(test->m_webView));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(printOperation.get()));

    g_assert(!webkit_print_operation_get_print_settings(printOperation.get()));
    g_assert(!webkit_print_operation_get_page_setup(printOperation.get()));

    GRefPtr<GtkPrintSettings> printSettings = adoptGRef(gtk_print_settings_new());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(printSettings.get()));

    GRefPtr<GtkPageSetup> pageSetup = adoptGRef(gtk_page_setup_new());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(pageSetup.get()));

    webkit_print_operation_set_print_settings(printOperation.get(), printSettings.get());
    webkit_print_operation_set_page_setup(printOperation.get(), pageSetup.get());

    g_assert(webkit_print_operation_get_print_settings(printOperation.get()) == printSettings.get());
    g_assert(webkit_print_operation_get_page_setup(printOperation.get()) == pageSetup.get());
}

static gboolean webViewPrintRequestedCallback(WebKitWebView* webView, WebKitPrintOperation* printOperation, WebViewTest* test)
{
    g_assert(webView == test->m_webView);

    g_assert(WEBKIT_IS_PRINT_OPERATION(printOperation));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(printOperation));

    g_assert(!webkit_print_operation_get_print_settings(printOperation));
    g_assert(!webkit_print_operation_get_page_setup(printOperation));

    g_main_loop_quit(test->m_mainLoop);

    return TRUE;
}

static void testWebViewPrintRequested(WebViewTest* test, gconstpointer)
{
    g_signal_connect(test->m_webView, "print-requested", G_CALLBACK(webViewPrintRequestedCallback), test);
    test->loadHtml("<html><body onLoad=\"print();\">WebKitGTK+ printing test</body></html>", 0);
    g_main_loop_run(test->m_mainLoop);
}

void beforeAll()
{
    WebViewTest::add("WebKitPrintOperation", "printing-settings", testPrintOperationPrintSettings);
    WebViewTest::add("WebKitWebView", "print-requested", testWebViewPrintRequested);
}

void afterAll()
{
}
