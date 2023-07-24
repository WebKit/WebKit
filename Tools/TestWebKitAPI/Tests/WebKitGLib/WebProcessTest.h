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

#include <functional>
#include <wtf/HashSet.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(GTK) && USE(GTK4)
#include <webkit/webkit-web-process-extension.h>
#elif PLATFORM(GTK)
#include <webkit2/webkit-web-extension.h>
#elif PLATFORM(WPE) && ENABLE(2022_GLIB_API)
#include <wpe/webkit-web-process-extension.h>
#elif PLATFORM(WPE)
#include <wpe/webkit-web-extension.h>
#endif

class Watcher {
public:
    ~Watcher()
    {
        checkLeaks();
    }

    void assertObjectIsDeletedWhenTestFinishes(GObject* object)
    {
        m_watchedObjects.add(object);
        g_object_weak_ref(object, [](gpointer data, GObject* whereTheObjectWas) {
            Watcher* watcher = static_cast<Watcher*>(data);
            watcher->m_watchedObjects.remove(whereTheObjectWas);
        }, this);
    }

    void checkLeaks()
    {
        if (m_watchedObjects.isEmpty())
            return;

        g_print("Leaked objects in WebProcess:");
        for (const auto object : m_watchedObjects)
            g_print(" %s(%p)", g_type_name_from_instance(reinterpret_cast<GTypeInstance*>(object)), object);
        g_print("\n");

        g_assert_true(m_watchedObjects.isEmpty());
    }

private:
    HashSet<GObject*> m_watchedObjects;
};

static Watcher s_watcher;

class WebProcessTest {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~WebProcessTest() { }
    virtual bool runTest(const char* testName, WebKitWebPage*) = 0;

    static void add(const String& testName, std::function<std::unique_ptr<WebProcessTest> ()>);
    static std::unique_ptr<WebProcessTest> create(const String& testName);
};

#define REGISTER_TEST(ClassName, TestName) \
    WebProcessTest::add(String::fromUTF8(TestName), ClassName::create)

