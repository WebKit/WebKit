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

inline Overflow RenderElement::effectiveOverflowBlockDirection() const { return writingMode().isHorizontal() ? effectiveOverflowY() : effectiveOverflowX(); }
inline Overflow RenderElement::effectiveOverflowInlineDirection() const { return writingMode().isHorizontal() ? effectiveOverflowX() : effectiveOverflowY(); }
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
        || shouldApplyLayoutContainment()
        || shouldApplyPaintContainment();
}

inline bool RenderElement::canContainFixedPositionObjects() const
{
    return isRenderView()
        || (canEstablishContainingBlockWithTransform() && hasTransformRelatedProperty())
        || (hasBackdropFilter() && !isDocumentElementRenderer())
        || (isRenderBlock() && style().willChange() && style().willChange()->createsContainingBlockForOutOfFlowPositioned(isDocumentElementRenderer()))
        || isRenderOrLegacyRenderSVGForeignObject()
        || shouldApplyLayoutContainment()
        || shouldApplyPaintContainment();
}

inline bool RenderElement::createsGroupForStyle(const RenderStyle& style)
{
    return style.hasOpacity() || style.hasMask() || style.clipPath() || style.hasFilter() || style.hasBackdropFilter() || style.hasBlendMode();
}

inline bool RenderElement::shouldApplyAnyContainment() const
{
    return shouldApplyLayoutContainment() || shouldApplySizeContainment() || shouldApplyInlineSizeContainment() || shouldApplyStyleContainment() || shouldApplyPaintContainment();
}

inline bool RenderElement::shouldApplySizeOrInlineSizeContainment() const
{
    return shouldApplySizeContainment() || shouldApplyInlineSizeContainment();
}

inline bool RenderElement::shouldApplyLayoutContainment() const
{
    // content-visibility hidden and auto turns on layout containment.
    auto& style = this->style();
    auto hasContainment = style.containsLayout() || style.contentVisibility() == ContentVisibility::Hidden || style.contentVisibility() == ContentVisibility::Auto;
    if (!hasContainment)
        return false;
    // Giving an element layout containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its principal box is an internal table box other than table-cell
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.isInternalTableBox() && style.display() != DisplayType::TableCell)
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (isInline() && !isAtomicInlineLevelBox()))
        return false;
    return true;
}

inline bool RenderElement::shouldApplySizeContainment() const
{
    auto& style = this->style();
    auto hasContainment = style.containsSize() || style.contentVisibility() == ContentVisibility::Hidden || (style.contentVisibility() == ContentVisibility::Auto && element() && !element()->isRelevantToUser());
    if (!hasContainment)
        return false;
    // Giving an element size containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its inner display type is table
    //   if its principal box is an internal table box
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.display() == DisplayType::Table || style.display() == DisplayType::InlineTable)
        return false;
    if (style.isInternalTableBox())
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (isInline() && !isAtomicInlineLevelBox()))
        return false;
    return true;
}

inline bool RenderElement::shouldApplyInlineSizeContainment() const
{
    auto& style = this->style();
    if (!style.containsInlineSize())
        return false;
    // Giving an element inline-size containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its inner display type is table
    //   if its principal box is an internal table box
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.display() == DisplayType::Table || style.display() == DisplayType::InlineTable)
        return false;
    if (style.isInternalTableBox())
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (isInline() && !isAtomicInlineLevelBox()))
        return false;
    return true;
}

inline bool RenderElement::shouldApplyStyleContainment() const
{
    // content-visibility hidden and auto turns on style containment.
    return style().containsStyle() || style().contentVisibility() == ContentVisibility::Hidden || style().contentVisibility() == ContentVisibility::Auto;
}

inline bool RenderElement::shouldApplyPaintContainment() const
{
    // content-visibility hidden and auto turns on paint containment.
    auto& style = this->style();
    auto hasContainment = style.containsPaint() || style.contentVisibility() == ContentVisibility::Hidden || style.contentVisibility() == ContentVisibility::Auto;
    if (!hasContainment)
        return false;
    // Giving an element paint containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its principal box is an internal table box other than table-cell
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.isInternalTableBox() && style.display() != DisplayType::TableCell)
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (isInline() && !isAtomicInlineLevelBox()))
        return false;
    return true;
}

inline bool RenderElement::visibleToHitTesting(const std::optional<HitTestRequest>& request) const
{
    auto visibility = !request || request->userTriggered() ? style().usedVisibility() : style().visibility();
    return visibility == Visibility::Visible
        && !isSkippedContent()
        && ((request && request->ignoreCSSPointerEventsProperty()) || usedPointerEvents() != PointerEvents::None);
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

inline bool isSkippedContentRoot(const RenderElement& renderer)
{
    if (!renderer.shouldApplySizeContainment())
        return false;

    switch (renderer.style().contentVisibility()) {
    case ContentVisibility::Visible:
        return false;
    case ContentVisibility::Hidden:
        return true;
    case ContentVisibility::Auto:
        return renderer.element() && !renderer.element()->isRelevantToUser();
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

} // namespace WebCore
