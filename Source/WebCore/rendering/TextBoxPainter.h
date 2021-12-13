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
#include "InlineIteratorTextBox.h"
#include "RenderObject.h"
#include "TextBoxSelectableRange.h"
#include "TextRun.h"

namespace WebCore {

class Color;
class Document;
class LegacyInlineTextBox;
class RenderCombineText;
class RenderStyle;
class RenderText;
class ShadowData;
class TextDecorationPainter;
struct CompositionUnderline;
struct MarkedText;
struct PaintInfo;
struct StyledMarkedText;

class TextBoxPainter {
public:
    TextBoxPainter(const LegacyInlineTextBox&, PaintInfo&, const LayoutPoint& paintOffset);
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    TextBoxPainter(const LayoutIntegration::InlineContent&, const InlineDisplay::Box&, PaintInfo&, const LayoutPoint& paintOffset);
#endif
    TextBoxPainter(const InlineIterator::TextBoxIterator&, PaintInfo&, const LayoutPoint& paintOffset);

    ~TextBoxPainter();

    void paint();

    static FloatRect calculateUnionOfAllDocumentMarkerBounds(const LegacyInlineTextBox&);

private:
    auto& textBox() const { return *m_textBox; }

    void paintBackground();
    void paintForegroundAndDecorations();
    void paintCompositionBackground();
    void paintCompositionUnderlines();
    void paintPlatformDocumentMarkers();

    enum class BackgroundStyle { Normal, Rounded };
    void paintBackground(unsigned startOffset, unsigned endOffset, const Color&, BackgroundStyle = BackgroundStyle::Normal);
    void paintBackground(const StyledMarkedText&);
    void paintForeground(const StyledMarkedText&);
    TextDecorationPainter createDecorationPainter(const StyledMarkedText&, const FloatRect&, const FloatRect&);
    void paintBackgroundDecorations(TextDecorationPainter&, const StyledMarkedText&, const FloatRect&);
    void paintForegroundDecorations(TextDecorationPainter&, const FloatRect&);
    void paintCompositionUnderline(const CompositionUnderline&);
    void paintPlatformDocumentMarker(const MarkedText&);

    static FloatRect calculateDocumentMarkerBounds(const InlineIterator::TextBoxIterator&, const MarkedText&);

    FloatRect computePaintRect(const LayoutPoint& paintOffset);
    bool computeHaveSelection() const;
    MarkedText createMarkedTextFromSelectionInBox();
    const FontCascade& fontCascade() const;
    FloatPoint textOriginFromPaintRect(const FloatRect&) const;

    const ShadowData* debugTextShadow() const;

    const InlineIterator::TextBoxIterator m_textBox;
    const RenderText& m_renderer;
    const Document& m_document;
    const RenderStyle& m_style;
    const TextRun m_paintTextRun;
    PaintInfo& m_paintInfo;
    const TextBoxSelectableRange m_selectableRange;
    const FloatRect m_paintRect;
    const bool m_isFirstLine;
    const bool m_isPrinting;
    const bool m_haveSelection;
    const bool m_containsComposition;
    const bool m_useCustomUnderlines;
    std::optional<bool> m_emphasisMarkExistsAndIsAbove { };
};

}
