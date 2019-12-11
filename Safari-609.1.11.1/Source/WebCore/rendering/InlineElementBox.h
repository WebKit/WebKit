/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "InlineBox.h"

namespace WebCore {

class InlineElementBox : public InlineBox {
    WTF_MAKE_ISO_ALLOCATED(InlineElementBox);
public:
    explicit InlineElementBox(RenderBoxModelObject& renderer)
        : InlineBox(renderer)
    {
    }

    InlineElementBox(RenderObject& renderer, FloatPoint topLeft, float logicalWidth, bool firstLine, bool constructed, bool dirty, bool extracted, bool isHorizontal, InlineBox* next, InlineBox* prev, InlineFlowBox* parent)
        : InlineBox(renderer, topLeft, logicalWidth, firstLine, constructed, dirty, extracted, isHorizontal, next, prev, parent)
    {
    }

    RenderBoxModelObject& renderer() const { return downcast<RenderBoxModelObject>(InlineBox::renderer()); }

    void deleteLine() final;
    void extractLine() final;
    void attachLine() final;

    void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) override;
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction) override;

private:
    bool isInlineElementBox() const final { return true; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_INLINE_BOX(InlineElementBox, isInlineElementBox())
