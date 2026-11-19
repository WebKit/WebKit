/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2010 Daniel Bates (dbates@intudata.com)
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
 *
 */

#include "config.h"
#include "RenderListOutsideMarker.h"

#include "BaselineAlignment.h"
#include "CSSFontSelector.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "FontCascade.h"
#include "FontCascadeInlines.h"
#include "FontCascadeDescription.h"
#include "GraphicsContext.h"
#include "InlineIteratorBoxInlines.h"
#include "PaintInfoInlines.h"
#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderMenuList.h"
#include "RenderObjectInlines.h"
#include "RenderTable.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleListStyleType.h"
#include "StyleScope.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderListOutsideMarker);

RenderListOutsideMarker::RenderListOutsideMarker(RenderListItem& listItem, UniqueRef<Style::ComputedStyle>&& style)
    : RenderBox(Type::ListOutsideMarker, protect(listItem.document()), WTF::move(style))
    , m_listItem(listItem)
{
    setInline(true);
    setBlockLevelReplacedOrAtomicInline(true); // pretend to be replaced
    ASSERT(isRenderListOutsideMarker());
}

RenderListOutsideMarker::~RenderListOutsideMarker() = default;

static Style::Difference adjustedStyleDifference(Style::Difference diff, const Style::ComputedStyle& oldStyle, const Style::ComputedStyle& newStyle)
{
    if (diff >= Style::DifferenceResult::Layout)
        return diff;
    // FIXME: Preferably we do this at Style::ComputedStyle::changeRequiresLayout but checking against pseudo(::marker) is not sufficient.
    auto needsLayout = oldStyle.listStylePosition() != newStyle.listStylePosition()
        || oldStyle.listStyleType() != newStyle.listStyleType()
        || oldStyle.display().isInlineType() != newStyle.display().isInlineType();
    return needsLayout ? Style::DifferenceResult::Layout : diff;
}

void RenderListOutsideMarker::styleWillChange(Style::Difference diff, const Style::ComputedStyle& newStyle)
{
    RenderBox::styleWillChange(adjustedStyleDifference(diff, style(), newStyle), newStyle);
}

void RenderListOutsideMarker::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    if (oldStyle)
        diff = adjustedStyleDifference(diff, *oldStyle, style());
    RenderBox::styleDidChange(diff, oldStyle);

    propagateStyleToAnonymousChildren(StylePropagationType::AllChildren);
}

bool RenderListOutsideMarker::isImage() const
{
    // `content` supersedes list-style-image (css-lists-3 §3.3), so a marker with generated content
    // is never treated as an image marker (affects inline margins, baseline, and layout attributes).
    return !listMarkerHasContent(style(), document()) && listMarkerImage(style());
}

RenderBlockFlow* RenderListOutsideMarker::contentContainer() const
{
    return dynamicDowncast<RenderBlockFlow>(firstChild());
}

void RenderListOutsideMarker::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (usedStyle().visibility() != UsedVisibility::Visible)
        return;

    LayoutPoint boxOrigin(paintOffset + location());
    LayoutRect overflowRect(visualOverflowRect());
    overflowRect.moveBy(boxOrigin);
    if (!paintInfo.rect.intersects(overflowRect))
        return;

    CheckedPtr container = contentContainer();
    if (!container)
        return;

    if (paintInfo.phase == PaintPhase::Accessibility) {
        paintInfo.accessibilityRegionContext()->takeBounds(*this, LayoutRect(boxOrigin, borderBoxSize()));
        return;
    }

    // Paint the generated-content subtree as an atomic inline: paintAsInlineBlock fans out all
    // sub-phases for Foreground and forwards Selection/EventRegion/TextClip to the child's paint().
    container->paintAsInlineBlock(paintInfo, blockStartRelativeToPhysicalForChild(*container, boxOrigin));
}

void RenderListOutsideMarker::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    if (CheckedPtr container = contentContainer())
        layoutContentContainer(*container);

    auto [marginStart, marginEnd] = inlineMargins();
    setMarginStart(marginStart, m_listItem->writingMode());
    setMarginEnd(marginEnd, m_listItem->writingMode());

    clearNeedsLayout();
}

void RenderListOutsideMarker::layoutContentContainer(RenderBlockFlow& container)
{
    // The marker participates in its list item's line as a single atomic inline. Lay its
    // generated-content subtree out at its max-content (shrink-to-fit) width, like an
    // inline-block, then adopt the resulting size and baseline.
    auto contentLogicalWidth = container.maxContentLogicalWidthContribution();
    container.setOverridingBorderBoxLogicalWidth(contentLogicalWidth);
    container.setNeedsLayout(MarkingBehavior::OnlyThis);
    container.layoutIfNeeded();
    container.clearOverridingBorderBoxLogicalWidth();

    setLogicalWidth(container.logicalWidth());

    if (CheckedPtr imageRenderer = isImage() ? dynamicDowncast<RenderBox>(container.firstChild()) : nullptr) {
        container.setLogicalTop(-imageRenderer->logicalTop());
        setLogicalHeight(imageRenderer->logicalHeight());
    } else if (listMarkerHasContent(style(), document())) {
        // A marker with `content` is the box of its content, which the line aligns by its first baseline.
        container.setLogicalTop(0_lu);
        setLogicalHeight(container.logicalHeight());
    } else {
        // The inline formatting context aligns a text marker box on the marker's primary-font baseline
        // (see InlineLineBoxBuilder), then we paint the content box at the marker box origin. Offset the
        // content box along the block axis so its own first-line baseline lands on that font baseline —
        // otherwise the content's line-box half-leading shifts it off the list item's baseline. Logical
        // setters keep this correct in vertical writing modes.
        setLogicalHeight(style().metricsOfPrimaryFont().intHeight());

        auto contentBaseline = container.firstLineBaseline().value_or(container.logicalHeight());
        auto baselineOffset = writingMode().prefersCentralBaseline() ? logicalHeight() / 2 : LayoutUnit { style().metricsOfPrimaryFont().ascent(BaselineAlignment::dominantBaseline(writingMode())) };
        container.setLogicalTop(baselineOffset - contentBaseline);
    }

    // The content box can extend outside the marker's border box (its baseline offset may be
    // negative, or the content taller than the marker font), so record its overflow. The marker
    // overrides layout() and otherwise reports no overflow, which would clip/mis-repaint the content.
    clearOverflow();
    addLayoutOverflow({ container.location(), container.borderBoxSize() });
    auto contentVisualOverflow = container.visualOverflowRect();
    contentVisualOverflow.moveBy(container.location());
    addVisualOverflow(contentVisualOverflow);
}

std::pair<LayoutUnit, LayoutUnit> RenderListOutsideMarker::computeIntrinsicLogicalWidthContributions()
{
    ASSERT(hasInvalidContentLogicalWidths());

    CheckedPtr container = contentContainer();
    ASSERT(container);

    // The marker is non-wrapping, so its min- and max-content widths are both the content's max-content width.
    // (A ::marker has no border or padding.)
    auto logicalWidth = container ? container->maxContentLogicalWidthContribution() : 0_lu;
    return { logicalWidth, logicalWidth };
}

std::pair<LayoutUnit, LayoutUnit> RenderListOutsideMarker::inlineMargins() const
{
    // In the list item's inline direction: the marker hangs off the start of the line, so the start margin pulls it
    // out by its width and the end margin keeps the gap between it and the content.
    constexpr int markerPadding = listMarkerImagePadding;
    auto logicalWidth = this->logicalWidth();
    if (isImage())
        return { -logicalWidth - markerPadding, markerPadding };

    int offset = style().metricsOfPrimaryFont().intAscent() * 2 / 3;
    if (listMarkerSynthesizesGlyph(style(), protect(document())))
        return { -offset - markerPadding - 1, offset + markerPadding + 1 - logicalWidth };

    return { -logicalWidth, 0 };
}

RenderListItem* RenderListOutsideMarker::listItem() const
{
    return m_listItem.get();
}

Node* RenderListOutsideMarker::nodeForHitTest() const
{
    return m_listItem ? m_listItem->element() : nullptr;
}

LayoutRect RenderListOutsideMarker::selectionRectForRepaint(const RenderLayerModelObject*, bool)
{
    ASSERT(!needsLayout());
    return { };
}

} // namespace WebCore
