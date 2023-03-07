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

#include "RenderObject.h"
#include "RenderStyle.h"

namespace WebCore {

inline bool RenderObject::isFixedPositioned() const
{
    return isOutOfFlowPositioned() && style().position() == PositionType::Fixed;
}

bool RenderObject::isAbsolutelyPositioned() const
{
    return isOutOfFlowPositioned() && style().position() == PositionType::Absolute;
}

bool RenderObject::shouldUsePositionedClipping() const
{
    return isAbsolutelyPositioned() || isSVGForeignObject();
}

inline bool RenderObject::isTransformed() const
{
    return hasTransformRelatedProperty() && (style().affectsTransform() || hasSVGTransform());
}

inline bool RenderObject::hasTransformOrPerspective() const
{
    return hasTransformRelatedProperty() && (isTransformed() || style().hasPerspective());
}

inline bool RenderObject::isBeforeContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (isText())
        return false;
    if (style().styleType() != PseudoId::Before)
        return false;
    return true;
}

inline bool RenderObject::isAfterContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (isText())
        return false;
    if (style().styleType() != PseudoId::After)
        return false;
    return true;
}

inline bool RenderObject::isBeforeOrAfterContent() const
{
    return isBeforeContent() || isAfterContent();
}

inline bool RenderObject::isBeforeContent(const RenderObject* obj)
{
    return obj && obj->isBeforeContent();
}

inline bool RenderObject::isAfterContent(const RenderObject* obj)
{
    return obj && obj->isAfterContent();
}

inline bool RenderObject::isBeforeOrAfterContent(const RenderObject* obj)
{
    return obj && obj->isBeforeOrAfterContent();
}

inline bool RenderObject::preservesNewline() const
{
    if (isSVGInlineText())
        return false;

    return style().preserveNewline();
}

inline bool RenderObject::isAnonymousBlock() const
{
    // This function must be kept in sync with anonymous block creation conditions in RenderBlock::createAnonymousBlock().
    // FIXME: That seems difficult. Can we come up with a simpler way to make behavior correct?
    // FIXME: Does this relatively long function benefit from being inlined?
    return isAnonymous()
        && (style().display() == DisplayType::Block || style().display() == DisplayType::Box)
        && style().styleType() == PseudoId::None
        && isRenderBlock()
#if ENABLE(MATHML)
        && !isRenderMathMLBlock()
#endif
        && !isListMarker()
        && !isRenderFragmentedFlow()
        && !isRenderMultiColumnSet()
        && !isRenderView();
}

inline bool RenderObject::isAtomicInlineLevelBox() const
{
    return style().isDisplayInlineType() && !(style().display() == DisplayType::Inline && !isReplacedOrInlineBlock());
}

inline bool RenderObject::hasPotentiallyScrollableOverflow() const
{
    // We only need to test one overflow dimension since 'visible' and 'clip' always get accompanied
    // with 'clip' or 'visible' in the other dimension (see Style::Adjuster::adjust).
    return hasNonVisibleOverflow() && style().overflowX() != Overflow::Clip && style().overflowX() != Overflow::Visible;
}

}
