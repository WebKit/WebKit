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
class SVGTextBoxPainter {
public:
    SVGTextBoxPainter(TextBoxPath&&, PaintInfo&, const LayoutPoint& paintOffset);
    ~SVGTextBoxPainter() = default;

    void paint();
    void paintSelectionBackground();

private:
    InlineIterator::SVGTextBoxIterator textBoxIterator() const;

    const RenderSVGInlineText& renderer() const { return m_renderer; }
    const RenderBoxModelObject& parentRenderer() const;
    OptionSet<RenderSVGResourceMode> paintingResourceMode() const { return m_paintingResourceMode; }

    void paintDecoration(OptionSet<TextDecorationLine>, const SVGTextFragment&);
    void paintDecorationWithStyle(OptionSet<TextDecorationLine>, const SVGTextFragment&, const RenderBoxModelObject&);
    void paintTextWithShadows(const RenderStyle&, TextRun&, const SVGTextFragment&, unsigned startPosition, unsigned endPosition);
    void paintText(const RenderStyle&, const RenderStyle& selectionStyle, const SVGTextFragment&, bool hasSelection, bool paintSelectedTextOnly);

    bool acquirePaintingResource(SVGPaintServerHandling&, float scalingFactor, const RenderBoxModelObject&, const RenderStyle&);
    void releasePaintingResource(SVGPaintServerHandling&);

    bool acquireLegacyPaintingResource(GraphicsContext*&, float scalingFactor, RenderBoxModelObject&, const RenderStyle&);
    void releaseLegacyPaintingResource(GraphicsContext*&, const Path*);

    std::pair<unsigned, unsigned> selectionStartEnd() const;

    const TextBoxPath m_textBox;
    const RenderSVGInlineText& m_renderer;
    PaintInfo& m_paintInfo;
    const TextBoxSelectableRange m_selectableRange;
    const LayoutPoint m_paintOffset;
    const bool m_haveSelection;

    OptionSet<RenderSVGResourceMode> m_paintingResourceMode;
    LegacyRenderSVGResource* m_legacyPaintingResource { nullptr };
};

class LegacySVGTextBoxPainter : public SVGTextBoxPainter<InlineIterator::BoxLegacyPath> {
public:
    LegacySVGTextBoxPainter(const SVGInlineTextBox&, PaintInfo&, const LayoutPoint& paintOffset);
};

class ModernSVGTextBoxPainter : public SVGTextBoxPainter<InlineIterator::BoxModernPath> {
public:
    ModernSVGTextBoxPainter(const LayoutIntegration::InlineContent&, size_t boxIndex, PaintInfo&, const LayoutPoint& paintOffset);
};

TextRun constructTextRun(StringView, TextDirection, const RenderStyle&, const SVGTextFragment&);
FloatRect selectionRectForTextFragment(const RenderSVGInlineText&, TextDirection, const SVGTextFragment&, unsigned startPosition, unsigned endPosition, const RenderStyle&);
bool mapStartEndPositionsIntoFragmentCoordinates(unsigned textBoxStart, const SVGTextFragment&, unsigned& startPosition, unsigned& endPosition);

}
