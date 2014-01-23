/*
 * Copyright (C) 2012 Igalia S.L.
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

#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <webkit2/webkit-web-extension.h>
#include <wtf/Deque.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/gobject/GUniquePtr.h>
#include <wtf/text/CString.h>

static const char introspectionXML[] =
    "<node>"
    " <interface name='org.webkit.gtk.WebExtensionTest'>"
    "  <method name='GetTitle'>"
    "   <arg type='t' name='pageID' direction='in'/>"
    "   <arg type='s' name='title' direction='out'/>"
    "  </method>"
    "  <method name='AbortProcess'>"
    "  </method>"
    "  <method name='RunJavaScriptInIsolatedWorld'>"
    "   <arg type='t' name='pageID' direction='in'/>"
    "   <arg type='s' name='script' direction='in'/>"
    "  </method>"
    "  <method name='GetInitializationUserData'>"
    "   <arg type='s' name='userData' direction='out'/>"
    "  </method>"
    "  <signal name='DocumentLoaded'/>"
    "  <signal name='URIChanged'>"
    "   <arg type='s' name='uri' direction='out'/>"
    "  </signal>"
    " </interface>"
    "</node>";

static GRefPtr<GVariant> initializationUserData;


typedef enum {
    DocumentLoadedSignal,
    URIChangedSignal,
} DelayedSignalType;

struct DelayedSignal {
    DelayedSignal(DelayedSignalType type)
        : type(type)
    {
    }

    DelayedSignal(DelayedSignalType type, const char* uri)
        : type(type)
        , uri(uri)
    {
    }

    DelayedSignalType type;
    CString uri;
};

Deque<OwnPtr<DelayedSignal>> delayedSignalsQueue;

static void emitDocumentLoaded(GDBusConnection* connection)
{
    bool ok = g_dbus_connection_emit_signal(
        connection,
        0,
        "/org/webkit/gtk/WebExtensionTest",
        "org.webkit.gtk.WebExtensionTest",
        "DocumentLoaded",
        0,
        0);
    g_assert(ok);
}

static void documentLoadedCallback(WebKitWebPage*, WebKitWebExtension* extension)
{
    gpointer data = g_object_get_data(G_OBJECT(extension), "dbus-connection");
    if (data)
        emitDocumentLoaded(G_DBUS_CONNECTION(data));
    else
        delayedSignalsQueue.append(adoptPtr(new DelayedSignal(DocumentLoadedSignal)));
}

static void emitURIChanged(GDBusConnection* connection, const char* uri)
{
    bool ok = g_dbus_connection_emit_signal(
        connection,
        0,
        "/org/webkit/gtk/WebExtensionTest",
        "org.webkit.gtk.WebExtensionTest",
        "URIChanged",
        g_variant_new("(s)", uri),
        0);
    g_assert(ok);
}

static void uriChangedCallback(WebKitWebPage* webPage, GParamSpec* pspec, WebKitWebExtension* extension)
{
    gpointer data = g_object_get_data(G_OBJECT(extension), "dbus-connection");
    if (data)
        emitURIChanged(G_DBUS_CONNECTION(data), webkit_web_page_get_uri(webPage));
    else
        delayedSignalsQueue.append(adoptPtr(new DelayedSignal(URIChangedSignal, webkit_web_page_get_uri(webPage))));
}

static gboolean sendRequestCallback(WebKitWebPage*, WebKitURIRequest* request, WebKitURIResponse*, gpointer)
{
    const char* requestURI = webkit_uri_request_get_uri(request);
    g_assert(requestURI);

    if (const char* suffix = g_strrstr(requestURI, "/remove-this/javascript.js")) {
        GUniquePtr<char> prefix(g_strndup(requestURI, strlen(requestURI) - strlen(suffix)));
        GUniquePtr<char> newURI(g_strdup_printf("%s/javascript.js", prefix.get()));
        webkit_uri_request_set_uri(request, newURI.get());
    } else if (g_str_has_suffix(requestURI, "/add-do-not-track-header")) {
        SoupMessageHeaders* headers = webkit_uri_request_get_http_headers(request);
        g_assert(headers);
        soup_message_headers_append(headers, "DNT", "1");
    } else if (g_str_has_suffix(requestURI, "/cancel-this.js"))
        return TRUE;

    return FALSE;
}

static void pageCreatedCallback(WebKitWebExtension* extension, WebKitWebPage* webPage, gpointer)
{
    g_signal_connect(webPage, "document-loaded", G_CALLBACK(documentLoadedCallback), extension);
    g_signal_connect(webPage, "notify::uri", G_CALLBACK(uriChangedCallback), extension);
    g_signal_connect(webPage, "send-request", G_CALLBACK(sendRequestCallback), 0);
}

static JSValueRef echoCallback(JSContextRef jsContext, JSObjectRef, JSObjectRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef*)
{
    if (argumentCount <= 0)
        return JSValueMakeUndefined(jsContext);

    JSRetainPtr<JSStringRef> string(Adopt, JSValueToStringCopy(jsContext, arguments[0], 0));
    return JSValueMakeString(jsContext, string.get());
}

static void windowObjectCleared(WebKitScriptWorld* world, WebKitWebPage* page, WebKitFrame* frame, gpointer)
{
    JSGlobalContextRef jsContext = webkit_frame_get_javascript_context_for_script_world(frame, world);
    g_assert(jsContext);
    JSObjectRef globalObject = JSContextGetGlobalObject(jsContext);
    g_assert(globalObject);

    JSRetainPtr<JSStringRef> functionName(Adopt, JSStringCreateWithUTF8CString("echo"));
    JSObjectRef function = JSObjectMakeFunctionWithCallback(jsContext, functionName.get(), echoCallback);
    JSObjectSetProperty(jsContext, globalObject, functionName.get(), function, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly, 0);
}

static WebKitWebPage* getWebPage(WebKitWebExtension* extension, uint64_t pageID, GDBusMethodInvocation* invocation)
{
    WebKitWebPage* page = webkit_web_extension_get_page(extension, pageID);
    if (!page) {
        g_dbus_method_invocation_return_error(
            invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
            "Invalid page ID: %" G_GUINT64_FORMAT, pageID);
        return 0;
    }

    g_assert_cmpuint(webkit_web_page_get_id(page), ==, pageID);
    return page;
}

static void methodCallCallback(GDBusConnection* connection, const char* sender, const char* objectPath, const char* interfaceName, const char* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData)
{
    if (g_strcmp0(interfaceName, "org.webkit.gtk.WebExtensionTest"))
        return;

    if (!g_strcmp0(methodName, "GetTitle")) {
        uint64_t pageID;
        g_variant_get(parameters, "(t)", &pageID);
        WebKitWebPage* page = getWebPage(WEBKIT_WEB_EXTENSION(userData), pageID, invocation);
        if (!page)
            return;

        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        GUniquePtr<char> title(webkit_dom_document_get_title(document));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", title.get()));
    } else if (!g_strcmp0(methodName, "RunJavaScriptInIsolatedWorld")) {
        uint64_t pageID;
        const char* script;
        g_variant_get(parameters, "(t&s)", &pageID, &script);
        WebKitWebPage* page = getWebPage(WEBKIT_WEB_EXTENSION(userData), pageID, invocation);
        if (!page)
            return;

        GRefPtr<WebKitScriptWorld> world = adoptGRef(webkit_script_world_new());
        g_assert(webkit_script_world_get_default() != world.get());
        WebKitFrame* frame = webkit_web_page_get_main_frame(page);
        JSGlobalContextRef jsContext = webkit_frame_get_javascript_context_for_script_world(frame, world.get());
        JSRetainPtr<JSStringRef> jsScript(Adopt, JSStringCreateWithUTF8CString(script));
        JSEvaluateScript(jsContext, jsScript.get(), 0, 0, 0, 0);
        g_dbus_method_invocation_return_value(invocation, 0);
    } else if (!g_strcmp0(methodName, "AbortProcess")) {
        abort();
    } else if (!g_strcmp0(methodName, "GetInitializationUserData")) {
        g_assert(initializationUserData);
        g_assert(g_variant_is_of_type(initializationUserData.get(), G_VARIANT_TYPE_STRING));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)",
            g_variant_get_string(initializationUserData.get(), nullptr)));
    }
}

static const GDBusInterfaceVTable interfaceVirtualTable = {
    methodCallCallback, 0, 0, { 0, }
};

static void busAcquiredCallback(GDBusConnection* connection, const char* name, gpointer userData)
{
    static GDBusNodeInfo* introspectionData = 0;
    if (!introspectionData)
        introspectionData = g_dbus_node_info_new_for_xml(introspectionXML, 0);

    GOwnPtr<GError> error;
    unsigned registrationID = g_dbus_connection_register_object(
        connection,
        "/org/webkit/gtk/WebExtensionTest",
        introspectionData->interfaces[0],
        &interfaceVirtualTable,
        g_object_ref(userData),
        static_cast<GDestroyNotify>(g_object_unref),
        &error.outPtr());
    if (!registrationID)
        g_warning("Failed to register object: %s\n", error->message);

    g_object_set_data(G_OBJECT(userData), "dbus-connection", connection);
    while (delayedSignalsQueue.size()) {
        OwnPtr<DelayedSignal> delayedSignal = delayedSignalsQueue.takeFirst();
        switch (delayedSignal->type) {
        case DocumentLoadedSignal:
            emitDocumentLoaded(connection);
            break;
        case URIChangedSignal:
            emitURIChanged(connection, delayedSignal->uri.data());
            break;
        }
    }
}

extern "C" void webkit_web_extension_initialize_with_user_data(WebKitWebExtension* extension, GVariant* userData)
{
    initializationUserData = userData;

    g_signal_connect(extension, "page-created", G_CALLBACK(pageCreatedCallback), extension);
    g_signal_connect(webkit_script_world_get_default(), "window-object-cleared", G_CALLBACK(windowObjectCleared), 0);

    g_bus_own_name(
        G_BUS_TYPE_SESSION,
        "org.webkit.gtk.WebExtensionTest",
        G_BUS_NAME_OWNER_FLAGS_NONE,
        busAcquiredCallback,
        0, 0,
        g_object_ref(extension),
        static_cast<GDestroyNotify>(g_object_unref));
}
