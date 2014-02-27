/*
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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
#include "WebKitAccessibleInterfaceTableCell.h"

#if HAVE(ACCESSIBILITY) && ATK_CHECK_VERSION(2,11,90)

#include "AccessibilityObject.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableCell.h"
#include "WebKitAccessibleUtil.h"
#include "WebKitAccessibleWrapperAtk.h"

using namespace WebCore;

static GPtrArray* convertToGPtrArray(const AccessibilityObject::AccessibilityChildrenVector& children)
{
    GPtrArray* array = g_ptr_array_new();
    for (const auto& child : children) {
        if (AtkObject* atkObject = child->wrapper())
            g_ptr_array_add(array, atkObject);
    }
    return array;
}

static AccessibilityObject* core(AtkTableCell* cell)
{
    if (!WEBKIT_IS_ACCESSIBLE(cell))
        return nullptr;

    return webkitAccessibleGetAccessibilityObject(WEBKIT_ACCESSIBLE(cell));
}

GPtrArray* webkitAccessibleTableCellGetColumnHeaderCells(AtkTableCell* cell)
{
    g_return_val_if_fail(ATK_TABLE_CELL(cell), nullptr);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(cell), nullptr);

    AccessibilityObject* axObject = core(cell);
    if (!axObject || !axObject->isTableCell())
        return nullptr;

    AccessibilityObject::AccessibilityChildrenVector columnHeaders;
    toAccessibilityTableCell(axObject)->columnHeaders(columnHeaders);

    return convertToGPtrArray(columnHeaders);
}

GPtrArray* webkitAccessibleTableCellGetRowHeaderCells(AtkTableCell* cell)
{
    g_return_val_if_fail(ATK_TABLE_CELL(cell), nullptr);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(cell), nullptr);

    AccessibilityObject* axObject = core(cell);
    if (!axObject || !axObject->isTableCell())
        return nullptr;

    AccessibilityObject::AccessibilityChildrenVector rowHeaders;
    toAccessibilityTableCell(axObject)->rowHeaders(rowHeaders);

    return convertToGPtrArray(rowHeaders);
}

void webkitAccessibleTableCellInterfaceInit(AtkTableCellIface* iface)
{
    iface->get_column_header_cells = webkitAccessibleTableCellGetColumnHeaderCells;
    iface->get_row_header_cells = webkitAccessibleTableCellGetRowHeaderCells;
}

#endif
