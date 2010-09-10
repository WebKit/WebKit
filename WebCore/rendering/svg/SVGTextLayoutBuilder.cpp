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
#include "SVGTextLayoutBuilder.h"

#include "RenderSVGInlineText.h"
#include "RenderSVGText.h"
#include "SVGTextLayoutUtilities.h"
#include "SVGTextPositioningElement.h"

// Set to a value > 0 to dump the layout vectors
#define DUMP_LAYOUT_VECTORS 0

namespace WebCore {

SVGTextLayoutBuilder::SVGTextLayoutBuilder()
{
}

void SVGTextLayoutBuilder::buildLayoutAttributesForTextSubtree(RenderSVGText* textRoot)
{
    ASSERT(textRoot);
    m_scopes.clear();

    // Build layout scopes.
    unsigned atCharacter = 0;
    buildLayoutScopes(textRoot, atCharacter);

    if (!atCharacter)
        return;

    // Add outermost scope, after text length is known.
    LayoutScope scope;
    buildLayoutScope(scope, textRoot, 0, atCharacter);
    m_scopes.prepend(scope);

    // Build layout information respecting scope of attribute values.
    buildLayoutAttributesFromScopes();

    atCharacter = 0;
    propagateLayoutAttributes(textRoot, atCharacter);
}

static inline void copyToDestinationVector(Vector<float>& destination, unsigned destinationStartOffset, Vector<float>& source, unsigned sourceStartOffset, unsigned length)
{
    ASSERT(destinationStartOffset + length <= destination.size());

    Vector<float>::iterator sourceBegin = source.begin() + sourceStartOffset;
    std::copy(sourceBegin, sourceBegin + length, destination.begin() + destinationStartOffset);
}

static inline void copyToDestinationVectorIfSourceRangeIsNotEmpty(Vector<float>& destination, unsigned destinationStartOffset, Vector<float>& source, unsigned sourceStartOffset, unsigned length)
{
    bool rangeEmpty = true;

    unsigned size = sourceStartOffset + length;
    for (unsigned i = sourceStartOffset; i < size; ++i) {
        if (source.at(i) == SVGTextLayoutAttributes::emptyValue())
            continue;
        rangeEmpty = false;
        break;
    }

    if (rangeEmpty)
        return;

    destination.resize(length);
    copyToDestinationVector(destination, destinationStartOffset, source, sourceStartOffset, length);
}

void SVGTextLayoutBuilder::propagateLayoutAttributes(RenderObject* start, unsigned& atCharacter)
{
    for (RenderObject* child = start->firstChild(); child; child = child->nextSibling()) { 
        if (!child->isSVGInlineText()) {
            if (child->isSVGInline())
                propagateLayoutAttributes(child, atCharacter);
            continue;
        }

        RenderSVGInlineText* text = static_cast<RenderSVGInlineText*>(child);
        unsigned textLength = text->textLength();

        // Build layout attributes for a single RenderSVGInlineText renderer.
        SVGTextLayoutAttributes attributes;

        // The x value list should always be as large as the text length.
        // Any values that are empty will be filled in by the actual text layout process later,
        // as we need to be able to query the x/y position for every character through SVG DOM.
        attributes.xValues().resize(textLength);
        copyToDestinationVector(attributes.xValues(), 0, m_attributes.xValues(), atCharacter, textLength);

        // Same for the y value list.
        attributes.yValues().resize(textLength);
        copyToDestinationVector(attributes.yValues(), 0, m_attributes.yValues(), atCharacter, textLength);

        // The dx/dy/rotate value lists may be empty.
        copyToDestinationVectorIfSourceRangeIsNotEmpty(attributes.dxValues(), 0, m_attributes.dxValues(), atCharacter, textLength);
        copyToDestinationVectorIfSourceRangeIsNotEmpty(attributes.dyValues(), 0, m_attributes.dyValues(), atCharacter, textLength);
        copyToDestinationVectorIfSourceRangeIsNotEmpty(attributes.rotateValues(), 0, m_attributes.rotateValues(), atCharacter, textLength);

        // Build CharacterData, which will be used to detect ligatures, holds kerning pairs (glyph name, unicode string) and character metrics.
        measureCharacters(text, attributes);

#if DUMP_LAYOUT_VECTORS > 0
        fprintf(stderr, "Dumping layout vector for RenderSVGInlineText, renderer=%p, node=%p\n", text, text->node());
        attributes.dump();
#endif

        text->storeLayoutAttributes(attributes);
        atCharacter += text->textLength();
    }
}

void SVGTextLayoutBuilder::buildLayoutScopes(RenderObject* start, unsigned& atCharacter)
{
    for (RenderObject* child = start->firstChild(); child; child = child->nextSibling()) { 
        if (child->isSVGInlineText()) {
            atCharacter += toRenderText(child)->textLength();
            continue;
        }

        if (!child->isSVGInline())
            continue;

        unsigned textContentStart = atCharacter;
        buildLayoutScopes(child, atCharacter);

        LayoutScope scope;
        buildLayoutScope(scope, child, textContentStart, atCharacter - textContentStart);
        m_scopes.append(scope);
    }
}

static inline void fillDestinationVectorWithLastSourceValue(Vector<float>& destination, unsigned destinationStartOffset, Vector<float>& source, unsigned length)
{
    if (source.isEmpty())
        return;

    float lastValue = source.last();

    unsigned rotateValuesSize = source.size();
    for (unsigned i = rotateValuesSize; i < length; ++i) {
        ASSERT(i + destinationStartOffset < destination.size());
        destination.at(i + destinationStartOffset) = lastValue;
    }
}

void SVGTextLayoutBuilder::buildLayoutAttributesFromScopes()
{
    ASSERT(!m_scopes.isEmpty());

    unsigned totalLength = m_scopes.first().textContentLength;
    if (!totalLength)
        return;

    m_attributes.fillWithEmptyValues(totalLength);

    // Build final list of x/y/dx/dy/rotate values for each character stores in the <text> subtree.
    for (unsigned atScope = 0; atScope < m_scopes.size(); ++atScope) {
        LayoutScope& scope = m_scopes.at(atScope);
        SVGTextLayoutAttributes& attributes = scope.attributes;

        copyToDestinationVector(m_attributes.xValues(), scope.textContentStart, attributes.xValues(), 0, attributes.xValues().size());
        copyToDestinationVector(m_attributes.yValues(), scope.textContentStart, attributes.yValues(), 0, attributes.yValues().size());
        copyToDestinationVector(m_attributes.dxValues(), scope.textContentStart, attributes.dxValues(), 0, attributes.dxValues().size());
        copyToDestinationVector(m_attributes.dyValues(), scope.textContentStart, attributes.dyValues(), 0, attributes.dyValues().size());
        copyToDestinationVector(m_attributes.rotateValues(), scope.textContentStart, attributes.rotateValues(), 0, attributes.rotateValues().size());

        // In horizontal (vertical) writing modes, the last y (x) value in the scope is the default y (x) value for all following characters, unless explicitely overriden.
        if (scope.isVerticalWritingMode)
            fillDestinationVectorWithLastSourceValue(m_attributes.xValues(), scope.textContentStart, attributes.xValues(), scope.textContentLength);
        else
            fillDestinationVectorWithLastSourceValue(m_attributes.yValues(), scope.textContentStart, attributes.yValues(), scope.textContentLength);

        // The last rotation value in the scope is the default rotation for all following character, unless explicitely overriden.
        fillDestinationVectorWithLastSourceValue(m_attributes.rotateValues(), scope.textContentStart, attributes.rotateValues(), scope.textContentLength);
    }
}

void SVGTextLayoutBuilder::measureCharacters(RenderSVGInlineText* text, SVGTextLayoutAttributes& attributes)
{
    ASSERT(text);
    ASSERT(text->style());
    const Font& font = text->style()->font();
    const UChar* characters = text->characters();
    int length = text->textLength();

    TextRun run(0, 0);
    run.disableSpacing();
    run.disableRoundingHacks();

    int charsConsumed = 0;
    for (int position = 0; position < length; position += charsConsumed) {
        run.setText(characters + position, 1);
        int extraCharsAvailable = length - position - 1;

        SVGTextLayoutAttributes::CharacterData characterData;
        characterData.width = font.floatWidth(run, extraCharsAvailable, characterData.spansCharacters, characterData.glyphName);
        characterData.height = font.height();
        characterData.unicodeString = String(characters + position, characterData.spansCharacters);
        attributes.characterDataValues().append(characterData);

        charsConsumed = characterData.spansCharacters;
    }
}

static inline void extractFloatValuesFromSVGLengthList(SVGElement* lengthContext, SVGLengthList* list, Vector<float>& floatValues, int textContentLength)
{
    ASSERT(lengthContext);
    ASSERT(list);
    ASSERT(textContentLength >= 0);

    ExceptionCode ec = 0;
    int length = list->numberOfItems();
    if (length > textContentLength)
        length = textContentLength;

    for (int i = 0; i < length; ++i) {
        SVGLength length(list->getItem(i, ec));
        ASSERT(!ec);
        floatValues.append(length.value(lengthContext));
    }
}

static inline void extractFloatValuesFromSVGNumberList(SVGNumberList* list, Vector<float>& floatValues, int textContentLength)
{
    ASSERT(list);
    ASSERT(textContentLength >= 0);

    ExceptionCode ec = 0;
    int length = list->numberOfItems();
    if (length > textContentLength)
        length = textContentLength;

    for (int i = 0; i < length; ++i) {
        float length(list->getItem(i, ec));
        ASSERT(!ec);
        floatValues.append(length);
    }
}

static inline SVGTextPositioningElement* svgTextPositioningElementForInlineRenderer(RenderObject* renderer)
{
    ASSERT(renderer);
    ASSERT(renderer->isSVGText() || renderer->isSVGInline());

    Node* node = renderer->node();
    ASSERT(node);
    ASSERT(node->isSVGElement());

    if (!node->hasTagName(SVGNames::textTag)
     && !node->hasTagName(SVGNames::tspanTag)
#if ENABLE(SVG_FONTS)
     && !node->hasTagName(SVGNames::altGlyphTag)
#endif
     && !node->hasTagName(SVGNames::trefTag))
        return 0;

    return static_cast<SVGTextPositioningElement*>(node);
}

void SVGTextLayoutBuilder::buildLayoutScope(LayoutScope& scope, RenderObject* renderer, unsigned textContentStart, unsigned textContentLength)
{
    ASSERT(renderer);
    ASSERT(renderer->style());

    scope.isVerticalWritingMode = isVerticalWritingMode(renderer->style()->svgStyle());
    scope.textContentStart = textContentStart;
    scope.textContentLength = textContentLength;

    SVGTextPositioningElement* element = svgTextPositioningElementForInlineRenderer(renderer);
    if (!element)
        return;

    SVGTextLayoutAttributes& attributes = scope.attributes;
    extractFloatValuesFromSVGLengthList(element, element->x(), attributes.xValues(), textContentLength);
    extractFloatValuesFromSVGLengthList(element, element->y(), attributes.yValues(), textContentLength);
    extractFloatValuesFromSVGLengthList(element, element->dx(), attributes.dxValues(), textContentLength);
    extractFloatValuesFromSVGLengthList(element, element->dy(), attributes.dyValues(), textContentLength);
    extractFloatValuesFromSVGNumberList(element->rotate(), attributes.rotateValues(), textContentLength);
}

}

#endif // ENABLE(SVG)
