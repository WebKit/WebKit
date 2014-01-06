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
#include <webkit2/webkit-web-extension.h>

class WebKitFrameTest : public WebProcessTest {
public:
    static PassOwnPtr<WebProcessTest> create() { return adoptPtr(new WebKitFrameTest()); }

private:
    guint64 webPageFromArgs(GVariant* args)
    {
        GVariantIter iter;
        g_variant_iter_init(&iter, args);

        const char* key;
        GVariant* value;
        while (g_variant_iter_loop(&iter, "{&sv}", &key, &value)) {
            if (!strcmp(key, "pageID") && g_variant_classify(value) == G_VARIANT_CLASS_UINT64)
                return g_variant_get_uint64(value);
        }

        g_assert_not_reached();
        return 0;
    }

    bool testMainFrame(WebKitWebExtension* extension, GVariant* args)
    {
        WebKitWebPage* page = webkit_web_extension_get_page(extension, webPageFromArgs(args));
        g_assert(WEBKIT_IS_WEB_PAGE(page));

        WebKitFrame* frame = webkit_web_page_get_main_frame(page);
        g_assert(WEBKIT_IS_FRAME(frame));
        g_assert(webkit_frame_is_main_frame(frame));

        return true;
    }

    bool testURI(WebKitWebExtension* extension, GVariant* args)
    {
        WebKitWebPage* page = webkit_web_extension_get_page(extension, webPageFromArgs(args));
        g_assert(WEBKIT_IS_WEB_PAGE(page));

        WebKitFrame* frame = webkit_web_page_get_main_frame(page);
        g_assert(WEBKIT_IS_FRAME(frame));
        g_assert_cmpstr(webkit_web_page_get_uri(page), ==, webkit_frame_get_uri(frame));

        return true;
    }

    bool testJavaScriptContext(WebKitWebExtension* extension, GVariant* args)
    {
        WebKitWebPage* page = webkit_web_extension_get_page(extension, webPageFromArgs(args));
        g_assert(WEBKIT_IS_WEB_PAGE(page));

        WebKitFrame* frame = webkit_web_page_get_main_frame(page);
        g_assert(WEBKIT_IS_FRAME(frame));
        g_assert(webkit_frame_get_javascript_global_context(frame));

        return true;
    }

    virtual bool runTest(const char* testName, WebKitWebExtension* extension, GVariant* args)
    {
        if (!strcmp(testName, "main-frame"))
            return testMainFrame(extension, args);
        if (!strcmp(testName, "uri"))
            return testURI(extension, args);
        if (!strcmp(testName, "javascript-context"))
            return testJavaScriptContext(extension, args);

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
