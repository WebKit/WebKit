/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FloatRect.h"
#include "GraphicsTypes.h"
#include "InlineIteratorInlineBox.h"
#include "LayoutRect.h"
#include "ShadowData.h"

namespace WebCore {

class Color;
class FillLayer;
class LegacyInlineFlowBox;
class RenderBoxModelObject;
class RenderStyle;
struct PaintInfo;

class InlineBoxPainter {
public:
    InlineBoxPainter(const LegacyInlineFlowBox&, PaintInfo&, const LayoutPoint& paintOffset);
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    InlineBoxPainter(const LayoutIntegration::InlineContent&, const InlineDisplay::Box&, PaintInfo&, const LayoutPoint& paintOffset);
#endif
    ~InlineBoxPainter();

    void paint();

private:
    InlineBoxPainter(const InlineIterator::InlineBox&, PaintInfo&, const LayoutPoint& paintOffset);

    void paintMask();
    void paintDecorations();
    void paintFillLayers(const Color&, const FillLayer&, const LayoutRect& paintRect, CompositeOperator);
    void paintFillLayer(const Color&, const FillLayer&, const LayoutRect& paintRect, CompositeOperator);
    void paintBoxShadow(ShadowStyle, const LayoutRect& paintRect);

    const RenderStyle& style() const;
    // FIXME: Make RenderBoxModelObject functions const.
    RenderBoxModelObject& renderer() const { return const_cast<RenderBoxModelObject&>(m_renderer); }
    bool isHorizontal() const { return m_isHorizontal; }

    const InlineIterator::InlineBox m_inlineBox;
    PaintInfo& m_paintInfo;
    const LayoutPoint m_paintOffset;
    const RenderBoxModelObject& m_renderer;
    const bool m_isFirstLine;
    const bool m_isRootInlineBox;
    const bool m_isHorizontal;
};

}
