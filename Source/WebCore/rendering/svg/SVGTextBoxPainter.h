/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "InlineIteratorSVGTextBox.h"
#include "LegacyRenderSVGResource.h"
#include "TextBoxPainter.h"

namespace WebCore {

class RenderBoxModelObject;
class RenderSVGInlineText;
class RenderStyle;
class SVGPaintServerHandling;
struct SVGTextFragment;

template<typename TextBoxPath>
class SVGTextBoxPainter : public TextBoxPainter<TextBoxPath> {
public:
    SVGTextBoxPainter(TextBoxPath&&, PaintInfo&, const LayoutPoint& paintOffset);
    ~SVGTextBoxPainter() = default;

    void paint();
    void paintSelectionBackground();

private:
    using TextBoxPainter<TextBoxPath>::textBox;
    using TextBoxPainter<TextBoxPath>::m_selectableRange;
    using TextBoxPainter<TextBoxPath>::m_paintInfo;
    using TextBoxPainter<TextBoxPath>::m_paintOffset;
    using TextBoxPainter<TextBoxPath>::m_renderer;
    using TextBoxPainter<TextBoxPath>::m_haveSelection;
    using TextBoxPainter<TextBoxPath>::selectionStartEnd;

    InlineIterator::SVGTextBoxIterator textBoxIterator() const;

    const RenderSVGInlineText& renderer() const; // { return downcast<RenderSVGInlineText>(m_renderer); }
    const RenderBoxModelObject& parentRenderer() const;
    OptionSet<RenderSVGResourceMode> paintingResourceMode() const { return m_paintingResourceMode; }

    FloatRect selectionRectForTextFragment(const SVGTextFragment&, unsigned startPosition, unsigned endPosition, const RenderStyle&) const;
    void paintDecoration(OptionSet<TextDecorationLine>, const SVGTextFragment&);
    void paintDecorationWithStyle(OptionSet<TextDecorationLine>, const SVGTextFragment&, const RenderBoxModelObject&);
    void paintTextWithShadows(const RenderStyle&, TextRun&, const SVGTextFragment&, unsigned startPosition, unsigned endPosition);
    void paintText(const RenderStyle&, const RenderStyle& selectionStyle, const SVGTextFragment&, bool hasSelection, bool paintSelectedTextOnly);

    bool acquirePaintingResource(SVGPaintServerHandling&, float scalingFactor, const RenderBoxModelObject&, const RenderStyle&);
    void releasePaintingResource(SVGPaintServerHandling&);

    bool acquireLegacyPaintingResource(GraphicsContext*&, float scalingFactor, RenderBoxModelObject&, const RenderStyle&);
    void releaseLegacyPaintingResource(GraphicsContext*&, const Path*);

    bool mapStartEndPositionsIntoFragmentCoordinates(const SVGTextFragment&, unsigned& startPosition, unsigned& endPosition) const;

    TextRun constructTextRun(const RenderStyle&, const SVGTextFragment&) const;

    OptionSet<RenderSVGResourceMode> m_paintingResourceMode;
    LegacyRenderSVGResource* m_legacyPaintingResource { nullptr };
};

class LegacySVGTextBoxPainter : public SVGTextBoxPainter<InlineIterator::BoxLegacyPath> {
public:
    LegacySVGTextBoxPainter(const SVGInlineTextBox&, PaintInfo&, const LayoutPoint& paintOffset);
};

}
