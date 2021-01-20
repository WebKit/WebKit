/*
 * Copyright (C) 2017 Aidan Holm <aidanholm@gmail.com>
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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

class WebKitDOMClientRectTest : public WebProcessTest {
public:
    static std::unique_ptr<WebProcessTest> create() { return std::unique_ptr<WebKitDOMClientRectTest>(new WebKitDOMClientRectTest()); }

private:
    static void checkClientRectPosition(WebKitDOMClientRect* clientRect)
    {
        // Expected values correspond to CSS in TestDOMClientRect.cpp
        g_assert_cmpfloat(webkit_dom_client_rect_get_top(clientRect), ==, -25);
        g_assert_cmpfloat(webkit_dom_client_rect_get_right(clientRect), ==, 50);
        g_assert_cmpfloat(webkit_dom_client_rect_get_bottom(clientRect), ==, 175);
        g_assert_cmpfloat(webkit_dom_client_rect_get_left(clientRect), ==, -50);
        g_assert_cmpfloat(webkit_dom_client_rect_get_width(clientRect), ==, 100);
        g_assert_cmpfloat(webkit_dom_client_rect_get_height(clientRect), ==, 200);
    }


    bool testDivBoundingClientRectPosition(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        WebKitDOMElement* div = webkit_dom_document_get_element_by_id(document, "rect");
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(div));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(div));

        GRefPtr<WebKitDOMClientRect> clientRect = adoptGRef(webkit_dom_element_get_bounding_client_rect(div));
        g_assert_true(WEBKIT_DOM_IS_CLIENT_RECT(clientRect.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(clientRect.get()));
        checkClientRectPosition(clientRect.get());

        return true;
    }

    bool testDivClientRectsPositionAndLength(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        WebKitDOMElement* div = webkit_dom_document_get_element_by_id(document, "rect");
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(div));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(div));

        GRefPtr<WebKitDOMClientRectList> clientRectList = adoptGRef(webkit_dom_element_get_client_rects(div));
        g_assert_true(WEBKIT_DOM_IS_CLIENT_RECT_LIST(clientRectList.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(clientRectList.get()));

        g_assert_cmpuint(webkit_dom_client_rect_list_get_length(clientRectList.get()), ==, 1);

        GRefPtr<WebKitDOMClientRect> clientRect = adoptGRef(webkit_dom_client_rect_list_item(clientRectList.get(), 0));
        g_assert_true(WEBKIT_DOM_IS_CLIENT_RECT(clientRect.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(clientRect.get()));
        checkClientRectPosition(clientRect.get());

        // Getting the clientRect twice should return the same pointer.
        GRefPtr<WebKitDOMClientRect> clientRect2 = adoptGRef(webkit_dom_client_rect_list_item(clientRectList.get(), 0));
        g_assert_true(clientRect.get() == clientRect2.get());

        return true;
    }

    bool runTest(const char* testName, WebKitWebPage* page) override
    {
        if (!strcmp(testName, "div-bounding-client-rect-position"))
            return testDivBoundingClientRectPosition(page);
        if (!strcmp(testName, "div-client-rects-position-and-length"))
            return testDivClientRectsPositionAndLength(page);

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(WebKitDOMClientRectTest, "WebKitDOMClientRect/div-bounding-client-rect-position");
    REGISTER_TEST(WebKitDOMClientRectTest, "WebKitDOMClientRect/div-client-rects-position-and-length");
}

G_GNUC_END_IGNORE_DEPRECATIONS;
