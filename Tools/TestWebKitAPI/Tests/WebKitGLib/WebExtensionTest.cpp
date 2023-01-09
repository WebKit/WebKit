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
#include "WebProcessTest.h"
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib/gstdio.h>
#if USE(GSTREAMER)
#include <gst/gst.h>
#endif
#include <jsc/jsc.h>
#include <stdlib.h>
#include <string.h>
#include <wtf/Deque.h>
#include <wtf/ProcessID.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

#if PLATFORM(GTK) && USE(GTK4)
#include <webkit/webkit-web-extension.h>
#elif PLATFORM(GTK)
#include <webkit2/webkit-web-extension.h>
#elif PLATFORM(WPE)
#include <wpe/webkit-web-extension.h>
#endif

static const char introspectionXML[] =
    "<node>"
    " <interface name='org.webkit.gtk.WebExtensionTest'>"
    "  <method name='GetTitle'>"
    "   <arg type='t' name='pageID' direction='in'/>"
    "   <arg type='s' name='title' direction='out'/>"
    "  </method>"
    "  <method name='InputElementIsUserEdited'>"
    "    <arg type='t' name='pageID' direction='in'/>"
    "    <arg type='s' name='elementID' direction='in'/>"
    "    <arg type='b' name='isUserEdited' direction='out'/>"
    "  </method>"
    "  <method name='AbortProcess'>"
    "  </method>"
    "  <method name='RunJavaScriptInIsolatedWorld'>"
    "   <arg type='t' name='pageID' direction='in'/>"
    "   <arg type='s' name='script' direction='in'/>"
    "  </method>"
    "  <method name='GetProcessIdentifier'>"
    "   <arg type='u' name='identifier' direction='out'/>"
    "  </method>"
    "  <signal name='PageCreated'>"
    "   <arg type='t' name='pageID' direction='out'/>"
    "  </signal>"
    "  <signal name='DocumentLoaded'/>"
    "  <signal name='FormControlsAssociated'>"
    "   <arg type='s' name='formIds' direction='out'/>"
    "  </signal>"
    "  <signal name='FormSubmissionWillSendDOMEvent'>"
    "    <arg type='s' name='formID' direction='out'/>"
    "    <arg type='s' name='textFieldNames' direction='out'/>"
    "    <arg type='s' name='textFieldValues' direction='out'/>"
    "    <arg type='b' name='targetFrameIsMainFrame' direction='out'/>"
    "    <arg type='b' name='sourceFrameIsMainFrame' direction='out'/>"
    "  </signal>"
    "  <signal name='FormSubmissionWillComplete'>"
    "    <arg type='s' name='formID' direction='out'/>"
    "    <arg type='s' name='textFieldNames' direction='out'/>"
    "    <arg type='s' name='textFieldValues' direction='out'/>"
    "    <arg type='b' name='targetFrameIsMainFrame' direction='out'/>"
    "    <arg type='b' name='sourceFrameIsMainFrame' direction='out'/>"
    "  </signal>"
    "  <signal name='URIChanged'>"
    "   <arg type='s' name='uri' direction='out'/>"
    "  </signal>"
    " </interface>"
    "</node>";


typedef enum {
    PageCreatedSignal,
    DocumentLoadedSignal,
    URIChangedSignal,
    FormControlsAssociatedSignal,
    FormSubmissionWillSendDOMEventSignal,
    FormSubmissionWillCompleteSignal,
} DelayedSignalType;

struct DelayedSignal {
    explicit DelayedSignal(DelayedSignalType type)
        : type(type)
    {
    }

    DelayedSignal(DelayedSignalType type, guint64 n)
        : type(type)
        , n(n)
    {
    }

    DelayedSignal(DelayedSignalType type, const char* str)
        : type(type)
        , str(str)
    {
    }

    DelayedSignal(DelayedSignalType type, const char* str, const char* str2, const char* str3, gboolean b, gboolean b2)
        : type(type)
        , str(str)
        , str2(str2)
        , str3(str3)
        , b(b)
        , b2(b2)
    {
    }

    DelayedSignalType type;
    CString str;
    CString str2;
    CString str3;
    gboolean b;
    gboolean b2;
    guint64 n;
};

Deque<DelayedSignal> delayedSignalsQueue;

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
    g_assert_true(ok);
}

static void documentLoadedCallback(WebKitWebPage* webPage, WebKitWebExtension* extension)
{
#if PLATFORM(GTK) && !USE(GTK4)
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    WebKitDOMDocument* document = webkit_web_page_get_dom_document(webPage);
    GRefPtr<WebKitDOMDOMWindow> window = adoptGRef(webkit_dom_document_get_default_view(document));
    webkit_dom_dom_window_webkit_message_handlers_post_message(window.get(), "dom", "DocumentLoaded");
    G_GNUC_END_IGNORE_DEPRECATIONS;
#endif

    webkit_web_page_send_message_to_view(webPage, webkit_user_message_new("DocumentLoaded", nullptr), nullptr, nullptr, nullptr);

    gpointer data = g_object_get_data(G_OBJECT(extension), "dbus-connection");
    if (data)
        emitDocumentLoaded(G_DBUS_CONNECTION(data));
    else
        delayedSignalsQueue.append(DelayedSignal(DocumentLoadedSignal));
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
    g_assert_true(ok);
}

static void uriChangedCallback(WebKitWebPage* webPage, GParamSpec* pspec, WebKitWebExtension* extension)
{
    gpointer data = g_object_get_data(G_OBJECT(extension), "dbus-connection");
    if (data)
        emitURIChanged(G_DBUS_CONNECTION(data), webkit_web_page_get_uri(webPage));
    else
        delayedSignalsQueue.append(DelayedSignal(URIChangedSignal, webkit_web_page_get_uri(webPage)));
}

static gboolean sendRequestCallback(WebKitWebPage*, WebKitURIRequest* request, WebKitURIResponse* redirectResponse, gpointer)
{
    gboolean returnValue = FALSE;
    const char* requestURI = webkit_uri_request_get_uri(request);
    g_assert_nonnull(requestURI);

    if (const char* suffix = g_strrstr(requestURI, "/remove-this/javascript.js")) {
        GUniquePtr<char> prefix(g_strndup(requestURI, strlen(requestURI) - strlen(suffix)));
        GUniquePtr<char> newURI(g_strdup_printf("%s/javascript.js", prefix.get()));
        webkit_uri_request_set_uri(request, newURI.get());
    } else if (const char* suffix = g_strrstr(requestURI, "/remove-this/javascript-after-redirection.js")) {
        // Redirected from /redirected.js, redirectResponse should be nullptr.
        g_assert_true(WEBKIT_IS_URI_RESPONSE(redirectResponse));
        g_assert_true(g_str_has_suffix(webkit_uri_response_get_uri(redirectResponse), "/redirected.js"));

        GUniquePtr<char> prefix(g_strndup(requestURI, strlen(requestURI) - strlen(suffix)));
        GUniquePtr<char> newURI(g_strdup_printf("%s/javascript-after-redirection.js", prefix.get()));
        webkit_uri_request_set_uri(request, newURI.get());
    } else if (g_str_has_suffix(requestURI, "/redirected.js")) {
        // Original request, redirectResponse should be nullptr.
        g_assert_null(redirectResponse);
    } else if (g_str_has_suffix(requestURI, "/add-do-not-track-header")) {
        SoupMessageHeaders* headers = webkit_uri_request_get_http_headers(request);
        g_assert_nonnull(headers);
        soup_message_headers_append(headers, "DNT", "1");
    } else if (g_str_has_suffix(requestURI, "/normal-change-request") && !g_strrstr(requestURI, "/redirect-js/")) {
        GUniquePtr<char> prefix(g_strndup(requestURI, strlen(requestURI) - strlen("/normal-change-request")));
        GUniquePtr<char> newURI(g_strdup_printf("%s/request-changed%s", prefix.get(), redirectResponse ? "-on-redirect" : ""));
        webkit_uri_request_set_uri(request, newURI.get());
    } else if (g_str_has_suffix(requestURI, "/http-get-method")) {
        g_assert_cmpstr(webkit_uri_request_get_http_method(request), ==, "GET");
        g_assert_cmpstr(webkit_uri_request_get_http_method(request), ==, SOUP_METHOD_GET);
    } else if (g_str_has_suffix(requestURI, "/http-post-method")) {
        g_assert_cmpstr(webkit_uri_request_get_http_method(request), ==, "POST");
        g_assert_cmpstr(webkit_uri_request_get_http_method(request), ==, SOUP_METHOD_POST);
        returnValue = TRUE;
    } else if (g_str_has_suffix(requestURI, "/cancel-this.js"))
        returnValue = TRUE;

    return returnValue;
}

static GVariant* serializeContextMenu(WebKitContextMenu* menu)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);
    GList* items = webkit_context_menu_get_items(menu);
    for (GList* it = items; it; it = g_list_next(it))
        g_variant_builder_add(&builder, "u", webkit_context_menu_item_get_stock_action(WEBKIT_CONTEXT_MENU_ITEM(it->data)));
    return g_variant_builder_end(&builder);
}

static GVariant* serializeNode(JSCValue* node)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);
    GRefPtr<JSCValue> value = adoptGRef(jsc_value_object_get_property(node, "nodeName"));
    g_variant_builder_add(&builder, "{sv}", "Name", g_variant_new_take_string(jsc_value_to_string(value.get())));
    value = adoptGRef(jsc_value_object_get_property(node, "nodeType"));
    g_variant_builder_add(&builder, "{sv}", "Type", g_variant_new_uint32(jsc_value_to_int32(value.get())));
    value = adoptGRef(jsc_value_object_get_property(node, "textContent"));
    g_variant_builder_add(&builder, "{sv}", "Contents", g_variant_new_take_string(jsc_value_to_string(value.get())));
    value = adoptGRef(jsc_value_object_get_property(node, "parentNode"));
    if (jsc_value_is_null(value.get()))
        g_variant_builder_add(&builder, "{sv}", "Parent", g_variant_new_string("ROOT"));
    else {
        value = jsc_value_object_get_property(value.get(), "nodeName");
        g_variant_builder_add(&builder, "{sv}", "Parent", g_variant_new_take_string(jsc_value_to_string(value.get())));
    }
    return g_variant_builder_end(&builder);
}

static gboolean contextMenuCallback(WebKitWebPage* page, WebKitContextMenu* menu, WebKitWebHitTestResult* hitTestResult, gpointer)
{
#if PLATFORM(GTK)
    g_assert_null(webkit_context_menu_get_event(menu));
#endif

    const char* pageURI = webkit_web_page_get_uri(page);
    if (!g_strcmp0(pageURI, "ContextMenuTestDefault")) {
        webkit_context_menu_set_user_data(menu, serializeContextMenu(menu));
        return FALSE;
    }

    if (!g_strcmp0(pageURI, "ContextMenuTestCustom")) {
        // Remove Back and Forward, and add Inspector action.
        webkit_context_menu_remove(menu, webkit_context_menu_first(menu));
        webkit_context_menu_remove(menu, webkit_context_menu_first(menu));
        webkit_context_menu_append(menu, webkit_context_menu_item_new_separator());
        webkit_context_menu_append(menu, webkit_context_menu_item_new_from_stock_action(WEBKIT_CONTEXT_MENU_ACTION_INSPECT_ELEMENT));
        webkit_context_menu_set_user_data(menu, serializeContextMenu(menu));
        return TRUE;
    }

    if (!g_strcmp0(pageURI, "ContextMenuTestClear")) {
        webkit_context_menu_remove_all(menu);
        return TRUE;
    }

    if (!g_strcmp0(pageURI, "ContextMenuTestNode")) {
        GRefPtr<JSCValue> jsNode = adoptGRef(webkit_web_hit_test_result_get_js_node(hitTestResult, nullptr));
        webkit_context_menu_set_user_data(menu, serializeNode(jsNode.get()));
        return TRUE;
    }

    return FALSE;
}

static void emitFormControlsAssociated(GDBusConnection* connection, const char* formIds)
{
    bool ok = g_dbus_connection_emit_signal(
        connection,
        nullptr,
        "/org/webkit/gtk/WebExtensionTest",
        "org.webkit.gtk.WebExtensionTest",
        "FormControlsAssociated",
        g_variant_new("(s)", formIds),
        nullptr);
    g_assert_true(ok);
}

#if !ENABLE(2022_GLIB_API)
static void formControlsAssociatedForFrameCallback(WebKitWebPage*, GPtrArray*, WebKitFrame*, WebKitWebExtension*)
{
    g_assert_not_reached();
}
#endif

static void formControlsAssociatedCallback(WebKitWebFormManager*, WebKitFrame*, GPtrArray* formElements, WebKitWebExtension* extension)
{
    GString* formIdsBuilder = g_string_new(nullptr);
    for (guint i = 0; i < formElements->len; ++i) {
        JSCValue* value = JSC_VALUE(g_ptr_array_index(formElements, i));
        g_assert_true(jsc_value_is_object(value));
        g_assert_true(jsc_value_object_is_instance_of(value, "Element"));
        GRefPtr<JSCValue> idValue = adoptGRef(jsc_value_object_get_property(value, "id"));
        GUniquePtr<char> elementID(jsc_value_to_string(idValue.get()));
        g_string_append(formIdsBuilder, elementID.get());
    }
    if (!formIdsBuilder->len) {
        g_string_free(formIdsBuilder, TRUE);
        return;
    }
    GUniquePtr<char> formIds(g_string_free(formIdsBuilder, FALSE));
    gpointer data = g_object_get_data(G_OBJECT(extension), "dbus-connection");
    if (data)
        emitFormControlsAssociated(G_DBUS_CONNECTION(data), formIds.get());
    else
        delayedSignalsQueue.append(DelayedSignal(FormControlsAssociatedSignal, formIds.get()));
}

static void emitFormSubmissionEvent(GDBusConnection* connection, const char* methodName, const char* formID, const char* names, const char* values, gboolean targetFrameIsMainFrame, gboolean sourceFrameIsMainFrame)
{
    bool ok = g_dbus_connection_emit_signal(
        connection,
        nullptr,
        "/org/webkit/gtk/WebExtensionTest",
        "org.webkit.gtk.WebExtensionTest",
        methodName,
        g_variant_new("(sssbb)", formID ? formID : "", names, values, targetFrameIsMainFrame, sourceFrameIsMainFrame),
        nullptr);
    g_assert_true(ok);
}

#if !ENABLE(2022_GLIB_API)
static void willSubmitFormDeprecatedCallback(WebKitWebPage*, WebKitDOMElement*, WebKitFormSubmissionStep, WebKitFrame*, WebKitFrame*, GPtrArray*, GPtrArray*, WebKitWebExtension*)
{
    g_assert_not_reached();
}
#endif

static void handleFormSubmissionCallback(WebKitWebExtension* extension, DelayedSignalType delayedSignalType, const char* methodName, JSCValue* form, WebKitFrame* sourceFrame, WebKitFrame* targetFrame)
{
    g_assert_true(jsc_value_is_object(form));
    g_assert_true(jsc_value_object_is_instance_of(form, "HTMLFormElement"));
    GRefPtr<JSCValue> idValue = adoptGRef(jsc_value_object_get_property(form, "id"));
    GUniquePtr<char> formID(jsc_value_to_string(idValue.get()));

    GRefPtr<JSCValue> elements = adoptGRef(jsc_value_object_get_property(form, "elements"));
    g_assert_true(JSC_IS_VALUE(elements.get()));
    GRefPtr<JSCValue> elementsLength = adoptGRef(jsc_value_object_get_property(elements.get(), "length"));
    auto elementCount = jsc_value_to_int32(elementsLength.get());
    GString* namesBuilder = g_string_new(nullptr);
    GString* valuesBuilder = g_string_new(nullptr);
    for (int i = 0; i < elementCount; ++i) {
        GRefPtr<JSCValue> element = adoptGRef(jsc_value_object_get_property_at_index(elements.get(), i));
        if (!jsc_value_object_is_instance_of(element.get(), "HTMLInputElement"))
            continue;

        GRefPtr<JSCValue> elementType = adoptGRef(jsc_value_object_get_property(element.get(), "type"));
        GUniquePtr<char> fieldType(jsc_value_to_string(elementType.get()));
        if (g_strcmp0(fieldType.get(), "text"))
            continue;

        GRefPtr<JSCValue> name = adoptGRef(jsc_value_object_get_property(element.get(), "name"));
        GUniquePtr<char> fieldName(jsc_value_to_string(name.get()));
        g_string_append(namesBuilder, fieldName.get());
        g_string_append_c(namesBuilder, ',');

        GRefPtr<JSCValue> value = adoptGRef(jsc_value_object_get_property(element.get(), "value"));
        GUniquePtr<char> fieldValue(jsc_value_to_string(value.get()));
        g_string_append(valuesBuilder, fieldValue.get());
        g_string_append_c(valuesBuilder, ',');
    }

    GUniquePtr<char> names(g_string_free(namesBuilder, FALSE));
    GUniquePtr<char> values(g_string_free(valuesBuilder, FALSE));
    gpointer data = g_object_get_data(G_OBJECT(extension), "dbus-connection");
    if (data)
        emitFormSubmissionEvent(G_DBUS_CONNECTION(data), methodName, formID.get(), names.get(), values.get(), webkit_frame_is_main_frame(targetFrame), webkit_frame_is_main_frame(sourceFrame));
    else
        delayedSignalsQueue.append(DelayedSignal(delayedSignalType, formID.get(), names.get(), values.get(), webkit_frame_is_main_frame(targetFrame), webkit_frame_is_main_frame(sourceFrame)));
}

static void willSendSubmitEventCallback(WebKitWebFormManager*, JSCValue* form, WebKitFrame* sourceFrame, WebKitFrame* targetFrame, WebKitWebExtension* extension)
{
    handleFormSubmissionCallback(extension, FormSubmissionWillSendDOMEventSignal, "FormSubmissionWillSendDOMEvent", form, sourceFrame, targetFrame);
}

static void willSubmitFormCallback(WebKitWebFormManager*, JSCValue* form, WebKitFrame* sourceFrame, WebKitFrame* targetFrame, WebKitWebExtension* extension)
{
    handleFormSubmissionCallback(extension, FormSubmissionWillCompleteSignal, "FormSubmissionWillComplete", form, sourceFrame, targetFrame);
}

static gboolean pageMessageReceivedCallback(WebKitWebPage* webPage, WebKitUserMessage* message, WebKitWebExtension* extension)
{
    const char* messageName = webkit_user_message_get_name(message);
    if (!g_strcmp0(messageName, "Test.Hello")) {
        auto* parameters = webkit_user_message_get_parameters(message);
        g_assert_nonnull(parameters);
        const char* parameter = nullptr;
        g_variant_get(parameters, "&s", &parameter);
        g_assert_cmpstr(parameter, ==, "WebProcess");
        g_assert_null(webkit_user_message_get_fd_list(message));
        webkit_user_message_send_reply(message, webkit_user_message_new("Test.Hello", g_variant_new("s", "UIProcess")));
        return TRUE;
    }

    if (!g_strcmp0(messageName, "Test.Optional")) {
        auto* parameters = webkit_user_message_get_parameters(message);
        g_assert_nonnull(parameters);
        const char* parameter1 = nullptr;
        const char* parameter2 = nullptr;
        g_variant_get(parameters, "(&sm&s)", &parameter1, &parameter2);
        g_assert_cmpstr(parameter1, ==, "Hello");
        webkit_user_message_send_reply(message, webkit_user_message_new("Test.Optional", g_variant_new("s", parameter2 ? parameter2 : "NULL")));
        return TRUE;
    }

    if (!g_strcmp0(messageName, "Test.Ping")) {
        g_assert_null(webkit_user_message_get_parameters(message));
        webkit_user_message_send_reply(message, webkit_user_message_new("Test.Pong", nullptr));
        return TRUE;
    }

    if (!g_strcmp0(messageName, "Test.OpenFile")) {
        auto* parameters = webkit_user_message_get_parameters(message);
        g_assert_nonnull(parameters);
        const char* filename = nullptr;
        g_variant_get(parameters, "&s", &filename);
        int fd = g_open(filename, O_RDONLY, 0);
        g_assert_cmpint(fd, !=, -1);
        GRefPtr<GUnixFDList> fdList = adoptGRef(g_unix_fd_list_new());
        GUniqueOutPtr<GError> error;
        g_unix_fd_list_append(fdList.get(), fd, &error.outPtr());
        g_assert_no_error(error.get());
        close(fd);

        webkit_user_message_send_reply(message, webkit_user_message_new_with_fd_list("Test.OpenFile", g_variant_new("h", 0), fdList.get()));
        return TRUE;
    }

    if (!g_strcmp0(messageName, "Test.Infinite")) {
        abort();
        return TRUE;
    }

    if (!g_strcmp0(messageName, "Test.AsyncPing")) {
        webkit_web_page_send_message_to_view(webPage, webkit_user_message_new("Test.Ping", nullptr), nullptr,
            [](GObject* object, GAsyncResult* result, gpointer) {
                auto* webPage = WEBKIT_WEB_PAGE(object);
                GUniqueOutPtr<GError> error;
                GRefPtr<WebKitUserMessage> reply = adoptGRef(webkit_web_page_send_message_to_view_finish(webPage, result, &error.outPtr()));
                g_assert_no_error(error.get());
                g_assert_cmpstr(webkit_user_message_get_name(reply.get()), ==, "Test.Pong");
                webkit_web_page_send_message_to_view(webPage, webkit_user_message_new("Test.AsyncPong", nullptr), nullptr, nullptr, nullptr);
            }, nullptr);
        return TRUE;
    }

    return FALSE;
}

static void emitPageCreated(GDBusConnection* connection, guint64 pageID)
{
    bool ok = g_dbus_connection_emit_signal(
        connection,
        nullptr,
        "/org/webkit/gtk/WebExtensionTest",
        "org.webkit.gtk.WebExtensionTest",
        "PageCreated",
        g_variant_new("(t)", pageID),
        nullptr);
    g_assert_true(ok);
}

static void pageCreatedCallback(WebKitWebExtension* extension, WebKitWebPage* webPage, gpointer)
{
    if (auto* data = g_object_get_data(G_OBJECT(extension), "dbus-connection"))
        emitPageCreated(G_DBUS_CONNECTION(data), webkit_web_page_get_id(webPage));
    else
        delayedSignalsQueue.append(DelayedSignal(PageCreatedSignal, webkit_web_page_get_id(webPage)));

    webkit_web_extension_send_message_to_context(extension, webkit_user_message_new("PageCreated", g_variant_new("(t)", webkit_web_page_get_id(webPage))),
        nullptr, nullptr, nullptr);

    g_signal_connect(webPage, "document-loaded", G_CALLBACK(documentLoadedCallback), extension);
    g_signal_connect(webPage, "notify::uri", G_CALLBACK(uriChangedCallback), extension);
    g_signal_connect(webPage, "send-request", G_CALLBACK(sendRequestCallback), nullptr);
    g_signal_connect(webPage, "context-menu", G_CALLBACK(contextMenuCallback), nullptr);
#if !ENABLE(2022_GLIB_API)
    g_signal_connect(webPage, "form-controls-associated-for-frame", G_CALLBACK(formControlsAssociatedForFrameCallback), extension);
    g_signal_connect(webPage, "will-submit-form", G_CALLBACK(willSubmitFormDeprecatedCallback), extension);
#endif
    g_signal_connect(webPage, "user-message-received", G_CALLBACK(pageMessageReceivedCallback), extension);

    auto* formManager = webkit_web_page_get_form_manager(webPage, nullptr);
    g_signal_connect(formManager, "form-controls-associated", G_CALLBACK(formControlsAssociatedCallback), extension);
    g_signal_connect(formManager, "will-send-submit-event", G_CALLBACK(willSendSubmitEventCallback), extension);
    g_signal_connect(formManager, "will-submit-form", G_CALLBACK(willSubmitFormCallback), extension);
}

static gboolean extensionMessageReceivedCallback(WebKitWebExtension* extension, WebKitUserMessage* message)
{
    const char* messageName = webkit_user_message_get_name(message);
    if (g_strcmp0(messageName, "RequestPing")) {
        webkit_web_extension_send_message_to_context(extension, webkit_user_message_new("Ping", nullptr), nullptr,
            [](GObject* object, GAsyncResult* result, gpointer) {
                auto* extension = WEBKIT_WEB_EXTENSION(object);
                GUniqueOutPtr<GError> error;
                GRefPtr<WebKitUserMessage> reply = adoptGRef(webkit_web_extension_send_message_to_context_finish(extension, result, &error.outPtr()));
                g_assert_no_error(error.get());
                g_assert_cmpstr(webkit_user_message_get_name(reply.get()), ==, "Pong");
                webkit_web_extension_send_message_to_context(extension, webkit_user_message_new("Test.FinishedPingRequest", nullptr), nullptr, nullptr, nullptr);
            }, nullptr);
        return TRUE;
    }

    return FALSE;
}

static char* echoCallback(const char* message)
{
    return g_strdup(message);
}

static void windowObjectCleared(WebKitScriptWorld* world, WebKitWebPage* page, WebKitFrame* frame, WebKitWebExtension* extension)
{
    webkit_web_page_send_message_to_view(page, webkit_user_message_new("WindowObjectCleared", nullptr), nullptr, nullptr, nullptr);
    GRefPtr<JSCContext> jsContext = adoptGRef(webkit_frame_get_js_context_for_script_world(frame, world));
    g_assert_true(JSC_IS_CONTEXT(jsContext.get()));
    GRefPtr<JSCValue> function = adoptGRef(jsc_value_new_function(jsContext.get(), "echo", G_CALLBACK(echoCallback), NULL, NULL, G_TYPE_STRING, 1, G_TYPE_STRING));
    jsc_context_set_value(jsContext.get(), "echo", function.get());

    auto* fileClass = jsc_context_register_class(jsContext.get(), "GFile", nullptr, nullptr, static_cast<GDestroyNotify>(g_object_unref));
    GRefPtr<JSCValue> constructor = adoptGRef(jsc_class_add_constructor(fileClass, "GFile", G_CALLBACK(g_file_new_for_path), nullptr, nullptr, G_TYPE_OBJECT, 1, G_TYPE_STRING));
    jsc_class_add_method(fileClass, "path", G_CALLBACK(g_file_get_path), nullptr, nullptr, G_TYPE_STRING, 0, G_TYPE_NONE);
    jsc_context_set_value(jsContext.get(), "GFile", constructor.get());
}

static void isolatedWorldWindowObjectCleared(WebKitScriptWorld* world, WebKitWebPage* page, WebKitFrame* frame, gpointer)
{
    webkit_web_page_send_message_to_view(page, webkit_user_message_new("WindowObjectClearedIsolatedWorld", nullptr), nullptr, nullptr, nullptr);
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

        GRefPtr<JSCContext> jsContext = adoptGRef(webkit_frame_get_js_context(webkit_web_page_get_main_frame(page)));
        GRefPtr<JSCValue> titleValue = adoptGRef(jsc_context_evaluate(jsContext.get(), "document.title", -1));
        GUniquePtr<char> title(jsc_value_to_string(titleValue.get()));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", title.get()));
    } else if (!g_strcmp0(methodName, "InputElementIsUserEdited")) {
        uint64_t pageID;
        const char* elementID;
        g_variant_get(parameters, "(t&s)", &pageID, &elementID);
        WebKitWebPage* page = getWebPage(WEBKIT_WEB_EXTENSION(userData), pageID, invocation);
        if (!page)
            return;

        WebKitFrame* frame = webkit_web_page_get_main_frame(page);
        GRefPtr<JSCContext> jsContext = adoptGRef(webkit_frame_get_js_context(frame));
        GRefPtr<JSCValue> jsDocument = adoptGRef(jsc_context_get_value(jsContext.get(), "document"));
        GRefPtr<JSCValue> jsInputElement = adoptGRef(jsc_value_object_invoke_method(jsDocument.get(), "getElementById", G_TYPE_STRING, elementID, G_TYPE_NONE));
        gboolean isUserEdited = webkit_web_form_manager_input_element_is_user_edited(jsInputElement.get());
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", isUserEdited));
    } else if (!g_strcmp0(methodName, "RunJavaScriptInIsolatedWorld")) {
        uint64_t pageID;
        const char* script;
        g_variant_get(parameters, "(t&s)", &pageID, &script);
        WebKitWebPage* page = getWebPage(WEBKIT_WEB_EXTENSION(userData), pageID, invocation);
        if (!page)
            return;

        GRefPtr<WebKitScriptWorld> world = adoptGRef(webkit_script_world_new());
        g_assert_true(webkit_script_world_get_default() != world.get());
        g_assert_true(g_str_has_prefix(webkit_script_world_get_name(world.get()), "UniqueWorld_"));
        WebKitFrame* frame = webkit_web_page_get_main_frame(page);
        GRefPtr<JSCContext> jsContext = adoptGRef(webkit_frame_get_js_context_for_script_world(frame, world.get()));
        GRefPtr<JSCValue> result = adoptGRef(jsc_context_evaluate(jsContext.get(), script, -1));
        g_dbus_method_invocation_return_value(invocation, 0);
    } else if (!g_strcmp0(methodName, "AbortProcess")) {
        abort();
    } else if (!g_strcmp0(methodName, "GetProcessIdentifier")) {
        g_dbus_method_invocation_return_value(invocation,
            g_variant_new("(u)", static_cast<guint32>(getCurrentProcessID())));
    }
}

static const GDBusInterfaceVTable interfaceVirtualTable = {
    methodCallCallback, 0, 0, { 0, }
};

static void dbusConnectionCreated(GObject*, GAsyncResult* result, gpointer userData)
{
    GUniqueOutPtr<GError> error;
    GDBusConnection* connection = g_dbus_connection_new_for_address_finish(result, &error.outPtr());
    g_assert(G_IS_DBUS_CONNECTION(connection));
    g_assert_no_error(error.get());

    static GDBusNodeInfo* introspectionData = nullptr;
    if (!introspectionData)
        introspectionData = g_dbus_node_info_new_for_xml(introspectionXML, nullptr);

    unsigned registrationID = g_dbus_connection_register_object(
        connection,
        "/org/webkit/gtk/WebExtensionTest",
        introspectionData->interfaces[0],
        &interfaceVirtualTable,
        g_object_ref(userData),
        static_cast<GDestroyNotify>(g_object_unref),
        &error.outPtr());
    if (!registrationID)
        g_error("Failed to register object: %s\n", error->message);

    g_object_set_data_full(G_OBJECT(userData), "dbus-connection", connection, g_object_unref);
    g_signal_connect_object(connection, "closed", G_CALLBACK(+[](GDBusConnection*, gboolean, GError*, gpointer userData) {
        g_object_set_data_full(G_OBJECT(userData), "dbus-connection", nullptr, nullptr);
    }), userData, static_cast<GConnectFlags>(0));
    while (delayedSignalsQueue.size()) {
        DelayedSignal delayedSignal = delayedSignalsQueue.takeFirst();
        switch (delayedSignal.type) {
        case PageCreatedSignal:
            emitPageCreated(connection, delayedSignal.n);
            break;
        case DocumentLoadedSignal:
            emitDocumentLoaded(connection);
            break;
        case URIChangedSignal:
            emitURIChanged(connection, delayedSignal.str.data());
            break;
        case FormControlsAssociatedSignal:
            emitFormControlsAssociated(connection, delayedSignal.str.data());
            break;
        case FormSubmissionWillCompleteSignal:
            emitFormSubmissionEvent(connection, "FormSubmissionWillComplete", delayedSignal.str.data(), delayedSignal.str2.data(), delayedSignal.str3.data(), delayedSignal.b, delayedSignal.b2);
            break;
        case FormSubmissionWillSendDOMEventSignal:
            emitFormSubmissionEvent(connection, "FormSubmissionWillSendDOMEvent", delayedSignal.str.data(), delayedSignal.str2.data(), delayedSignal.str3.data(), delayedSignal.b, delayedSignal.b2);
            break;
        }
    }
}

static void registerGResource(void)
{
    GUniquePtr<char> resourcesPath(g_build_filename(WEBKIT_TEST_RESOURCES_DIR, "webkitglib-tests-resources.gresource", nullptr));
    GResource* resource = g_resource_load(resourcesPath.get(), nullptr);
    g_assert_nonnull(resource);

    g_resources_register(resource);
    g_resource_unref(resource);
}

extern "C" WTF_EXPORT_DECLARATION void webkit_web_extension_initialize_with_user_data(WebKitWebExtension* extension, GVariant* userData)
{
    WebKitScriptWorld* isolatedWorld = webkit_script_world_new_with_name("WebExtensionTestScriptWorld");
    g_assert_true(WEBKIT_IS_SCRIPT_WORLD(isolatedWorld));
    g_assert_cmpstr(webkit_script_world_get_name(isolatedWorld), ==, "WebExtensionTestScriptWorld");
    g_object_set_data_full(G_OBJECT(extension), "wk-script-world", isolatedWorld, g_object_unref);

    g_signal_connect(extension, "user-message-received", G_CALLBACK(extensionMessageReceivedCallback), nullptr);
    g_signal_connect(extension, "page-created", G_CALLBACK(pageCreatedCallback), extension);
    g_signal_connect(webkit_script_world_get_default(), "window-object-cleared", G_CALLBACK(windowObjectCleared), extension);
    g_signal_connect(isolatedWorld, "window-object-cleared", G_CALLBACK(isolatedWorldWindowObjectCleared), nullptr);

    registerGResource();

    g_assert_nonnull(userData);
    const char* guid;
    const char* address;
    g_variant_get(userData, "(&s&s)", &guid, &address);
    g_assert_nonnull(guid);
    g_assert_nonnull(address);

    g_dbus_connection_new_for_address(address, G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, nullptr, nullptr,
        dbusConnectionCreated, extension);
}
