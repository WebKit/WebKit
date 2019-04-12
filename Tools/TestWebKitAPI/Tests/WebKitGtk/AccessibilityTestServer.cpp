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

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static const char introspectionXML[] =
    "<node>"
    " <interface name='org.webkit.gtk.AccessibilityTest'>"
    "  <method name='LoadHTML'>"
    "   <arg type='s' name='html' direction='in'/>"
    "   <arg type='s' name='baseURI' direction='in'/>"
    "  </method>"
    " </interface>"
    "</node>";

static void loadChangedCallback(WebKitWebView* webView, WebKitLoadEvent loadEvent, GDBusMethodInvocation* invocation)
{
    if (loadEvent != WEBKIT_LOAD_FINISHED)
        return;

    g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(loadChangedCallback), invocation);
    g_dbus_method_invocation_return_value(invocation, nullptr);
}

static const GDBusInterfaceVTable interfaceVirtualTable = {
    // methodCall
    [](GDBusConnection* connection, const char* sender, const char* objectPath, const char* interfaceName, const char* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        if (g_strcmp0(interfaceName, "org.webkit.gtk.AccessibilityTest"))
            return;

        auto* webView = WEBKIT_WEB_VIEW(userData);

        if (!g_strcmp0(methodName, "LoadHTML")) {
            const char* html;
            const char* baseURI;
            g_variant_get(parameters, "(&s&s)", &html, &baseURI);
            g_signal_connect(webView, "load-changed", G_CALLBACK(loadChangedCallback), invocation);
            webkit_web_view_load_html(webView, html, baseURI && *baseURI ? baseURI : nullptr);
        }
    },
    nullptr,
    nullptr,
    { 0, }
};

int main(int argc, char** argv)
{
    // Make sure that the ATK bridge is loaded.
    g_setenv("GTK_MODULES", "atk-bridge", TRUE);

    gtk_init(&argc, &argv);

    GtkWidget* webView = webkit_web_view_new();

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(window, "delete-event", G_CALLBACK(gtk_main_quit), nullptr);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webView));
    gtk_widget_show_all(window);

    g_bus_own_name(G_BUS_TYPE_SESSION, "org.webkit.gtk.AccessibilityTest", G_BUS_NAME_OWNER_FLAGS_NONE,
        [](GDBusConnection* connection, const char* name, gpointer userData) {
            static GDBusNodeInfo *introspectionData = nullptr;
            if (!introspectionData)
                introspectionData = g_dbus_node_info_new_for_xml(introspectionXML, nullptr);

            g_dbus_connection_register_object(connection, "/org/webkit/gtk/AccessibilityTest", introspectionData->interfaces[0],
                &interfaceVirtualTable, userData, nullptr, nullptr);
        }, nullptr, nullptr, webView, nullptr);

    gtk_main();
}
