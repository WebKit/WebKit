/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
 * Copyright (C) 2009, 2010, 2011, 2012 Igalia S.L.
 *
 * Portions from Mozilla a11y, copyright as follows:
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
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
#include "WebKitAccessibleInterfaceTable.h"

#if ENABLE(ACCESSIBILITY)

#include "AccessibilityListBox.h"
#include "AccessibilityObject.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableCell.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableElement.h"
#include "RenderElement.h"
#include "WebKitAccessible.h"
#include "WebKitAccessibleInterfaceText.h"
#include "WebKitAccessibleUtil.h"

using namespace WebCore;

static AccessibilityObject* core(AtkTable* table)
{
    if (!WEBKIT_IS_ACCESSIBLE(table))
        return nullptr;

    return &webkitAccessibleGetAccessibilityObject(WEBKIT_ACCESSIBLE(table));
}

static AccessibilityTableCell* cell(AtkTable* table, guint row, guint column)
{
    AccessibilityObject* accTable = core(table);
    if (is<AccessibilityTable>(*accTable))
        return downcast<AccessibilityTable>(*accTable).cellForColumnAndRow(column, row);
    return nullptr;
}

static gint cellIndex(AccessibilityTableCell* axCell, AccessibilityTable* axTable)
{
    // Calculate the cell's index as if we had a traditional Gtk+ table in
    // which cells are all direct children of the table, arranged row-first.
    AccessibilityObject::AccessibilityChildrenVector allCells;
    axTable->cells(allCells);
    AccessibilityObject::AccessibilityChildrenVector::iterator position;
    position = std::find(allCells.begin(), allCells.end(), axCell);
    if (position == allCells.end())
        return -1;
    return position - allCells.begin();
}

static AccessibilityTableCell* cellAtIndex(AtkTable* table, gint index)
{
    AccessibilityObject* accTable = core(table);
    if (is<AccessibilityTable>(*accTable)) {
        AccessibilityObject::AccessibilityChildrenVector allCells;
        downcast<AccessibilityTable>(*accTable).cells(allCells);
        if (0 <= index && static_cast<unsigned>(index) < allCells.size())
            return downcast<AccessibilityTableCell>(allCells[index].get());
    }
    return nullptr;
}

static AtkObject* webkitAccessibleTableRefAt(AtkTable* table, gint row, gint column)
{
    g_return_val_if_fail(ATK_TABLE(table), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), 0);

    AccessibilityTableCell* axCell = cell(table, row, column);
    if (!axCell)
        return 0;

    auto* cell = axCell->wrapper();
    if (!cell)
        return 0;

    // This method transfers full ownership over the returned
    // AtkObject, so an extra reference is needed here.
    return ATK_OBJECT(g_object_ref(cell));
}

static gint webkitAccessibleTableGetIndexAt(AtkTable* table, gint row, gint column)
{
    g_return_val_if_fail(ATK_TABLE(table), -1);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), -1);

    AccessibilityTableCell* axCell = cell(table, row, column);
    AccessibilityTable* axTable = downcast<AccessibilityTable>(core(table));
    return cellIndex(axCell, axTable);
}

static gint webkitAccessibleTableGetColumnAtIndex(AtkTable* table, gint index)
{
    g_return_val_if_fail(ATK_TABLE(table), -1);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), -1);

    AccessibilityTableCell* axCell = cellAtIndex(table, index);
    if (axCell) {
        std::pair<unsigned, unsigned> columnRange;
        axCell->columnIndexRange(columnRange);
        return columnRange.first;
    }
    return -1;
}

static gint webkitAccessibleTableGetRowAtIndex(AtkTable* table, gint index)
{
    g_return_val_if_fail(ATK_TABLE(table), -1);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), -1);

    AccessibilityTableCell* axCell = cellAtIndex(table, index);
    if (axCell) {
        std::pair<unsigned, unsigned> rowRange;
        axCell->rowIndexRange(rowRange);
        return rowRange.first;
    }
    return -1;
}

static gint webkitAccessibleTableGetNColumns(AtkTable* table)
{
    g_return_val_if_fail(ATK_TABLE(table), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), 0);

    AccessibilityObject* accTable = core(table);
    if (!is<AccessibilityTable>(*accTable))
        return 0;

    if (int columnCount = downcast<AccessibilityTable>(*accTable).axColumnCount())
        return columnCount;

    return downcast<AccessibilityTable>(*accTable).columnCount();
}

static gint webkitAccessibleTableGetNRows(AtkTable* table)
{
    g_return_val_if_fail(ATK_TABLE(table), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), 0);

    AccessibilityObject* accTable = core(table);
    if (!is<AccessibilityTable>(*accTable))
        return 0;

    if (int rowCount = downcast<AccessibilityTable>(*accTable).axRowCount())
        return rowCount;

    return downcast<AccessibilityTable>(*accTable).rowCount();
}

static gint webkitAccessibleTableGetColumnExtentAt(AtkTable* table, gint row, gint column)
{
    g_return_val_if_fail(ATK_TABLE(table), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), 0);

    AccessibilityTableCell* axCell = cell(table, row, column);
    if (axCell) {
        std::pair<unsigned, unsigned> columnRange;
        axCell->columnIndexRange(columnRange);
        return columnRange.second;
    }
    return 0;
}

static gint webkitAccessibleTableGetRowExtentAt(AtkTable* table, gint row, gint column)
{
    g_return_val_if_fail(ATK_TABLE(table), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), 0);

    AccessibilityTableCell* axCell = cell(table, row, column);
    if (axCell) {
        std::pair<unsigned, unsigned> rowRange;
        axCell->rowIndexRange(rowRange);
        return rowRange.second;
    }
    return 0;
}

static AtkObject* webkitAccessibleTableGetColumnHeader(AtkTable* table, gint column)
{
    g_return_val_if_fail(ATK_TABLE(table), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), 0);

    AccessibilityObject* accTable = core(table);
    if (is<AccessibilityTable>(*accTable)) {
        AccessibilityObject::AccessibilityChildrenVector columnHeaders;
        downcast<AccessibilityTable>(*accTable).columnHeaders(columnHeaders);

        for (const auto& columnHeader : columnHeaders) {
            std::pair<unsigned, unsigned> columnRange;
            downcast<AccessibilityTableCell>(*columnHeader).columnIndexRange(columnRange);
            if (columnRange.first <= static_cast<unsigned>(column) && static_cast<unsigned>(column) < columnRange.first + columnRange.second)
                return ATK_OBJECT(columnHeader->wrapper());
        }
    }
    return nullptr;
}

static AtkObject* webkitAccessibleTableGetRowHeader(AtkTable* table, gint row)
{
    g_return_val_if_fail(ATK_TABLE(table), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), 0);

    AccessibilityObject* accTable = core(table);
    if (is<AccessibilityTable>(*accTable)) {
        AccessibilityObject::AccessibilityChildrenVector rowHeaders;
        downcast<AccessibilityTable>(*accTable).rowHeaders(rowHeaders);

        for (const auto& rowHeader : rowHeaders) {
            std::pair<unsigned, unsigned> rowRange;
            downcast<AccessibilityTableCell>(*rowHeader).rowIndexRange(rowRange);
            if (rowRange.first <= static_cast<unsigned>(row) && static_cast<unsigned>(row) < rowRange.first + rowRange.second)
                return ATK_OBJECT(rowHeader->wrapper());
        }
    }
    return nullptr;
}

static AtkObject* webkitAccessibleTableGetCaption(AtkTable* table)
{
    g_return_val_if_fail(ATK_TABLE(table), nullptr);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), nullptr);

    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject()) {
        Node* node = accTable->node();
        if (is<HTMLTableElement>(node)) {
            auto caption = downcast<HTMLTableElement>(*node).caption();
            if (caption)
                return ATK_OBJECT(AccessibilityObject::firstAccessibleObjectFromNode(caption->renderer()->element())->wrapper());
        }
    }
    return nullptr;
}

static const gchar* webkitAccessibleTableGetColumnDescription(AtkTable* table, gint column)
{
    g_return_val_if_fail(ATK_TABLE(table), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), 0);

    AtkObject* columnHeader = atk_table_get_column_header(table, column);
    if (columnHeader && ATK_IS_TEXT(columnHeader))
        return atk_text_get_text(ATK_TEXT(columnHeader), 0, -1);

    return 0;
}

static const gchar* webkitAccessibleTableGetRowDescription(AtkTable* table, gint row)
{
    g_return_val_if_fail(ATK_TABLE(table), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(table), 0);

    AtkObject* rowHeader = atk_table_get_row_header(table, row);
    if (rowHeader && ATK_IS_TEXT(rowHeader))
        return atk_text_get_text(ATK_TEXT(rowHeader), 0, -1);

    return 0;
}

void webkitAccessibleTableInterfaceInit(AtkTableIface* iface)
{
    iface->ref_at = webkitAccessibleTableRefAt;
    iface->get_index_at = webkitAccessibleTableGetIndexAt;
    iface->get_column_at_index = webkitAccessibleTableGetColumnAtIndex;
    iface->get_row_at_index = webkitAccessibleTableGetRowAtIndex;
    iface->get_n_columns = webkitAccessibleTableGetNColumns;
    iface->get_n_rows = webkitAccessibleTableGetNRows;
    iface->get_column_extent_at = webkitAccessibleTableGetColumnExtentAt;
    iface->get_row_extent_at = webkitAccessibleTableGetRowExtentAt;
    iface->get_column_header = webkitAccessibleTableGetColumnHeader;
    iface->get_row_header = webkitAccessibleTableGetRowHeader;
    iface->get_caption = webkitAccessibleTableGetCaption;
    iface->get_column_description = webkitAccessibleTableGetColumnDescription;
    iface->get_row_description = webkitAccessibleTableGetRowDescription;
}

#endif
