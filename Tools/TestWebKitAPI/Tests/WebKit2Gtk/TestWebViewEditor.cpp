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

class EditorTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(EditorTest);

    static const unsigned kClipboardWaitTimeout = 50;
    static const unsigned kClipboardWaitMaxTries = 2;

    EditorTest()
        : m_clipboard(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD))
        , m_canExecuteEditingCommand(false)
        , m_triesCount(0)
    {
        gtk_clipboard_clear(m_clipboard);
    }

    static void canExecuteEditingCommandReadyCallback(GObject*, GAsyncResult* result, EditorTest* test)
    {
        GUniqueOutPtr<GError> error;
        test->m_canExecuteEditingCommand = webkit_web_view_can_execute_editing_command_finish(test->m_webView, result, &error.outPtr());
        g_assert(!error.get());
        g_main_loop_quit(test->m_mainLoop);
    }

    bool canExecuteEditingCommand(const char* command)
    {
        m_canExecuteEditingCommand = false;
        webkit_web_view_can_execute_editing_command(m_webView, command, 0, reinterpret_cast<GAsyncReadyCallback>(canExecuteEditingCommandReadyCallback), this);
        g_main_loop_run(m_mainLoop);
        return m_canExecuteEditingCommand;
    }

    static gboolean waitForClipboardText(EditorTest* test)
    {
        test->m_triesCount++;
        if (gtk_clipboard_wait_is_text_available(test->m_clipboard) || test->m_triesCount > kClipboardWaitMaxTries) {
            g_main_loop_quit(test->m_mainLoop);
            return FALSE;
        }

        return TRUE;
    }

    void copyClipboard()
    {
        webkit_web_view_execute_editing_command(m_webView, WEBKIT_EDITING_COMMAND_COPY);
        // There's no way to know when the selection has been copied to
        // the clipboard, so use a timeout source to query the clipboard.
        m_triesCount = 0;
        g_timeout_add(kClipboardWaitTimeout, reinterpret_cast<GSourceFunc>(waitForClipboardText), this);
        g_main_loop_run(m_mainLoop);
    }

    gchar* cutSelection()
    {
        g_assert(canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
        g_assert(canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));

        webkit_web_view_execute_editing_command(m_webView, WEBKIT_EDITING_COMMAND_CUT);
        // There's no way to know when the selection has been cut to
        // the clipboard, so use a timeout source to query the clipboard.
        m_triesCount = 0;
        g_timeout_add(kClipboardWaitTimeout, reinterpret_cast<GSourceFunc>(waitForClipboardText), this);
        g_main_loop_run(m_mainLoop);

        return gtk_clipboard_wait_for_text (m_clipboard);
    }

    GtkClipboard* m_clipboard;
    bool m_canExecuteEditingCommand;
    size_t m_triesCount;
};

static const char* selectedSpanHTMLFormat =
    "<html><body contentEditable=\"%s\">"
    "<span id=\"mainspan\">All work and no play <span id=\"subspan\">make Jack a dull</span> boy.</span>"
    "<script>document.getSelection().collapse();\n"
    "document.getSelection().selectAllChildren(document.getElementById('subspan'));\n"
    "</script></body></html>";

static void testWebViewEditorCutCopyPasteNonEditable(EditorTest* test, gconstpointer)
{
    // Nothing loaded yet.
    g_assert(!test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
    g_assert(!test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_COPY));
    g_assert(!test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));

    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();

    g_assert(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_COPY));
    // It's not possible to cut and paste when content is not editable
    // even if there's a selection.
    g_assert(!test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
    g_assert(!test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));

    test->copyClipboard();
    GUniquePtr<char> clipboardText(gtk_clipboard_wait_for_text(test->m_clipboard));
    g_assert_cmpstr(clipboardText.get(), ==, "make Jack a dull");
}

static void testWebViewEditorCutCopyPasteEditable(EditorTest* test, gconstpointer)
{
    // Nothing loaded yet.
    g_assert(!test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
    g_assert(!test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_COPY));
    g_assert(!test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));

    g_assert(!test->isEditable());
    test->setEditable(true);
    g_assert(test->isEditable());

    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();

    // There's a selection.
    g_assert(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
    g_assert(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_COPY));
    g_assert(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));

    test->copyClipboard();
    GUniquePtr<char> clipboardText(gtk_clipboard_wait_for_text(test->m_clipboard));
    g_assert_cmpstr(clipboardText.get(), ==, "make Jack a dull");
}

static void testWebViewEditorSelectAllNonEditable(EditorTest* test, gconstpointer)
{
    g_assert(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_SELECT_ALL));

    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();

    g_assert(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_SELECT_ALL));

    test->copyClipboard();
    GUniquePtr<char> clipboardText(gtk_clipboard_wait_for_text(test->m_clipboard));

    // Initially only the subspan is selected.
    g_assert_cmpstr(clipboardText.get(), ==, "make Jack a dull");

    webkit_web_view_execute_editing_command(test->m_webView, WEBKIT_EDITING_COMMAND_SELECT_ALL);
    test->copyClipboard();
    clipboardText.reset(gtk_clipboard_wait_for_text(test->m_clipboard));

    // The mainspan should be selected after calling SELECT_ALL.
    g_assert_cmpstr(clipboardText.get(), ==, "All work and no play make Jack a dull boy.");
}

static void testWebViewEditorSelectAllEditable(EditorTest* test, gconstpointer)
{
    g_assert(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_SELECT_ALL));

    g_assert(!test->isEditable());
    test->setEditable(true);
    g_assert(test->isEditable());

    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();

    g_assert(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_SELECT_ALL));

    test->copyClipboard();
    GUniquePtr<char> clipboardText(gtk_clipboard_wait_for_text(test->m_clipboard));

    // Initially only the subspan is selected.
    g_assert_cmpstr(clipboardText.get(), ==, "make Jack a dull");

    webkit_web_view_execute_editing_command(test->m_webView, WEBKIT_EDITING_COMMAND_SELECT_ALL);
    test->copyClipboard();
    clipboardText.reset(gtk_clipboard_wait_for_text(test->m_clipboard));

    // The mainspan should be selected after calling SELECT_ALL.
    g_assert_cmpstr(clipboardText.get(), ==, "All work and no play make Jack a dull boy.");
}

static void loadContentsAndTryToCutSelection(EditorTest* test, bool contentEditable)
{
    // View is not editable by default.
    g_assert(!test->isEditable());

    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, contentEditable ? "true" : "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();

    g_assert(!test->isEditable());
    test->setEditable(true);
    g_assert(test->isEditable());

    // Cut the selection to the clipboard to see if the view is indeed editable.
    GUniquePtr<char> clipboardText(test->cutSelection());
    g_assert_cmpstr(clipboardText.get(), ==, "make Jack a dull");

    // Reset the editable for next test.
    test->setEditable(false);
    g_assert(!test->isEditable());
}

static void testWebViewEditorNonEditable(EditorTest* test)
{
    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();

    g_assert(!test->isEditable());
    test->setEditable(true);
    g_assert(test->isEditable());
    test->setEditable(false);
    g_assert(!test->isEditable());

    // Check if view is indeed non-editable.
    g_assert(!test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
    g_assert(!test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));
}

static void testWebViewEditorEditable(EditorTest* test, gconstpointer)
{
    testWebViewEditorNonEditable(test);

    // Reset the editable for next test.
    test->setEditable(false);
    g_assert(!test->isEditable());

    loadContentsAndTryToCutSelection(test, true);

    // Reset the editable for next test.
    test->setEditable(false);
    g_assert(!test->isEditable());

    loadContentsAndTryToCutSelection(test, false);
}

void beforeAll()
{
    EditorTest::add("WebKitWebView", "editable/editable", testWebViewEditorEditable);
    EditorTest::add("WebKitWebView", "cut-copy-paste/non-editable", testWebViewEditorCutCopyPasteNonEditable);
    EditorTest::add("WebKitWebView", "cut-copy-paste/editable", testWebViewEditorCutCopyPasteEditable);
    EditorTest::add("WebKitWebView", "select-all/non-editable", testWebViewEditorSelectAllNonEditable);
    EditorTest::add("WebKitWebView", "select-all/editable", testWebViewEditorSelectAllEditable);
}

void afterAll()
{
}
