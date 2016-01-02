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

#include <JavaScriptCore/JSRetainPtr.h>
#include <gio/gio.h>
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
    g_assert(testsMap().contains(testName));
    return testsMap().get(testName)();
}

static JSValueRef runTest(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> stringValue(Adopt, JSValueToStringCopy(context, arguments[0], nullptr));
    g_assert(stringValue);
    size_t testPathLength = JSStringGetMaximumUTF8CStringSize(stringValue.get());
    GUniquePtr<char> testPath(static_cast<char*>(g_malloc(testPathLength)));
    JSStringGetUTF8CString(stringValue.get(), testPath.get(), testPathLength);

    WebKitWebPage* webPage = WEBKIT_WEB_PAGE(JSObjectGetPrivate(thisObject));
    g_assert(WEBKIT_IS_WEB_PAGE(webPage));
    // Test /WebKitDOMNode/dom-cache is an exception, because it's called 3 times, so
    // the WebPage is destroyed after the third time.
    if (g_str_equal(testPath.get(), "WebKitDOMNode/dom-cache")) {
        static unsigned domCacheTestRunCount = 0;
        if (++domCacheTestRunCount == 3)
            WebProcessTest::assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webPage));
    } else
        WebProcessTest::assertObjectIsDeletedWhenTestFinishes(G_OBJECT(webPage));

    std::unique_ptr<WebProcessTest> test = WebProcessTest::create(String::fromUTF8(testPath.get()));
    return JSValueMakeBoolean(context, test->runTest(g_strrstr(testPath.get(), "/") + 1, webPage));
}

static const JSStaticFunction webProcessTestRunnerStaticFunctions[] =
{
    { "runTest", runTest, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { nullptr, nullptr, 0 }
};

static void webProcessTestRunnerFinalize(JSObjectRef object)
{
    g_object_unref(JSObjectGetPrivate(object));

    if (s_watchedObjects.isEmpty())
        return;

    g_print("Leaked objects in WebProcess:");
    for (const auto object : s_watchedObjects)
        g_print(" %s(%p)", g_type_name_from_instance(reinterpret_cast<GTypeInstance*>(object)), object);
    g_print("\n");

    g_assert(s_watchedObjects.isEmpty());
}

static void windowObjectClearedCallback(WebKitScriptWorld* world, WebKitWebPage* webPage, WebKitFrame* frame, WebKitWebExtension* extension)
{
    JSGlobalContextRef context = webkit_frame_get_javascript_context_for_script_world(frame, world);
    JSObjectRef globalObject = JSContextGetGlobalObject(context);

    JSClassDefinition classDefinition = kJSClassDefinitionEmpty;
    classDefinition.className = "WebProcessTestRunner";
    classDefinition.staticFunctions = webProcessTestRunnerStaticFunctions;
    classDefinition.finalize = webProcessTestRunnerFinalize;

    JSClassRef jsClass = JSClassCreate(&classDefinition);
    JSObjectRef classObject = JSObjectMake(context, jsClass, g_object_ref(webPage));
    JSRetainPtr<JSStringRef> propertyString(Adopt, JSStringCreateWithUTF8CString("WebProcessTestRunner"));
    JSObjectSetProperty(context, globalObject, propertyString.get(), classObject, kJSPropertyAttributeNone, nullptr);
    JSClassRelease(jsClass);
}

extern "C" void webkit_web_extension_initialize(WebKitWebExtension* extension)
{
    g_signal_connect(webkit_script_world_get_default(), "window-object-cleared", G_CALLBACK(windowObjectClearedCallback), extension);
}
