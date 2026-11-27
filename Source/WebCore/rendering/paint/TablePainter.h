/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "LayoutPoint.h"
#include "LayoutRect.h"

namespace WebCore {

enum class BorderStyle : uint8_t;
enum class BoxSide : uint8_t;
enum class CSSPropertyID : uint16_t;

class CollapsedBorderValue;
class RenderBox;
class RenderTable;
class RenderTableCell;
class RenderTableRow;
class RenderTableSection;
struct PaintInfo;

namespace Paint {

// Paints the structure of tables: the table, its sections and rows, the backgrounds behind cells and the
// collapsed borders. The cells' own contents paint with BlockPainter.
class TablePainter {
public:
    static void paint(RenderTable&, PaintInfo&, const LayoutPoint& paintOffset);
    static void paintObject(RenderTable&, PaintInfo&, const LayoutPoint& paintOffset);

    static void paint(RenderTableSection&, PaintInfo&, const LayoutPoint& paintOffset);
    static void paintObject(RenderTableSection&, PaintInfo&, const LayoutPoint& paintOffset);

    // Only called for rows with a self-painting layer. The section paints the other rows cell by cell.
    static void paint(RenderTableRow&, PaintInfo&, const LayoutPoint& paintOffset);

    static void paintBackgroundsBehindCell(RenderTableCell&, PaintInfo&, LayoutPoint paintOffset, RenderBox& backgroundObject, LayoutPoint backgroundPaintOffset);

private:
    // The collapsed borders are painted in one pass per border style, in precedence order.
    template<typename Function> static void paintCollapsedBorderPasses(RenderTable&, const PaintInfo&, NOESCAPE const Function& paintPass);
    static void paintCollapsedBordersForRow(RenderTable&, PaintInfo&, RenderTableRow&, const LayoutPoint& paintOffset);
    static void paintCollapsedBorders(RenderTableCell&, PaintInfo&, const LayoutPoint& paintOffset, const CollapsedBorderValue& currentBorder);

    // If currentBorder is set, paints only the collapsed borders of that style.
    static void paintSection(RenderTableSection&, PaintInfo&, const LayoutPoint& paintOffset, const CollapsedBorderValue* currentBorder);
    static void paintContents(RenderTableSection&, PaintInfo&, const LayoutPoint& paintOffset, const CollapsedBorderValue* currentBorder);
    static void paintCell(RenderTableSection&, RenderTableCell&, PaintInfo&, const LayoutPoint& paintOffset);
    static void paintRowGroupBorder(RenderTableSection&, const PaintInfo&, bool antialias, LayoutRect, BoxSide, CSSPropertyID borderColor, BorderStyle, BorderStyle tableBorderStyle);
    static void paintRowGroupBorderIfRequired(RenderTableSection&, const PaintInfo&, const LayoutPoint& paintOffset, unsigned row, unsigned column, BoxSide, const CollapsedBorderValue& currentBorder, RenderTableCell* = nullptr);

    static void paintRowOutline(RenderTableRow&, PaintInfo&, const LayoutPoint& paintOffset);
    static void paintRowShadow(RenderTableRow&, PaintInfo&, const LayoutPoint& paintOffset);
};

} // namespace Paint
} // namespace WebCore
