/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG)
#include "SVGTextLayoutEngine.h"

#include "RenderSVGInlineText.h"
#include "RenderSVGTextPath.h"
#include "SVGElement.h"
#include "SVGInlineTextBox.h"
#include "SVGTextLayoutEngineBaseline.h"
#include "SVGTextLayoutEngineSpacing.h"

// Set to a value > 0 to dump the text fragments
#define DUMP_TEXT_FRAGMENTS 0

namespace WebCore {

SVGTextLayoutEngine::SVGTextLayoutEngine()
    : m_x(0)
    , m_y(0)
    , m_dx(0)
    , m_dy(0)
    , m_isVerticalText(false)
    , m_inPathLayout(false)
    , m_textPathLength(0)
    , m_textPathCurrentOffset(0)
    , m_textPathSpacing(0)
    , m_textPathScaling(1)
{
}

void SVGTextLayoutEngine::updateCharacerPositionIfNeeded(float& x, float& y)
{
    if (m_inPathLayout)
        return;

    // Replace characters x/y position, with the current text position plus any
    // relative adjustments, if it doesn't specify an absolute position itself.
    if (x == SVGTextLayoutAttributes::emptyValue()) 
        x = m_x + m_dx;

    if (y == SVGTextLayoutAttributes::emptyValue())
        y = m_y + m_dy;

    m_dx = 0;
    m_dy = 0;
}

void SVGTextLayoutEngine::updateCurrentTextPosition(float x, float y, float glyphAdvance)
{
    // Update current text position after processing the character.
    if (m_isVerticalText) {
        m_x = x;
        m_y = y + glyphAdvance;
    } else {
        m_x = x + glyphAdvance;
        m_y = y;
    }
}

void SVGTextLayoutEngine::updateRelativePositionAdjustmentsIfNeeded(Vector<float>& dxValues, Vector<float>& dyValues, unsigned positionListOffset)
{
    // Update relative positioning information.
    if (dxValues.isEmpty() && dyValues.isEmpty())
        return;

    float dx = 0;
    if (!dxValues.isEmpty()) {
        float& dxCurrent = dxValues.at(positionListOffset);
        if (dxCurrent != SVGTextLayoutAttributes::emptyValue())
            dx = dxCurrent;
    }

    float dy = 0;
    if (!dyValues.isEmpty()) {
        float& dyCurrent = dyValues.at(positionListOffset);
        if (dyCurrent != SVGTextLayoutAttributes::emptyValue())
            dy = dyCurrent;
    }

    if (m_inPathLayout) {
        if (m_isVerticalText) {
            m_dx += dx;
            m_dy = dy;
        } else {
            m_dx = dx;
            m_dy += dy;
        }

        return;
    }

    m_dx = dx;
    m_dy = dy;
}

void SVGTextLayoutEngine::recordTextFragment(SVGInlineTextBox* textBox, RenderSVGInlineText* text, unsigned positionListOffset, const SVGTextMetrics& lastCharacterMetrics)
{
    ASSERT(!m_currentTextFragment.length);

    // Figure out length of fragment.
    m_currentTextFragment.length = positionListOffset - m_currentTextFragment.positionListOffset;

    // Figure out fragment metrics.
    if (m_currentTextFragment.length == 1) {
        // Fast path, can rely on already computed per-character metrics.
        m_currentTextFragment.width = lastCharacterMetrics.width();
        m_currentTextFragment.height = lastCharacterMetrics.height();
    } else {
        // Need to measure the whole range (range metrics != sum of character metrics)
        SVGTextMetrics metrics = SVGTextMetrics::measureCharacterRange(text, m_currentTextFragment.positionListOffset, m_currentTextFragment.length);
        m_currentTextFragment.width = metrics.width();
        m_currentTextFragment.height = metrics.height();
    }

    textBox->textFragments().append(m_currentTextFragment);
    m_currentTextFragment = SVGTextFragment();
}

bool SVGTextLayoutEngine::parentDefinesTextLength(RenderObject* parent) const
{
    RenderObject* currentParent = parent;
    while (currentParent) {
        SVGTextContentElement* textContentElement = SVGTextContentElement::elementFromRenderer(currentParent);
        if (textContentElement) {
            SVGTextContentElement::SVGLengthAdjustType lengthAdjust = static_cast<SVGTextContentElement::SVGLengthAdjustType>(textContentElement->lengthAdjust());
            if (lengthAdjust == SVGTextContentElement::LENGTHADJUST_SPACING && textContentElement->textLength().value(textContentElement) > 0)
                return true;
        }

        if (currentParent->isSVGText())
            return false;

        currentParent = currentParent->parent();
    }

    ASSERT_NOT_REACHED();
    return false;
}

void SVGTextLayoutEngine::beginTextPathLayout(RenderObject* object, SVGTextLayoutEngine& lineLayout)
{
    ASSERT(object);

    m_inPathLayout = true;
    RenderSVGTextPath* textPath = toRenderSVGTextPath(object);

    m_textPath = textPath->layoutPath();
    m_textPathStartOffset = textPath->startOffset();
    m_textPathLength = m_textPath.length();
    if (m_textPathStartOffset > 0 && m_textPathStartOffset <= 1)
        m_textPathStartOffset *= m_textPathLength;

    float totalLength = 0;
    unsigned totalCharacters = 0;

    lineLayout.m_chunkLayoutBuilder.buildTextChunks(lineLayout.m_lineLayoutBoxes);
    const Vector<SVGTextChunk>& textChunks = lineLayout.m_chunkLayoutBuilder.textChunks();

    unsigned size = textChunks.size();
    for (unsigned i = 0; i < size; ++i) {
        const SVGTextChunk& chunk = textChunks.at(i);

        float length = 0;
        unsigned characters = 0;
        chunk.calculateLength(length, characters);

        // Handle text-anchor as additional start offset for text paths.
        m_textPathStartOffset += chunk.calculateTextAnchorShift(length);

        totalLength += length;
        totalCharacters += characters;
    }

    m_textPathCurrentOffset = m_textPathStartOffset;

    // Eventually handle textLength adjustments.
    SVGTextContentElement::SVGLengthAdjustType lengthAdjust = SVGTextContentElement::LENGTHADJUST_UNKNOWN;
    float desiredTextLength = 0;

    if (SVGTextContentElement* textContentElement = SVGTextContentElement::elementFromRenderer(textPath)) {
        lengthAdjust = static_cast<SVGTextContentElement::SVGLengthAdjustType>(textContentElement->lengthAdjust());
        desiredTextLength = textContentElement->textLength().value(textContentElement);
    }

    if (!desiredTextLength)
        return;

    if (lengthAdjust == SVGTextContentElement::LENGTHADJUST_SPACING)
        m_textPathSpacing = (desiredTextLength - totalLength) / totalCharacters;
    else
        m_textPathScaling = desiredTextLength / totalLength;
}

void SVGTextLayoutEngine::endTextPathLayout()
{
    m_inPathLayout = false;
    m_textPath = Path();
    m_textPathLength = 0;
    m_textPathStartOffset = 0;
    m_textPathCurrentOffset = 0;
    m_textPathSpacing = 0;
    m_textPathScaling = 1;
}

void SVGTextLayoutEngine::layoutInlineTextBox(SVGInlineTextBox* textBox)
{
    ASSERT(textBox);

    RenderSVGInlineText* text = toRenderSVGInlineText(textBox->textRenderer());
    ASSERT(text);
    ASSERT(text->parent());
    ASSERT(text->parent()->node());
    ASSERT(text->parent()->node()->isSVGElement());

    const RenderStyle* style = text->style();
    ASSERT(style);

    textBox->clearTextFragments();
    m_isVerticalText = style->svgStyle()->isVerticalWritingMode();
    layoutTextOnLineOrPath(textBox, text, style);

    if (m_inPathLayout) {
        m_pathLayoutBoxes.append(textBox);
        return;
    }

    m_lineLayoutBoxes.append(textBox);
}

void SVGTextLayoutEngine::finishLayout()
{
    // After all text fragments are stored in their correpsonding SVGInlineTextBoxes, we can layout individual text chunks.
    // Chunk layouting is only performed for line layout boxes, not for path layout, where it has already been done.
    m_chunkLayoutBuilder.layoutTextChunks(m_lineLayoutBoxes);

    // Finalize transform matrices, after the chunk layout corrections have been applied, and all fragment x/y positions are finalized.
    if (!m_lineLayoutBoxes.isEmpty()) {
#if DUMP_TEXT_FRAGMENTS > 0
        fprintf(stderr, "Line layout: ");
#endif

        finalizeTransformMatrices(m_lineLayoutBoxes);
    }

    if (!m_pathLayoutBoxes.isEmpty()) {
#if DUMP_TEXT_FRAGMENTS > 0
        fprintf(stderr, "Path layout: ");
#endif
        finalizeTransformMatrices(m_pathLayoutBoxes);
    }
}

void SVGTextLayoutEngine::finalizeTransformMatrices(Vector<SVGInlineTextBox*>& boxes)
{
    unsigned boxCount = boxes.size();

#if DUMP_TEXT_FRAGMENTS > 0
    fprintf(stderr, "Dumping all text fragments in text sub tree, %i boxes\n", boxCount);

    for (unsigned boxPosition = 0; boxPosition < boxCount; ++boxPosition) {
        SVGInlineTextBox* textBox = boxes.at(boxPosition);
        Vector<SVGTextFragment>& fragments = textBox->textFragments();
        fprintf(stderr, "-> Box %i: Dumping text fragments for SVGInlineTextBox, textBox=%p, textRenderer=%p\n", boxPosition, textBox, textBox->textRenderer());
        fprintf(stderr, "        textBox properties, start=%i, len=%i\n", textBox->start(), textBox->len());
        fprintf(stderr, "   textRenderer properties, textLength=%i\n", textBox->textRenderer()->textLength());

        const UChar* characters = textBox->textRenderer()->characters();

        unsigned fragmentCount = fragments.size();
        for (unsigned i = 0; i < fragmentCount; ++i) {
            SVGTextFragment& fragment = fragments.at(i);
            String fragmentString(characters + fragment.positionListOffset, fragment.length);
            fprintf(stderr, "    -> Fragment %i, x=%lf, y=%lf, width=%lf, height=%lf, positionListOffset=%i, length=%i, characters='%s'\n"
                          , i, fragment.x, fragment.y, fragment.width, fragment.height, fragment.positionListOffset, fragment.length, fragmentString.utf8().data());
        }
    }
#endif


    if (!boxCount)
        return;

    AffineTransform textBoxTransformation;
    for (unsigned boxPosition = 0; boxPosition < boxCount; ++boxPosition) {
        SVGInlineTextBox* textBox = boxes.at(boxPosition);
        Vector<SVGTextFragment>& fragments = textBox->textFragments();

        unsigned fragmentCount = fragments.size();
        for (unsigned i = 0; i < fragmentCount; ++i) {
            SVGTextFragment& fragment = fragments.at(i);
            AffineTransform& transform = fragment.transform;
            if (!transform.isIdentity()) {
                transform.translateRight(fragment.x, fragment.y);
                transform.translate(-fragment.x, -fragment.y);
            }

            m_chunkLayoutBuilder.transformationForTextBox(textBox, textBoxTransformation);
            if (textBoxTransformation.isIdentity())
                continue;

            if (transform.isIdentity())
                transform = textBoxTransformation;
            else
                transform.multiply(textBoxTransformation);
        }
    }

    boxes.clear();
}

void SVGTextLayoutEngine::layoutTextOnLineOrPath(SVGInlineTextBox* textBox, RenderSVGInlineText* text, const RenderStyle* style)
{
    SVGElement* lengthContext = static_cast<SVGElement*>(text->parent()->node());
    
    RenderObject* textParent = text->parent();
    bool definesTextLength = textParent ? parentDefinesTextLength(textParent) : false;

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    SVGTextLayoutAttributes& attributes = text->layoutAttributes();
    Vector<float>& xValues = attributes.xValues();
    Vector<float>& yValues = attributes.yValues();
    Vector<float>& dxValues = attributes.dxValues();
    Vector<float>& dyValues = attributes.dyValues();
    Vector<float>& rotateValues = attributes.rotateValues();
    Vector<SVGTextMetrics>& textMetricsValues = attributes.textMetricsValues();

    unsigned boxStart = textBox->start();
    unsigned boxLength = textBox->len();
    unsigned textMetricsSize = textMetricsValues.size();
    ASSERT(textMetricsSize <= xValues.size());
    ASSERT(textMetricsSize <= yValues.size());
    ASSERT(xValues.size() == yValues.size());

    if (boxLength > textMetricsSize)
        textMetricsSize = boxLength; 

    unsigned positionListOffset = 0;
    unsigned metricsListOffset = 0;
    const UChar* characters = text->characters();

    const Font& font = style->font();
    SVGTextLayoutEngineSpacing spacingLayout(font);
    SVGTextLayoutEngineBaseline baselineLayout(font);

    bool didStartTextFragment = false;
    bool applySpacingToNextCharacter = false;

    float lastAngle = 0;
    float baselineShift = baselineLayout.calculateBaselineShift(svgStyle, lengthContext);
    baselineShift -= baselineLayout.calculateAlignmentBaselineShift(m_isVerticalText, text);

    // Main layout algorithm.
    unsigned positionListSize = xValues.size();
    for (; metricsListOffset < textMetricsSize && positionListOffset < positionListSize; ++metricsListOffset) {
        SVGTextMetrics& metrics = textMetricsValues.at(metricsListOffset);
        // Advance to text box start location.
        if (positionListOffset < boxStart) {
            positionListOffset += metrics.length();
            continue;
        }

        // Stop if we've finished processing this text box.
        if (positionListOffset >= boxStart + boxLength)
            break;
 
        float x = xValues.at(positionListOffset);
        float y = yValues.at(positionListOffset);

        // When we've advanced to the box start offset, determine using the original x/y values,
        // wheter this character starts a new text chunk, before doing any further processing.
        if (positionListOffset == boxStart)
            textBox->setStartsNewTextChunk(text->characterStartsNewTextChunk(boxStart));

        if (metrics == SVGTextMetrics::emptyMetrics()) {
            positionListOffset += metrics.length();
            continue;
        }

        const UChar* currentCharacter = characters + positionListOffset;
        float angle = 0;
        if (!rotateValues.isEmpty()) {
            float newAngle = rotateValues.at(positionListOffset);
            if (newAngle != SVGTextLayoutAttributes::emptyValue())
                angle = newAngle;
        }

        // Calculate glyph orientation angle.
        float orientationAngle = baselineLayout.calculateGlyphOrientationAngle(m_isVerticalText, svgStyle, *currentCharacter);

        // Calculate glyph advance & x/y orientation shifts.
        float xOrientationShift = 0;
        float yOrientationShift = 0;
        float glyphAdvance = baselineLayout.calculateGlyphAdvanceAndOrientation(m_isVerticalText, metrics, orientationAngle, xOrientationShift, yOrientationShift);

        // Assign current text position to x/y values, if needed.
        updateCharacerPositionIfNeeded(x, y);

        // Apply dx/dy value adjustments to current text position, if needed.
        updateRelativePositionAdjustmentsIfNeeded(dxValues, dyValues, positionListOffset);

        // Calculate SVG Fonts kerning, if needed.
        float kerning = spacingLayout.calculateSVGKerning(m_isVerticalText, metrics.glyph());

        // Calculate CSS 'kerning', 'letter-spacing' and 'word-spacing' for next character, if needed.
        float spacing = spacingLayout.calculateCSSKerningAndSpacing(svgStyle, lengthContext, currentCharacter);

        float textPathOffset = 0;
        if (m_inPathLayout) {
            float scaledGlyphAdvance = glyphAdvance * m_textPathScaling;
            if (m_isVerticalText) {
                // If there's an absolute y position available, it marks the beginning of a new position along the path.
                if (y != SVGTextLayoutAttributes::emptyValue())
                    m_textPathCurrentOffset = y + m_textPathStartOffset;

                m_textPathCurrentOffset += m_dy - kerning;
                m_dy = 0;

                // Apply dx/dy correction and setup translations that move to the glyph midpoint.
                xOrientationShift += m_dx + baselineShift;
                yOrientationShift -= scaledGlyphAdvance / 2;
            } else {
                // If there's an absolute x position available, it marks the beginning of a new position along the path.
                if (x != SVGTextLayoutAttributes::emptyValue())
                    m_textPathCurrentOffset = x + m_textPathStartOffset;

                m_textPathCurrentOffset += m_dx - kerning;
                m_dx = 0;

                // Apply dx/dy correction and setup translations that move to the glyph midpoint.
                xOrientationShift -= scaledGlyphAdvance / 2;
                yOrientationShift += m_dy - baselineShift;
            }

            // Calculate current offset along path.
            textPathOffset = m_textPathCurrentOffset + scaledGlyphAdvance / 2;

            // Move to next character.
            m_textPathCurrentOffset += scaledGlyphAdvance + m_textPathSpacing + spacing * m_textPathScaling;

            // Skip character, if we're before the path.
            if (textPathOffset < 0) {
                positionListOffset += metrics.length();
                continue;
            }

            // Stop processing, if the next character lies behind the path.
            if (textPathOffset > m_textPathLength)
                break;

            bool ok = false;
            FloatPoint point = m_textPath.pointAtLength(textPathOffset, ok);
            ASSERT(ok);

            x = point.x();
            y = point.y();
            angle = m_textPath.normalAngleAtLength(textPathOffset, ok);
            ASSERT(ok);

            // For vertical text on path, the actual angle has to be rotated 90 degrees anti-clockwise, not the orientation angle!
            if (m_isVerticalText)
                angle -= 90;
        } else {
            // Apply all previously calculated shift values.
            if (m_isVerticalText) {
                x += baselineShift;
                y -= kerning;
            } else {
                x -= kerning;
                y -= baselineShift;
            }

            x += m_dx;
            y += m_dy;
        }

        // Determine wheter we have to start a new fragment.
        bool shouldStartNewFragment = false;

        if (m_dx || m_dy)
            shouldStartNewFragment = true;

        if (!shouldStartNewFragment && (m_isVerticalText || m_inPathLayout))
            shouldStartNewFragment = true;

        if (!shouldStartNewFragment && (angle || angle != lastAngle || orientationAngle))
            shouldStartNewFragment = true;

        if (!shouldStartNewFragment && (kerning || applySpacingToNextCharacter || definesTextLength))
            shouldStartNewFragment = true;

        // If we already started a fragment, close it now.
        if (didStartTextFragment && shouldStartNewFragment) {
            applySpacingToNextCharacter = false;
            recordTextFragment(textBox, text, positionListOffset, textMetricsValues.at(metricsListOffset - 1));
        }

        // Eventually start a new fragment, if not yet done.
        if (!didStartTextFragment || shouldStartNewFragment) {
            ASSERT(!m_currentTextFragment.positionListOffset);
            ASSERT(!m_currentTextFragment.length);

            didStartTextFragment = true;
            m_currentTextFragment.positionListOffset = positionListOffset;
            m_currentTextFragment.x = x;
            m_currentTextFragment.y = y;

            // Build fragment transformation.
            if (angle)
                m_currentTextFragment.transform.rotate(angle);

            if (xOrientationShift || yOrientationShift)
                m_currentTextFragment.transform.translate(xOrientationShift, yOrientationShift);

            if (orientationAngle)
                m_currentTextFragment.transform.rotate(orientationAngle);

            if (m_inPathLayout && m_textPathScaling != 1) {
                if (m_isVerticalText)
                    m_currentTextFragment.transform.scaleNonUniform(1, m_textPathScaling);
                else
                    m_currentTextFragment.transform.scaleNonUniform(m_textPathScaling, 1);
            }
        }

        // Update current text position, after processing of the current character finished.
        if (m_inPathLayout)
            updateCurrentTextPosition(x, y, glyphAdvance);
        else {
            // Apply CSS 'kerning', 'letter-spacing' and 'word-spacing' to next character, if needed.
            if (spacing)
                applySpacingToNextCharacter = true;

            float xNew = x - m_dx;
            float yNew = y - m_dy;

            if (m_isVerticalText)
                xNew -= baselineShift;
            else
                yNew += baselineShift;

            updateCurrentTextPosition(xNew, yNew, glyphAdvance + spacing);
        }

        positionListOffset += metrics.length();
        lastAngle = angle;
    }

    if (!didStartTextFragment)
        return;

    // Close last open fragment, if needed.
    recordTextFragment(textBox, text, positionListOffset, textMetricsValues.at(metricsListOffset - 1));
}

}

#endif // ENABLE(SVG)
