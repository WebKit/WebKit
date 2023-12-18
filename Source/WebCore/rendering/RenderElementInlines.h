/**
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "RenderElement.h"
#include "RenderObjectInlines.h"

namespace WebCore {

inline Overflow RenderElement::effectiveOverflowBlockDirection() const { return style().isHorizontalWritingMode() ? effectiveOverflowY() : effectiveOverflowX(); }
inline Overflow RenderElement::effectiveOverflowInlineDirection() const { return style().isHorizontalWritingMode() ? effectiveOverflowX() : effectiveOverflowY(); }
inline bool RenderElement::hasBackdropFilter() const { return style().hasBackdropFilter(); }
inline bool RenderElement::hasBackground() const { return style().hasBackground(); }
inline bool RenderElement::hasBlendMode() const { return style().hasBlendMode(); }
inline bool RenderElement::hasClip() const { return isOutOfFlowPositioned() && style().hasClip(); }
inline bool RenderElement::hasClipOrNonVisibleOverflow() const { return hasClip() || hasNonVisibleOverflow(); }
inline bool RenderElement::hasClipPath() const { return style().clipPath(); }
inline bool RenderElement::hasFilter() const { return style().hasFilter(); }
inline bool RenderElement::hasHiddenBackface() const { return style().backfaceVisibility() == BackfaceVisibility::Hidden; }
inline bool RenderElement::hasMask() const { return style().hasMask(); }
inline bool RenderElement::hasOutline() const { return style().hasOutline() || hasOutlineAnnotation(); }
inline bool RenderElement::hasShapeOutside() const { return style().shapeOutside(); }
inline bool RenderElement::isTransparent() const { return style().hasOpacity(); }
inline float RenderElement::opacity() const { return style().opacity(); }
inline FloatRect RenderElement::transformReferenceBoxRect() const { return transformReferenceBoxRect(style()); }
inline FloatRect RenderElement::transformReferenceBoxRect(const RenderStyle& style) const { return referenceBoxRect(transformBoxToCSSBoxType(style.transformBox())); }

inline bool RenderElement::canContainAbsolutelyPositionedObjects() const
{
    return isRenderView()
        || style().position() != PositionType::Static
        || (canEstablishContainingBlockWithTransform() && hasTransformRelatedProperty())
        || (hasBackdropFilter() && !isDocumentElementRenderer())
        || (isRenderBlock() && style().willChange() && style().willChange()->createsContainingBlockForAbsolutelyPositioned(isDocumentElementRenderer()))
        || isRenderOrLegacyRenderSVGForeignObject()
        || shouldApplyLayoutOrPaintContainment();
}

inline bool RenderElement::canContainFixedPositionObjects() const
{
    return isRenderView()
        || (canEstablishContainingBlockWithTransform() && hasTransformRelatedProperty())
        || (hasBackdropFilter() && !isDocumentElementRenderer())
        || (isRenderBlock() && style().willChange() && style().willChange()->createsContainingBlockForOutOfFlowPositioned(isDocumentElementRenderer()))
        || isRenderOrLegacyRenderSVGForeignObject()
        || shouldApplyLayoutOrPaintContainment();
}

inline bool RenderElement::createsGroupForStyle(const RenderStyle& style)
{
    return style.hasOpacity() || style.hasMask() || style.clipPath() || style.hasFilter() || style.hasBackdropFilter() || style.hasBlendMode();
}

inline bool RenderElement::shouldApplyAnyContainment() const
{
    return shouldApplyLayoutOrPaintContainment() || shouldApplySizeOrStyleContainment(style().containsSizeOrInlineSize() || style().containsStyle());
}

inline bool RenderElement::shouldApplyInlineSizeContainment() const
{
    return isSkippedContentRoot() || shouldApplySizeOrStyleContainment(style().containsInlineSize());
}

inline bool RenderElement::shouldApplyLayoutContainment() const
{
    return shouldApplyLayoutOrPaintContainment(style().containsLayout() || style().contentVisibility() != ContentVisibility::Visible);
}

inline bool RenderElement::shouldApplyLayoutOrPaintContainment(bool containsAccordingToStyle) const
{
    return containsAccordingToStyle && (!isInline() || isAtomicInlineLevelBox()) && !(isRenderRubyText() || style().display() == DisplayType::RubyAnnotation) && (!isTablePart() || isRenderBlockFlow());
}

inline bool RenderElement::shouldApplyLayoutOrPaintContainment() const
{
    return shouldApplyLayoutOrPaintContainment(style().containsLayoutOrPaint()) || shouldApplySizeOrStyleContainment(style().contentVisibility() != ContentVisibility::Visible);
}

inline bool RenderElement::shouldApplyPaintContainment() const
{
    return shouldApplyLayoutOrPaintContainment(style().containsPaint()) || shouldApplySizeOrStyleContainment(style().contentVisibility() != ContentVisibility::Visible);
}

inline bool RenderElement::shouldApplySizeContainment() const
{
    return isSkippedContentRoot() || shouldApplySizeOrStyleContainment(style().containsSize());
}

inline bool RenderElement::shouldApplySizeOrInlineSizeContainment() const
{
    return isSkippedContentRoot() || shouldApplySizeOrStyleContainment(style().containsSizeOrInlineSize());
}

// FIXME: try to avoid duplication with isSkippedContentRoot.
inline bool RenderElement::shouldApplySizeOrStyleContainment(bool containsAccordingToStyle) const
{
    return containsAccordingToStyle && (!isInline() || isAtomicInlineLevelBox()) && !(isRenderRubyText() || style().display() == DisplayType::RubyAnnotation) && (!isTablePart() || isRenderTableCaption()) && !isRenderTable();
}

inline bool RenderElement::shouldApplyStyleContainment() const
{
    return shouldApplySizeOrStyleContainment(style().containsStyle() || style().contentVisibility() != ContentVisibility::Visible);
}

inline bool RenderElement::visibleToHitTesting(const std::optional<HitTestRequest>& request) const
{
    return style().visibility() == Visibility::Visible
        && !isSkippedContent()
        && ((request && request->ignoreCSSPointerEventsProperty()) || style().effectivePointerEvents() != PointerEvents::None);
}

inline int adjustForAbsoluteZoom(int value, const RenderElement& renderer)
{
    return adjustForAbsoluteZoom(value, renderer.style());
}

inline LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize size, const RenderElement& renderer)
{
    return adjustLayoutSizeForAbsoluteZoom(size, renderer.style());
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value, const RenderElement& renderer)
{
    return adjustLayoutUnitForAbsoluteZoom(value, renderer.style());
}

} // namespace WebCore
