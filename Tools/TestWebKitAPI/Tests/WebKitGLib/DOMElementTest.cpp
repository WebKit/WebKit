/*
 * Copyright (C) 2018 Igalia S.L.
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
#include <wtf/glib/GUniquePtr.h>

class DOMElementTest : public WebProcessTest {
public:
    static std::unique_ptr<WebProcessTest> create() { return std::unique_ptr<WebProcessTest>(new DOMElementTest()); }

private:
    bool testAutoFill(WebKitWebPage* page)
    {
        WebKitFrame* frame = webkit_web_page_get_main_frame(page);
        g_assert_true(WEBKIT_IS_FRAME(frame));

        GRefPtr<JSCContext> jsContext = adoptGRef(webkit_frame_get_js_context(frame));
        g_assert_true(JSC_IS_CONTEXT(jsContext.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(jsContext.get()));

        GRefPtr<JSCValue> jsInputElement = adoptGRef(jsc_context_evaluate(jsContext.get(), "document.getElementById('auto-fill')", -1));
        g_assert_true(JSC_IS_VALUE(jsInputElement.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(jsInputElement.get()));
        g_assert_true(jsc_value_is_object(jsInputElement.get()));
        g_assert_true(jsc_value_object_is_instance_of(jsInputElement.get(), "HTMLInputElement"));
        g_assert_false(webkit_web_form_manager_input_element_is_auto_filled(jsInputElement.get()));

        GRefPtr<JSCValue> value = adoptGRef(jsc_value_object_get_property(jsInputElement.get(), "value"));
        g_assert_true(JSC_IS_VALUE(value.get()));
        g_assert_true(jsc_value_is_string(value.get()));
        GUniquePtr<char> valueString(jsc_value_to_string(value.get()));
        g_assert_cmpstr(valueString.get(), ==, "");

        webkit_web_form_manager_input_element_auto_fill(jsInputElement.get(), "auto filled value");
        value = adoptGRef(jsc_value_object_get_property(jsInputElement.get(), "value"));
        g_assert_true(JSC_IS_VALUE(value.get()));
        g_assert_true(jsc_value_is_string(value.get()));
        valueString.reset(jsc_value_to_string(value.get()));
        g_assert_cmpstr(valueString.get(), ==, "auto filled value");
        g_assert_true(webkit_web_form_manager_input_element_is_auto_filled(jsInputElement.get()));

        return true;
    }

    bool runTest(const char* testName, WebKitWebPage* page) override
    {
        if (!strcmp(testName, "auto-fill"))
            return testAutoFill(page);

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(DOMElementTest, "WebKitDOMElement/auto-fill");
}
