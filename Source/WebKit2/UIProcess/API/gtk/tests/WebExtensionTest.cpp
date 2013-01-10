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

#include <gio/gio.h>
#include <webkit2/webkit-web-extension.h>
#include <wtf/gobject/GOwnPtr.h>

static const char introspectionXML[] =
    "<node>"
    " <interface name='org.webkit.gtk.WebExtensionTest'>"
    "  <method name='GetTitle'>"
    "   <arg type='t' name='pageID' direction='in'/>"
    "   <arg type='s' name='title' direction='out'/>"
    "  </method>"
    " </interface>"
    "</node>";

static void methodCallCallback(GDBusConnection*, const char* sender, const char* objectPath, const char* interfaceName, const char* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData)
{
    if (g_strcmp0(interfaceName, "org.webkit.gtk.WebExtensionTest"))
        return;

    if (!g_strcmp0(methodName, "GetTitle")) {
        uint64_t pageID;
        g_variant_get(parameters, "(t)", &pageID);

        WebKitWebExtension* extension = WEBKIT_WEB_EXTENSION(userData);
        WebKitWebPage* page = webkit_web_extension_get_page(extension, pageID);
        if (!page) {
            g_dbus_method_invocation_return_error(
                invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                "Invalid page ID: %"G_GUINT64_FORMAT, pageID);
            return;
        }

        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        GOwnPtr<char> title(webkit_dom_document_get_title(document));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", title.get()));
    }
}

static const GDBusInterfaceVTable interfaceVirtualTable = {
    methodCallCallback, 0, 0
};

static void busAcquiredCallback(GDBusConnection* connection, const char* name, gpointer userData)
{
    static GDBusNodeInfo *introspectionData = 0;
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
}

extern "C" void webkit_web_extension_initialize(WebKitWebExtension* extension)
{
    g_bus_own_name(
        G_BUS_TYPE_SESSION,
        "org.webkit.gtk.WebExtensionTest",
        G_BUS_NAME_OWNER_FLAGS_NONE,
        busAcquiredCallback,
        0, 0,
        g_object_ref(extension),
        static_cast<GDestroyNotify>(g_object_unref));
}
