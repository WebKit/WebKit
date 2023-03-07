/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include "RenderStyle.h"

namespace WebCore {

inline bool RenderElement::canContainFixedPositionObjects() const
{
    return isRenderView()
        || (canEstablishContainingBlockWithTransform() && hasTransformRelatedProperty())
        || (isRenderBlock() && style().willChange() && style().willChange()->createsContainingBlockForOutOfFlowPositioned()) // FIXME: will-change should create containing blocks on inline boxes (bug 225035)
        || isSVGForeignObjectOrLegacySVGForeignObject()
        || shouldApplyLayoutOrPaintContainment();
}

inline bool RenderElement::canContainAbsolutelyPositionedObjects() const
{
    return isRenderView()
        || style().position() != PositionType::Static
        || (canEstablishContainingBlockWithTransform() && hasTransformRelatedProperty())
        || (isRenderBlock() && style().willChange() && style().willChange()->createsContainingBlockForAbsolutelyPositioned()) // FIXME: will-change should create containing blocks on inline boxes (bug 225035)
        || isSVGForeignObjectOrLegacySVGForeignObject()
        || shouldApplyLayoutOrPaintContainment();
}

inline bool RenderElement::shouldApplyLayoutContainment() const
{
    return shouldApplyLayoutOrPaintContainment(style().containsLayout() || style().contentVisibility() != ContentVisibility::Visible);
}

inline bool RenderElement::shouldApplyPaintContainment() const
{
    return shouldApplyLayoutOrPaintContainment(style().containsPaint() || style().contentVisibility() != ContentVisibility::Visible);
}

inline bool RenderElement::shouldApplyLayoutOrPaintContainment() const
{
    return shouldApplyLayoutOrPaintContainment(style().containsLayoutOrPaint() || style().contentVisibility() != ContentVisibility::Visible);
}

inline bool RenderElement::shouldApplySizeOrStyleContainment(bool containsAccordingToStyle) const
{
    return containsAccordingToStyle && (!isInline() || isAtomicInlineLevelBox()) && !isRubyText() && (!isTablePart() || isTableCaption()) && !isTable();
}

inline bool RenderElement::shouldApplyLayoutOrPaintContainment(bool containsAccordingToStyle) const
{
    return containsAccordingToStyle && (!isInline() || isAtomicInlineLevelBox()) && !isRubyText() && (!isTablePart() || isRenderBlockFlow());
}

inline bool RenderElement::shouldApplyAnyContainment() const
{
    return shouldApplyLayoutOrPaintContainment() || shouldApplySizeOrStyleContainment(style().containsSizeOrInlineSize() || style().containsStyle());
}

inline bool RenderElement::shouldApplySizeContainment() const
{
    return shouldApplySizeOrStyleContainment(style().containsSize() || style().contentVisibility() == ContentVisibility::Hidden);
}

inline bool RenderElement::shouldApplyInlineSizeContainment() const
{
    return shouldApplySizeOrStyleContainment(style().containsInlineSize());
}

inline bool RenderElement::shouldApplySizeOrInlineSizeContainment() const
{
    return shouldApplySizeOrStyleContainment(style().containsSizeOrInlineSize() || style().contentVisibility() == ContentVisibility::Hidden);
}

inline bool RenderElement::shouldApplyStyleContainment() const
{
    return shouldApplySizeOrStyleContainment(style().containsStyle() || style().contentVisibility() != ContentVisibility::Visible);
}

}
