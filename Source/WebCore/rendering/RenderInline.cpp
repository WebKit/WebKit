/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "RenderInline.h"

#include "Chrome.h"
#include "FloatQuad.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "LegacyInlineFlowBox.h"
#include "LegacyInlineTextBox.h"
#include "OutlinePainter.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderElementInlines.h"
#include "RenderFragmentedFlow.h"
#include "RenderGeometryMap.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderListOutsideMarker.h"
#include "RenderObjectInlines.h"
#include "RenderSVGInline.h"
#include "RenderTable.h"
#include "RenderTheme.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "Settings.h"
#include "TransformState.h"
#include "VisiblePosition.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderInline);

RenderInline::RenderInline(Type type, Element& element, Style::ComputedStyle&& style)
    : RenderBoxModelObject(type, element, WTF::move(style), TypeFlag::IsInlineBox, { })
{
    setChildrenInline(true);
    ASSERT(isInlineBox());
}

RenderInline::RenderInline(Type type, Document& document, Style::ComputedStyle&& style)
    : RenderBoxModelObject(type, document, WTF::move(style), TypeFlag::IsInlineBox, { })
{
    setChildrenInline(true);
    ASSERT(isInlineBox());
}

RenderInline::~RenderInline() = default;

void RenderInline::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    RenderBoxModelObject::styleDidChange(diff, oldStyle);

    propagateStyleToAnonymousChildren(StylePropagationType::AllChildren);
}

void RenderInline::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this))
        lineLayout->paint(paintInfo, paintOffset, this);
}

ASCIILiteral RenderInline::renderName() const
{
    if (isRelativelyPositioned())
        return "RenderInline (relative positioned)"_s;
    if (isStickilyPositioned())
        return "RenderInline (sticky positioned)"_s;
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement() || isAnonymous())
        return "RenderInline (generated)"_s;
    return "RenderInline"_s;
}

bool RenderInline::nodeAtPoint(const HitTestRequest& request, HitTestResult& result,
    const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    ASSERT(layer());
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this))
        return lineLayout->hitTest(request, result, locationInContainer, accumulatedOffset, hitTestAction, this);
    return false;
}

auto RenderInline::localRectsForRepaint(RepaintOutlineBounds) const -> RepaintRects
{
    // RepaintOutlineBounds is unused for inlines.

    // Only first-letter renderers are allowed in here during layout. They mutate the tree triggering repaints.
#ifndef NDEBUG
    auto insideSelfPaintingInlineBox = [&] {
        if (hasSelfPaintingLayer())
            return true;
        auto* containingBlock = this->containingBlock();
        for (auto* ancestor = this->parent(); ancestor && ancestor != containingBlock; ancestor = ancestor->parent()) {
            if (ancestor->hasSelfPaintingLayer())
                return true;
        }
        return false;
    };
    ASSERT_UNUSED(insideSelfPaintingInlineBox, !view().frameView().layoutContext().isPaintOffsetCacheEnabled() || style().pseudoElementType() == PseudoElementType::FirstLetter || insideSelfPaintingInlineBox());
#endif

    if (!firstLegacyInlineBoxFor(*this) && !LayoutIntegration::LineLayout::containing(*this))
        return { };

    auto repaintRect = visualOverflowRect();
    repaintRect.inflate(LayoutUnit { style().usedOutlineSize(style().usedZoomForLength(), style().deviceScaleFactor()) });
    return { repaintRect };
}

LayoutRect RenderInline::rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const
{
    LayoutRect r(RenderBoxModelObject::rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    for (auto& child : childrenOfType<RenderElement>(*this))
        r.unite(child.rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    return r;
}

void RenderInline::imageChanged(WrappedImagePtr image, const IntRect*)
{
    if (!parent())
        return;

    bool isNonEmpty;
    RefPtr styleImage = Style::findLayerUsedImage(style().backgroundLayers(), image, isNonEmpty);
    if (styleImage && isNonEmpty) {
        if (auto styleable = Styleable::fromRenderer(*this))
            protect(document())->didLoadImage(protect(styleable->element).get(), protect(styleImage->cachedImage()));
    }

    // FIXME: We can do better.
    repaint();
}


bool RenderInline::requiresLayer() const
{
    return isInFlowPositioned()
        || createsGroup()
        || hasClipPath()
        || style().willChange().canCreateStackingContext()
        || hasRunningAcceleratedAnimations()
        || requiresRenderingConsolidationForViewTransition();
}

} // namespace WebCore
