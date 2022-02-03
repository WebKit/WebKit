/*
 * Copyright (C) 2021 Igalia S.L.
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
#include "AccessibilityObjectAtspi.h"

#if USE(ATSPI)

#include "AccessibilityRootAtspi.h"
#include <gio/gio.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

GDBusInterfaceVTable AccessibilityObjectAtspi::s_actionFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(methodName, "GetDescription"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", ""));
        else if (!g_strcmp0(methodName, "GetName")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", !index ? atspiObject->actionName().utf8().data() : ""));
        } else if (!g_strcmp0(methodName, "GetLocalizedName")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", !index ? atspiObject->localizedActionName().utf8().data() : ""));
        } else if (!g_strcmp0(methodName, "GetKeyBinding")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", !index ? atspiObject->actionKeyBinding().utf8().data() : ""));
        } else if (!g_strcmp0(methodName, "DoAction")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", !index ? atspiObject->doAction() : FALSE));
        }
    },
    // get_property
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GError** error, gpointer userData) -> GVariant* {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(propertyName, "NActions"))
            return g_variant_new_int32(1);

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

String AccessibilityObjectAtspi::actionName() const
{
    return m_coreObject ? m_coreObject->actionVerb() : String();
}

String AccessibilityObjectAtspi::localizedActionName() const
{
    return m_coreObject ? m_coreObject->localizedActionVerb() : String();
}

String AccessibilityObjectAtspi::actionKeyBinding() const
{
    return m_coreObject ? m_coreObject->accessKey() : String();
}

bool AccessibilityObjectAtspi::doAction() const
{
    return m_coreObject ? m_coreObject->performDefaultAction() : false;
}

} // namespace WebCore

#endif // USE(ATSPI)
