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

#include <WebKitWebProcessExtensionPrivate.h>
#include <gio/gio.h>
#include <jsc/jsc.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

typedef HashMap<String, std::function<std::unique_ptr<WebProcessTest> ()>> TestsMap;
static TestsMap& testsMap()
{
    static NeverDestroyed<TestsMap> s_testsMap;
    return s_testsMap;
}

void WebProcessTest::add(const String& testName, std::function<std::unique_ptr<WebProcessTest> ()> closure)
{
    testsMap().add(testName, WTFMove(closure));
}

std::unique_ptr<WebProcessTest> WebProcessTest::create(const String& testName)
{
    g_assert_true(testsMap().contains(testName));
    return testsMap().get(testName)();
}

static gboolean runTest(WebKitWebPage* webPage, const char* testPath)
{
    g_assert_true(WEBKIT_IS_WEB_PAGE(webPage));
    s_watcher.assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webPage));
    g_assert_nonnull(testPath);

    std::unique_ptr<WebProcessTest> test = WebProcessTest::create(String::fromUTF8(testPath));
    return test->runTest(g_strrstr(testPath, "/") + 1, webPage);
}

static void webProcessTestRunnerFinalize(WebKitWebPage* webPage)
{
    g_object_unref(webPage);
    s_watcher.checkLeaks();
}

static void windowObjectClearedCallback(WebKitScriptWorld* world, WebKitWebPage* webPage, WebKitFrame* frame, WebKitWebProcessExtension* extension)
{
    if (g_strcmp0(webkit_web_page_get_uri(webPage), "webprocess://test") || !webkit_frame_is_main_frame(frame))
        return;

    GRefPtr<JSCContext> context = adoptGRef(webkit_frame_get_js_context_for_script_world(frame, world));
    s_watcher.assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    auto* jsClass = jsc_context_register_class(context.get(), "WebProcessTestRunner", nullptr, nullptr, reinterpret_cast<GDestroyNotify>(webProcessTestRunnerFinalize));
    s_watcher.assertObjectIsDeletedWhenTestFinishes(G_OBJECT(jsClass));
    jsc_class_add_method(jsClass, "runTest", G_CALLBACK(runTest), NULL, NULL, G_TYPE_BOOLEAN, 1, G_TYPE_STRING);
    GRefPtr<JSCValue> testRunner = adoptGRef(jsc_value_new_object(context.get(), g_object_ref(webPage), jsClass));
    s_watcher.assertObjectIsDeletedWhenTestFinishes(G_OBJECT(testRunner.get()));
    jsc_context_set_value(context.get(), "WebProcessTestRunner", testRunner.get());
}

#if ENABLE(2022_GLIB_API)
extern "C" WTF_EXPORT_DECLARATION void webkit_web_process_extension_initialize(WebKitWebProcessExtension* extension)
#else
extern "C" WTF_EXPORT_DECLARATION void webkit_web_extension_initialize(WebKitWebExtension* extension)
#endif
{
    webkitWebProcessExtensionSetGarbageCollectOnPageDestroy(extension);
    g_signal_connect(webkit_script_world_get_default(), "window-object-cleared", G_CALLBACK(windowObjectClearedCallback), extension);
}
