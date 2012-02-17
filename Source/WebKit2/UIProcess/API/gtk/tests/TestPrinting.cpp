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
#include <glib/gstdio.h>
#include <wtf/gobject/GRefPtr.h>

#ifdef HAVE_GTK_UNIX_PRINTING
#include <gtk/gtkunixprint.h>
#endif

static char* kTempDirectory;

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

#ifdef HAVE_GTK_UNIX_PRINTING
static void testPrintOperationPrintLoadChanged(WebKitWebView*, WebKitLoadEvent loadEvent, WebViewTest* test)
{
    if (loadEvent != WEBKIT_LOAD_FINISHED)
        return;
    g_main_loop_quit(test->m_mainLoop);
}

static void testPrintOperationPrintFinished(WebKitPrintOperation* printOperation, WebViewTest* test)
{
    g_object_unref(printOperation);
    g_main_loop_quit(test->m_mainLoop);
}

static gboolean testPrintOperationPrintPrinter(GtkPrinter* printer, gpointer userData)
{
    if (strcmp(gtk_printer_get_name(printer), "Print to File"))
        return FALSE;

    GtkPrinter** foundPrinter = static_cast<GtkPrinter**>(userData);
    *foundPrinter = static_cast<GtkPrinter*>(g_object_ref(printer));
    return TRUE;
}

static void testPrintOperationPrint(WebViewTest* test, gconstpointer)
{
    g_signal_connect(test->m_webView, "load-changed", G_CALLBACK(testPrintOperationPrintLoadChanged), test);
    test->loadHtml("<html><body>WebKitGTK+ printing test</body></html>", 0);
    g_main_loop_run(test->m_mainLoop);

    GtkPrinter* printer = 0;
    gtk_enumerate_printers(testPrintOperationPrintPrinter, &printer, 0, TRUE);
    if (!printer) {
        g_message("%s", "Cannot test WebKitPrintOperation/print: no suitable printer found");
        return;
    }

    GOwnPtr<char> outputFilename(g_build_filename(kTempDirectory, "webkit-print.pdf", NULL));
    GRefPtr<GFile> outputFile = adoptGRef(g_file_new_for_path(outputFilename.get()));
    GOwnPtr<char> outputURI(g_file_get_uri(outputFile.get()));

    GRefPtr<GtkPrintSettings> printSettings = adoptGRef(gtk_print_settings_new());
    gtk_print_settings_set_printer(printSettings.get(), gtk_printer_get_name(printer));
    gtk_print_settings_set(printSettings.get(), GTK_PRINT_SETTINGS_OUTPUT_URI, outputURI.get());
    g_object_unref(printer);

    GRefPtr<WebKitPrintOperation> printOperation = webkit_print_operation_new(test->m_webView);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(printOperation.get()));
    g_signal_connect(printOperation.get(), "finished", G_CALLBACK(testPrintOperationPrintFinished), test);
    webkit_print_operation_set_print_settings(printOperation.get(), printSettings.get());
    webkit_print_operation_print(printOperation.get());
    g_main_loop_run(test->m_mainLoop);

    GRefPtr<GFileInfo> fileInfo = adoptGRef(g_file_query_info(outputFile.get(), G_FILE_ATTRIBUTE_STANDARD_SIZE","G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                              static_cast<GFileQueryInfoFlags>(0), 0, 0));
    g_assert(fileInfo.get());
    g_assert_cmpint(g_file_info_get_size(fileInfo.get()), >, 0);
    g_assert_cmpstr(g_file_info_get_content_type(fileInfo.get()), ==, "application/pdf");

    g_file_delete(outputFile.get(), 0, 0);
}
#endif // HAVE_GTK_UNIX_PRINTING

void beforeAll()
{
    kTempDirectory = g_dir_make_tmp("WebKit2Tests-XXXXXX", 0);
    g_assert(kTempDirectory);

    WebViewTest::add("WebKitPrintOperation", "printing-settings", testPrintOperationPrintSettings);
    WebViewTest::add("WebKitWebView", "print-requested", testWebViewPrintRequested);
#ifdef HAVE_GTK_UNIX_PRINTING
    WebViewTest::add("WebKitPrintOperation", "print", testPrintOperationPrint);
#endif
}

void afterAll()
{
    g_rmdir(kTempDirectory);
}
