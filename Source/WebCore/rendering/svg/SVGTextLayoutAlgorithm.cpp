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

#include "config.h"
#include "SVGTextLayoutAlgorithm.h"

#include "ComplexTextController.h"
#include "FontCascadeCache.h"
#include "FontCascadeInlines.h"
#include "InlineDisplayBoxInlines.h"
#include "LayoutElementBox.h"
#include "LayoutInlineTextBox.h"
#include "PathArcLengthMapper.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGTextPath.h"
#include "SVGGeometryElement.h"
#include "SVGLengthContext.h"
#include "SVGNames.h"
#include "SVGTextFragment.h"
#include "SVGTextLayoutAttributes.h"
#include "SVGTextLayoutEngineBaseline.h"
#include "SVGTextLayoutEngineSpacing.h"
#include "SVGTextMetrics.h"
#include "SVGTextPositioningElement.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "TextUtil.h"
#include "WidthIterator.h"
#include <unicode/ubrk.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

// SVG specific inputs live on the renderers and the DOM.
static RenderSVGInlineText* LEGACY_NODELETE svgInlineText(const Layout::InlineTextBox& inlineTextBox)
{
    return dynamicDowncast<RenderSVGInlineText>(inlineTextBox.rendererForIntegration());
}

static const SVGTextContentElement* LEGACY_NODELETE textContentElement(const Layout::ElementBox& elementBox)
{
    // Anonymous inline boxes, like the ones for display: contents, have no element.
    auto* renderer = elementBox.rendererForIntegration();
    if (!renderer || renderer->isAnonymous())
        return nullptr;
    return SVGTextContentElement::elementFromRenderer(renderer);
}

static const RenderSVGTextPath* LEGACY_NODELETE svgTextPath(const Layout::ElementBox& elementBox)
{
    return dynamicDowncast<RenderSVGTextPath>(elementBox.rendererForIntegration());
}

static const Vector<SVGTextMetrics>& LEGACY_NODELETE textMetrics(const Layout::InlineTextBox& inlineTextBox)
{
    return svgInlineText(inlineTextBox)->layoutAttributes()->textMetricsValues();
}

// Text boxes have a style with the non-inherited properties reset. The SVG text properties like alignment-baseline and
// baseline-shift come from the parent.
static const Style::ComputedStyle& LEGACY_NODELETE textStyle(const Layout::InlineTextBox& inlineTextBox)
{
    return inlineTextBox.parent().style();
}

static bool LEGACY_NODELETE isUnspecified(float value)
{
    return std::isnan(value);
}

static float LEGACY_NODELETE valueAt(std::span<const float> values, unsigned index)
{
    return values.empty() ? SVGTextLayoutAttributes::emptyValue() : values[index];
}

static float LEGACY_NODELETE valueOrZeroAt(std::span<const float> values, unsigned index)
{
    auto value = valueAt(values, index);
    return isUnspecified(value) ? 0 : value;
}

struct TextLength {
    float length { 0 };
    SVGLengthAdjustType lengthAdjust { SVGLengthAdjustUnknown };
};

static std::optional<TextLength> textLength(const SVGTextContentElement& element)
{
    auto lengthContext = SVGLengthContext { &element };
    auto length = element.specifiedTextLength().value(lengthContext);
    if (length <= 0)
        return { };
    auto lengthAdjust = element.lengthAdjust();
    if (lengthAdjust != SVGLengthAdjustSpacing && lengthAdjust != SVGLengthAdjustSpacingAndGlyphs)
        return { };
    return TextLength { length, lengthAdjust };
}

static bool shouldUseComplexTextController(const FontCascade& scaledFont, const TextRun& run)
{
    auto codePath = scaledFont.codePath(run);
#if PLATFORM(GTK) || PLATFORM(WPE)
    if (codePath != FontCascade::CodePath::Complex && scaledFont.shouldUseComplexTextControllerForSimpleText())
        return true;
#endif
    return codePath == FontCascade::CodePath::Complex;
}

// One metrics entry per measured unit (a character or a surrogate pair), covering the whole text. Measured with the
// scaled font and kept until the text or the font changes.
static void ensureTextMetrics(RenderSVGInlineText& text)
{
    if (text.hasValidTextMetrics())
        return;
    text.setHasValidTextMetrics(true);

    auto& metrics = text.layoutAttributes()->textMetricsValues();
    metrics.clear();

    auto run = SVGTextMetrics::constructTextRun(text);
    auto length = run.length();
    if (!length)
        return;

    CheckedRef scaledFont = text.scaledFont();
    auto scalingFactor = text.scalingFactor();
    ASSERT(scalingFactor);
    auto height = scaledFont->metricsOfPrimaryFont().height() / scalingFactor;
    metrics.reserveInitialCapacity(length);

    auto appendMetrics = [&](unsigned metricsLength, float width) {
        metrics.append(SVGTextMetrics(metricsLength, width / scalingFactor, height));
    };

    if (shouldUseComplexTextController(scaledFont, run)) {
        FontCascadeCache::forCurrentThread().invalidate();
        auto controller = ComplexTextController { scaledFont, run, true };
        auto totalWidth = 0.f;
        for (unsigned position = 0; position < length;) {
            unsigned metricsLength = U16_IS_LEAD(run[position]) && position + 1 < length && U16_IS_TRAIL(run[position + 1]) ? 2 : 1;
            controller.advance(position + metricsLength, nullptr);
            auto widthSoFar = controller.runWidthSoFar();
            appendMetrics(metricsLength, widthSoFar - totalWidth);
            totalWidth = widthSoFar;
            position += metricsLength;
        }
        return;
    }

    auto canUseSimplifiedTextMeasuring = [&] {
        if (run.direction() != TextDirection::LTR)
            return false;
        if (auto cachedValue = text.canUseSimplifiedTextMeasuring())
            return *cachedValue;
        auto value = Layout::TextUtil::canUseSimplifiedTextMeasuring(run.text(), scaledFont, text.style().collapseWhiteSpace(), nullptr);
        text.setCanUseSimplifiedTextMeasuring(value);
        return value;
    }();

    if (canUseSimplifiedTextMeasuring) {
        auto view = run.text();
        for (unsigned position = 0; position < length; ++position)
            appendMetrics(1, scaledFont->widthForTextUsingSimplifiedMeasuring(view.substring(position, 1), TextDirection::LTR));
        return;
    }

    auto iterator = WidthIterator { scaledFont, run };
    auto totalWidth = 0.f;
    for (unsigned position = 0; position < length;) {
        GlyphBuffer glyphBuffer;
        auto before = iterator.currentCharacterIndex();
        iterator.advance(position + 1, glyphBuffer);
        auto after = iterator.currentCharacterIndex();
        unsigned metricsLength = after > before ? after - before : 1;
        auto widthSoFar = iterator.runWidthSoFar();
        appendMetrics(metricsLength, widthSoFar - totalWidth);
        totalWidth = widthSoFar;
        position += metricsLength;
    }
}

SVGTextLayoutAlgorithm::SVGTextLayoutAlgorithm(const Layout::ElementBox& root, const InlineDisplay::Boxes& displayBoxes, Vector<Vector<SVGTextFragment>>& fragmentsForBoxes)
    : m_root(root)
    , m_displayBoxes(displayBoxes)
    , m_fragmentsForBoxes(fragmentsForBoxes)
    , m_isVertical(root.writingMode().isVertical())
{
}

SVGTextLayoutAlgorithm::~SVGTextLayoutAlgorithm() = default;

const SVGTextMetrics& SVGTextLayoutAlgorithm::metrics(const Character& character) const
{
    ASSERT(character.hasMetrics());
    return textMetrics(m_textBoxes[character.textBoxIndex].inlineTextBox)[character.metricsIndex];
}

float SVGTextLayoutAlgorithm::glyphLength(const Character& character) const
{
    auto& characterMetrics = metrics(character);
    return m_isVertical ? characterMetrics.height() : characterMetrics.width();
}

unsigned SVGTextLayoutAlgorithm::typographicCharacterCount(std::span<const unsigned> characterIndexes) const
{
    unsigned count = 0;
    for (auto index : characterIndexes) {
        if (m_characters[index].hasMetrics())
            ++count;
    }
    return count;
}

// The passes follow the steps of the SVG 2 text layout algorithm.
// https://w3c.github.io/svgwg/svg2-draft/text.html#TextLayoutAlgorithm
void SVGTextLayoutAlgorithm::layout()
{
    collectTextContent();
    if (m_characters.isEmpty())
        return;

    collectTextBoxes();
    if (m_displayOrder.isEmpty())
        return;

    computeCSSPositions();
    resolveCharacterPositioning();
    adjustRelativePositions();
    applyTextLength();
    adjustAbsolutePositions();
    applyAnchoring();
    positionOnPaths();
    buildFragments();
}

void SVGTextLayoutAlgorithm::collectTextContent()
{
    unsigned contentLength = 0;

    auto collect = [&](auto& collect, const Layout::ElementBox& elementBox) -> void {
        auto elementIndex = [&] -> std::optional<unsigned> {
            RefPtr element = textContentElement(elementBox);
            if (!element)
                return { };
            m_elements.append({ elementBox, element, contentLength, contentLength, !!svgTextPath(elementBox) });
            return m_elements.size() - 1;
        }();

        for (CheckedPtr child = elementBox.firstChild(); child; child = child->nextSibling()) {
            if (CheckedPtr inlineTextBox = dynamicDowncast<Layout::InlineTextBox>(*child)) {
                CheckedPtr text = svgInlineText(*inlineTextBox);
                if (!text)
                    continue;
                ensureTextMetrics(*text);
                m_textContents.append({ *inlineTextBox, contentLength });
                contentLength += inlineTextBox->content().length();
                continue;
            }
            if (CheckedPtr childElementBox = dynamicDowncast<Layout::ElementBox>(*child); childElementBox && childElementBox->isInlineBox())
                collect(collect, *childElementBox);
        }

        if (elementIndex) {
            m_elements[*elementIndex].end = contentLength;
            m_elementsInPostOrder.append(*elementIndex);
        }
    };
    collect(collect, m_root);

    m_characters.resize(contentLength);

    for (auto& textContent : m_textContents) {
        auto& content = textContent.inlineTextBox->content();
        auto& metrics = textMetrics(textContent.inlineTextBox);
        // "Set middle to true if the character at index i is the second or later character that corresponds to a typographic character."
        // https://w3c.github.io/svgwg/svg2-draft/text.html#TermTypographicCharacterUnit
        // Latin-1 has no combining marks, so every character starts a grapheme cluster except a line feed after a carriage return.
        auto clusterIterator = std::optional<NonSharedCharacterBreakIterator> { };
        if (!content.is8Bit())
            clusterIterator.emplace(StringView { content });
        auto isClusterStart = [&](unsigned position) -> bool {
            if (clusterIterator)
                return ubrk_isBoundary(*clusterIterator, position);
            return !position || content[position] != newlineCharacter || content[position - 1] != carriageReturn;
        };
        unsigned position = 0;
        for (size_t metricsIndex = 0; metricsIndex < metrics.size() && position < content.length(); ++metricsIndex) {
            auto& character = m_characters[textContent.start + position];
            character.metricsIndex = metricsIndex;
            character.isClusterStart = isClusterStart(position);
            position += metrics[metricsIndex].length();
        }
    }
}

void SVGTextLayoutAlgorithm::collectTextBoxes()
{
    // Text boxes are mostly in content order. Search forward from the previous match.
    size_t textContentCursor = 0;
    auto textContent = [&](const Layout::InlineTextBox& inlineTextBox) -> TextContent* {
        for (size_t i = 0; i < m_textContents.size(); ++i) {
            auto index = (textContentCursor + i) % m_textContents.size();
            if (m_textContents[index].inlineTextBox.ptr() == &inlineTextBox) {
                textContentCursor = index;
                return &m_textContents[index];
            }
        }
        return nullptr;
    };

    auto textPathElementIndex = [&](const Layout::ElementBox& elementBox) -> unsigned {
        for (unsigned index = 0; index < m_elements.size(); ++index) {
            if (m_elements[index].elementBox.ptr() == &elementBox)
                return index;
        }
        ASSERT_NOT_REACHED();
        return 0;
    };

    // Returns the index past the content of the inline box at inlineBoxIndex.
    auto collect = [&](auto& collect, size_t inlineBoxIndex, unsigned textPathIndex) -> size_t {
        CheckedRef inlineBox = m_displayBoxes[inlineBoxIndex].layoutBox();
        auto isWithinInlineBox = [&](const InlineDisplay::Box& displayBox) {
            for (auto* ancestor = &displayBox.layoutBox().parent();; ancestor = &ancestor->parent()) {
                if (ancestor == inlineBox.ptr())
                    return true;
                if (!ancestor->isInlineBox())
                    return false;
            }
        };

        auto index = inlineBoxIndex + 1;
        while (index < m_displayBoxes.size() && isWithinInlineBox(m_displayBoxes[index])) {
            auto& displayBox = m_displayBoxes[index];
            CheckedRef layoutBox = displayBox.layoutBox();

            if (displayBox.isText()) {
                if (auto* textContentForBox = textContent(downcast<Layout::InlineTextBox>(layoutBox))) {
                    auto& textContent = *textContentForBox;
                    auto start = textContent.start + static_cast<unsigned>(displayBox.text().start());
                    auto length = static_cast<unsigned>(displayBox.text().length());

                    CheckedRef style = textStyle(textContent.inlineTextBox);
                    auto baselineLayout = SVGTextLayoutEngineBaseline { protect(style->fontCascade()) };
                    auto baselineShift = baselineLayout.calculateBaselineShift(style) - baselineLayout.calculateAlignmentBaselineShift(m_isVertical, style);

                    // Characters outside the display boxes are not addressable:
                    // "Set addressable to false if the character at index i was: [...] discarded during layout due to being a collapsed
                    // white space character, a soft hyphen character, or a bidi control character; or [...] trimmed from the start or
                    // end of a line."
                    auto textBoxIndex = m_textBoxes.size();
                    m_textBoxes.append({ index, textContent.inlineTextBox, textContent.start, start, length, baselineShift, textPathIndex });
                    for (auto position = start; position < start + length; ++position) {
                        auto& character = m_characters[position];
                        character.textBoxIndex = textBoxIndex;
                        character.isAddressable = true;
                        character.isOnPath = textPathIndex != notFound32;
                        m_displayOrder.append(position);
                    }
                }
                ++index;
                continue;
            }

            CheckedRef elementBox = downcast<Layout::ElementBox>(layoutBox);
            auto contentTextPathIndex = textPathIndex;
            if (svgTextPath(elementBox) && textContentElement(elementBox)) {
                // FIXME: Bidi reordering may split a text path into several inline boxes, each restarting at the start offset.
                m_textPaths.append({ elementBox, textPathElementIndex(elementBox) });
                contentTextPathIndex = m_textPaths.size() - 1;
            }
            index = collect(collect, index, contentTextPathIndex);
        }
        return index;
    };
    auto endIndex = collect(collect, 0, notFound32);
    ASSERT_UNUSED(endIndex, endIndex == m_displayBoxes.size());
}

// "The CSS renderer returns a set of typographic characters with their positions resulting from laying out the text as if the text
// were absolutely positioned."
// Glyphs advance from 0 in display order. The display boxes come from the line layout, which never wraps SVG text.
void SVGTextLayoutAlgorithm::computeCSSPositions()
{
    auto position = 0.f;
    for (auto& textBox : m_textBoxes) {
        CheckedRef style = textStyle(textBox.inlineTextBox);
        auto& content = textBox.inlineTextBox->content();
        auto baselineLayout = SVGTextLayoutEngineBaseline { protect(style->fontCascade()) };
        auto spacingLayout = SVGTextLayoutEngineSpacing { protect(style->fontCascade()) };

        for (auto index = textBox.start; index < textBox.start + textBox.length; ++index) {
            auto& character = m_characters[index];
            if (m_isVertical)
                character.y = position;
            else
                character.x = position;

            if (!character.hasMetrics())
                continue;

            auto offset = index - textBox.contentStart;
            char32_t codePoint = content[offset];
            if (U16_IS_LEAD(codePoint) && offset + 1 < content.length() && U16_IS_TRAIL(content[offset + 1]))
                codePoint = U16_GET_SUPPLEMENTARY(codePoint, content[offset + 1]);

            character.orientationAngle = baselineLayout.calculateGlyphOrientationAngle(m_isVertical, style, codePoint);
            character.advance = baselineLayout.calculateGlyphAdvanceAndOrientation(m_isVertical, metrics(character), character.orientationAngle, character.xOrientationShift, character.yOrientationShift);
            character.spacing = spacingLayout.calculateCSSSpacing(codePoint);
            position += character.advance + character.spacing;
        }
    }
}

// "Position adjustments (e.g. values in a 'x' attribute) specified by a node apply to all characters in that node including
// characters in the node's descendants. Adjustments specified in descendant nodes, however, override adjustments from ancestor
// nodes."
// https://w3c.github.io/svgwg/svg2-draft/text.html#ResolveGlyphPositioning
void SVGTextLayoutAlgorithm::resolveCharacterPositioning()
{
    auto isInTextPath = [&](const TextContentElement& element) {
        for (auto& textPath : m_textPaths) {
            auto& textPathElement = m_elements[textPath.elementIndex];
            if (element.start >= textPathElement.start && element.end <= textPathElement.end)
                return true;
        }
        return false;
    };
    auto isReordered = !std::ranges::is_sorted(m_displayOrder);

    for (auto& element : m_elements) {
        if (element.isTextPath) {
            // "A 'textPath' element always creates an anchored chunk."
            for (auto index = element.start; index < element.end; ++index) {
                if (m_characters[index].isAddressable) {
                    m_characters[index].startsAnchoredChunk = true;
                    break;
                }
            }
            continue;
        }

        RefPtr positioningElement = dynamicDowncast<SVGTextPositioningElement>(element.element);
        if (!positioningElement)
            continue;

        Ref xList = positioningElement->x();
        Ref yList = positioningElement->y();
        Ref dxList = positioningElement->dx();
        Ref dyList = positioningElement->dy();
        Ref rotateList = positioningElement->rotate();

        unsigned xCount = xList->size();
        unsigned yCount = yList->size();
        unsigned dxCount = dxList->size();
        unsigned dyCount = dyList->size();
        unsigned rotateCount = rotateList->items().size();
        if (!xCount && !yCount && !dxCount && !dyCount && !rotateCount)
            continue;

        auto ensureValues = [&](auto& values, unsigned listCount) {
            if (listCount && values.isEmpty())
                values.fill(SVGTextLayoutAttributes::emptyValue(), m_characters.size());
        };
        ensureValues(m_resolvedX, xCount);
        ensureValues(m_resolvedY, yCount);
        ensureValues(m_resolvedDX, dxCount);
        ensureValues(m_resolvedDY, dyCount);

        auto inTextPath = isInTextPath(element);
        auto newChunkCount = inTextPath ? (m_isVertical ? yCount : xCount) : std::max(xCount, yCount);

        // FIXME: For bidi content the lists are assigned in display order like the old engine did. The spec assigns them in logical order.
        auto reorderedCharacterIndexes = Vector<unsigned> { };
        auto characterIndexes = [&] -> std::span<const unsigned> {
            if (!isReordered) {
                auto begin = std::ranges::lower_bound(m_displayOrder, element.start) - m_displayOrder.begin();
                auto end = std::ranges::lower_bound(m_displayOrder, element.end) - m_displayOrder.begin();
                return m_displayOrder.subspan(begin, end - begin);
            }
            for (auto index : m_displayOrder) {
                if (index >= element.start && index < element.end)
                    reorderedCharacterIndexes.append(index);
            }
            return reorderedCharacterIndexes.span();
        }();

        auto lengthContext = SVGLengthContext { positioningElement.get() };
        unsigned listIndex = 0;
        auto lastRotate = 0.f;
        for (auto index : characterIndexes) {
            auto& character = m_characters[index];
            // "For the purposes of mapping text positioning attributes, the index is measured in Unicode code points"
            // https://w3c.github.io/svgwg/svg2-draft/text.html#TermAddressableCharacter
            if (!character.isAddressable || !character.hasMetrics())
                continue;

            if (listIndex < newChunkCount)
                character.startsAnchoredChunk = true;
            // "The 'x' attribute is ignored for vertical text on a path." and "The 'y' attribute is ignored for horizontal text on a path."
            if (listIndex < xCount && !(inTextPath && m_isVertical))
                m_resolvedX[index] = xList->items()[listIndex]->value().value(lengthContext);
            if (listIndex < yCount && !(inTextPath && !m_isVertical))
                m_resolvedY[index] = yList->items()[listIndex]->value().value(lengthContext);
            if (listIndex < dxCount)
                m_resolvedDX[index] = dxList->items()[listIndex]->value().value(lengthContext);
            if (listIndex < dyCount)
                m_resolvedDY[index] = dyList->items()[listIndex]->value().value(lengthContext);
            if (listIndex < rotateCount)
                lastRotate = rotateList->items()[listIndex]->value();
            // "Otherwise, if rotate is not empty, then set result[index + j] to result[index + j − 1]."
            // Reduce modulo a full turn so multiples of 360 are the identity.
            if (rotateCount)
                character.rotate = fmodf(lastRotate, 360);
            ++listIndex;
        }
    }

    m_characters[m_displayOrder.first()].startsAnchoredChunk = true;
}

// "Adjust positions: dx, dy"
// "Let shift.x = shift.x + resolve_dx[i] and shift.y = shift.y + resolve_dy[i]."
void SVGTextLayoutAlgorithm::adjustRelativePositions()
{
    if (m_resolvedDX.isEmpty() && m_resolvedDY.isEmpty())
        return;

    auto shiftX = 0.f;
    auto shiftY = 0.f;
    for (auto index : m_displayOrder) {
        auto& character = m_characters[index];
        auto dx = valueOrZeroAt(m_resolvedDX, index);
        auto dy = valueOrZeroAt(m_resolvedDY, index);
        shiftX += dx;
        shiftY += dy;
        character.x += shiftX;
        character.y += shiftY;
        if (character.isClusterStart && (dx || dy))
            character.isPositioned = true;
    }
}

// "Adjust positions: x, y"
// "If resolved_x[index] is set, then let shift.x = resolved_x[index] − result.x[index]."
// The dx and dy of the same character add to an absolute x or y, as in SVG 1.1.
void SVGTextLayoutAlgorithm::adjustAbsolutePositions()
{
    if (m_resolvedX.isEmpty() && m_resolvedY.isEmpty())
        return;

    auto shiftX = 0.f;
    auto shiftY = 0.f;
    for (auto index : m_displayOrder) {
        auto& character = m_characters[index];
        if (character.isClusterStart) {
            if (auto x = valueAt(m_resolvedX, index); !isUnspecified(x)) {
                shiftX = x + valueOrZeroAt(m_resolvedDX, index) - character.x;
                character.isPositioned = true;
            }
            if (auto y = valueAt(m_resolvedY, index); !isUnspecified(y)) {
                shiftY = y + valueOrZeroAt(m_resolvedDY, index) - character.y;
                character.isPositioned = true;
            }
        }
        character.x += shiftX;
        character.y += shiftY;
    }
}

std::pair<float, float> SVGTextLayoutAlgorithm::extent(std::span<const unsigned> characterIndexes) const
{
    auto start = std::optional<float> { };
    auto end = 0.f;
    for (auto index : characterIndexes) {
        auto& character = m_characters[index];
        if (!character.hasMetrics())
            continue;
        auto position = m_isVertical ? character.y : character.x;
        if (!start)
            start = position;
        end = position + glyphLength(character) * character.lengthAdjustScale;
    }
    if (!start)
        return { };
    return { *start, end };
}

// Procedure "resolve text length", children before parents.
// "Find the distance delta = 'textLength' computed value − (b − a)."
// "Find the per-character adjustment δ = delta/n."
// https://w3c.github.io/svgwg/svg2-draft/text.html#ResolveTextLength
// FIXME: A resolved descendant node should count as a single typographic character.
void SVGTextLayoutAlgorithm::applyTextLength()
{
    for (auto elementIndex : m_elementsInPostOrder) {
        auto& element = m_elements[elementIndex];
        // Text paths apply textLength along the path.
        if (element.isTextPath || element.start == element.end)
            continue;

        auto elementTextLength = textLength(protect(*element.element));
        if (!elementTextLength)
            continue;

        auto characterIndexes = Vector<unsigned> { };
        for (auto index : m_displayOrder) {
            if (index >= element.start && index < element.end && !m_characters[index].isOnPath)
                characterIndexes.append(index);
        }
        if (characterIndexes.isEmpty())
            continue;

        // Absolute x and y are applied after this step, so the extent is the advance of the whole element.
        auto [start, end] = extent(characterIndexes);
        auto totalLength = end - start;
        if (totalLength <= 0)
            continue;

        if (elementTextLength->lengthAdjust == SVGLengthAdjustSpacing) {
            auto characterCount = typographicCharacterCount(characterIndexes);
            auto spacing = characterCount > 1 ? (elementTextLength->length - totalLength) / (characterCount - 1) : 0.f;
            auto shift = 0.f;
            for (auto index : characterIndexes) {
                auto& character = m_characters[index];
                character.isInSpacingTextLength = true;
                if (!character.hasMetrics())
                    continue;
                if (m_isVertical)
                    character.y += shift;
                else
                    character.x += shift;
                shift += spacing;
            }
            continue;
        }

        // "Indicates that the advance values are adjusted and the glyphs themselves stretched or compressed in one axis"
        // https://w3c.github.io/svgwg/svg2-draft/text.html#TextElementLengthAdjustAttribute
        // Positions scale around the start of the element and each glyph stretches in place. Absolute x and y are applied
        // after this step, so they stay exact.
        auto scale = elementTextLength->length / totalLength;
        for (auto index : characterIndexes) {
            auto& character = m_characters[index];
            auto& position = m_isVertical ? character.y : character.x;
            position = start + (position - start) * scale;
            character.lengthAdjustScale *= scale;
        }
    }
}

float SVGTextLayoutAlgorithm::anchorShift(std::span<const unsigned> chunk) const
{
    auto& style = textStyle(m_textBoxes[m_characters[chunk.front()].textBoxIndex].inlineTextBox);
    auto [start, end] = extent(chunk);
    auto length = end - start;
    auto isRightToLeft = style.writingMode().isBidiRTL();
    switch (style.textAnchor()) {
    case TextAnchor::Start:
        return isRightToLeft ? -length : 0;
    case TextAnchor::Middle:
        return -length / 2;
    case TextAnchor::End:
        return isRightToLeft ? 0 : -length;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

// "The 'text-anchor' property is applied to each individual text chunk within a given 'text' element."
// "The positions are determined before applying the 'text-anchor' property."
// https://w3c.github.io/svgwg/svg2-draft/text.html#TextAnchoringProperties
void SVGTextLayoutAlgorithm::applyAnchoring()
{
    auto textPathIndex = [&](unsigned index) {
        return m_textBoxes[m_characters[index].textBoxIndex].textPathIndex;
    };

    size_t chunkStart = 0;
    for (size_t i = 1; i <= m_displayOrder.size(); ++i) {
        if (i < m_displayOrder.size()) {
            auto startsChunk = m_characters[m_displayOrder[i]].startsAnchoredChunk || textPathIndex(m_displayOrder[i]) != textPathIndex(m_displayOrder[i - 1]);
            if (!startsChunk)
                continue;
        }

        auto chunk = m_displayOrder.subspan(chunkStart, i - chunkStart);
        chunkStart = i;

        // On a path text-anchor is an additional start offset, see positionOnPaths().
        if (textPathIndex(chunk.front()) != notFound32)
            continue;

        auto shift = anchorShift(chunk);
        if (!shift)
            continue;
        for (auto index : chunk) {
            if (m_isVertical)
                m_characters[index].y += shift;
            else
                m_characters[index].x += shift;
        }
    }
}

static float computeTextPathStartOffset(const RenderSVGTextPath& textPath, float pathLength)
{
    auto& startOffset = textPath.startOffset();

    // 'pathLength' has no effect on percentage distance-along-a-path calculations.
    if (startOffset.lengthType() == SVGLengthType::Percentage)
        return startOffset.valueAsPercentage() * pathLength;

    auto offset = startOffset.valueInSpecifiedUnits();

    // Inline path data has no referenced element, so there is no author-declared length.
    if (textPath.usesPathAttribute())
        return offset;

    RefPtr targetElement = textPath.targetElement();
    if (!targetElement || !targetElement->hasAttribute(SVGNames::pathLengthAttr))
        return offset;

    // A declared zero is a scaling factor of infinity, but zero scaled infinitely must stay zero.
    // A negative value is an error and is ignored.
    // https://w3c.github.io/svgwg/svg2-draft/paths.html#PathLengthAttribute
    auto declaredLength = targetElement->pathLength();
    if (declaredLength < 0 || !offset)
        return offset;
    if (!declaredLength)
        return offset * std::numeric_limits<float>::infinity();
    return offset * (pathLength / declaredLength);
}

// The "Position on path" step. Glyphs are placed at their midpoint along the path:
// "mid is x + advance / 2 + offset"
// https://w3c.github.io/svgwg/svg2-draft/text.html#TextLayoutAlgorithm
// https://w3c.github.io/svgwg/svg2-draft/text.html#TextpathLayoutRules
void SVGTextLayoutAlgorithm::positionOnPaths()
{
    for (unsigned textPathIndex = 0; textPathIndex < m_textPaths.size(); ++textPathIndex) {
        auto& textPath = m_textPaths[textPathIndex];

        auto rangeStart = notFound;
        size_t rangeEnd = 0;
        for (size_t i = 0; i < m_displayOrder.size(); ++i) {
            if (m_textBoxes[m_characters[m_displayOrder[i]].textBoxIndex].textPathIndex != textPathIndex)
                continue;
            if (rangeStart == notFound)
                rangeStart = i;
            rangeEnd = i + 1;
        }
        if (rangeStart == notFound)
            continue;
        auto range = m_displayOrder.subspan(rangeStart, rangeEnd - rangeStart);

        CheckedRef textPathRenderer = *svgTextPath(textPath.elementBox);
        auto path = textPathRenderer->layoutPath();
        if (path.isEmpty()) {
            for (auto index : range)
                m_characters[index].isHidden = true;
            continue;
        }

        auto mapper = path.arcLengthMapper();
        auto pathLength = mapper.totalLength();
        auto isReversed = textPathRenderer->isLayoutPathReversed();

        // Text-anchor and textLength use the anchored chunks of the path content as laid out on a line.
        auto totalAnchorShift = 0.f;
        auto totalLength = 0.f;
        size_t chunkStart = 0;
        for (size_t i = 1; i <= range.size(); ++i) {
            if (i < range.size() && !m_characters[range[i]].startsAnchoredChunk)
                continue;
            auto chunk = range.subspan(chunkStart, i - chunkStart);
            chunkStart = i;
            totalAnchorShift += anchorShift(chunk);
            auto [start, end] = extent(chunk);
            totalLength += end - start;
        }

        auto pathSpacing = 0.f;
        auto scaling = 1.f;
        if (auto elementTextLength = textLength(protect(*m_elements[textPath.elementIndex].element))) {
            if (elementTextLength->lengthAdjust == SVGLengthAdjustSpacing) {
                if (auto characterCount = typographicCharacterCount(range); characterCount > 1)
                    pathSpacing = (elementTextLength->length - totalLength) / (characterCount - 1);
            } else if (totalLength)
                scaling = elementTextLength->length / totalLength;
        }
        textPath.scaling = scaling;

        // "Let offset be the value of the 'textPath' element's 'startOffset' attribute, adjusted due to any 'pathLength' attribute on
        // the referenced element."
        auto startOffset = computeTextPathStartOffset(textPathRenderer, pathLength) + totalAnchorShift;
        auto currentOffset = startOffset;
        auto perpendicularShift = 0.f;
        auto textPositionAfterPath = std::optional<FloatPoint> { };
        auto linePositionAfterPath = std::optional<FloatPoint> { };

        for (auto index : range) {
            auto& character = m_characters[index];
            if (!character.hasMetrics())
                continue;

            auto& textBox = m_textBoxes[character.textBoxIndex];
            auto inlinePosition = valueAt(m_isVertical ? m_resolvedY : m_resolvedX, index);
            auto inlineShift = valueAt(m_isVertical ? m_resolvedDY : m_resolvedDX, index);
            auto blockShift = valueAt(m_isVertical ? m_resolvedDX : m_resolvedDY, index);

            // An absolute inline position marks the beginning of a new position along the path.
            if (!isUnspecified(inlinePosition))
                currentOffset = inlinePosition + startOffset;
            if (!isUnspecified(inlineShift))
                currentOffset += inlineShift;
            if (!isUnspecified(blockShift))
                perpendicularShift += blockShift;

            auto scaledAdvance = character.advance * scaling;
            auto xOrientationShift = character.xOrientationShift;
            auto yOrientationShift = character.yOrientationShift;
            // Move to the glyph midpoint.
            if (m_isVertical) {
                xOrientationShift += perpendicularShift + textBox.baselineShift;
                yOrientationShift -= scaledAdvance / 2;
            } else {
                xOrientationShift -= scaledAdvance / 2;
                yOrientationShift += perpendicularShift - textBox.baselineShift;
            }

            auto midpointOffset = currentOffset + scaledAdvance / 2;
            currentOffset += scaledAdvance + pathSpacing + character.spacing * scaling;

            auto lineAdvance = character.advance + character.spacing;
            linePositionAfterPath = FloatPoint { character.x, character.y } + (m_isVertical ? FloatSize { 0, lineAdvance } : FloatSize { lineAdvance, 0 });

            // "If path is not a closed subpath and mid < 0 or mid > length, set the "hidden" flag of result[index] to true."
            // FIXME: Closed subpaths should wrap around.
            if (midpointOffset < 0 || midpointOffset > pathLength) {
                character.isHidden = true;
                continue;
            }

            // For side="right", sample the forward path at the mirrored arc length and flip each glyph.
            auto position = mapper.positionAtLength(isReversed ? pathLength - midpointOffset : midpointOffset);
            auto angle = character.rotate + position.angleInDegrees;
            if (isReversed)
                angle += 180;
            // For vertical text the actual angle is rotated 90 degrees anti-clockwise, not the orientation angle.
            if (m_isVertical)
                angle -= 90;

            character.x = position.point.x();
            character.y = position.point.y();
            character.rotate = angle;
            character.xOrientationShift = xOrientationShift;
            character.yOrientationShift = yOrientationShift;
            character.isPositioned = true;

            textPositionAfterPath = position.point + (m_isVertical ? FloatSize { 0, character.advance } : FloatSize { character.advance, 0 });
        }

        if (!textPositionAfterPath || !linePositionAfterPath)
            continue;

        // Text following the path continues from the last glyph on the path until it is positioned explicitly.
        auto delta = *textPositionAfterPath - *linePositionAfterPath;
        auto shiftX = true;
        auto shiftY = true;
        for (auto index : m_displayOrder.subspan(rangeEnd)) {
            auto& character = m_characters[index];
            if (character.isOnPath)
                break;
            if (!isUnspecified(valueAt(m_resolvedX, index)))
                shiftX = false;
            if (!isUnspecified(valueAt(m_resolvedY, index)))
                shiftY = false;
            if (!shiftX && !shiftY)
                break;
            if (shiftX)
                character.x += delta.width();
            if (shiftY)
                character.y += delta.height();
        }
    }
}

void SVGTextLayoutAlgorithm::buildFragments()
{
    for (auto& textBox : m_textBoxes) {
        auto& fragments = m_fragmentsForBoxes[textBox.displayBoxIndex];
        ASSERT(fragments.isEmpty());

        auto& metrics = textMetrics(textBox.inlineTextBox);
        auto isOnPath = textBox.textPathIndex != notFound32;
        auto pathScaling = isOnPath ? m_textPaths[textBox.textPathIndex].scaling : 1.f;

        auto fragment = std::optional<SVGTextFragment> { };
        auto fragmentLengthAdjustScale = 1.f;
        auto fragmentHasSpacing = false;
        auto lastRotate = 0.f;

        auto closeFragment = [&](unsigned endIndex) {
            if (!fragment)
                return;
            fragment->length = endIndex - textBox.contentStart - fragment->characterOffset;
            auto inlineLength = 0.f;
            auto metricsEnd = fragment->metricsListOffset;
            for (unsigned offset = 0; offset < fragment->length && metricsEnd < metrics.size(); ++metricsEnd) {
                inlineLength += m_isVertical ? metrics[metricsEnd].height() : metrics[metricsEnd].width();
                offset += metrics[metricsEnd].length();
            }
            auto& lastMetrics = metrics[metricsEnd - 1];
            fragment->width = m_isVertical ? lastMetrics.width() : inlineLength;
            fragment->height = m_isVertical ? inlineLength : lastMetrics.height();
            fragments.append(*fragment);
            fragment = { };
        };

        for (auto index = textBox.start; index < textBox.start + textBox.length; ++index) {
            auto& character = m_characters[index];
            if (character.isHidden) {
                closeFragment(index);
                continue;
            }
            if (!character.hasMetrics())
                continue;

            auto startsFragment = [&] {
                if (!fragment)
                    return true;
                // A combining mark stays with its base rather than being shaped alone. Bug 266832.
                if (!character.isClusterStart)
                    return false;
                return character.isPositioned || m_isVertical || isOnPath || character.rotate || lastRotate || character.orientationAngle
                    || fragmentHasSpacing || character.isInSpacingTextLength || character.lengthAdjustScale != fragmentLengthAdjustScale;
            }();
            lastRotate = character.rotate;

            if (!startsFragment) {
                fragmentHasSpacing |= !!character.spacing;
                continue;
            }

            closeFragment(index);

            fragment = SVGTextFragment { };
            fragment->characterOffset = index - textBox.contentStart;
            fragment->metricsListOffset = character.metricsIndex;
            fragment->x = character.x;
            fragment->y = character.y;
            if (!isOnPath) {
                if (m_isVertical)
                    fragment->x += textBox.baselineShift;
                else
                    fragment->y -= textBox.baselineShift;
            }
            // Query APIs subtract the baseline shift to return the unshifted position.
            fragment->baselineShift = textBox.baselineShift;

            if (character.rotate)
                fragment->transform.rotate(character.rotate);
            if (character.xOrientationShift || character.yOrientationShift)
                fragment->transform.translate(character.xOrientationShift, character.yOrientationShift);
            if (character.orientationAngle)
                fragment->transform.rotate(character.orientationAngle);

            fragment->isTextOnPath = isOnPath && pathScaling != 1;
            if (fragment->isTextOnPath) {
                if (m_isVertical)
                    fragment->lengthAdjustTransform.scaleNonUniform(1, pathScaling);
                else
                    fragment->lengthAdjustTransform.scaleNonUniform(pathScaling, 1);
            } else if (character.lengthAdjustScale != 1) {
                // The glyphs stretch from the fragment origin.
                auto& transform = fragment->lengthAdjustTransform;
                transform.translate(fragment->x, fragment->y);
                if (m_isVertical)
                    transform.scaleNonUniform(1, character.lengthAdjustScale);
                else
                    transform.scaleNonUniform(character.lengthAdjustScale, 1);
                transform.translate(-fragment->x, -fragment->y);
            }

            fragmentLengthAdjustScale = character.lengthAdjustScale;
            fragmentHasSpacing = !!character.spacing;
        }
        closeFragment(textBox.start + textBox.length);
    }
}

} // namespace WebCore
