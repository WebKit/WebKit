/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2019 Google Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "TableHitTester.h"

#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "RenderBoxInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderTable.h"
#include "RenderTableCellInlines.h"
#include "RenderTableInlines.h"
#include "RenderTableRow.h"
#include "RenderTableSection.h"
#include "RenderTableSectionInlines.h"

namespace WebCore {
namespace Paint {

bool TableHitTester::hitTest(RenderTable& table, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    LayoutPoint adjustedLocation = accumulatedOffset + table.location();

    // Check kids first.
    if (!table.hasNonVisibleOverflow() || locationInContainer.intersects(table.overflowClipRect(adjustedLocation))) {
        for (CheckedPtr child = table.lastChild(); child; child = child->previousSibling()) {
            CheckedPtr box = dynamicDowncast<RenderBox>(*child);
            if (box && !box->hasSelfPaintingLayer() && (box->isRenderTableSection() || box->isRenderTableCaption())) {
                LayoutPoint childPoint = table.blockStartRelativeToPhysicalForChild(*box, adjustedLocation);
                if (box->nodeAtPoint(request, result, locationInContainer, childPoint, action)) {
                    table.updateHitTestResult(result, toLayoutPoint(locationInContainer.point() - childPoint));
                    return true;
                }
            }
        }
    }

    // Check our bounds next.
    LayoutRect boundsRect(adjustedLocation, table.borderBoxSize());
    if (table.visibleToHitTesting(request) && (action == HitTestAction::BlockBackground || action == HitTestAction::ChildBlockBackground) && locationInContainer.intersects(boundsRect)) {
        table.updateHitTestResult(result, table.physicalToBlockStartRelative(locationInContainer.point() - toLayoutSize(adjustedLocation)));
        if (result.addNodeToListBasedTestResult(protect(table.nodeForHitTest()).get(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

bool TableHitTester::hitTest(RenderTableSection& section, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    // If we have no children then we have nothing to do.
    if (!section.firstRow())
        return false;

    // Table sections cannot ever be hit tested. Effectively they do not exist.
    // Just forward to our children always.
    LayoutPoint adjustedLocation = accumulatedOffset + section.location();

    if (section.hasNonVisibleOverflow() && !locationInContainer.intersects(section.overflowClipRect(adjustedLocation)))
        return false;

    if (section.hasOverflowingCell()) {
        for (CheckedPtr row = section.lastRow(); row; row = row->previousRow()) {
            // FIXME: We have to skip over inline flows, since they can show up inside table rows
            // at the moment (a demoted inline <form> for example). If we ever implement a
            // table-specific hit-test method (which we should do for performance reasons anyway),
            // then we can remove this check.
            if (!row->hasSelfPaintingLayer()) {
                auto rowOffset = section.blockStartRelativeToPhysicalForChild(*row, adjustedLocation);
                if (row->nodeAtPoint(request, result, locationInContainer, rowOffset, action))
                    return true;
            }
        }
        return false;
    }

    section.recalcCellsIfNeeded();

    LayoutRect hitTestRect = locationInContainer.boundingBox();
    hitTestRect.moveBy(-adjustedLocation);

    LayoutRect tableAlignedRect = section.logicalRectForWritingModeAndDirection(hitTestRect);
    CellSpan rowSpan = section.spannedRows(tableAlignedRect, RenderTableSection::DoNotIncludeAllIntersectingCells);
    CellSpan columnSpan = section.spannedColumns(tableAlignedRect, RenderTableSection::DoNotIncludeAllIntersectingCells);

    // Now iterate over the spanned rows and columns.
    for (unsigned hitRow = rowSpan.start; hitRow < rowSpan.end; ++hitRow) {
        for (unsigned hitColumn = columnSpan.start; hitColumn < columnSpan.end; ++hitColumn) {
            RenderTableSection::CellStruct& current = section.cellAt(hitRow, hitColumn);

            // If the cell is empty, there's nothing to do
            if (!current.hasCells())
                continue;

            for (unsigned i = current.cells.size() ; i; ) {
                --i;
                CheckedPtr cell = current.cells[i];
                CheckedRef row = downcast<RenderTableRow>(*cell->parent());
                auto rowPoint = section.blockStartRelativeToPhysicalForChild(row, adjustedLocation) + row->location();
                auto cellPoint = row->blockStartRelativeToPhysicalForChild(*cell, rowPoint);
                if (static_cast<RenderObject*>(cell.get())->nodeAtPoint(request, result, locationInContainer, cellPoint, action)) {
                    section.updateHitTestResult(result, locationInContainer.point() - toLayoutSize(cellPoint));
                    return true;
                }
            }
            if (!request.resultIsElementList())
                break;
        }
        if (!request.resultIsElementList())
            break;
    }

    return false;
}

bool TableHitTester::hitTest(RenderTableRow& row, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    // Table rows cannot ever be hit tested. Effectively they do not exist.
    // Just forward to our children always.
    if (!row.section())
        return false;

    auto adjustedOffset = accumulatedOffset + row.location();
    for (CheckedPtr cell = row.lastCell(); cell; cell = cell->previousCell()) {
        // FIXME: We have to skip over inline flows, since they can show up inside table rows
        // at the moment (a demoted inline <form> for example). If we ever implement a
        // table-specific hit-test method (which we should do for performance reasons anyway),
        // then we can remove this check.
        if (!cell->hasSelfPaintingLayer()) {
            auto cellPoint = row.blockStartRelativeToPhysicalForChild(*cell, adjustedOffset);
            if (cell->nodeAtPoint(request, result, locationInContainer, cellPoint, action)) {
                row.updateHitTestResult(result, locationInContainer.point() - toLayoutSize(cellPoint));
                return true;
            }
        }
    }

    return false;
}

} // namespace Paint
} // namespace WebCore
