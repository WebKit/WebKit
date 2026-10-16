/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "AffineTransform.h"
#include "InlineDisplayContent.h"
#include <wtf/CheckedRef.h>
#include <wtf/Vector.h>

namespace WebCore {

class SVGTextContentElement;
class SVGTextMetrics;
struct SVGTextFragment;

namespace Layout {
class ElementBox;
class InlineTextBox;
}

// Positions the characters of SVG text following the SVG 2 text layout algorithm and produces the SVGTextFragments of the text display boxes.
// https://w3c.github.io/svgwg/svg2-draft/text.html#TextLayoutAlgorithm
class SVGTextLayoutAlgorithm {
public:
    SVGTextLayoutAlgorithm(const Layout::ElementBox& root, const InlineDisplay::Boxes&, Vector<Vector<SVGTextFragment>>& fragmentsForBoxes);
    ~SVGTextLayoutAlgorithm();

    void layout();

private:
    static constexpr unsigned notFound32 = std::numeric_limits<unsigned>::max();

    struct Character {
        // Position of the glyph origin, without the baseline shift.
        float x { 0 };
        float y { 0 };
        // Advance in the inline direction, without letter and word spacing.
        float advance { 0 };
        float spacing { 0 };
        float rotate { 0 };
        float orientationAngle { 0 };
        float xOrientationShift { 0 };
        float yOrientationShift { 0 };
        // The metrics entry that starts at this character, if any.
        unsigned metricsIndex { notFound32 };
        unsigned textBoxIndex { notFound32 };
        // lengthAdjust="spacingAndGlyphs" stretching of the glyph in the inline direction.
        float lengthAdjustScale { 1 };
        bool isAddressable : 1 { false };
        bool isClusterStart : 1 { false };
        bool startsAnchoredChunk : 1 { false };
        bool isHidden : 1 { false };
        bool isPositioned : 1 { false };
        bool isOnPath : 1 { false };
        bool isInSpacingTextLength : 1 { false };

        bool hasMetrics() const { return metricsIndex != notFound32; }
    };

    struct TextContent {
        CheckedRef<const Layout::InlineTextBox> inlineTextBox;
        unsigned start { 0 };
    };

    struct TextBox {
        size_t displayBoxIndex { 0 };
        CheckedRef<const Layout::InlineTextBox> inlineTextBox;
        // Start of the text content of the layout box in m_characters.
        unsigned contentStart { 0 };
        unsigned start { 0 };
        unsigned length { 0 };
        float baselineShift { 0 };
        unsigned textPathIndex { notFound32 };
    };

    struct TextContentElement {
        CheckedRef<const Layout::ElementBox> elementBox;
        RefPtr<const SVGTextContentElement> element;
        unsigned start { 0 };
        unsigned end { 0 };
        bool isTextPath { false };
    };

    struct TextPath {
        CheckedRef<const Layout::ElementBox> elementBox;
        unsigned elementIndex { 0 };
        // lengthAdjust="spacingAndGlyphs" along the path.
        float scaling { 1 };
    };

    void collectTextContent();
    void collectTextBoxes();
    void computeCSSPositions();
    void resolveCharacterPositioning();
    void adjustRelativePositions();
    void applyTextLength();
    void adjustAbsolutePositions();
    void applyAnchoring();
    void positionOnPaths();
    void buildFragments();

    float anchorShift(std::span<const unsigned> chunk) const;
    std::pair<float, float> extent(std::span<const unsigned> characterIndexes) const;
    float glyphLength(const Character&) const;
    const SVGTextMetrics& metrics(const Character&) const;
    unsigned typographicCharacterCount(std::span<const unsigned> characterIndexes) const;

    const CheckedRef<const Layout::ElementBox> m_root;
    const InlineDisplay::Boxes& m_displayBoxes;
    Vector<Vector<SVGTextFragment>>& m_fragmentsForBoxes;
    const bool m_isVertical { false };

    // Every character of the text content, in content order.
    // Sized for a typical label.
    Vector<Character, 16> m_characters;
    Vector<TextContent, 2> m_textContents;
    Vector<TextBox, 2> m_textBoxes;
    // Text content elements in tree order.
    Vector<TextContentElement, 2> m_elements;
    // m_elements indexes, children before parents.
    Vector<unsigned, 2> m_elementsInPostOrder;
    Vector<TextPath> m_textPaths;
    // Addressable characters in the order glyphs advance.
    Vector<unsigned, 16> m_displayOrder;

    Vector<float, 16> m_resolvedX;
    Vector<float, 16> m_resolvedY;
    Vector<float, 16> m_resolvedDX;
    Vector<float, 16> m_resolvedDY;

};

} // namespace WebCore
