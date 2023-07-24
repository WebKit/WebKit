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
#include "AccessibilityAtspiEnums.h"
#include <gio/gio.h>

namespace WebCore {

GDBusInterfaceVTable AccessibilityObjectAtspi::s_tableCellFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant*, GDBusMethodInvocation* invocation, gpointer userData) {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(methodName, "GetRowHeaderCells")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a(so)"));
            for (const auto& wrapper : atspiObject->cellRowHeaders())
                g_variant_builder_add(&builder, "@(so)", wrapper->reference());
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(a(so))", &builder));
        } else if (!g_strcmp0(methodName, "GetColumnHeaderCells")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a(so)"));
            for (const auto& wrapper : atspiObject->cellColumnHeaders())
                g_variant_builder_add(&builder, "@(so)", wrapper->reference());
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(a(so))", &builder));
        } else if (!g_strcmp0(methodName, "GetRowColumnSpan")) {
            auto position = atspiObject->cellPosition();
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(iiii)", position.first.value_or(-1), position.second.value_or(-1), atspiObject->rowSpan(), atspiObject->columnSpan()));
        }
    },
    // get_property
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GError** error, gpointer userData) -> GVariant* {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(propertyName, "ColumnSpan"))
            return g_variant_new_int32(atspiObject->columnSpan());
        if (!g_strcmp0(propertyName, "Position")) {
            auto position = atspiObject->cellPosition();
            return g_variant_new("(ii)", position.first.value_or(-1), position.second.value_or(-1));
        }
        if (!g_strcmp0(propertyName, "RowSpan"))
            return g_variant_new_int32(atspiObject->rowSpan());
        if (!g_strcmp0(propertyName, "Table")) {
            auto* axObject = atspiObject->m_coreObject;
            if (!axObject || !axObject->isExposedTableCell())
                return AccessibilityAtspi::singleton().nullReference();

            AccessibilityObjectAtspi* wrapper = atspiObject.ptr();
            while (auto parent = wrapper->parent()) {
                wrapper = parent.value();
                if (!wrapper)
                    break;

                wrapper->updateBackingStore();
                axObject = wrapper->m_coreObject;
                if (axObject && axObject->isTable())
                    break;
            }
            return wrapper ? wrapper->reference() : AccessibilityAtspi::singleton().nullReference();
        }

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

Vector<RefPtr<AccessibilityObjectAtspi>> AccessibilityObjectAtspi::cellRowHeaders() const
{
    if (!m_coreObject)
        return { };

    return wrapperVector(m_coreObject->rowHeaders());
}

Vector<RefPtr<AccessibilityObjectAtspi>> AccessibilityObjectAtspi::cellColumnHeaders() const
{
    if (!m_coreObject)
        return { };

    return wrapperVector(m_coreObject->columnHeaders());
}

unsigned AccessibilityObjectAtspi::rowSpan() const
{
    return m_coreObject ? m_coreObject->rowIndexRange().second : 0;
}

unsigned AccessibilityObjectAtspi::columnSpan() const
{
    return m_coreObject ? m_coreObject->columnIndexRange().second : 0;
}

std::pair<std::optional<unsigned>, std::optional<unsigned>> AccessibilityObjectAtspi::cellPosition() const
{
    std::pair<std::optional<unsigned>, std::optional<unsigned>> position;
    if (!m_coreObject)
        return position;

    position.first = m_coreObject->rowIndexRange().first;
    position.second = m_coreObject->columnIndexRange().first;

    return position;
}

} // namespace WebCore

#endif // USE(ATSPI)
