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
#include <gio/gio.h>

class WebKitFrameTest : public WebProcessTest {
public:
    static std::unique_ptr<WebProcessTest> create() { return std::unique_ptr<WebProcessTest>(new WebKitFrameTest()); }

private:
    bool testMainFrame(WebKitWebPage* page)
    {
        WebKitFrame* frame = webkit_web_page_get_main_frame(page);
        g_assert(WEBKIT_IS_FRAME(frame));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(frame));
        g_assert(webkit_frame_is_main_frame(frame));

        return true;
    }

    bool testURI(WebKitWebPage* page)
    {
        WebKitFrame* frame = webkit_web_page_get_main_frame(page);
        g_assert(WEBKIT_IS_FRAME(frame));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(frame));
        g_assert_cmpstr(webkit_web_page_get_uri(page), ==, webkit_frame_get_uri(frame));

        return true;
    }

    bool testJavaScriptContext(WebKitWebPage* page)
    {
        WebKitFrame* frame = webkit_web_page_get_main_frame(page);
        g_assert(WEBKIT_IS_FRAME(frame));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(frame));
        g_assert(webkit_frame_get_javascript_global_context(frame));

        return true;
    }

    bool runTest(const char* testName, WebKitWebPage* page) override
    {
        if (!strcmp(testName, "main-frame"))
            return testMainFrame(page);
        if (!strcmp(testName, "uri"))
            return testURI(page);
        if (!strcmp(testName, "javascript-context"))
            return testJavaScriptContext(page);

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(WebKitFrameTest, "WebKitFrame/main-frame");
    REGISTER_TEST(WebKitFrameTest, "WebKitFrame/uri");
    REGISTER_TEST(WebKitFrameTest, "WebKitFrame/javascript-context");
}
