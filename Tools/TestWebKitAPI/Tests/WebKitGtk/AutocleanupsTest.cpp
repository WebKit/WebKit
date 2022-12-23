/*
 * Copyright (C) 2013 Igalia S.L.
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

#if USE(GTK4)
#include <webkit/webkit-web-extension.h>
#else
#include <webkit2/webkit-web-extension.h>
#endif

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC

class AutocleanupsTest : public WebProcessTest {
public:
    static std::unique_ptr<WebProcessTest> create() { return std::unique_ptr<WebProcessTest>(new AutocleanupsTest()); }

private:
    bool testWebProcessAutocleanups(WebKitWebPage* webPage)
    {
        // Transfer none
        g_autoptr(WebKitWebPage) page = WEBKIT_WEB_PAGE(g_object_ref(G_OBJECT(webPage)));
        g_assert_true(WEBKIT_IS_WEB_PAGE(page));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(page));

#if !USE(GTK4)
        // Transfer none
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        g_autoptr(WebKitDOMDocument) document = WEBKIT_DOM_DOCUMENT(g_object_ref(G_OBJECT(webkit_web_page_get_dom_document(page))));
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        G_GNUC_END_IGNORE_DEPRECATIONS;
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        // Transfer full
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        g_autoptr(WebKitDOMDOMWindow) window = webkit_dom_document_get_default_view(document);
        g_assert_true(WEBKIT_DOM_IS_DOM_WINDOW(window));
        G_GNUC_END_IGNORE_DEPRECATIONS;
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(window));

        // Transfer full
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        g_autoptr(WebKitDOMRange) range = webkit_dom_document_create_range(document);
        g_assert_true(WEBKIT_DOM_IS_RANGE(range));
        G_GNUC_END_IGNORE_DEPRECATIONS;
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(range));
#endif

        return true;
    }

    bool runTest(const char* testName, WebKitWebPage* page) override
    {
        if (!strcmp(testName, "web-process-autocleanups"))
            return testWebProcessAutocleanups(page);

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(AutocleanupsTest, "Autocleanups/web-process-autocleanups");
}

#endif // G_DEFINE_AUTOPTR_CLEANUP_FUNC
