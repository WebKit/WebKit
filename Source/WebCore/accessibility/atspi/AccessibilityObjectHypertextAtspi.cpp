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
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

GDBusInterfaceVTable AccessibilityObjectAtspi::s_hypertextFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(methodName, "GetNLinks")) {
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", atspiObject->hyperlinkCount()));
        } else if (!g_strcmp0(methodName, "GetLink")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            auto* wrapper = index >= 0 ? atspiObject->hyperlink(index) : nullptr;
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", wrapper ? wrapper->hyperlinkReference() : AccessibilityAtspi::singleton().nullReference()));
        } else if (!g_strcmp0(methodName, "GetLinkIndex")) {
            int offset;
            g_variant_get(parameters, "(i)", &offset);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", offset >= 0 ? atspiObject->hyperlinkIndex(offset).value_or(-1) : -1));
        }
    },
    // get_property
    nullptr,
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

unsigned AccessibilityObjectAtspi::hyperlinkCount() const
{
    if (!m_coreObject)
        return 0;

    unsigned linkCount = 0;
    const auto& children = m_coreObject->children();
    for (const auto& child : children) {
        if (child->accessibilityIsIgnored())
            continue;

        auto* wrapper = child->wrapper();
        if (wrapper && wrapper->interfaces().contains(Interface::Hyperlink))
            linkCount++;
    }

    return linkCount;
}

AccessibilityObjectAtspi* AccessibilityObjectAtspi::hyperlink(unsigned index) const
{
    if (!m_coreObject)
        return nullptr;

    const auto& children = m_coreObject->children();
    if (index >= children.size())
        return nullptr;

    int linkIndex = -1;
    for (const auto& child : children) {
        if (child->accessibilityIsIgnored())
            continue;

        auto* wrapper = child->wrapper();
        if (!wrapper || !wrapper->interfaces().contains(Interface::Hyperlink))
            continue;

        if (static_cast<unsigned>(++linkIndex) == index)
            return wrapper;
    }

    return nullptr;
}

std::optional<unsigned> AccessibilityObjectAtspi::hyperlinkIndex(unsigned offset) const
{
    return characterIndex(objectReplacementCharacter, offset);
}

} // namespace WebCore

#endif // USE(ATSPI)
