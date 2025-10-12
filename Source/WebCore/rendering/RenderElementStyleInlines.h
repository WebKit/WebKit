/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/RenderElementInlines.h>
#include <WebCore/RenderStyleInlines.h>

namespace WebCore {

inline bool RenderElement::hasBackdropFilter() const { return style().hasBackdropFilter(); }
inline bool RenderElement::hasBackground() const { return style().hasBackground(); }
inline bool RenderElement::hasBlendMode() const { return style().hasBlendMode(); }
inline bool RenderElement::hasClip() const { return isOutOfFlowPositioned() && style().hasClip(); }
inline bool RenderElement::hasClipOrNonVisibleOverflow() const { return hasClip() || hasNonVisibleOverflow(); }
inline bool RenderElement::hasClipPath() const { return style().hasClipPath(); }
inline bool RenderElement::hasFilter() const { return style().hasFilter(); }
inline bool RenderElement::hasHiddenBackface() const { return style().backfaceVisibility() == BackfaceVisibility::Hidden; }
inline bool RenderElement::hasMask() const { return style().hasMask(); }
inline bool RenderElement::hasOutline() const { return style().hasOutline() || hasOutlineAnnotation(); }
inline bool RenderElement::hasShapeOutside() const { return !style().shapeOutside().isNone(); }
inline bool RenderElement::isTransparent() const { return style().hasOpacity(); }
inline float RenderElement::opacity() const { return style().opacity().value.value; }
inline FloatRect RenderElement::transformReferenceBoxRect() const { return transformReferenceBoxRect(style()); }
inline FloatRect RenderElement::transformReferenceBoxRect(const RenderStyle& style) const { return referenceBoxRect(transformBoxToCSSBoxType(style.transformBox())); }

#if HAVE(CORE_MATERIAL)
inline bool RenderElement::hasAppleVisualEffect() const { return style().hasAppleVisualEffect(); }
inline bool RenderElement::hasAppleVisualEffectRequiringBackdropFilter() const { return style().hasAppleVisualEffectRequiringBackdropFilter(); }
#endif

inline bool RenderElement::mayContainOutOfFlowPositionedObjects(const RenderStyle* styleToUse) const
{
    auto& style = styleToUse ? *styleToUse : this->style();
    return isRenderView()
    || (canEstablishContainingBlockWithTransform() && (styleToUse ? styleToUse->hasTransformRelatedProperty() : hasTransformRelatedProperty()))
    || (style.hasBackdropFilter() && !isDocumentElementRenderer())
    || (style.hasFilter() && !isDocumentElementRenderer())
#if HAVE(CORE_MATERIAL)
    || (style.hasAppleVisualEffectRequiringBackdropFilter() && !isDocumentElementRenderer())
#endif
    || isRenderOrLegacyRenderSVGForeignObject()
    || shouldApplyLayoutContainment(styleToUse)
    || shouldApplyPaintContainment(styleToUse)
    || isViewTransitionContainingBlock();
}

inline bool RenderElement::canContainAbsolutelyPositionedObjects(const RenderStyle* styleToUse) const
{
    auto& style = styleToUse ? *styleToUse : this->style();
    return mayContainOutOfFlowPositionedObjects(styleToUse) || style.position() != PositionType::Static || (isRenderBlock() && style.willChange() && style.willChange()->createsContainingBlockForAbsolutelyPositioned(isDocumentElementRenderer()));
}

inline bool RenderElement::canContainFixedPositionObjects(const RenderStyle* styleToUse) const
{
    auto& style = styleToUse ? *styleToUse : this->style();
    return mayContainOutOfFlowPositionedObjects(styleToUse) || (isRenderBlock() && style.willChange() && style.willChange()->createsContainingBlockForOutOfFlowPositioned(isDocumentElementRenderer()));
}

inline bool RenderElement::createsGroupForStyle(const RenderStyle& style)
{
    return style.hasOpacity()
    || style.hasMask()
    || style.hasClipPath()
    || style.hasFilter()
    || style.hasBackdropFilter()
#if HAVE(CORE_MATERIAL)
    || style.hasAppleVisualEffect()
#endif
    || style.hasBlendMode();
}

inline bool RenderElement::shouldApplyAnyContainment() const
{
    return shouldApplyLayoutContainment() || shouldApplySizeContainment() || shouldApplyInlineSizeContainment() || shouldApplyStyleContainment() || shouldApplyPaintContainment();
}

inline bool RenderElement::shouldApplySizeOrInlineSizeContainment() const
{
    return shouldApplySizeContainment() || shouldApplyInlineSizeContainment();
}

inline bool RenderElement::shouldApplyLayoutContainment(const RenderStyle* styleToUse) const
{
    return element() && WebCore::shouldApplyLayoutContainment(styleToUse ? *styleToUse : style(), *element());
}

inline bool RenderElement::shouldApplySizeContainment() const
{
    return element() && WebCore::shouldApplySizeContainment(style(), *element());
}

inline bool RenderElement::shouldApplyInlineSizeContainment() const
{
    return element() && WebCore::shouldApplyInlineSizeContainment(style(), *element());
}

inline bool RenderElement::shouldApplyStyleContainment() const
{
    return element() && WebCore::shouldApplyStyleContainment(style(), *element());
}

inline bool RenderElement::shouldApplyPaintContainment(const RenderStyle* styleToUse) const
{
    return element() && WebCore::shouldApplyPaintContainment(styleToUse ? *styleToUse : style(), *element());
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

}
