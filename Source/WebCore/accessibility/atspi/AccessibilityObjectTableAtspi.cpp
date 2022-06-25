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
#include "AccessibilityObject.h" // NOLINT: check-webkit-style has problems with files that do not have primary header
#include "HTMLTableCaptionElement.h"
#include "HTMLTableElement.h"
#include "RenderElement.h"
#include <gio/gio.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

GDBusInterfaceVTable AccessibilityObjectAtspi::s_tableFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(methodName, "GetAccessibleAt")) {
            int row, column;
            g_variant_get(parameters, "(ii)", &row, &column);
            auto* cell = row >= 0 && column >= 0 ? atspiObject->cell(row, column) : nullptr;
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", cell ? cell->reference() : AccessibilityAtspi::singleton().nullReference()));
        } else if (!g_strcmp0(methodName, "GetIndexAt")) {
            int row, column;
            g_variant_get(parameters, "(ii)", &row, &column);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", row >= 0 && column >= 0 ? atspiObject->cellIndex(row, column).value_or(-1) : -1));
        } else if (!g_strcmp0(methodName, "GetRowAtIndex")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", index >= 0 ? atspiObject->rowAtIndex(index).value_or(-1) : -1));
        } else if (!g_strcmp0(methodName, "GetColumnAtIndex")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", index >= 0 ? atspiObject->columnAtIndex(index).value_or(-1) : -1));
        } else if (!g_strcmp0(methodName, "GetRowDescription")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", index >= 0 ? atspiObject->rowDescription(index).utf8().data() : ""));
        } else if (!g_strcmp0(methodName, "GetColumnDescription")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", index >= 0 ? atspiObject->columnDescription(index).utf8().data() : ""));
        } else if (!g_strcmp0(methodName, "GetRowExtentAt")) {
            int row, column;
            g_variant_get(parameters, "(ii)", &row, &column);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", row >= 0 && column >= 0 ? atspiObject->rowExtent(row, column) : -1));
        } else if (!g_strcmp0(methodName, "GetColumnExtentAt")) {
            int row, column;
            g_variant_get(parameters, "(ii)", &row, &column);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", row >= 0 && column >= 0 ? atspiObject->columnExtent(row, column) : -1));
        } else if (!g_strcmp0(methodName, "GetRowHeader")) {
            int row;
            g_variant_get(parameters, "(i)", &row);
            auto* header = row >= 0 ? atspiObject->rowHeader(row) : nullptr;
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", header ? header->reference() : AccessibilityAtspi::singleton().nullReference()));
        } else if (!g_strcmp0(methodName, "GetColumnHeader")) {
            int column;
            g_variant_get(parameters, "(i)", &column);
            auto* header = column >= 0 ? atspiObject->columnHeader(column) : nullptr;
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", header ? header->reference() : AccessibilityAtspi::singleton().nullReference()));
        } else if (!g_strcmp0(methodName, "GetRowColumnExtentsAtIndex")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            if (index < 0) {
                g_dbus_method_invocation_return_value(invocation, g_variant_new("(biiiib)", FALSE, -1, -1, -1, -1, FALSE));
                return;
            }

            auto row = atspiObject->rowAtIndex(index);
            auto column = atspiObject->columnAtIndex(index);
            auto* cell = atspiObject->m_coreObject ? atspiObject->m_coreObject->cellForColumnAndRow(*column, *row) : nullptr;
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(biiiib)", cell && cell->isTableCell() ? TRUE : FALSE,
                row.value_or(-1), column.value_or(-1), row && column ? atspiObject->rowExtent(*row, *column) : -1,
                row && column ? atspiObject->columnExtent(*row, *column) : -1, FALSE));
        } else if (!g_strcmp0(methodName, "GetSelectedRows")
            || !g_strcmp0(methodName, "GetSelectedColumns")
            || !g_strcmp0(methodName, "IsRowSelected")
            || !g_strcmp0(methodName, "IsColumnSelected")
            || !g_strcmp0(methodName, "IsSelected")
            || !g_strcmp0(methodName, "AddRowSelection")
            || !g_strcmp0(methodName, "AddColumnSelection")
            || !g_strcmp0(methodName, "RemoveRowSelection")
            || !g_strcmp0(methodName, "RemoveColumnSelection"))
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    },
    // get_property
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GError** error, gpointer userData) -> GVariant* {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(propertyName, "NRows"))
            return g_variant_new_int32(atspiObject->rowCount());
        if (!g_strcmp0(propertyName, "NColumns"))
            return g_variant_new_int32(atspiObject->columnCount());
        if (!g_strcmp0(propertyName, "Caption")) {
            auto* caption = atspiObject->tableCaption();
            return caption ? caption->reference() : AccessibilityAtspi::singleton().nullReference();
        }
        if (!g_strcmp0(propertyName, "Summary"))
            return AccessibilityAtspi::singleton().nullReference();
        if (!g_strcmp0(propertyName, "NSelectedRows"))
            return g_variant_new_int32(0);
        if (!g_strcmp0(propertyName, "NSelectedColumns"))
            return g_variant_new_int32(0);

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

unsigned AccessibilityObjectAtspi::rowCount() const
{
    return m_coreObject ? m_coreObject->rowCount() : 0;
}

unsigned AccessibilityObjectAtspi::columnCount() const
{
    return m_coreObject ? m_coreObject->columnCount() : 0;
}

AccessibilityObjectAtspi* AccessibilityObjectAtspi::cell(unsigned row, unsigned column) const
{
    if (!m_coreObject)
        return nullptr;

    if (auto* tableCell = m_coreObject->cellForColumnAndRow(column, row))
        return tableCell->wrapper();

    return nullptr;
}

AccessibilityObjectAtspi* AccessibilityObjectAtspi::tableCaption() const
{
    if (!m_coreObject)
        return nullptr;

    if (auto* node = m_coreObject->node()) {
        if (!is<HTMLTableElement>(node))
            return nullptr;

        if (auto caption = downcast<HTMLTableElement>(*node).caption()) {
            if (auto* renderer = caption->renderer()) {
                if (auto* element = AccessibilityObject::firstAccessibleObjectFromNode(renderer->element()))
                    return element->wrapper();
            }
        }
    }

    return nullptr;
}

std::optional<unsigned> AccessibilityObjectAtspi::cellIndex(unsigned row, unsigned column) const
{
    if (!m_coreObject)
        return std::nullopt;

    auto* cell = m_coreObject->cellForColumnAndRow(column, row);
    if (!cell)
        return std::nullopt;

    auto cells = m_coreObject->cells();
    AXCoreObject::AccessibilityChildrenVector::iterator position;
    position = std::find(cells.begin(), cells.end(), cell);
    if (position == cells.end())
        return std::nullopt;
    return position - cells.begin();
}

std::optional<unsigned> AccessibilityObjectAtspi::rowAtIndex(unsigned index) const
{
    if (!m_coreObject)
        return std::nullopt;

    auto cells = m_coreObject->cells();
    if (index >= cells.size())
        return std::nullopt;

    return cells[index]->rowIndexRange().first;
}

std::optional<unsigned> AccessibilityObjectAtspi::columnAtIndex(unsigned index) const
{
    if (!m_coreObject)
        return std::nullopt;

    auto cells = m_coreObject->cells();
    if (index >= cells.size())
        return std::nullopt;

    return cells[index]->columnIndexRange().first;
}

AccessibilityObjectAtspi* AccessibilityObjectAtspi::rowHeader(unsigned row) const
{
    if (!m_coreObject)
        return nullptr;

    auto headers = m_coreObject->rowHeaders();
    for (const auto& header : headers) {
        auto range = header->rowIndexRange();
        if (range.first <= row && row < range.first + range.second) {
            if (auto* wrapper = header->wrapper())
                return wrapper;
        }
    }

    return nullptr;
}

AccessibilityObjectAtspi* AccessibilityObjectAtspi::columnHeader(unsigned column) const
{
    if (!m_coreObject)
        return nullptr;

    auto headers = m_coreObject->columnHeaders();
    for (const auto& header : headers) {
        auto range = header->columnIndexRange();
        if (range.first <= column && column < range.first + range.second) {
            if (auto* wrapper = header->wrapper())
                return wrapper;
        }
    }

    return nullptr;
}

String AccessibilityObjectAtspi::rowDescription(unsigned row) const
{
    if (!m_coreObject)
        return { };

    StringBuilder builder;
    bool isFirst = true;
    auto headers = m_coreObject->rowHeaders();
    for (const auto& header : headers) {
        auto* wrapper = header->wrapper();
        if (!wrapper)
            continue;

        auto range = header->rowIndexRange();
        if (range.first <= row && row < range.first + range.second) {
            wrapper->updateBackingStore();
            auto text = wrapper->text();
            if (text.isEmpty())
                continue;

            if (!isFirst)
                builder.append(' ');
            else
                isFirst = false;
            builder.append(text);
        }
    }
    return builder.toString();
}

String AccessibilityObjectAtspi::columnDescription(unsigned column) const
{
    if (!m_coreObject)
        return { };

    StringBuilder builder;
    bool isFirst = true;
    auto headers = m_coreObject->columnHeaders();
    for (const auto& header : headers) {
        auto* wrapper = header->wrapper();
        if (!wrapper)
            continue;

        auto range = header->columnIndexRange();
        if (range.first <= column && column < range.first + range.second) {
            wrapper->updateBackingStore();
            auto text = wrapper->text();
            if (text.isEmpty())
                continue;

            if (!isFirst)
                builder.append(' ');
            else
                isFirst = false;
            builder.append(text);
        }
    }
    return builder.toString();
}

unsigned AccessibilityObjectAtspi::rowExtent(unsigned row, unsigned column) const
{
    if (!m_coreObject)
        return 0;

    auto* cell = m_coreObject->cellForColumnAndRow(column, row);
    return cell ? cell->rowIndexRange().second : 0;
}

unsigned AccessibilityObjectAtspi::columnExtent(unsigned row, unsigned column) const
{
    if (!m_coreObject)
        return 0;

    auto* cell = m_coreObject->cellForColumnAndRow(column, row);
    return cell ? cell->columnIndexRange().second : 0;
}

Vector<RefPtr<AccessibilityObjectAtspi>> AccessibilityObjectAtspi::cells() const
{
    if (!m_coreObject)
        return { };

    return wrapperVector(m_coreObject->cells());
}

Vector<RefPtr<AccessibilityObjectAtspi>> AccessibilityObjectAtspi::rows() const
{
    if (!m_coreObject)
        return { };

    return wrapperVector(m_coreObject->rows());
}

Vector<RefPtr<AccessibilityObjectAtspi>> AccessibilityObjectAtspi::rowHeaders() const
{
    if (!m_coreObject)
        return { };

    return wrapperVector(m_coreObject->rowHeaders());
}

Vector<RefPtr<AccessibilityObjectAtspi>> AccessibilityObjectAtspi::columnHeaders() const
{
    if (!m_coreObject)
        return { };

    return wrapperVector(m_coreObject->columnHeaders());
}

} // namespace WebCore

#endif // USE(ATSPI)
