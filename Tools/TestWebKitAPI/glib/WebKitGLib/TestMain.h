/*
 * Copyright (C) 2011 Igalia S.L.
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

#pragma once

#include <cairo.h>
#include <glib-object.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

#if PLATFORM(GTK) && USE(GTK4)
#include <webkit/webkit.h>
#elif PLATFORM(GTK)
#include <webkit2/webkit2.h>
#elif PLATFORM(WPE)
#include <WPEToolingBackends/HeadlessViewBackend.h>
#include <wpe/webkit.h>
#endif

#define TEST_PATH_FORMAT "/webkit/%s/%s"

#define MAKE_GLIB_TEST_FIXTURE(ClassName) \
    static void setUp(ClassName* fixture, gconstpointer data) \
    { \
        new (fixture) ClassName; \
    } \
    static void tearDown(ClassName* fixture, gconstpointer data) \
    { \
        fixture->~ClassName(); \
    } \
    static void add(const char* suiteName, const char* testName, void (*testFunc)(ClassName*, const void*)) \
    { \
        GUniquePtr<gchar> testPath(g_strdup_printf(TEST_PATH_FORMAT, suiteName, testName)); \
        g_test_add(testPath.get(), ClassName, 0, ClassName::setUp, testFunc, ClassName::tearDown); \
    }

#define MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(ClassName, setup, teardown) \
    static void setUp(ClassName* fixture, gconstpointer data) \
    { \
        setup(); \
        new (fixture) ClassName; \
    } \
    static void tearDown(ClassName* fixture, gconstpointer data) \
    { \
        fixture->~ClassName(); \
        teardown(); \
    } \
    static void add(const char* suiteName, const char* testName, void (*testFunc)(ClassName*, const void*)) \
    { \
        GUniquePtr<gchar> testPath(g_strdup_printf(TEST_PATH_FORMAT, suiteName, testName)); \
        g_test_add(testPath.get(), ClassName, 0, ClassName::setUp, testFunc, ClassName::tearDown); \
    }

#define ASSERT_CMP_CSTRING(s1, cmp, s2) \
    do {                                                                 \
        CString __s1 = (s1);                                             \
        CString __s2 = (s2);                                             \
        if (g_strcmp0(__s1.data(), __s2.data()) cmp 0) ;                 \
        else {                                                           \
            g_assertion_message_cmpstr(G_LOG_DOMAIN, __FILE__, __LINE__, \
                G_STRFUNC, #s1 " " #cmp " " #s2, __s1.data(), #cmp, __s2.data()); \
        }                                                                \
    } while (0)

#if !defined(g_assert_cmpfloat_with_epsilon)
#define g_assert_cmpfloat_with_epsilon(n1,n2,epsilon)                   \
    do {                                                                \
        double __n1 = (n1);                                             \
        double __n2 = (n2);                                             \
        double __epsilon = (epsilon);                                   \
        if ((((__n1) > (__n2) ? (__n1) - (__n2) : (__n2) - (__n1)) < (__epsilon))) ; \
        else {                                                          \
            g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, \
                G_STRFUNC, #n1 " == " #n2 " (+/- " #epsilon ")", __n1, "==", __n2, 'f'); \
        }                                                               \
    } while(0)
#endif

class Test {
public:
    MAKE_GLIB_TEST_FIXTURE(Test);

    static GRefPtr<WebKitWebView> adoptView(gpointer view)
    {
        g_assert_true(WEBKIT_IS_WEB_VIEW(view));
#if PLATFORM(GTK)
        g_assert_true(g_object_is_floating(view));
        return GRefPtr<WebKitWebView>(WEBKIT_WEB_VIEW(view));
#elif PLATFORM(WPE)
        return adoptGRef(WEBKIT_WEB_VIEW(view));
#endif
    }

    static const char* dataDirectory();

    static void initializeWebProcessExtensionsCallback(WebKitWebContext* context, Test* test)
    {
        test->initializeWebProcessExtensions();
    }

    Test()
    {
#if ENABLE(2022_GLIB_API)
        m_networkSession = adoptGRef(webkit_network_session_new(dataDirectory(), dataDirectory()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_networkSession.get()));

        m_webContext = adoptGRef(WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, "memory-pressure-settings", s_memoryPressureSettings, nullptr)));
#else
        GRefPtr<WebKitWebsiteDataManager> websiteDataManager = adoptGRef(webkit_website_data_manager_new(
            "base-data-directory", dataDirectory(),
            "base-cache-directory", dataDirectory(),
            nullptr));

        m_webContext = adoptGRef(WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT,
            "website-data-manager", websiteDataManager.get(),
#if PLATFORM(GTK)
            "process-swap-on-cross-site-navigation-enabled", TRUE,
            "use-system-appearance-for-scrollbars", FALSE,
#endif
            "memory-pressure-settings", s_memoryPressureSettings,
            nullptr)));
#endif
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_webContext.get()));
#if ENABLE(2022_GLIB_API)
        g_signal_connect(m_webContext.get(), "initialize-web-process-extensions", G_CALLBACK(initializeWebProcessExtensionsCallback), this);
#else
        g_signal_connect(m_webContext.get(), "initialize-web-extensions", G_CALLBACK(initializeWebProcessExtensionsCallback), this);
#endif
    }

    virtual ~Test()
    {
        g_signal_handlers_disconnect_matched(m_webContext.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        m_webContext = nullptr;
#if ENABLE(2022_GLIB_API)
        m_networkSession = nullptr;
#endif
        if (m_watchedObjects.isEmpty())
            return;

        g_print("Leaked objects:");
        HashSet<GObject*>::const_iterator end = m_watchedObjects.end();
        for (HashSet<GObject*>::const_iterator it = m_watchedObjects.begin(); it != end; ++it)
            g_print(" %s(%p - %u left)", g_type_name_from_instance(reinterpret_cast<GTypeInstance*>(*it)), *it, (*it)->ref_count);
        g_print("\n");

        g_assert_true(m_watchedObjects.isEmpty());
    }

    virtual void initializeWebProcessExtensions()
    {
#if ENABLE(2022_GLIB_API)
        webkit_web_context_set_web_process_extensions_directory(m_webContext.get(), WEBKIT_TEST_WEB_PROCESS_EXTENSIONS_DIR);
        webkit_web_context_set_web_process_extensions_initialization_user_data(m_webContext.get(),
            g_variant_new("(ss)", g_dbus_server_get_guid(s_dbusServer.get()), g_dbus_server_get_client_address(s_dbusServer.get())));
#else
        webkit_web_context_set_web_extensions_directory(m_webContext.get(), WEBKIT_TEST_WEB_PROCESS_EXTENSIONS_DIR);
        webkit_web_context_set_web_extensions_initialization_user_data(m_webContext.get(),
            g_variant_new("(ss)", g_dbus_server_get_guid(s_dbusServer.get()), g_dbus_server_get_client_address(s_dbusServer.get())));
#endif
    }

#if PLATFORM(WPE)
    static WebKitWebViewBackend* createWebViewBackend()
    {
        // Don't make warnings fatal when creating the backend, since atk produces warnings when a11y bus is not running.
        removeLogFatalFlag(G_LOG_LEVEL_WARNING);
        auto* headlessBackend = new WPEToolingBackends::HeadlessViewBackend(800, 600);
        addLogFatalFlag(G_LOG_LEVEL_WARNING);
        // Make the view initially hidden for consistency with GTK+ tests.
        wpe_view_backend_remove_activity_state(headlessBackend->backend(), wpe_view_activity_state_visible | wpe_view_activity_state_focused);
        return webkit_web_view_backend_new(headlessBackend->backend(), [](gpointer userData) {
            delete static_cast<WPEToolingBackends::HeadlessViewBackend*>(userData);
        }, headlessBackend);
    }
#endif

    static WebKitWebView* createWebView()
    {
#if PLATFORM(GTK)
        return WEBKIT_WEB_VIEW(webkit_web_view_new());
#elif PLATFORM(WPE)
        return webkit_web_view_new(createWebViewBackend());
#endif
    }

    static WebKitWebView* createWebView(WebKitWebContext* context)
    {
        return WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
                                            "backend", createWebViewBackend(),
#endif
                                            "web-context", context,
                                            nullptr));
    }

    static WebKitWebView* createWebView(WebKitWebView* relatedView)
    {
        return WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
            "backend", createWebViewBackend(),
#endif
            "related-view", relatedView,
            nullptr));
    }

    static WebKitWebView* createWebView(WebKitUserContentManager* contentManager)
    {
        return WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
                                            "backend", createWebViewBackend(),
#endif
                                            "user-content-manager", contentManager,
                                            nullptr));
    }

    static WebKitWebView* createWebView(WebKitSettings* settings)
    {
        return WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
                                            "backend", createWebViewBackend(),
#endif
                                            "settings", settings,
                                            nullptr));
    }

    static void objectFinalized(Test* test, GObject* finalizedObject)
    {
        test->m_watchedObjects.remove(finalizedObject);
    }

    void assertObjectIsDeletedWhenTestFinishes(GObject* object)
    {
        m_watchedObjects.add(object);
        g_object_weak_ref(object, reinterpret_cast<GWeakNotify>(objectFinalized), this);
    }


    enum ResourcesDir {
        WebKitGLibResources,
        WebKit2Resources,
    };

    static CString getResourcesDir(ResourcesDir resourcesDir = WebKitGLibResources)
    {
        switch (resourcesDir) {
        case WebKitGLibResources: {
            GUniquePtr<char> resourcesDir(g_build_filename(WEBKIT_SRC_DIR, "Tools", "TestWebKitAPI", "Tests", "WebKitGLib", "resources", nullptr));
            return resourcesDir.get();
        }
        case WebKit2Resources: {
            GUniquePtr<char> resourcesDir(g_build_filename(WEBKIT_SRC_DIR, "Tools", "TestWebKitAPI", "Tests", "WebKit", nullptr));
            return resourcesDir.get();
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    static void addLogFatalFlag(unsigned flag)
    {
        unsigned fatalMask = g_log_set_always_fatal(static_cast<GLogLevelFlags>(G_LOG_FATAL_MASK));
        fatalMask |= flag;
        g_log_set_always_fatal(static_cast<GLogLevelFlags>(fatalMask));
    }

    static void removeLogFatalFlag(unsigned flag)
    {
        unsigned fatalMask = g_log_set_always_fatal(static_cast<GLogLevelFlags>(G_LOG_FATAL_MASK));
        fatalMask &= ~flag;
        g_log_set_always_fatal(static_cast<GLogLevelFlags>(fatalMask));
    }

    static bool cairoSurfacesEqual(cairo_surface_t* s1, cairo_surface_t* s2)
    {
        return (cairo_image_surface_get_format(s1) == cairo_image_surface_get_format(s2)
            && cairo_image_surface_get_width(s1) == cairo_image_surface_get_width(s2)
            && cairo_image_surface_get_height(s1) == cairo_image_surface_get_height(s2)
            && cairo_image_surface_get_stride(s1) == cairo_image_surface_get_stride(s2)
            && !memcmp(const_cast<const void*>(reinterpret_cast<void*>(cairo_image_surface_get_data(s1))),
                const_cast<const void*>(reinterpret_cast<void*>(cairo_image_surface_get_data(s2))),
                cairo_image_surface_get_height(s1)*cairo_image_surface_get_stride(s1)));
    }

    HashSet<GObject*> m_watchedObjects;
    GRefPtr<WebKitWebContext> m_webContext;
#if ENABLE(2022_GLIB_API)
    GRefPtr<WebKitNetworkSession> m_networkSession;
#endif
    static GRefPtr<GDBusServer> s_dbusServer;
    static Vector<GRefPtr<GDBusConnection>> s_dbusConnections;
    static HashMap<uint64_t, GDBusConnection*> s_dbusConnectionPageMap;
    static WebKitMemoryPressureSettings* s_memoryPressureSettings;
};
