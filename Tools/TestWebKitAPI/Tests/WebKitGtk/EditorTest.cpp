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

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

class WebKitWebEditorTest : public WebProcessTest {
public:
    static std::unique_ptr<WebProcessTest> create() { return std::unique_ptr<WebProcessTest>(new WebKitWebEditorTest()); }

private:
    static void selectionChangedCallback(bool* selectionChanged)
    {
        *selectionChanged = true;
    }

    void testSelectionSelectAll(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert(WEBKIT_DOM_IS_DOCUMENT(document));

        webkit_dom_document_exec_command(document, "SelectAll", false, "");
    }

    void testSelectionCollapse(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert(WEBKIT_DOM_IS_DOCUMENT(document));
        GRefPtr<WebKitDOMDOMWindow> domWindow = adoptGRef(webkit_dom_document_get_default_view(document));
        g_assert(WEBKIT_DOM_IS_DOM_WINDOW(domWindow.get()));
        GRefPtr<WebKitDOMDOMSelection> domSelection = adoptGRef(webkit_dom_dom_window_get_selection(domWindow.get()));
        g_assert(WEBKIT_DOM_IS_DOM_SELECTION(domSelection.get()));

        webkit_dom_dom_selection_collapse_to_start(domSelection.get(), nullptr);
    }

    void testSelectionModifyMove(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert(WEBKIT_DOM_IS_DOCUMENT(document));
        GRefPtr<WebKitDOMDOMWindow> domWindow = adoptGRef(webkit_dom_document_get_default_view(document));
        g_assert(WEBKIT_DOM_IS_DOM_WINDOW(domWindow.get()));
        GRefPtr<WebKitDOMDOMSelection> domSelection = adoptGRef(webkit_dom_dom_window_get_selection(domWindow.get()));
        g_assert(WEBKIT_DOM_IS_DOM_SELECTION(domSelection.get()));

        webkit_dom_dom_selection_modify(domSelection.get(), "move", "forward", "character");
    }

    void testSelectionModifyExtend(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert(WEBKIT_DOM_IS_DOCUMENT(document));
        GRefPtr<WebKitDOMDOMWindow> domWindow = adoptGRef(webkit_dom_document_get_default_view(document));
        g_assert(WEBKIT_DOM_IS_DOM_WINDOW(domWindow.get()));
        GRefPtr<WebKitDOMDOMSelection> domSelection = adoptGRef(webkit_dom_dom_window_get_selection(domWindow.get()));
        g_assert(WEBKIT_DOM_IS_DOM_SELECTION(domSelection.get()));

        webkit_dom_dom_selection_modify(domSelection.get(), "extend", "forward", "word");
    }

    void testSelectionUnselect(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert(WEBKIT_DOM_IS_DOCUMENT(document));

        webkit_dom_document_exec_command(document, "Unselect", false, "");
    }

    bool runTest(const char* testName, WebKitWebPage* page) override
    {
        if (!strcmp(testName, "selection-changed")) {
            bool selectionChanged = false;

            WebKitWebEditor* editor = webkit_web_page_get_editor(page);
            g_assert(WEBKIT_IS_WEB_EDITOR(editor));
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(editor));
            g_signal_connect_swapped(editor, "selection-changed", G_CALLBACK(selectionChangedCallback), &selectionChanged);

            testSelectionSelectAll(page);
            g_assert(selectionChanged);

            selectionChanged = false;
            testSelectionCollapse(page);
            g_assert(selectionChanged);

            selectionChanged = false;
            testSelectionModifyMove(page);
            g_assert(selectionChanged);

            selectionChanged = false;
            testSelectionModifyExtend(page);
            g_assert(selectionChanged);

            selectionChanged = false;
            testSelectionUnselect(page);
            g_assert(selectionChanged);

            return true;
        }

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(WebKitWebEditorTest, "WebKitWebEditor/selection-changed");
}
