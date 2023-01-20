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

#include "AccessibilityAtspiEnums.h"
#include "AccessibilityObject.h"
#include <gio/gio.h>

namespace WebCore {

GDBusInterfaceVTable AccessibilityObjectAtspi::s_imageFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(methodName, "GetImageExtents")) {
            uint32_t coordinateType;
            g_variant_get(parameters, "(u)", &coordinateType);
            auto rect = atspiObject->elementRect(static_cast<Atspi::CoordinateType>(coordinateType));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("((iiii))", rect.x(), rect.y(), rect.width(), rect.height()));
        } else if (!g_strcmp0(methodName, "GetImagePosition")) {
            uint32_t coordinateType;
            g_variant_get(parameters, "(u)", &coordinateType);
            auto rect = atspiObject->elementRect(static_cast<Atspi::CoordinateType>(coordinateType));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(ii)", rect.x(), rect.y()));
        } else if (!g_strcmp0(methodName, "GetImageSize")) {
            auto rect = atspiObject->elementRect(Atspi::CoordinateType::ParentCoordinates);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(ii)", rect.width(), rect.height()));
        }
    },
    // get_property
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GError** error, gpointer userData) -> GVariant* {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(propertyName, "ImageDescription"))
            return g_variant_new_string(atspiObject->imageDescription().utf8().data());
        if (!g_strcmp0(propertyName, "ImageLocale"))
            return g_variant_new_string(atspiObject->locale().utf8().data());

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

String AccessibilityObjectAtspi::imageDescription() const
{
    if (!m_coreObject)
        return { };

    Vector<AccessibilityText> textOrder;
    m_coreObject->accessibilityText(textOrder);

    bool visibleTextAvailable = false;
    for (const AccessibilityText& text : textOrder) {
        if (text.textSource == AccessibilityTextSource::Alternative)
            return text.text;

        switch (text.textSource) {
        case AccessibilityTextSource::Visible:
        case AccessibilityTextSource::Children:
        case AccessibilityTextSource::LabelByElement:
            visibleTextAvailable = true;
        default:
            break;
        }

        if (text.textSource == AccessibilityTextSource::TitleTag && !visibleTextAvailable)
            return text.text;
    }

    return { };
}

} // namespace WebCore

#endif // USE(ATSPI)
