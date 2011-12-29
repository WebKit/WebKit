/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "RenderTable.h"

#include "AutoTableLayout.h"
#include "CollapsedBorderValue.h"
#include "DeleteButtonController.h"
#include "Document.h"
#include "FixedTableLayout.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "HTMLNames.h"
#include "LayoutRepainter.h"
#include "RenderLayer.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableSection.h"
#include "RenderView.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderTable::RenderTable(Node* node)
    : RenderBlock(node)
    , m_header(0)
    , m_footer(0)
    , m_firstBody(0)
    , m_currentBorder(0)
    , m_collapsedBordersValid(false)
    , m_hasColElements(false)
    , m_needsSectionRecalc(false)
    , m_hSpacing(0)
    , m_vSpacing(0)
    , m_borderStart(0)
    , m_borderEnd(0)
{
    setChildrenInline(false);
    m_columnPos.fill(0, 1);
    
}

RenderTable::~RenderTable()
{
}

void RenderTable::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren();

    ETableLayout oldTableLayout = oldStyle ? oldStyle->tableLayout() : TAUTO;

    // In the collapsed border model, there is no cell spacing.
    m_hSpacing = collapseBorders() ? 0 : style()->horizontalBorderSpacing();
    m_vSpacing = collapseBorders() ? 0 : style()->verticalBorderSpacing();
    m_columnPos[0] = m_hSpacing;

    if (!m_tableLayout || style()->tableLayout() != oldTableLayout) {
        // According to the CSS2 spec, you only use fixed table layout if an
        // explicit width is specified on the table.  Auto width implies auto table layout.
        if (style()->tableLayout() == TFIXED && !style()->logicalWidth().isAuto())
            m_tableLayout = adoptPtr(new FixedTableLayout(this));
        else
            m_tableLayout = adoptPtr(new AutoTableLayout(this));
    }

    // If border was changed, invalidate collapsed borders cache.
    if (!needsLayout() && oldStyle && oldStyle->border() != style()->border())
        invalidateCollapsedBorders();
}

static inline void resetSectionPointerIfNotBefore(RenderTableSection*& ptr, RenderObject* before)
{
    if (!before || !ptr)
        return;
    RenderObject* o = before->previousSibling();
    while (o && o != ptr)
        o = o->previousSibling();
    if (!o)
        ptr = 0;
}

void RenderTable::addChild(RenderObject* child, RenderObject* beforeChild)
{
    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChild)
        beforeChild = afterPseudoElementRenderer();

    bool wrapInAnonymousSection = !child->isPositioned();

    if (child->isRenderBlock() && child->style()->display() == TABLE_CAPTION) {
        m_captions.append(toRenderBlock(child));
        setNeedsSectionRecalc();
        wrapInAnonymousSection = false;
    } else if (child->isTableCol()) {
        m_hasColElements = true;
        wrapInAnonymousSection = false;
    } else if (child->isTableSection()) {
        switch (child->style()->display()) {
            case TABLE_HEADER_GROUP:
                resetSectionPointerIfNotBefore(m_header, beforeChild);
                if (!m_header) {
                    m_header = toRenderTableSection(child);
                } else {
                    resetSectionPointerIfNotBefore(m_firstBody, beforeChild);
                    if (!m_firstBody) 
                        m_firstBody = toRenderTableSection(child);
                }
                wrapInAnonymousSection = false;
                break;
            case TABLE_FOOTER_GROUP:
                resetSectionPointerIfNotBefore(m_footer, beforeChild);
                if (!m_footer) {
                    m_footer = toRenderTableSection(child);
                    wrapInAnonymousSection = false;
                    break;
                }
                // Fall through.
            case TABLE_ROW_GROUP:
                resetSectionPointerIfNotBefore(m_firstBody, beforeChild);
                if (!m_firstBody)
                    m_firstBody = toRenderTableSection(child);
                wrapInAnonymousSection = false;
                break;
            default:
                ASSERT_NOT_REACHED();
        }
    } else if (child->isTableCell() || child->isTableRow())
        wrapInAnonymousSection = true;
    else
        wrapInAnonymousSection = true;

    if (!wrapInAnonymousSection) {
        // If the next renderer is actually wrapped in an anonymous table section, we need to go up and find that.
        while (beforeChild && beforeChild->parent() != this)
            beforeChild = beforeChild->parent();

        RenderBox::addChild(child, beforeChild);
        return;
    }

    if (!beforeChild && lastChild() && lastChild()->isTableSection() && lastChild()->isAnonymous() && !lastChild()->isBeforeContent()) {
        lastChild()->addChild(child);
        return;
    }

    if (beforeChild && !beforeChild->isAnonymous() && beforeChild->parent() == this) {
        RenderObject* section = beforeChild->previousSibling();
        if (section && section->isTableSection() && section->isAnonymous()) {
            section->addChild(child);
            return;
        }
    }

    RenderObject* lastBox = beforeChild;
    while (lastBox && lastBox->parent()->isAnonymous() && !lastBox->isTableSection() && lastBox->style()->display() != TABLE_CAPTION && lastBox->style()->display() != TABLE_COLUMN_GROUP)
        lastBox = lastBox->parent();
    if (lastBox && lastBox->isAnonymous() && !isAfterContent(lastBox)) {
        if (beforeChild == lastBox)
            beforeChild = lastBox->firstChild();
        lastBox->addChild(child, beforeChild);
        return;
    }

    if (beforeChild && !beforeChild->isTableSection() && beforeChild->style()->display() != TABLE_CAPTION && beforeChild->style()->display() != TABLE_COLUMN_GROUP)
        beforeChild = 0;
    RenderTableSection* section = new (renderArena()) RenderTableSection(document() /* anonymous */);
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    newStyle->inheritFrom(style());
    newStyle->setDisplay(TABLE_ROW_GROUP);
    section->setStyle(newStyle.release());
    addChild(section, beforeChild);
    section->addChild(child);
}

void RenderTable::removeChild(RenderObject* oldChild)
{
    RenderBox::removeChild(oldChild);
 
    size_t index = m_captions.find(oldChild);
    if (index != notFound) {
        m_captions.remove(index);
        if (node())
            node()->setNeedsStyleRecalc();
    }
    setNeedsSectionRecalc();
}

void RenderTable::computeLogicalWidth()
{
    recalcSectionsIfNeeded();

    if (isPositioned())
        computePositionedLogicalWidth();

    RenderBlock* cb = containingBlock();

    LayoutUnit availableLogicalWidth = containingBlockLogicalWidthForContent();
    bool hasPerpendicularContainingBlock = cb->style()->isHorizontalWritingMode() != style()->isHorizontalWritingMode();
    LayoutUnit containerWidthInInlineDirection = hasPerpendicularContainingBlock ? perpendicularContainingBlockLogicalHeight() : availableLogicalWidth;

    LengthType logicalWidthType = style()->logicalWidth().type();
    if (logicalWidthType > Relative && style()->logicalWidth().isPositive()) {
        // Percent or fixed table
        // HTML tables size as though CSS width includes border/padding, CSS tables do not.
        LayoutUnit borders = 0;
        if (logicalWidthType != Percent && (!node() || !node()->hasTagName(tableTag))) {
            bool collapsing = collapseBorders();
            LayoutUnit borderAndPaddingBefore = borderBefore() + (collapsing ? 0 : paddingBefore());
            LayoutUnit borderAndPaddingAfter = borderAfter() + (collapsing ? 0 : paddingAfter());
            borders = borderAndPaddingBefore + borderAndPaddingAfter;
        }
        setLogicalWidth(style()->logicalWidth().calcMinValue(containerWidthInInlineDirection) + borders);
        setLogicalWidth(max(minPreferredLogicalWidth(), logicalWidth()));
    } else {
        // Subtract out any fixed margins from our available width for auto width tables.
        LayoutUnit marginTotal = 0;
        if (!style()->marginStart().isAuto())
            marginTotal += style()->marginStart().calcValue(availableLogicalWidth);
        if (!style()->marginEnd().isAuto())
            marginTotal += style()->marginEnd().calcValue(availableLogicalWidth);
            
        // Subtract out our margins to get the available content width.
        LayoutUnit availableContentLogicalWidth = max<LayoutUnit>(0, containerWidthInInlineDirection - marginTotal);
        
        // Ensure we aren't bigger than our max width or smaller than our min width.
        setLogicalWidth(min(availableContentLogicalWidth, maxPreferredLogicalWidth()));
    }

    setLogicalWidth(max(logicalWidth(), minPreferredLogicalWidth()));

    // Finally, with our true width determined, compute our margins for real.
    setMarginStart(0);
    setMarginEnd(0);
    if (!hasPerpendicularContainingBlock)
        computeInlineDirectionMargins(cb, availableLogicalWidth, logicalWidth());
    else {
        setMarginStart(style()->marginStart().calcMinValue(availableLogicalWidth));
        setMarginEnd(style()->marginEnd().calcMinValue(availableLogicalWidth));
    }
}

void RenderTable::adjustLogicalHeightForCaption(RenderBlock* caption)
{
    IntRect captionRect(caption->x(), caption->y(), caption->width(), caption->height());

    caption->setLogicalLocation(IntPoint(caption->marginStart(), caption->marginBefore() + logicalHeight()));
    if (!selfNeedsLayout() && caption->checkForRepaintDuringLayout())
        caption->repaintDuringLayoutIfMoved(captionRect);

    setLogicalHeight(logicalHeight() + caption->logicalHeight() + caption->marginBefore() + caption->marginAfter());
}

void RenderTable::layout()
{
    ASSERT(needsLayout());

    if (simplifiedLayout())
        return;

    recalcSectionsIfNeeded();
        
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    LayoutStateMaintainer statePusher(view(), this, locationOffset(), style()->isFlippedBlocksWritingMode());

    setLogicalHeight(0);
    m_overflow.clear();

    initMaxMarginValues();
    
    LayoutUnit oldLogicalWidth = logicalWidth();
    computeLogicalWidth();

    if (logicalWidth() != oldLogicalWidth) {
        for (unsigned i = 0; i < m_captions.size(); i++)
            m_captions[i]->setNeedsLayout(true, false);
    }
    // FIXME: The optimisation below doesn't work since the internal table
    // layout could have changed.  we need to add a flag to the table
    // layout that tells us if something has changed in the min max
    // calculations to do it correctly.
//     if ( oldWidth != width() || columns.size() + 1 != columnPos.size() )
    m_tableLayout->layout();

    setCellLogicalWidths();

    LayoutUnit totalSectionLogicalHeight = 0;
    LayoutUnit oldTableLogicalTop = 0;
    for (unsigned i = 0; i < m_captions.size(); i++)
        oldTableLogicalTop += m_captions[i]->logicalHeight() + m_captions[i]->marginBefore() + m_captions[i]->marginAfter();

    bool collapsing = collapseBorders();

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection()) {
            child->layoutIfNeeded();
            RenderTableSection* section = toRenderTableSection(child);
            totalSectionLogicalHeight += section->calcRowLogicalHeight();
            if (collapsing)
                section->recalcOuterBorder();
            ASSERT(!section->needsLayout());
        } else if (child->isTableCol()) {
            child->layoutIfNeeded();
            ASSERT(!child->needsLayout());
        }
    }

    // Only lay out one caption, since it's the only one we're going to end up painting.
    for (unsigned i = 0; i < m_captions.size(); i++)
        m_captions[i]->layoutIfNeeded();

    // If any table section moved vertically, we will just repaint everything from that
    // section down (it is quite unlikely that any of the following sections
    // did not shift).
    bool sectionMoved = false;
    LayoutUnit movedSectionLogicalTop = 0;

    // FIXME: Collapse caption margin.
    if (!m_captions.isEmpty()) {
        for (unsigned i = 0; i < m_captions.size(); i++) {
            if (m_captions[i]->style()->captionSide() == CAPBOTTOM)
                continue;
            adjustLogicalHeightForCaption(m_captions[i]);
        }
        if (logicalHeight() != oldTableLogicalTop) {
            sectionMoved = true;
            movedSectionLogicalTop = min(logicalHeight(), oldTableLogicalTop);
        }
    }

    LayoutUnit borderAndPaddingBefore = borderBefore() + (collapsing ? 0 : paddingBefore());
    LayoutUnit borderAndPaddingAfter = borderAfter() + (collapsing ? 0 : paddingAfter());

    setLogicalHeight(logicalHeight() + borderAndPaddingBefore);

    if (!isPositioned())
        computeLogicalHeight();

    Length logicalHeightLength = style()->logicalHeight();
    LayoutUnit computedLogicalHeight = 0;
    if (logicalHeightLength.isFixed()) {
        // HTML tables size as though CSS height includes border/padding, CSS tables do not.
        LayoutUnit borders = node() && node()->hasTagName(tableTag) ? (borderAndPaddingBefore + borderAndPaddingAfter) : 0;
        computedLogicalHeight = logicalHeightLength.value() - borders;
    } else if (logicalHeightLength.isPercent())
        computedLogicalHeight = computePercentageLogicalHeight(logicalHeightLength);
    computedLogicalHeight = max<LayoutUnit>(0, computedLogicalHeight);

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection())
            // FIXME: Distribute extra height between all table body sections instead of giving it all to the first one.
            toRenderTableSection(child)->layoutRows(child == firstBody() ? max<LayoutUnit>(0, computedLogicalHeight - totalSectionLogicalHeight) : 0);
    }

    if (!firstBody() && computedLogicalHeight > totalSectionLogicalHeight && !document()->inQuirksMode()) {
        // Completely empty tables (with no sections or anything) should at least honor specified height
        // in strict mode.
        setLogicalHeight(logicalHeight() + computedLogicalHeight);
    }

    LayoutUnit sectionLogicalLeft = style()->isLeftToRightDirection() ? borderStart() : borderEnd();
    if (!collapsing)
        sectionLogicalLeft += style()->isLeftToRightDirection() ? paddingStart() : paddingEnd();

    // position the table sections
    RenderTableSection* section = topSection();
    while (section) {
        if (!sectionMoved && section->logicalTop() != logicalHeight()) {
            sectionMoved = true;
            movedSectionLogicalTop = min(logicalHeight(), section->logicalTop()) + (style()->isHorizontalWritingMode() ? section->minYVisualOverflow() : section->minXVisualOverflow());
        }
        section->setLogicalLocation(LayoutPoint(sectionLogicalLeft, logicalHeight()));

        setLogicalHeight(logicalHeight() + section->logicalHeight());
        section = sectionBelow(section);
    }

    setLogicalHeight(logicalHeight() + borderAndPaddingAfter);

    for (unsigned i = 0; i < m_captions.size(); i++) {
        if (m_captions[i]->style()->captionSide() != CAPBOTTOM)
            continue;
        adjustLogicalHeightForCaption(m_captions[i]);
    }

    if (isPositioned())
        computeLogicalHeight();

    // table can be containing block of positioned elements.
    // FIXME: Only pass true if width or height changed.
    layoutPositionedObjects(true);

    updateLayerTransform();

    // Layout was changed, so probably borders too.
    invalidateCollapsedBorders();

    computeOverflow(clientLogicalBottom());

    statePusher.pop();

    if (view()->layoutState()->pageLogicalHeight())
        setPageLogicalOffset(view()->layoutState()->pageLogicalOffset(logicalTop()));

    bool didFullRepaint = repainter.repaintAfterLayout();
    // Repaint with our new bounds if they are different from our old bounds.
    if (!didFullRepaint && sectionMoved) {
        if (style()->isHorizontalWritingMode())
            repaintRectangle(LayoutRect(minXVisualOverflow(), movedSectionLogicalTop, maxXVisualOverflow() - minXVisualOverflow(), maxYVisualOverflow() - movedSectionLogicalTop));
        else
            repaintRectangle(LayoutRect(movedSectionLogicalTop, minYVisualOverflow(), maxXVisualOverflow() - movedSectionLogicalTop, maxYVisualOverflow() - minYVisualOverflow()));
    }

    setNeedsLayout(false);
}

// Collect all the unique border values that we want to paint in a sorted list.
void RenderTable::recalcCollapsedBorders()
{
    if (m_collapsedBordersValid)
        return;
    m_collapsedBordersValid = true;
    m_collapsedBorders.clear();
    RenderObject* stop = nextInPreOrderAfterChildren();
    for (RenderObject* o = firstChild(); o && o != stop; o = o->nextInPreOrder()) {
        if (o->isTableCell())
            toRenderTableCell(o)->collectBorderValues(m_collapsedBorders);
    }
    RenderTableCell::sortBorderValues(m_collapsedBorders);
}


void RenderTable::addOverflowFromChildren()
{
    // Add overflow from borders.
    // Technically it's odd that we are incorporating the borders into layout overflow, which is only supposed to be about overflow from our
    // descendant objects, but since tables don't support overflow:auto, this works out fine.
    if (collapseBorders()) {
        int rightBorderOverflow = width() + outerBorderRight() - borderRight();
        int leftBorderOverflow = borderLeft() - outerBorderLeft();
        int bottomBorderOverflow = height() + outerBorderBottom() - borderBottom();
        int topBorderOverflow = borderTop() - outerBorderTop();
        IntRect borderOverflowRect(leftBorderOverflow, topBorderOverflow, rightBorderOverflow - leftBorderOverflow, bottomBorderOverflow - topBorderOverflow);
        if (borderOverflowRect != borderBoxRect()) {
            addLayoutOverflow(borderOverflowRect);
            addVisualOverflow(borderOverflowRect);
        }
    }

    // Add overflow from our caption.
    for (unsigned i = 0; i < m_captions.size(); i++) 
        addOverflowFromChild(m_captions[i]);

    // Add overflow from our sections.
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection()) {
            RenderTableSection* section = toRenderTableSection(child);
            addOverflowFromChild(section);
        }
    }
}

void RenderTable::setCellLogicalWidths()
{
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection())
            toRenderTableSection(child)->setCellLogicalWidths();
    }
}

void RenderTable::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + location();

    PaintPhase paintPhase = paintInfo.phase;

    if (!isRoot()) {
        LayoutRect overflowBox = visualOverflowRect();
        flipForWritingMode(overflowBox);
        overflowBox.inflate(maximalOutlineSize(paintInfo.phase));
        overflowBox.moveBy(adjustedPaintOffset);
        if (!overflowBox.intersects(paintInfo.rect))
            return;
    }

    bool pushedClip = pushContentsClip(paintInfo, adjustedPaintOffset);
    paintObject(paintInfo, adjustedPaintOffset);
    if (pushedClip)
        popContentsClip(paintInfo, paintPhase, adjustedPaintOffset);
}

void RenderTable::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;
    if ((paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) && hasBoxDecorations() && style()->visibility() == VISIBLE)
        paintBoxDecorations(paintInfo, paintOffset);

    if (paintPhase == PaintPhaseMask) {
        paintMask(paintInfo, paintOffset);
        return;
    }

    // We're done.  We don't bother painting any children.
    if (paintPhase == PaintPhaseBlockBackground)
        return;
    
    // We don't paint our own background, but we do let the kids paint their backgrounds.
    if (paintPhase == PaintPhaseChildBlockBackgrounds)
        paintPhase = PaintPhaseChildBlockBackground;

    PaintInfo info(paintInfo);
    info.phase = paintPhase;
    info.updatePaintingRootForChildren(this);

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isBox() && !toRenderBox(child)->hasSelfPaintingLayer() && (child->isTableSection() || (child->isRenderBlock() && child->style()->display() == TABLE_CAPTION))) {
            LayoutPoint childPoint = flipForWritingModeForChild(toRenderBox(child), paintOffset);
            child->paint(info, childPoint);
        }
    }
    
    if (collapseBorders() && paintPhase == PaintPhaseChildBlockBackground && style()->visibility() == VISIBLE) {
        recalcCollapsedBorders();
        // Using our cached sorted styles, we then do individual passes,
        // painting each style of border from lowest precedence to highest precedence.
        info.phase = PaintPhaseCollapsedTableBorders;
        size_t count = m_collapsedBorders.size();
        for (size_t i = 0; i < count; ++i) {
            m_currentBorder = &m_collapsedBorders[i];
            for (RenderObject* child = firstChild(); child; child = child->nextSibling())
                if (child->isTableSection()) {
                    LayoutPoint childPoint = flipForWritingModeForChild(toRenderTableSection(child), paintOffset);
                    child->paint(info, childPoint);
                }
        }
        m_currentBorder = 0;
    }

    // Paint outline.
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseSelfOutline) && hasOutline() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, LayoutRect(paintOffset, size()));
}

void RenderTable::subtractCaptionRect(LayoutRect& rect) const
{
    for (unsigned i = 0; i < m_captions.size(); i++) {
        LayoutUnit captionLogicalHeight = m_captions[i]->logicalHeight() + m_captions[i]->marginBefore() + m_captions[i]->marginAfter();
        bool captionIsBefore = (m_captions[i]->style()->captionSide() != CAPBOTTOM) ^ style()->isFlippedBlocksWritingMode();
        if (style()->isHorizontalWritingMode()) {
            rect.setHeight(rect.height() - captionLogicalHeight);
            if (captionIsBefore)
                rect.move(0, captionLogicalHeight);
        } else {
            rect.setWidth(rect.width() - captionLogicalHeight);
            if (captionIsBefore)
                rect.move(captionLogicalHeight, 0);
        }
    }
}

void RenderTable::paintBoxDecorations(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(this))
        return;

    LayoutRect rect(paintOffset, size());
    subtractCaptionRect(rect);

    paintBoxShadow(paintInfo, rect, style(), Normal);
    paintBackground(paintInfo, rect);
    paintBoxShadow(paintInfo, rect, style(), Inset);

    if (style()->hasBorder() && !collapseBorders())
        paintBorder(paintInfo, rect, style());
}

void RenderTable::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    LayoutRect rect(paintOffset, size());
    subtractCaptionRect(rect);

    paintMaskImages(paintInfo, rect);
}

void RenderTable::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    recalcSectionsIfNeeded();
    recalcBordersInRowDirection();

    m_tableLayout->computePreferredLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    for (unsigned i = 0; i < m_captions.size(); i++)
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, m_captions[i]->minPreferredLogicalWidth());

    setPreferredLogicalWidthsDirty(false);
}

RenderTableSection* RenderTable::topNonEmptySection() const
{
    RenderTableSection* section = topSection();
    if (section && !section->numRows())
        section = sectionBelow(section, SkipEmptySections);
    return section;
}

void RenderTable::splitColumn(unsigned position, unsigned firstSpan)
{
    // we need to add a new columnStruct
    unsigned oldSize = m_columns.size();
    m_columns.grow(oldSize + 1);
    unsigned oldSpan = m_columns[position].span;
    ASSERT(oldSpan > firstSpan);
    m_columns[position].span = firstSpan;
    memmove(m_columns.data() + position + 1, m_columns.data() + position, (oldSize - position) * sizeof(ColumnStruct));
    m_columns[position + 1].span = oldSpan - firstSpan;

    // Propagate the change in our columns representation to the sections that don't need
    // cell recalc. If they do, they will be synced up directly with m_columns later.
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isTableSection())
            continue;

        RenderTableSection* section = toRenderTableSection(child);
        if (section->needsCellRecalc())
            continue;

        section->splitColumn(position, firstSpan);
    }

    m_columnPos.grow(numEffCols() + 1);
    setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderTable::appendColumn(unsigned span)
{
    unsigned pos = m_columns.size();
    unsigned newSize = pos + 1;
    m_columns.grow(newSize);
    m_columns[pos].span = span;

    // Propagate the change in our columns representation to the sections that don't need
    // cell recalc. If they do, they will be synced up directly with m_columns later.
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isTableSection())
            continue;

        RenderTableSection* section = toRenderTableSection(child);
        if (section->needsCellRecalc())
            continue;

        section->appendColumn(pos);
    }

    m_columnPos.grow(numEffCols() + 1);
    setNeedsLayoutAndPrefWidthsRecalc();
}

RenderTableCol* RenderTable::nextColElement(RenderTableCol* current) const
{
    RenderObject* next = current->firstChild();
    if (!next)
        next = current->nextSibling();
    if (!next && current->parent()->isTableCol())
        next = current->parent()->nextSibling();

    while (next) {
        if (next->isTableCol())
            return toRenderTableCol(next);
        if (!m_captions.contains(next))
            return 0;
        next = next->nextSibling();
    }
    
    return 0;
}

RenderTableCol* RenderTable::colElement(unsigned col, bool* startEdge, bool* endEdge) const
{
    if (!m_hasColElements)
        return 0;
    RenderObject* child = firstChild();
    unsigned cCol = 0;

    while (child) {
        if (child->isTableCol())
            break;
        if (!m_captions.contains(child))
            return 0;
        child = child->nextSibling();
    }
    if (!child)
        return 0;

    RenderTableCol* colElem = toRenderTableCol(child);
    while (colElem) {
        unsigned span = colElem->span();
        if (!colElem->firstChild()) {
            unsigned startCol = cCol;
            ASSERT(span >= 1);
            unsigned endCol = cCol + span - 1;
            cCol += span;
            if (cCol > col) {
                if (startEdge)
                    *startEdge = startCol == col;
                if (endEdge)
                    *endEdge = endCol == col;
                return colElem;
            }
        }
        colElem = nextColElement(colElem);
    }

    return 0;
}

void RenderTable::recalcSections() const
{
    ASSERT(needsSectionRecalc());

    m_header = 0;
    m_footer = 0;
    m_firstBody = 0;
    m_hasColElements = false;

    // We need to get valid pointers to caption, head, foot and first body again
    RenderObject* nextSibling;
    for (RenderObject* child = firstChild(); child; child = nextSibling) {
        nextSibling = child->nextSibling();
        switch (child->style()->display()) {
            case TABLE_COLUMN:
            case TABLE_COLUMN_GROUP:
                m_hasColElements = true;
                break;
            case TABLE_HEADER_GROUP:
                if (child->isTableSection()) {
                    RenderTableSection* section = toRenderTableSection(child);
                    if (!m_header)
                        m_header = section;
                    else if (!m_firstBody)
                        m_firstBody = section;
                    section->recalcCellsIfNeeded();
                }
                break;
            case TABLE_FOOTER_GROUP:
                if (child->isTableSection()) {
                    RenderTableSection* section = toRenderTableSection(child);
                    if (!m_footer)
                        m_footer = section;
                    else if (!m_firstBody)
                        m_firstBody = section;
                    section->recalcCellsIfNeeded();
                }
                break;
            case TABLE_ROW_GROUP:
                if (child->isTableSection()) {
                    RenderTableSection* section = toRenderTableSection(child);
                    if (!m_firstBody)
                        m_firstBody = section;
                    section->recalcCellsIfNeeded();
                }
                break;
            default:
                break;
        }
    }

    // repair column count (addChild can grow it too much, because it always adds elements to the last row of a section)
    unsigned maxCols = 0;
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection()) {
            RenderTableSection* section = toRenderTableSection(child);
            unsigned sectionCols = section->numColumns();
            if (sectionCols > maxCols)
                maxCols = sectionCols;
        }
    }
    
    m_columns.resize(maxCols);
    m_columnPos.resize(maxCols + 1);

    ASSERT(selfNeedsLayout());

    m_needsSectionRecalc = false;
}

LayoutUnit RenderTable::calcBorderStart() const
{
    if (collapseBorders()) {
        // Determined by the first cell of the first row. See the CSS 2.1 spec, section 17.6.2.
        if (!numEffCols())
            return 0;

        LayoutUnit borderWidth = 0;

        const BorderValue& tb = style()->borderStart();
        if (tb.style() == BHIDDEN)
            return 0;
        if (tb.style() > BHIDDEN)
            borderWidth = tb.width();

        if (RenderTableCol* colGroup = colElement(0)) {
            const BorderValue& gb = colGroup->style()->borderStart();
            if (gb.style() == BHIDDEN)
                return 0;
            if (gb.style() > BHIDDEN)
                borderWidth = max<LayoutUnit>(borderWidth, gb.width());
        }
        
        if (const RenderTableSection* topNonEmptySection = this->topNonEmptySection()) {
            const BorderValue& sb = topNonEmptySection->style()->borderStart();
            if (sb.style() == BHIDDEN)
                return 0;

            if (sb.style() > BHIDDEN)
                borderWidth = max<LayoutUnit>(borderWidth, sb.width());

            const RenderTableSection::CellStruct& cs = topNonEmptySection->cellAt(0, 0);
            
            if (cs.hasCells()) {
                const BorderValue& cb = cs.primaryCell()->style()->borderStart(); // FIXME: Make this work with perpendicualr and flipped cells.
                if (cb.style() == BHIDDEN)
                    return 0;

                const BorderValue& rb = cs.primaryCell()->parent()->style()->borderStart();
                if (rb.style() == BHIDDEN)
                    return 0;

                if (cb.style() > BHIDDEN)
                    borderWidth = max<LayoutUnit>(borderWidth, cb.width());
                if (rb.style() > BHIDDEN)
                    borderWidth = max<LayoutUnit>(borderWidth, rb.width());
            }
        }
        return (borderWidth + (style()->isLeftToRightDirection() ? 0 : 1)) / 2;
    }
    return RenderBlock::borderStart();
}

LayoutUnit RenderTable::calcBorderEnd() const
{
    if (collapseBorders()) {
        // Determined by the last cell of the first row. See the CSS 2.1 spec, section 17.6.2.
        if (!numEffCols())
            return 0;

        LayoutUnit borderWidth = 0;

        const BorderValue& tb = style()->borderEnd();
        if (tb.style() == BHIDDEN)
            return 0;
        if (tb.style() > BHIDDEN)
            borderWidth = tb.width();

        unsigned endColumn = numEffCols() - 1;
        if (RenderTableCol* colGroup = colElement(endColumn)) {
            const BorderValue& gb = colGroup->style()->borderEnd();
            if (gb.style() == BHIDDEN)
                return 0;
            if (gb.style() > BHIDDEN)
                borderWidth = max<LayoutUnit>(borderWidth, gb.width());
        }

        if (const RenderTableSection* topNonEmptySection = this->topNonEmptySection()) {
            const BorderValue& sb = topNonEmptySection->style()->borderEnd();
            if (sb.style() == BHIDDEN)
                return 0;

            if (sb.style() > BHIDDEN)
                borderWidth = max<LayoutUnit>(borderWidth, sb.width());

            const RenderTableSection::CellStruct& cs = topNonEmptySection->cellAt(0, endColumn);
            
            if (cs.hasCells()) {
                const BorderValue& cb = cs.primaryCell()->style()->borderEnd(); // FIXME: Make this work with perpendicular and flipped cells.
                if (cb.style() == BHIDDEN)
                    return 0;

                const BorderValue& rb = cs.primaryCell()->parent()->style()->borderEnd();
                if (rb.style() == BHIDDEN)
                    return 0;

                if (cb.style() > BHIDDEN)
                    borderWidth = max<LayoutUnit>(borderWidth, cb.width());
                if (rb.style() > BHIDDEN)
                    borderWidth = max<LayoutUnit>(borderWidth, rb.width());
            }
        }
        return (borderWidth + (style()->isLeftToRightDirection() ? 1 : 0)) / 2;
    }
    return RenderBlock::borderEnd();
}

void RenderTable::recalcBordersInRowDirection()
{
    m_borderStart = calcBorderStart();
    m_borderEnd = calcBorderEnd();
}

LayoutUnit RenderTable::borderBefore() const
{
    if (collapseBorders())
        return outerBorderBefore();
    return RenderBlock::borderBefore();
}

LayoutUnit RenderTable::borderAfter() const
{
    if (collapseBorders())
        return outerBorderAfter();
    return RenderBlock::borderAfter();
}

LayoutUnit RenderTable::outerBorderBefore() const
{
    if (!collapseBorders())
        return 0;
    LayoutUnit borderWidth = 0;
    if (RenderTableSection* topSection = this->topSection()) {
        borderWidth = topSection->outerBorderBefore();
        if (borderWidth < 0)
            return 0;   // Overridden by hidden
    }
    const BorderValue& tb = style()->borderBefore();
    if (tb.style() == BHIDDEN)
        return 0;
    if (tb.style() > BHIDDEN)
        borderWidth = max<LayoutUnit>(borderWidth, tb.width() / 2);
    return borderWidth;
}

LayoutUnit RenderTable::outerBorderAfter() const
{
    if (!collapseBorders())
        return 0;
    LayoutUnit borderWidth = 0;
    RenderTableSection* bottomSection;
    if (footer())
        bottomSection = footer();
    else {
        RenderObject* child;
        for (child = lastChild(); child && !child->isTableSection(); child = child->previousSibling()) { }
        bottomSection = child ? toRenderTableSection(child) : 0;
    }
    if (bottomSection) {
        borderWidth = bottomSection->outerBorderAfter();
        if (borderWidth < 0)
            return 0;   // Overridden by hidden
    }
    const BorderValue& tb = style()->borderAfter();
    if (tb.style() == BHIDDEN)
        return 0;
    if (tb.style() > BHIDDEN)
        borderWidth = max<LayoutUnit>(borderWidth, (tb.width() + 1) / 2);
    return borderWidth;
}

LayoutUnit RenderTable::outerBorderStart() const
{
    if (!collapseBorders())
        return 0;

    LayoutUnit borderWidth = 0;

    const BorderValue& tb = style()->borderStart();
    if (tb.style() == BHIDDEN)
        return 0;
    if (tb.style() > BHIDDEN)
        borderWidth = (tb.width() + (style()->isLeftToRightDirection() ? 0 : 1)) / 2;

    bool allHidden = true;
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isTableSection())
            continue;
        LayoutUnit sw = toRenderTableSection(child)->outerBorderStart();
        if (sw < 0)
            continue;
        allHidden = false;
        borderWidth = max(borderWidth, sw);
    }
    if (allHidden)
        return 0;

    return borderWidth;
}

LayoutUnit RenderTable::outerBorderEnd() const
{
    if (!collapseBorders())
        return 0;

    LayoutUnit borderWidth = 0;

    const BorderValue& tb = style()->borderEnd();
    if (tb.style() == BHIDDEN)
        return 0;
    if (tb.style() > BHIDDEN)
        borderWidth = (tb.width() + (style()->isLeftToRightDirection() ? 1 : 0)) / 2;

    bool allHidden = true;
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isTableSection())
            continue;
        LayoutUnit sw = toRenderTableSection(child)->outerBorderEnd();
        if (sw < 0)
            continue;
        allHidden = false;
        borderWidth = max(borderWidth, sw);
    }
    if (allHidden)
        return 0;

    return borderWidth;
}

RenderTableSection* RenderTable::sectionAbove(const RenderTableSection* section, SkipEmptySectionsValue skipEmptySections) const
{
    recalcSectionsIfNeeded();

    if (section == header())
        return 0;

    RenderObject* prevSection = section == footer() ? lastChild() : section->previousSibling();
    while (prevSection) {
        if (prevSection->isTableSection() && prevSection != header() && prevSection != footer() && (skipEmptySections == DoNotSkipEmptySections || toRenderTableSection(prevSection)->numRows()))
            break;
        prevSection = prevSection->previousSibling();
    }
    if (!prevSection && header() && (skipEmptySections == DoNotSkipEmptySections || header()->numRows()))
        prevSection = header();
    return toRenderTableSection(prevSection);
}

RenderTableSection* RenderTable::sectionBelow(const RenderTableSection* section, SkipEmptySectionsValue skipEmptySections) const
{
    recalcSectionsIfNeeded();

    if (section == footer())
        return 0;

    RenderObject* nextSection = section == header() ? firstChild() : section->nextSibling();
    while (nextSection) {
        if (nextSection->isTableSection() && nextSection != header() && nextSection != footer() && (skipEmptySections  == DoNotSkipEmptySections || toRenderTableSection(nextSection)->numRows()))
            break;
        nextSection = nextSection->nextSibling();
    }
    if (!nextSection && footer() && (skipEmptySections == DoNotSkipEmptySections || footer()->numRows()))
        nextSection = footer();
    return toRenderTableSection(nextSection);
}

RenderTableCell* RenderTable::cellAbove(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    // Find the section and row to look in
    unsigned r = cell->row();
    RenderTableSection* section = 0;
    unsigned rAbove = 0;
    if (r > 0) {
        // cell is not in the first row, so use the above row in its own section
        section = cell->section();
        rAbove = r - 1;
    } else {
        section = sectionAbove(cell->section(), SkipEmptySections);
        if (section) {
            ASSERT(section->numRows());
            rAbove = section->numRows() - 1;
        }
    }

    // Look up the cell in the section's grid, which requires effective col index
    if (section) {
        unsigned effCol = colToEffCol(cell->col());
        RenderTableSection::CellStruct& aboveCell = section->cellAt(rAbove, effCol);
        return aboveCell.primaryCell();
    } else
        return 0;
}

RenderTableCell* RenderTable::cellBelow(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    // Find the section and row to look in
    unsigned r = cell->row() + cell->rowSpan() - 1;
    RenderTableSection* section = 0;
    unsigned rBelow = 0;
    if (r < cell->section()->numRows() - 1) {
        // The cell is not in the last row, so use the next row in the section.
        section = cell->section();
        rBelow = r + 1;
    } else {
        section = sectionBelow(cell->section(), SkipEmptySections);
        if (section)
            rBelow = 0;
    }

    // Look up the cell in the section's grid, which requires effective col index
    if (section) {
        unsigned effCol = colToEffCol(cell->col());
        RenderTableSection::CellStruct& belowCell = section->cellAt(rBelow, effCol);
        return belowCell.primaryCell();
    } else
        return 0;
}

RenderTableCell* RenderTable::cellBefore(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    RenderTableSection* section = cell->section();
    unsigned effCol = colToEffCol(cell->col());
    if (!effCol)
        return 0;
    
    // If we hit a colspan back up to a real cell.
    RenderTableSection::CellStruct& prevCell = section->cellAt(cell->row(), effCol - 1);
    return prevCell.primaryCell();
}

RenderTableCell* RenderTable::cellAfter(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    unsigned effCol = colToEffCol(cell->col() + cell->colSpan());
    if (effCol >= numEffCols())
        return 0;
    return cell->section()->primaryCellAt(cell->row(), effCol);
}

RenderBlock* RenderTable::firstLineBlock() const
{
    return 0;
}

void RenderTable::updateFirstLetter()
{
}

LayoutUnit RenderTable::firstLineBoxBaseline() const
{
    if (isWritingModeRoot())
        return -1;

    recalcSectionsIfNeeded();

    const RenderTableSection* topNonEmptySection = this->topNonEmptySection();
    if (!topNonEmptySection)
        return -1;

    return topNonEmptySection->logicalTop() + topNonEmptySection->firstLineBoxBaseline();
}

LayoutRect RenderTable::overflowClipRect(const LayoutPoint& location, RenderRegion* region, OverlayScrollbarSizeRelevancy relevancy)
{
    LayoutRect rect = RenderBlock::overflowClipRect(location, region, relevancy);
    
    // If we have a caption, expand the clip to include the caption.
    // FIXME: Technically this is wrong, but it's virtually impossible to fix this
    // for real until captions have been re-written.
    // FIXME: This code assumes (like all our other caption code) that only top/bottom are
    // supported.  When we actually support left/right and stop mapping them to top/bottom,
    // we might have to hack this code first (depending on what order we do these bug fixes in).
    if (!m_captions.isEmpty()) {
        if (style()->isHorizontalWritingMode()) {
            rect.setHeight(height());
            rect.setY(location.y());
        } else {
            rect.setWidth(width());
            rect.setX(location.x());
        }
    }

    return rect;
}

bool RenderTable::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    LayoutPoint adjustedLocation = accumulatedOffset + location();

    // Check kids first.
    if (!hasOverflowClip() || overflowClipRect(adjustedLocation, result.region()).intersects(result.rectForPoint(pointInContainer))) {
        for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
            if (child->isBox() && !toRenderBox(child)->hasSelfPaintingLayer() && (child->isTableSection() || (child->isRenderBlock() && child->style()->display() == TABLE_CAPTION))) {
                LayoutPoint childPoint = flipForWritingModeForChild(toRenderBox(child), adjustedLocation);
                if (child->nodeAtPoint(request, result, pointInContainer, childPoint, action)) {
                    updateHitTestResult(result, toLayoutPoint(pointInContainer - childPoint));
                    return true;
                }
            }
        }
    }

    // Check our bounds next.
    LayoutRect boundsRect(adjustedLocation, size());
    if (visibleToHitTesting() && (action == HitTestBlockBackground || action == HitTestChildBlockBackground) && boundsRect.intersects(result.rectForPoint(pointInContainer))) {
        updateHitTestResult(result, flipForWritingMode(pointInContainer - toLayoutSize(adjustedLocation)));
        if (!result.addNodeToRectBasedTestResult(node(), pointInContainer, boundsRect))
            return true;
    }

    return false;
}

}
