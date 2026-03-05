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
#include <WebCore/RenderStyle+GettersInlines.h>
#include <WebCore/StyleContainmentCheckerInlines.h>

namespace WebCore {

inline bool RenderElement::hasBackdropFilter() const { return !style().backdropFilter().isNone(); }
inline bool RenderElement::hasBackground() const { return style().hasBackground(); }
inline bool RenderElement::hasBlendMode() const { return style().blendMode() != BlendMode::Normal; }
inline bool RenderElement::hasClip() const { return isOutOfFlowPositioned() && !style().clip().isAuto(); }
inline bool RenderElement::hasClipOrNonVisibleOverflow() const { return hasClip() || hasNonVisibleOverflow(); }
inline bool RenderElement::hasClipPath() const { return !style().clipPath().isNone(); }
inline bool RenderElement::hasFilter() const { return !style().filter().isNone(); }
inline bool RenderElement::hasHiddenBackface() const { return style().backfaceVisibility() == BackfaceVisibility::Hidden; }
inline bool RenderElement::hasMask() const { return Style::hasImageInAnyLayer(style().maskLayers()) || !style().maskBorderSource().isNone(); }
inline bool RenderElement::hasOutline() const { return style().hasOutline() || hasOutlineAnnotation(); }
inline bool RenderElement::hasShapeOutside() const { return !style().shapeOutside().isNone(); }
inline bool RenderElement::isTransparent() const { return !style().opacity().isOpaque(); }
inline float RenderElement::opacity() const { return style().opacity().value.value; }
inline FloatRect RenderElement::transformReferenceBoxRect() const { return transformReferenceBoxRect(style()); }
inline FloatRect RenderElement::transformReferenceBoxRect(const RenderStyle& style) const { return referenceBoxRect(transformBoxToCSSBoxType(style.transformBox())); }

#if HAVE(CORE_MATERIAL)
inline bool RenderElement::hasAppleVisualEffect() const { return style().appleVisualEffect() != AppleVisualEffect::None; }
inline bool RenderElement::hasAppleVisualEffectRequiringBackdropFilter() const { return appleVisualEffectNeedsBackdrop(style().appleVisualEffect()); }
#endif

inline bool RenderElement::mayContainOutOfFlowPositionedObjects(const RenderStyle* styleToUse) const
{
    auto shouldApplyLayoutOrPaintContainment = [this](const auto& style) {
        RefPtr element = this->element();
        if (!element)
            return false;

        Style::ContainmentChecker checker { style, *element };

        return checker.shouldApplyLayoutContainment()
            || checker.shouldApplyPaintContainment();
    };

    auto& style = styleToUse ? *styleToUse : this->style();
    return isRenderView()
        || (canEstablishContainingBlockWithTransform() && (styleToUse ? styleToUse->hasTransformRelatedProperty() : hasTransformRelatedProperty()))
        || (!style.backdropFilter().isNone() && !isDocumentElementRenderer())
        || (!style.filter().isNone() && !isDocumentElementRenderer())
#if HAVE(CORE_MATERIAL)
        || (appleVisualEffectNeedsBackdrop(style.appleVisualEffect()) && !isDocumentElementRenderer())
#endif
        || isRenderOrLegacyRenderSVGForeignObject()
        || shouldApplyLayoutOrPaintContainment(style)
        || isViewTransitionContainingBlock();
}

inline bool RenderElement::canContainAbsolutelyPositionedObjects(const RenderStyle* styleToUse) const
{
    auto& style = styleToUse ? *styleToUse : this->style();
    return mayContainOutOfFlowPositionedObjects(styleToUse)
        || style.position() != PositionType::Static
        || (isRenderBlock() && style.willChange().createsContainingBlockForAbsolutelyPositioned(isDocumentElementRenderer()));
}

inline bool RenderElement::canContainFixedPositionObjects(const RenderStyle* styleToUse) const
{
    auto& style = styleToUse ? *styleToUse : this->style();
    return mayContainOutOfFlowPositionedObjects(styleToUse)
        || (isRenderBlock() && style.willChange().createsContainingBlockForOutOfFlowPositioned(isDocumentElementRenderer()));
}

inline bool RenderElement::createsGroupForStyle(const RenderStyle& style)
{
    return !style.opacity().isOpaque()
        || Style::hasImageInAnyLayer(style.maskLayers())
        || !style.maskBorderSource().isNone()
        || !style.clipPath().isNone()
        || !style.filter().isNone()
        || !style.backdropFilter().isNone()
#if HAVE(CORE_MATERIAL)
        || style.appleVisualEffect() != AppleVisualEffect::None
#endif
        || style.blendMode() != BlendMode::Normal;
}

inline bool RenderElement::shouldApplyAnyContainment() const
{
    RefPtr element = this->element();
    if (!element)
        return false;

    Style::ContainmentChecker checker { style(), *element };

    return checker.shouldApplyLayoutContainment()
        || checker.shouldApplySizeContainment()
        || checker.shouldApplyInlineSizeContainment()
        || checker.shouldApplyStyleContainment()
        || checker.shouldApplyPaintContainment();
}

inline bool RenderElement::shouldApplySizeOrInlineSizeContainment() const
{
    RefPtr element = this->element();
    if (!element)
        return false;

    Style::ContainmentChecker checker { style(), *element };

    return checker.shouldApplySizeContainment()
        || checker.shouldApplyInlineSizeContainment();
}

inline bool RenderElement::shouldApplyLayoutContainment() const
{
    return element() && Style::ContainmentChecker { style(), *element() }.shouldApplyLayoutContainment();
}

inline bool RenderElement::shouldApplySizeContainment() const
{
    return element() && Style::ContainmentChecker { style(), *element() }.shouldApplySizeContainment();
}

inline bool RenderElement::shouldApplyInlineSizeContainment() const
{
    return element() && Style::ContainmentChecker { style(), *element() }.shouldApplyInlineSizeContainment();
}

inline bool RenderElement::shouldApplyStyleContainment() const
{
    return element() && Style::ContainmentChecker { style(), *element() }.shouldApplyStyleContainment();
}

inline bool RenderElement::shouldApplyPaintContainment() const
{
    return element() && Style::ContainmentChecker { style(), *element() }.shouldApplyPaintContainment();
}

inline bool RenderElement::visibleToHitTesting(const std::optional<HitTestRequest>& request) const
{
    auto visibility = !request || request->userTriggered() ? style().usedVisibility() : style().visibility();
    return visibility == Visibility::Visible
        && !isSkippedContent()
        && ((request && request->ignoreCSSPointerEventsProperty()) || usedPointerEvents() != PointerEvents::None);
}

} // namespace WebCore
