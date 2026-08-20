/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include <wpe/wpe-platform.h>
#include <wtf/glib/GUniquePtr.h>

class EditorKeyBindingTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(EditorKeyBindingTest);

    void loadContentsAndWait(const char* html, int width = 0, int height = 0)
    {
        showInWindow(width, height);
        loadHtml(html, nullptr);
        waitUntilLoadFinished();
    }

    CString evaluateString(const char* javascript)
    {
        GUniqueOutPtr<GError> error;
        auto* result = runJavaScriptAndWaitUntilFinished(javascript, &error.outPtr());
        g_assert_no_error(error.get());
        g_assert_nonnull(result);
        GUniquePtr<char> string(javascriptResultToCString(result));
        return string.get();
    }

    WPEClipboard* clipboard() const
    {
        auto* view = webkit_web_view_get_wpe_view(m_webView.get());
        g_assert_true(WPE_IS_VIEW(view));
        return wpe_display_get_clipboard(wpe_view_get_display(view));
    }

    // The web process writes to the clipboard without waiting for the UI
    // process, so the copy is only visible here once the change lands.
    CString copyWithKeyStroke(unsigned keyVal, OptionSet<Modifiers> modifiers)
    {
        auto changeCount = wpe_clipboard_get_change_count(clipboard());
        keyStroke(keyVal, modifiers);

        unsigned triesCount = 20;
        while (wpe_clipboard_get_change_count(clipboard()) == changeCount && triesCount--)
            wait(0.1);

        gsize length;
        GUniquePtr<char> text(wpe_clipboard_read_text(clipboard(), "text/plain;charset=utf-8", &length));
        return text ? CString(std::span<const char>(text.get(), length)) : CString();
    }
};

static const char* selectedSpanHTML =
    "<html><body>"
    "<span id=\"source\">All work and no play <span id=\"selected\">make Jack a dull</span> boy.</span>"
    "<script>getSelection().selectAllChildren(document.getElementById('selected'));</script>"
    "</body></html>";

static void testEditorCopyKeyBindingNonEditable(EditorKeyBindingTest* test, gconstpointer)
{
    test->loadContentsAndWait(selectedSpanHTML);

    auto copied = test->copyWithKeyStroke(KEY(c), { WebViewTest::Modifiers::Control });
    g_assert_cmpstr(copied.data(), ==, "make Jack a dull");
}

static void testEditorCopyKeyBindingEditable(EditorKeyBindingTest* test, gconstpointer)
{
    test->loadContentsAndWait(
        "<html><body>"
        "<input id=\"source\" value=\"and no play\">"
        "<script>const source = document.getElementById('source');"
        "source.focus(); source.setSelectionRange(0, source.value.length);</script>"
        "</body></html>");

    auto copied = test->copyWithKeyStroke(KEY(c), { WebViewTest::Modifiers::Control });
    g_assert_cmpstr(copied.data(), ==, "and no play");
}

static void testEditorSelectAllKeyBindingNonEditable(EditorKeyBindingTest* test, gconstpointer)
{
    test->loadContentsAndWait(selectedSpanHTML);

    auto selection = test->evaluateString("getSelection().toString();");
    g_assert_cmpstr(selection.data(), ==, "make Jack a dull");

    test->keyStroke(KEY(a), { WebViewTest::Modifiers::Control });
    selection = test->evaluateString("getSelection().toString().trim();");
    g_assert_cmpstr(selection.data(), ==, "All work and no play make Jack a dull boy.");
}

// A command the selection does not allow has to fall through to the page, or
// the arrow keys would stop scrolling everything that is not editable.
static void testEditorArrowKeysStillScroll(EditorKeyBindingTest* test, gconstpointer)
{
    test->loadContentsAndWait("<html><body style=\"height: 5000px\">All work and no play.</body></html>", 200, 200);

    test->keyStroke(KEY(Down));
    test->assertJavaScriptBecomesTrue("window.scrollY > 0");
}

void beforeAll()
{
    EditorKeyBindingTest::add("WebKitWebView", "editor-copy-key-binding-non-editable", testEditorCopyKeyBindingNonEditable);
    EditorKeyBindingTest::add("WebKitWebView", "editor-copy-key-binding-editable", testEditorCopyKeyBindingEditable);
    EditorKeyBindingTest::add("WebKitWebView", "editor-select-all-key-binding-non-editable", testEditorSelectAllKeyBindingNonEditable);
    EditorKeyBindingTest::add("WebKitWebView", "editor-arrow-keys-still-scroll", testEditorArrowKeysStillScroll);
}

void afterAll()
{
}
