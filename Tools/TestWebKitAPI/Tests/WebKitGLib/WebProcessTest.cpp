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

#include "WebKitWebExtensionPrivate.h"
#include <gio/gio.h>
#include <jsc/jsc.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

static HashSet<GObject*> s_watchedObjects;

typedef HashMap<String, std::function<std::unique_ptr<WebProcessTest> ()>> TestsMap;
static TestsMap& testsMap()
{
    static NeverDestroyed<TestsMap> s_testsMap;
    return s_testsMap;
}

static void checkLeaks()
{
    if (s_watchedObjects.isEmpty())
        return;

    g_print("Leaked objects in WebProcess:");
    for (const auto object : s_watchedObjects)
        g_print(" %s(%p)", g_type_name_from_instance(reinterpret_cast<GTypeInstance*>(object)), object);
    g_print("\n");

    g_assert_true(s_watchedObjects.isEmpty());
}

void WebProcessTest::add(const String& testName, std::function<std::unique_ptr<WebProcessTest> ()> closure)
{
    testsMap().add(testName, WTFMove(closure));
}

void WebProcessTest::assertObjectIsDeletedWhenTestFinishes(GObject* object)
{
    s_watchedObjects.add(object);
    g_object_weak_ref(object, [](gpointer, GObject* finalizedObject) {
        s_watchedObjects.remove(finalizedObject);
    }, nullptr);
}

std::unique_ptr<WebProcessTest> WebProcessTest::create(const String& testName)
{
    g_assert_true(testsMap().contains(testName));
    return testsMap().get(testName)();
}

static gboolean runTest(WebKitWebPage* webPage, const char* testPath)
{
    g_assert_true(WEBKIT_IS_WEB_PAGE(webPage));
    WebProcessTest::assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webPage));
    g_assert_nonnull(testPath);

    std::unique_ptr<WebProcessTest> test = WebProcessTest::create(String::fromUTF8(testPath));
    return test->runTest(g_strrstr(testPath, "/") + 1, webPage);
}

static void webProcessTestRunnerFinalize(WebKitWebPage* webPage)
{
    g_object_unref(webPage);
    checkLeaks();
}

static void windowObjectClearedCallback(WebKitScriptWorld* world, WebKitWebPage* webPage, WebKitFrame* frame, WebKitWebExtension* extension)
{
    if (g_strcmp0(webkit_web_page_get_uri(webPage), "webprocess://test"))
        return;

    GRefPtr<JSCContext> context = adoptGRef(webkit_frame_get_js_context_for_script_world(frame, world));
    WebProcessTest::assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    auto* jsClass = jsc_context_register_class(context.get(), "WebProcessTestRunner", nullptr, nullptr, reinterpret_cast<GDestroyNotify>(webProcessTestRunnerFinalize));
    WebProcessTest::assertObjectIsDeletedWhenTestFinishes(G_OBJECT(jsClass));
    jsc_class_add_method(jsClass, "runTest", G_CALLBACK(runTest), NULL, NULL, G_TYPE_BOOLEAN, 1, G_TYPE_STRING);
    GRefPtr<JSCValue> testRunner = adoptGRef(jsc_value_new_object(context.get(), g_object_ref(webPage), jsClass));
    WebProcessTest::assertObjectIsDeletedWhenTestFinishes(G_OBJECT(testRunner.get()));
    jsc_context_set_value(context.get(), "WebProcessTestRunner", testRunner.get());
}

extern "C" void webkit_web_extension_initialize(WebKitWebExtension* extension)
{
    webkitWebExtensionSetGarbageCollectOnPageDestroy(extension);
    g_signal_connect(webkit_script_world_get_default(), "window-object-cleared", G_CALLBACK(windowObjectClearedCallback), extension);
}

static void __attribute__((destructor)) checkLeaksAtExit()
{
    checkLeaks();
}
