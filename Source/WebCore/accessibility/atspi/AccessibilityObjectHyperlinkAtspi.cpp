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

#include "AccessibilityAtspi.h"
#include <gio/gio.h>
#include <wtf/URL.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

GDBusInterfaceVTable AccessibilityObjectAtspi::s_hyperlinkFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(methodName, "GetObject")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", !index ? atspiObject->reference() : AccessibilityAtspi::singleton().nullReference()));
        } else if (!g_strcmp0(methodName, "GetURI")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", !index ? atspiObject->url().string().utf8().data() : ""));
        } else if (!g_strcmp0(methodName, "IsValid"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", atspiObject->m_coreObject ? TRUE : FALSE));
    },
    // get_property
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GError** error, gpointer userData) -> GVariant* {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(propertyName, "NAnchors"))
            return g_variant_new_int32(1);
        if (!g_strcmp0(propertyName, "StartIndex"))
            return g_variant_new_int32(atspiObject->offsetInParent());
        if (!g_strcmp0(propertyName, "EndIndex"))
            return g_variant_new_int32(atspiObject->offsetInParent() + 1);

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

URL AccessibilityObjectAtspi::url() const
{
    return m_coreObject ? m_coreObject->url() : URL();
}

unsigned AccessibilityObjectAtspi::offsetInParent() const
{
    if (!m_coreObject)
        return 0;

    auto* parent = m_coreObject->parentObjectUnignored();
    if (!parent || !parent->wrapper())
        return 0;

    int index = -1;
    const auto& children = parent->children();
    for (const auto& child : children) {
        if (child->accessibilityIsIgnored())
            continue;

        auto* wrapper = child->wrapper();
        if (!wrapper || !wrapper->interfaces().contains(Interface::Hyperlink))
            continue;

        index++;
        if (wrapper == this)
            break;
    }

    if (index == -1)
        return 0;

    return parent->wrapper()->characterOffset(objectReplacementCharacter, index).value_or(0);
}

} // namespace WebCore

#endif // USE(ATSPI)
