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
#include <wtf/glib/GRefPtr.h>

class Clipboard {
public:
    Clipboard(GMainLoop* loop)
        : m_mainLoop(loop)
#if USE(GTK4)
        , m_clipboard(gdk_display_get_clipboard(gdk_display_get_default()))
#else
        , m_clipboard(gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD))
#endif
    {
    }

    void clear()
    {
#if USE(GTK4)
        g_assert_true(gdk_clipboard_set_content(m_clipboard, nullptr));
#else
        gtk_clipboard_clear(m_clipboard);
#endif
        m_text = nullptr;
    }

    bool containsText() const
    {
#if USE(GTK4)
        return gdk_content_formats_contain_gtype(gdk_clipboard_get_formats(m_clipboard), G_TYPE_STRING);
#else
        return gtk_clipboard_wait_is_text_available(m_clipboard);
#endif
    }

    bool waitForText()
    {
        // There's no way to know when the selection has been written to
        // the clipboard, so use a timeout source to query the clipboard.
        static const unsigned kClipboardWaitTimeout = 50;
        static const unsigned kClipboardWaitMaxTries = 2;
        m_triesCount = 0;
        g_timeout_add(kClipboardWaitTimeout, [](gpointer userData) -> gboolean {
            auto* clipboard = static_cast<Clipboard*>(userData);
            clipboard->m_triesCount++;
            if (clipboard->containsText() || clipboard->m_triesCount > kClipboardWaitMaxTries) {
                g_main_loop_quit(clipboard->m_mainLoop);
                return FALSE;
            }
            return TRUE;
        }, this);
        g_main_loop_run(m_mainLoop);
        return containsText();
    }

    const char* readText()
    {
        m_text = nullptr;
        if (waitForText()) {
#if USE(GTK4)
            gdk_clipboard_read_text_async(m_clipboard, nullptr, [](GObject* gdkClipboard, GAsyncResult* result, gpointer userData) {
                auto* clipboard = static_cast<Clipboard*>(userData);
                clipboard->m_text.reset(gdk_clipboard_read_text_finish(GDK_CLIPBOARD(gdkClipboard), result, nullptr));
                g_main_loop_quit(clipboard->m_mainLoop);
            }, this);
            g_main_loop_run(m_mainLoop);
#else
            m_text.reset(gtk_clipboard_wait_for_text(m_clipboard));
#endif
        }
        return m_text.get();
    }

private:
    GMainLoop* m_mainLoop { nullptr };
#if USE(GTK4)
    GdkClipboard* m_clipboard { nullptr };
#else
    GtkClipboard* m_clipboard { nullptr };
#endif
    GUniquePtr<char> m_text;
    unsigned m_triesCount { 0 };
};

class EditorTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(EditorTest);

    EditorTest()
        : m_clipboard(m_mainLoop)
    {
        showInWindow();
        m_clipboard.clear();
        loadURI("about:blank");
        waitUntilLoadFinished();
    }

    ~EditorTest()
    {
        m_clipboard.clear();
    }

    static gboolean webViewDrawCallback(GMainLoop* mainLoop)
    {
        g_main_loop_quit(mainLoop);
        return FALSE;
    }

    void flushEditorState()
    {
#if USE(GTK4)
        auto* surface = gtk_native_get_surface(gtk_widget_get_native(GTK_WIDGET(m_webView)));
        auto* clock = gdk_surface_get_frame_clock(surface);
        auto signalID = g_signal_connect_swapped(clock, "paint", G_CALLBACK(webViewDrawCallback), m_mainLoop);
#else
        auto signalID = g_signal_connect_swapped(m_webView, "draw", G_CALLBACK(webViewDrawCallback), m_mainLoop);
#endif
        gtk_widget_queue_draw(GTK_WIDGET(m_webView));
        g_main_loop_run(m_mainLoop);
#if USE(GTK4)
        g_signal_handler_disconnect(clock, signalID);
#else
        g_signal_handler_disconnect(m_webView, signalID);
#endif
    }

    static void canExecuteEditingCommandReadyCallback(GObject*, GAsyncResult* result, EditorTest* test)
    {
        GUniqueOutPtr<GError> error;
        test->m_canExecuteEditingCommand = webkit_web_view_can_execute_editing_command_finish(test->m_webView, result, &error.outPtr());
        g_assert_no_error(error.get());
        g_main_loop_quit(test->m_mainLoop);
    }

    bool canExecuteEditingCommand(const char* command)
    {
        m_canExecuteEditingCommand = false;
        webkit_web_view_can_execute_editing_command(m_webView, command, 0, reinterpret_cast<GAsyncReadyCallback>(canExecuteEditingCommandReadyCallback), this);
        g_main_loop_run(m_mainLoop);

        if (!strcmp(command, WEBKIT_EDITING_COMMAND_CUT))
            g_assert_cmpint(m_canExecuteEditingCommand, ==, webkit_editor_state_is_cut_available(editorState()));
        else if (!strcmp(command, WEBKIT_EDITING_COMMAND_COPY))
            g_assert_cmpint(m_canExecuteEditingCommand, ==, webkit_editor_state_is_copy_available(editorState()));
        else if (!strcmp(command, WEBKIT_EDITING_COMMAND_PASTE) || !strcmp(command, WEBKIT_EDITING_COMMAND_PASTE_AS_PLAIN_TEXT))
            g_assert_cmpint(m_canExecuteEditingCommand, ==, webkit_editor_state_is_paste_available(editorState()));
        // FIXME: Figure out how to add tests for undo and redo. It will probably require using user
        // scripts to message the UI process when the content has been altered.
        else if (!strcmp(command, WEBKIT_EDITING_COMMAND_UNDO))
            g_assert_cmpint(m_canExecuteEditingCommand, ==, webkit_editor_state_is_undo_available(editorState()));
        else if (!strcmp(command, WEBKIT_EDITING_COMMAND_REDO))
            g_assert_cmpint(m_canExecuteEditingCommand, ==, webkit_editor_state_is_redo_available(editorState()));

        return m_canExecuteEditingCommand;
    }

    const char* copyClipboard()
    {
        webkit_web_view_execute_editing_command(m_webView, WEBKIT_EDITING_COMMAND_COPY);
        return m_clipboard.readText();
    }

    const char* cutSelection()
    {
        g_assert_true(canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
        g_assert_true(canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));
        g_assert_true(canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE_AS_PLAIN_TEXT));

        webkit_web_view_execute_editing_command(m_webView, WEBKIT_EDITING_COMMAND_CUT);
        return m_clipboard.readText();
    }

    WebKitEditorState* editorState()
    {
        if (m_editorState)
            return m_editorState;

        m_editorState = webkit_web_view_get_editor_state(m_webView);
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_editorState));
        return m_editorState;
    }

    static void quitMainLoopInCallback(EditorTest* test)
    {
        g_main_loop_quit(test->m_mainLoop);
    }

    unsigned typingAttributes()
    {
        return webkit_editor_state_get_typing_attributes(editorState());
    }

    unsigned waitUntilTypingAttributesChanged()
    {
        unsigned long handlerID = g_signal_connect_swapped(editorState(), "notify::typing-attributes", G_CALLBACK(quitMainLoopInCallback), this);
        g_main_loop_run(m_mainLoop);
        g_signal_handler_disconnect(m_editorState, handlerID);
        return typingAttributes();
    }

    Clipboard m_clipboard;
    bool m_canExecuteEditingCommand { false };
    WebKitEditorState* m_editorState { nullptr };
};

static const char* selectedSpanHTMLFormat =
    "<html><body contentEditable=\"%s\">"
    "<span id=\"mainspan\">All work and no play <span id=\"subspan\">make Jack a dull</span> boy.</span>"
    "<script>document.getSelection().removeAllRanges();\n"
    "document.getSelection().selectAllChildren(document.getElementById('subspan'));\n"
    "</script></body></html>";

static void testWebViewEditorCutCopyPasteNonEditable(EditorTest* test, gconstpointer)
{
    // Nothing loaded yet.
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_COPY));
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE_AS_PLAIN_TEXT));

    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();
    test->flushEditorState();

    g_assert_true(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_COPY));
    // It's not possible to cut and paste when content is not editable
    // even if there's a selection.
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE_AS_PLAIN_TEXT));

    g_assert_cmpstr(test->copyClipboard(), ==, "make Jack a dull");
}

static void testWebViewEditorCutCopyPasteEditable(EditorTest* test, gconstpointer)
{
    // Nothing loaded yet.
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_COPY));
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE_AS_PLAIN_TEXT));

    g_assert_false(test->isEditable());
    test->setEditable(true);
    g_assert_true(test->isEditable());

    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();
    test->flushEditorState();

    // There's a selection.
    g_assert_true(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
    g_assert_true(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_COPY));
    g_assert_true(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));
    g_assert_true(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE_AS_PLAIN_TEXT));

    g_assert_cmpstr(test->copyClipboard(), ==, "make Jack a dull");
}

static void testWebViewEditorSelectAllNonEditable(EditorTest* test, gconstpointer)
{
    g_assert_true(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_SELECT_ALL));

    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();
    test->flushEditorState();

    g_assert_true(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_SELECT_ALL));

    // Initially only the subspan is selected.
    g_assert_cmpstr(test->copyClipboard(), ==, "make Jack a dull");

    webkit_web_view_execute_editing_command(test->m_webView, WEBKIT_EDITING_COMMAND_SELECT_ALL);
    // The mainspan should be selected after calling SELECT_ALL.
    g_assert_cmpstr(test->copyClipboard(), ==, "All work and no play make Jack a dull boy.");
}

static void testWebViewEditorSelectAllEditable(EditorTest* test, gconstpointer)
{
    g_assert_true(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_SELECT_ALL));

    g_assert_false(test->isEditable());
    test->setEditable(true);
    g_assert_true(test->isEditable());

    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();
    test->flushEditorState();

    g_assert_true(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_SELECT_ALL));

    // Initially only the subspan is selected.
    g_assert_cmpstr(test->copyClipboard(), ==, "make Jack a dull");

    webkit_web_view_execute_editing_command(test->m_webView, WEBKIT_EDITING_COMMAND_SELECT_ALL);
    // The mainspan should be selected after calling SELECT_ALL.
    g_assert_cmpstr(test->copyClipboard(), ==, "All work and no play make Jack a dull boy.");
}

static void loadContentsAndTryToCutSelection(EditorTest* test, bool contentEditable)
{
    // View is not editable by default.
    g_assert_false(test->isEditable());

    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, contentEditable ? "true" : "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();
    test->flushEditorState();

    g_assert_false(test->isEditable());
    test->setEditable(true);
    g_assert_true(test->isEditable());

    test->flushEditorState();

    // Cut the selection to the clipboard to see if the view is indeed editable.
    g_assert_cmpstr(test->cutSelection(), ==, "make Jack a dull");

    // Reset the editable for next test.
    test->setEditable(false);
    g_assert_false(test->isEditable());
}

static void testWebViewEditorNonEditable(EditorTest* test)
{
    GUniquePtr<char> selectedSpanHTML(g_strdup_printf(selectedSpanHTMLFormat, "false"));
    test->loadHtml(selectedSpanHTML.get(), nullptr);
    test->waitUntilLoadFinished();
    test->flushEditorState();

    g_assert_false(test->isEditable());
    test->setEditable(true);
    g_assert_true(test->isEditable());
    test->setEditable(false);
    g_assert_false(test->isEditable());

    // Check if view is indeed non-editable.
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_CUT));
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE));
    g_assert_false(test->canExecuteEditingCommand(WEBKIT_EDITING_COMMAND_PASTE_AS_PLAIN_TEXT));
}

static void testWebViewEditorEditable(EditorTest* test, gconstpointer)
{
    testWebViewEditorNonEditable(test);

    // Reset the editable for next test.
    test->setEditable(false);
    g_assert_false(test->isEditable());

    loadContentsAndTryToCutSelection(test, true);

    // Reset the editable for next test.
    test->setEditable(false);
    g_assert_false(test->isEditable());

    loadContentsAndTryToCutSelection(test, false);
}

static void testWebViewEditorEditorStateTypingAttributes(EditorTest* test, gconstpointer)
{
    static const char* typingAttributesHTML =
        "<html><body>"
        "normal <b>bold </b><i>italic </i><u>underline </u><strike>strike </strike>"
        "<b><i>boldanditalic </i></b>"
        "</body></html>";

    test->loadHtml(typingAttributesHTML, nullptr);
    test->waitUntilLoadFinished();
    test->flushEditorState();
    test->setEditable(true);

    unsigned typingAttributes = test->typingAttributes();
    g_assert_cmpuint(typingAttributes, ==, WEBKIT_EDITOR_TYPING_ATTRIBUTE_NONE);

    webkit_web_view_execute_editing_command(test->m_webView, "MoveWordForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH);

    webkit_web_view_execute_editing_command(test->m_webView, "MoveWordForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD);
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH);

    webkit_web_view_execute_editing_command(test->m_webView, "MoveWordForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC);
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH);

    webkit_web_view_execute_editing_command(test->m_webView, "MoveWordForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE);
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH);

    webkit_web_view_execute_editing_command(test->m_webView, "MoveWordForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD);
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH);

    // Selections.
    webkit_web_view_execute_editing_command(test->m_webView, "MoveToBeginningOfDocument");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveWordForwardAndModifySelection");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_cmpuint(typingAttributes, ==, WEBKIT_EDITOR_TYPING_ATTRIBUTE_NONE);

    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveWordForwardAndModifySelection");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH);

    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveWordForwardAndModifySelection");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD);
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH);

    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveWordForwardAndModifySelection");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC);
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH);

    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveWordForwardAndModifySelection");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE);
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH);

    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveForward");
    webkit_web_view_execute_editing_command(test->m_webView, "MoveWordForwardAndModifySelection");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD);
    g_assert_true(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE);
    g_assert_false(typingAttributes & WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH);

    webkit_web_view_execute_editing_command(test->m_webView, "SelectAll");
    typingAttributes = test->waitUntilTypingAttributesChanged();
    g_assert_cmpuint(typingAttributes, ==, WEBKIT_EDITOR_TYPING_ATTRIBUTE_NONE);
}

static void testWebViewEditorInsertImage(EditorTest* test, gconstpointer)
{
    test->loadHtml("<html><body></body></html>", "file:///");
    test->waitUntilLoadFinished();
    test->flushEditorState();
    test->setEditable(true);

    GUniquePtr<char> imagePath(g_build_filename(Test::getResourcesDir().data(), "blank.ico", nullptr));
    GUniquePtr<char> imageURI(g_filename_to_uri(imagePath.get(), nullptr, nullptr));
    webkit_web_view_execute_editing_command_with_argument(test->m_webView, WEBKIT_EDITING_COMMAND_INSERT_IMAGE, imageURI.get());
    GUniqueOutPtr<GError> error;
    WebKitJavascriptResult* javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('IMG')[0].src", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    GUniquePtr<char> resultString(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(resultString.get(), ==, imageURI.get());
}

static void testWebViewEditorCreateLink(EditorTest* test, gconstpointer)
{
    test->loadHtml("<html><body onload=\"document.getSelection().selectAllChildren(document.body);\">webkitgtk.org</body></html>", nullptr);
    test->waitUntilLoadFinished();
    test->flushEditorState();
    test->setEditable(true);

    static const char* webkitGTKURL = "http://www.webkitgtk.org/";
    webkit_web_view_execute_editing_command_with_argument(test->m_webView, WEBKIT_EDITING_COMMAND_CREATE_LINK, webkitGTKURL);
    GUniqueOutPtr<GError> error;
    WebKitJavascriptResult* javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('A')[0].href;", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    GUniquePtr<char> resultString(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(resultString.get(), ==, webkitGTKURL);
    javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('A')[0].innerText;", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    resultString.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(resultString.get(), ==, "webkitgtk.org");

    // When there isn't text selected, the URL is used as link text.
    webkit_web_view_execute_editing_command(test->m_webView, "MoveToEndOfLine");
    webkit_web_view_execute_editing_command_with_argument(test->m_webView, WEBKIT_EDITING_COMMAND_CREATE_LINK, webkitGTKURL);
    javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('A')[1].href;", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    resultString.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(resultString.get(), ==, webkitGTKURL);
    javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('A')[1].innerText;", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    resultString.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(resultString.get(), ==, webkitGTKURL);
}

void beforeAll()
{
    EditorTest::add("WebKitWebView", "editable/editable", testWebViewEditorEditable);
    EditorTest::add("WebKitWebView", "cut-copy-paste/non-editable", testWebViewEditorCutCopyPasteNonEditable);
    EditorTest::add("WebKitWebView", "cut-copy-paste/editable", testWebViewEditorCutCopyPasteEditable);
    EditorTest::add("WebKitWebView", "select-all/non-editable", testWebViewEditorSelectAllNonEditable);
    EditorTest::add("WebKitWebView", "select-all/editable", testWebViewEditorSelectAllEditable);
    EditorTest::add("WebKitWebView", "editor-state/typing-attributes", testWebViewEditorEditorStateTypingAttributes);
    EditorTest::add("WebKitWebView", "insert/image", testWebViewEditorInsertImage);
    EditorTest::add("WebKitWebView", "insert/link", testWebViewEditorCreateLink);
}

void afterAll()
{
}
