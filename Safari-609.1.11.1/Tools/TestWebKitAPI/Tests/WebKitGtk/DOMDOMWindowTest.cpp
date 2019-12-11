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
#include <wtf/RunLoop.h>

class WebKitDOMDOMWindowTest;
static gboolean loadedCallback(WebKitDOMDOMWindow*, WebKitDOMEvent*, WebKitDOMDOMWindowTest*);
static gboolean clickedCallback(WebKitDOMDOMWindow*, WebKitDOMEvent*, WebKitDOMDOMWindowTest*);

class WebKitDOMDOMWindowTest : public WebProcessTest {
public:
    static std::unique_ptr<WebProcessTest> create() { return makeUnique<WebKitDOMDOMWindowTest>(); }

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

    bool testSignals(WebKitWebExtension* extension, GVariant* args)
    {
        notify("ready", "");

        WebKitWebPage* page = webkit_web_extension_get_page(extension, webPageFromArgs(args));
        g_assert_true(WEBKIT_IS_WEB_PAGE(page));
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));

        WebKitDOMDOMWindow* domWindow = webkit_dom_document_get_default_view(document);
        g_assert_nonnull(domWindow);

        // The "load" WebKitDOMDOMWindow signal is issued before this test is
        // called. There's no way to capture it here. We simply assume that
        // the document is loaded and notify the uiprocess accordingly
        // notify("loaded", "");

        webkit_dom_event_target_add_event_listener(
            WEBKIT_DOM_EVENT_TARGET(domWindow),
            "load",
            G_CALLBACK(loadedCallback),
            false,
            this);

        // loadedCallback() will stop this loop
        RunLoop::run();

        document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));

        WebKitDOMElement* element = webkit_dom_document_get_element_by_id(document, "test");
        g_assert_nonnull(element);

        webkit_dom_event_target_add_event_listener(
            WEBKIT_DOM_EVENT_TARGET(element),
            "click",
            G_CALLBACK(clickedCallback),
            false,
            this);

        // The "click" action will be issued in the uiprocess and that will
        // trigger the dom event here.
        // clickedCallback() will stop this loop
        RunLoop::run();

        return true;
    }

    bool testDispatchEvent(WebKitWebExtension* extension, GVariant* args)
    {
        notify("ready", "");

        WebKitWebPage* page = webkit_web_extension_get_page(extension, webPageFromArgs(args));
        g_assert_true(WEBKIT_IS_WEB_PAGE(page));
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));

        WebKitDOMDOMWindow* domWindow = webkit_dom_document_get_default_view(document);
        g_assert_nonnull(domWindow);

        // The "load" WebKitDOMDOMWindow signal is issued before this test is
        // called. There's no way to capture it here. We simply assume that
        // the document is loaded and notify the uiprocess accordingly
        // notify("loaded", "");

        webkit_dom_event_target_add_event_listener(
            WEBKIT_DOM_EVENT_TARGET(domWindow),
            "load",
            G_CALLBACK(loadedCallback),
            false,
            this);

        // loadedCallback() will stop this loop
        RunLoop::run();

        document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));

        WebKitDOMElement* element = webkit_dom_document_get_element_by_id(document, "test");
        g_assert_nonnull(element);

        WebKitDOMEvent* event = webkit_dom_document_create_event(document, "MouseEvent", 0);
        g_assert_nonnull(event);
        g_assert_true(WEBKIT_DOM_IS_EVENT(event));
        g_assert_true(WEBKIT_DOM_IS_MOUSE_EVENT(event));

        glong clientX, clientY;
        clientX = webkit_dom_element_get_client_left(element);
        clientY = webkit_dom_element_get_client_top(element);

        webkit_dom_event_target_add_event_listener(
            WEBKIT_DOM_EVENT_TARGET(element),
            "click",
            G_CALLBACK(clickedCallback),
            false,
            this);

        webkit_dom_mouse_event_init_mouse_event(WEBKIT_DOM_MOUSE_EVENT(event),
            "click", TRUE, TRUE,
            domWindow, 0, 0, 0, clientX, clientY,
            FALSE, FALSE, FALSE, FALSE,
            1, WEBKIT_DOM_EVENT_TARGET(element));

        webkit_dom_event_target_dispatch_event(WEBKIT_DOM_EVENT_TARGET(element), event, 0);

        // clickedCallback() will stop this loop
        RunLoop::run();

        return true;
    }

    bool testGetComputedStyle(WebKitWebExtension* extension, GVariant* args)
    {
        WebKitWebPage* page = webkit_web_extension_get_page(extension, webPageFromArgs(args));
        g_assert_true(WEBKIT_IS_WEB_PAGE(page));
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        WebKitDOMDOMWindow* domWindow = webkit_dom_document_get_default_view(document);
        g_assert_nonnull(domWindow);
        WebKitDOMElement* element = webkit_dom_document_get_element_by_id(document, "test");
        g_assert_nonnull(element);
        g_assert_true(WEBKIT_DOM_IS_ELEMENT(element));
        WebKitDOMCSSStyleDeclaration* cssStyle = webkit_dom_dom_window_get_computed_style(domWindow, element, 0);
        gchar* fontSize = webkit_dom_css_style_declaration_get_property_value(cssStyle, "font-size");
        g_assert_cmpstr(fontSize, ==, "16px");

        return true;
    }

    virtual bool runTest(const char* testName, WebKitWebExtension* extension, GVariant* args)
    {
        if (!strcmp(testName, "signals"))
            return testSignals(extension, args);
        if (!strcmp(testName, "dispatch-event"))
            return testDispatchEvent(extension, args);
        if (!strcmp(testName, "get-computed-style"))
            return testGetComputedStyle(extension, args);

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(WebKitDOMDOMWindowTest, "WebKitDOMDOMWindow/signals");
    REGISTER_TEST(WebKitDOMDOMWindowTest, "WebKitDOMDOMWindow/dispatch-event");
    REGISTER_TEST(WebKitDOMDOMWindowTest, "WebKitDOMDOMWindow/get-computed-style");
}

static gboolean loadedCallback(WebKitDOMDOMWindow* view, WebKitDOMEvent* event, WebKitDOMDOMWindowTest* test)
{
    test->notify("loaded", "");

    // Stop the loop and let testSignals() or testDispatchEvent() continue its course
    RunLoop::current().stop();

    return FALSE;
}

static gboolean clickedCallback(WebKitDOMDOMWindow* view, WebKitDOMEvent* event, WebKitDOMDOMWindowTest* test)
{
    test->notify("clicked", "");
    test->notify("finish", "");

    // Stop the loop and let testSignals() or testDispatchEvent() continue its course
    RunLoop::current().stop();

    return FALSE;
}
