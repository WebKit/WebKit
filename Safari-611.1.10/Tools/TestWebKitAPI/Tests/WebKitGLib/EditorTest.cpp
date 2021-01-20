/*
 * Copyright (C) 2015 Red Hat Inc.
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

#include "WebProcessTest.h"

class WebKitWebEditorTest : public WebProcessTest {
public:
    static std::unique_ptr<WebProcessTest> create() { return std::unique_ptr<WebProcessTest>(new WebKitWebEditorTest()); }

private:
    static void selectionChangedCallback(bool* selectionChanged)
    {
        *selectionChanged = true;
    }

    bool testSelectionChanged(WebKitWebPage* page)
    {
        bool selectionChanged = false;

        WebKitWebEditor* editor = webkit_web_page_get_editor(page);
        g_assert_true(WEBKIT_IS_WEB_EDITOR(editor));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(editor));
        g_signal_connect_swapped(editor, "selection-changed", G_CALLBACK(selectionChangedCallback), &selectionChanged);

        GRefPtr<JSCContext> jsContext = adoptGRef(webkit_frame_get_js_context(webkit_web_page_get_main_frame(page)));
        g_assert_true(JSC_IS_CONTEXT(jsContext.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(jsContext.get()));

        static const char* steps[] = {
            "document.execCommand('SelectAll')",
            "window.getSelection().collapseToStart()",
            "window.getSelection().modify('move', 'forward', 'character')",
            "window.getSelection().modify('extend', 'forward', 'word')",
            "document.execCommand('Unselect')",
            nullptr
        };

        GRefPtr<JSCValue> result;
        unsigned i = 0;
        while (const char* command = steps[i++]) {
            g_assert_false(selectionChanged);
            result = adoptGRef(jsc_context_evaluate(jsContext.get(), command, -1));
            g_assert_true(JSC_IS_VALUE(result.get()));
            g_assert_true(selectionChanged);
            selectionChanged = false;
        }

        return true;
    }

    bool runTest(const char* testName, WebKitWebPage* page) override
    {
        if (!strcmp(testName, "selection-changed"))
            return testSelectionChanged(page);

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(WebKitWebEditorTest, "WebKitWebEditor/selection-changed");
}
