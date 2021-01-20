/*
 * Copyright (C) 2012, 2019 Igalia S.L.
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

#include "TestMain.h"

// The libatspi headers don't use G_BEGIN_DECLS
extern "C" {
#include <atspi/atspi.h>
}

class AccessibilityTest : public Test {
public:
    MAKE_GLIB_TEST_FIXTURE(AccessibilityTest);

    AccessibilityTest()
    {
        GUniquePtr<char> testServerPath(g_build_filename(WEBKIT_EXEC_PATH, "TestWebKitAPI", "WebKit2Gtk", "AccessibilityTestServer", nullptr));
        char* args[3];
        args[0] = testServerPath.get();
        args[1] = const_cast<char*>(g_dbus_server_get_client_address(s_dbusServer.get()));
        args[2] = nullptr;

        g_assert_true(g_spawn_async(nullptr, args, nullptr, G_SPAWN_DEFAULT, nullptr, nullptr, &m_childProcessID, nullptr));
    }

    ~AccessibilityTest()
    {
        if (m_childProcessID) {
            g_spawn_close_pid(m_childProcessID);
            kill(m_childProcessID, SIGTERM);
        }
    }

    void loadHTMLAndWaitUntilFinished(const char* html, const char* baseURI)
    {
        ensureProxy();

        GUniqueOutPtr<GError> error;
        GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(m_proxy.get(), "LoadHTML",
            g_variant_new("(ss)", html, baseURI ? baseURI : ""), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error.outPtr()));
        g_assert_no_error(error.get());
    }

    GRefPtr<AtspiAccessible> findTestServerApplication()
    {
        // Only one desktop is supported by ATSPI at the moment.
        GRefPtr<AtspiAccessible> desktop = adoptGRef(atspi_get_desktop(0));

        int childCount = atspi_accessible_get_child_count(desktop.get(), nullptr);
        for (int i = 0; i < childCount; ++i) {
            GRefPtr<AtspiAccessible> current = adoptGRef(atspi_accessible_get_child_at_index(desktop.get(), i, nullptr));
            if (!g_strcmp0(atspi_accessible_get_name(current.get(), nullptr), "AccessibilityTestServer"))
                return current;
        }

        return 0;
    }

    GRefPtr<AtspiAccessible> findDocumentWeb(AtspiAccessible* accessible)
    {
        int childCount = atspi_accessible_get_child_count(accessible, nullptr);
        for (int i = 0; i < childCount; ++i) {
            GRefPtr<AtspiAccessible> child = adoptGRef(atspi_accessible_get_child_at_index(accessible, i, nullptr));
            if (atspi_accessible_get_role(child.get(), nullptr) == ATSPI_ROLE_DOCUMENT_WEB)
                return child;

            if (auto documentWeb = findDocumentWeb(child.get()))
                return documentWeb;
        }
        return nullptr;
    }

    GRefPtr<AtspiAccessible> findRootObject(AtspiAccessible* application)
    {
        // Find the document web, its parent is the scroll view (WebCore root object) and its parent is
        // the GtkPlug (WebProcess root element).
        auto documentWeb = findDocumentWeb(application);
        if (!documentWeb)
            return nullptr;

        auto parent = adoptGRef(atspi_accessible_get_parent(documentWeb.get(), nullptr));
        return parent ? adoptGRef(atspi_accessible_get_parent(parent.get(), nullptr)) : nullptr;
    }

    void waitUntilChildrenRemoved(AtspiAccessible* accessible)
    {
        m_eventSource = accessible;
        GRefPtr<AtspiEventListener> listener = adoptGRef(atspi_event_listener_new(
            [](AtspiEvent* event, gpointer userData) {
                auto* test = static_cast<AccessibilityTest*>(userData);
                if (event->source == test->m_eventSource)
                    g_main_loop_quit(test->m_mainLoop.get());
        }, this, nullptr));
        atspi_event_listener_register(listener.get(), "object:children-changed:remove", nullptr);
        g_main_loop_run(m_mainLoop.get());
        m_eventSource = nullptr;
    }

private:
    void ensureProxy()
    {
        if (m_proxy)
            return;

        m_mainLoop = adoptGRef(g_main_loop_new(nullptr, FALSE));

        if (s_dbusConnections.isEmpty()) {
            g_idle_add([](gpointer userData) -> gboolean {
                if (s_dbusConnections.isEmpty())
                    return TRUE;

                g_main_loop_quit(static_cast<GMainLoop*>(userData));
                return FALSE;
            }, m_mainLoop.get());
            g_main_loop_run(m_mainLoop.get());
        }

        m_proxy = adoptGRef(g_dbus_proxy_new_sync(s_dbusConnections[0].get(), static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS),
            nullptr, nullptr, "/org/webkit/gtk/AccessibilityTest", "org.webkit.gtk.AccessibilityTest", nullptr, nullptr));
        g_assert_true(G_IS_DBUS_PROXY(m_proxy.get()));
    }

    GPid m_childProcessID { 0 };
    GRefPtr<GDBusProxy> m_proxy;
    GRefPtr<GMainLoop> m_mainLoop;
    AtspiAccessible* m_eventSource { nullptr };
};

static void testAtspiBasicHierarchy(AccessibilityTest* test, gconstpointer)
{
    test->loadHTMLAndWaitUntilFinished(
        "<html>"
        "  <body>"
        "   <h1>This is a test</h1>"
        "   <p>This is a paragraph with some plain text.</p>"
        "   <p>This paragraph contains <a href=\"http://www.webkitgtk.org\">a link</a> in the middle.</p>"
        "  </body>"
        "</html>",
        nullptr);

    auto testServerApp = test->findTestServerApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testServerApp.get()));
    GUniquePtr<char> name(atspi_accessible_get_name(testServerApp.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "AccessibilityTestServer");
    g_assert_cmpint(atspi_accessible_get_role(testServerApp.get(), nullptr), ==, ATSPI_ROLE_APPLICATION);

    auto rootObject = test->findRootObject(testServerApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(rootObject.get()));
    g_assert_cmpint(atspi_accessible_get_role(rootObject.get(), nullptr), ==, ATSPI_ROLE_FILLER);

    auto scrollView = adoptGRef(atspi_accessible_get_child_at_index(rootObject.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(scrollView.get()));
    g_assert_cmpint(atspi_accessible_get_role(scrollView.get(), nullptr), ==, ATSPI_ROLE_SCROLL_PANE);

    auto documentWeb = adoptGRef(atspi_accessible_get_child_at_index(scrollView.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_role(documentWeb.get(), nullptr), ==, ATSPI_ROLE_DOCUMENT_WEB);

    auto h1 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(h1.get()));
    name.reset(atspi_accessible_get_name(h1.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "This is a test");
    g_assert_cmpint(atspi_accessible_get_role(h1.get(), nullptr), ==, ATSPI_ROLE_HEADING);

    auto p1 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(p1.get()));
    g_assert_cmpint(atspi_accessible_get_role(p1.get(), nullptr), ==, ATSPI_ROLE_PARAGRAPH);

    auto p2 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 2, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(p2.get()));
    g_assert_cmpint(atspi_accessible_get_role(p2.get(), nullptr), ==, ATSPI_ROLE_PARAGRAPH);

    auto link = adoptGRef(atspi_accessible_get_child_at_index(p2.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(link.get()));
    name.reset(atspi_accessible_get_name(link.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "a link");
    g_assert_cmpint(atspi_accessible_get_role(link.get(), nullptr), ==, ATSPI_ROLE_LINK);

    test->loadHTMLAndWaitUntilFinished(
        "<html>"
        "  <body>"
        "   <h1>This is another test</h1>"
        "   <img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3AYWDTMVwnSZnwAAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBkLmUHAAAAFklEQVQI12P8z8DAwMDAxMDAwMDAAAANHQEDK+mmyAAAAABJRU5ErkJggg=='/>"
        "  </body>"
        "</html>",
        nullptr);

    // Check that children-changed::remove is emitted on the root object on navigation,
    // and the a11y hierarchy is updated.
    test->waitUntilChildrenRemoved(rootObject.get());

    documentWeb = test->findDocumentWeb(testServerApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_role(documentWeb.get(), nullptr), ==, ATSPI_ROLE_DOCUMENT_WEB);

    h1 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(h1.get()));
    name.reset(atspi_accessible_get_name(h1.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "This is another test");
    g_assert_cmpint(atspi_accessible_get_role(h1.get(), nullptr), ==, ATSPI_ROLE_HEADING);

    auto section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_role(section.get(), nullptr), ==, ATSPI_ROLE_SECTION);

    auto img = adoptGRef(atspi_accessible_get_child_at_index(section.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(img.get()));
    g_assert_cmpint(atspi_accessible_get_role(img.get(), nullptr), ==, ATSPI_ROLE_IMAGE);
}

void beforeAll()
{
    AccessibilityTest::add("WebKitAccessibility", "atspi-basic-hierarchy", testAtspiBasicHierarchy);
}

void afterAll()
{
}
