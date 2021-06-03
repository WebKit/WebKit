/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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
#include "SVGTextQuery.h"

#include "FloatConversion.h"
#include "LegacyInlineFlowBox.h"
#include "RenderBlockFlow.h"
#include "RenderInline.h"
#include "RenderSVGText.h"
#include "SVGInlineTextBox.h"
#include "VisiblePosition.h"

#include <wtf/MathExtras.h>

namespace WebCore {

// Base structure for callback user data
struct SVGTextQuery::Data {
    Data()
        : isVerticalText(false)
        , processedCharacters(0)
        , textRenderer(0)
        , textBox(0)
    {
    }

    bool isVerticalText;
    unsigned processedCharacters;
    RenderSVGInlineText* textRenderer;
    const SVGInlineTextBox* textBox;
};

static inline LegacyInlineFlowBox* flowBoxForRenderer(RenderObject* renderer)
{
    if (!renderer)
        return nullptr;

    if (is<RenderBlockFlow>(*renderer)) {
        // If we're given a block element, it has to be a RenderSVGText.
        ASSERT(is<RenderSVGText>(*renderer));
        RenderBlockFlow& renderBlock = downcast<RenderBlockFlow>(*renderer);

        // RenderSVGText only ever contains a single line box.
        auto flowBox = renderBlock.firstRootBox();
        ASSERT(flowBox == renderBlock.lastRootBox());
        return flowBox;
    }

    if (is<RenderInline>(*renderer)) {
        // We're given a RenderSVGInline or objects that derive from it (RenderSVGTSpan / RenderSVGTextPath)
        RenderInline& renderInline = downcast<RenderInline>(*renderer);

        // RenderSVGInline only ever contains a single line box.
        LegacyInlineFlowBox* flowBox = renderInline.firstLineBox();
        ASSERT(flowBox == renderInline.lastLineBox());
        return flowBox;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

SVGTextQuery::SVGTextQuery(RenderObject* renderer)
{
    collectTextBoxesInFlowBox(flowBoxForRenderer(renderer));
}

void SVGTextQuery::collectTextBoxesInFlowBox(LegacyInlineFlowBox* flowBox)
{
    if (!flowBox)
        return;

    for (auto* child = flowBox->firstChild(); child; child = child->nextOnLine()) {
        if (is<LegacyInlineFlowBox>(*child)) {
            // Skip generated content.
            if (!child->renderer().node())
                continue;

            collectTextBoxesInFlowBox(downcast<LegacyInlineFlowBox>(child));
            continue;
        }

        if (is<SVGInlineTextBox>(*child))
            m_textBoxes.append(downcast<SVGInlineTextBox>(child));
    }
}

bool SVGTextQuery::executeQuery(Data* queryData, ProcessTextFragmentCallback fragmentCallback) const
{
    ASSERT(!m_textBoxes.isEmpty());

    unsigned processedCharacters = 0;
    unsigned textBoxCount = m_textBoxes.size();

    // Loop over all text boxes
    for (unsigned textBoxPosition = 0; textBoxPosition < textBoxCount; ++textBoxPosition) {
        queryData->textBox = m_textBoxes.at(textBoxPosition);
        queryData->textRenderer = &queryData->textBox->renderer();

        queryData->isVerticalText = queryData->textRenderer->style().isVerticalWritingMode();
        const Vector<SVGTextFragment>& fragments = queryData->textBox->textFragments();
    
        // Loop over all text fragments in this text box, firing a callback for each.
        unsigned fragmentCount = fragments.size();
        for (unsigned i = 0; i < fragmentCount; ++i) {
            const SVGTextFragment& fragment = fragments.at(i);
            if ((this->*fragmentCallback)(queryData, fragment))
                return true;

            processedCharacters += fragment.length;
        }

        queryData->processedCharacters = processedCharacters;
    }

    return false;
}

bool SVGTextQuery::mapStartEndPositionsIntoFragmentCoordinates(Data* queryData, const SVGTextFragment& fragment, unsigned& startPosition, unsigned& endPosition) const
{
    // Reuse the same logic used for text selection & painting, to map our query start/length into start/endPositions of the current text fragment.
    ASSERT(startPosition >= queryData->processedCharacters);
    ASSERT(endPosition >= queryData->processedCharacters);
    startPosition -= queryData->processedCharacters;
    endPosition -= queryData->processedCharacters;

    if (startPosition >= endPosition)
        return false;

    modifyStartEndPositionsRespectingLigatures(queryData, startPosition, endPosition);
    if (!queryData->textBox->mapStartEndPositionsIntoFragmentCoordinates(fragment, startPosition, endPosition))
        return false;

    ASSERT_WITH_SECURITY_IMPLICATION(startPosition < endPosition);
    return true;
}

void SVGTextQuery::modifyStartEndPositionsRespectingLigatures(Data* queryData, unsigned& startPosition, unsigned& endPosition) const
{
    SVGTextLayoutAttributes* layoutAttributes = queryData->textRenderer->layoutAttributes();
    Vector<SVGTextMetrics>& textMetricsValues = layoutAttributes->textMetricsValues();
    unsigned boxStart = queryData->textBox->start();
    unsigned boxLength = queryData->textBox->len();

    unsigned textMetricsOffset = 0;
    unsigned textMetricsSize = textMetricsValues.size();

    unsigned positionOffset = 0;
    unsigned positionSize = layoutAttributes->context().text().length();

    bool alterStartPosition = true;
    bool alterEndPosition = true;

    std::optional<unsigned> lastPositionOffset;
    for (; textMetricsOffset < textMetricsSize && positionOffset < positionSize; ++textMetricsOffset) {
        SVGTextMetrics& metrics = textMetricsValues[textMetricsOffset];

        // Advance to text box start location.
        if (positionOffset < boxStart) {
            positionOffset += metrics.length();
            continue;
        }

        // Stop if we've finished processing this text box.
        if (positionOffset >= boxStart + boxLength)
            break;

        // If the start position maps to a character in the metrics list, we don't need to modify it.
        if (startPosition == positionOffset)
            alterStartPosition = false;

        // If the start position maps to a character in the metrics list, we don't need to modify it.
        if (endPosition == positionOffset)
            alterEndPosition = false;

        // Detect ligatures.
        if (lastPositionOffset && lastPositionOffset.value() - positionOffset > 1) {
            if (alterStartPosition && startPosition > lastPositionOffset.value() && startPosition < positionOffset) {
                startPosition = lastPositionOffset.value();
                alterStartPosition = false;
            }

            if (alterEndPosition && endPosition > lastPositionOffset.value() && endPosition < positionOffset) {
                endPosition = positionOffset;
                alterEndPosition = false;
            }
        }

        if (!alterStartPosition && !alterEndPosition)
            break;

        lastPositionOffset = positionOffset;
        positionOffset += metrics.length();
    }

    if (!alterStartPosition && !alterEndPosition)
        return;

    if (lastPositionOffset && lastPositionOffset.value() - positionOffset > 1) {
        if (alterStartPosition && startPosition > lastPositionOffset.value() && startPosition < positionOffset)
            startPosition = lastPositionOffset.value();

        if (alterEndPosition && endPosition > lastPositionOffset.value() && endPosition < positionOffset)
            endPosition = positionOffset;
    }
}

// numberOfCharacters() implementation
bool SVGTextQuery::numberOfCharactersCallback(Data*, const SVGTextFragment&) const
{
    // no-op
    return false;
}

unsigned SVGTextQuery::numberOfCharacters() const
{
    if (m_textBoxes.isEmpty())
        return 0;

    Data data;
    executeQuery(&data, &SVGTextQuery::numberOfCharactersCallback);
    return data.processedCharacters;
}

// textLength() implementation
struct TextLengthData : SVGTextQuery::Data {
    TextLengthData()
        : textLength(0)
    {
    }

    float textLength;
};

bool SVGTextQuery::textLengthCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    TextLengthData* data = static_cast<TextLengthData*>(queryData);
    data->textLength += queryData->isVerticalText ? fragment.height : fragment.width;
    return false;
}

float SVGTextQuery::textLength() const
{
    if (m_textBoxes.isEmpty())
        return 0;

    TextLengthData data;
    executeQuery(&data, &SVGTextQuery::textLengthCallback);
    return data.textLength;
}

// subStringLength() implementation
struct SubStringLengthData : SVGTextQuery::Data {
    SubStringLengthData(unsigned queryStartPosition, unsigned queryLength)
        : startPosition(queryStartPosition)
        , length(queryLength)
        , subStringLength(0)
    {
    }

    unsigned startPosition;
    unsigned length;

    float subStringLength;
};

bool SVGTextQuery::subStringLengthCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    SubStringLengthData* data = static_cast<SubStringLengthData*>(queryData);

    unsigned startPosition = data->startPosition;
    unsigned endPosition = startPosition + data->length;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    SVGTextMetrics metrics = SVGTextMetrics::measureCharacterRange(*queryData->textRenderer, fragment.characterOffset + startPosition, endPosition - startPosition);
    data->subStringLength += queryData->isVerticalText ? metrics.height() : metrics.width();
    return false;
}

float SVGTextQuery::subStringLength(unsigned startPosition, unsigned length) const
{
    if (m_textBoxes.isEmpty())
        return 0;

    SubStringLengthData data(startPosition, length);
    executeQuery(&data, &SVGTextQuery::subStringLengthCallback);
    return data.subStringLength;
}

// startPositionOfCharacter() implementation
struct StartPositionOfCharacterData : SVGTextQuery::Data {
    StartPositionOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
    {
    }

    unsigned position;
    FloatPoint startPosition;
};

bool SVGTextQuery::startPositionOfCharacterCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    StartPositionOfCharacterData* data = static_cast<StartPositionOfCharacterData*>(queryData);

    unsigned startPosition = data->position;
    unsigned endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    data->startPosition = FloatPoint(fragment.x, fragment.y);

    if (startPosition) {
        SVGTextMetrics metrics = SVGTextMetrics::measureCharacterRange(*queryData->textRenderer, fragment.characterOffset, startPosition);
        if (queryData->isVerticalText)
            data->startPosition.move(0, metrics.height());
        else
            data->startPosition.move(metrics.width(), 0);
    }

    AffineTransform fragmentTransform;
    fragment.buildFragmentTransform(fragmentTransform, SVGTextFragment::TransformIgnoringTextLength);
    if (fragmentTransform.isIdentity())
        return true;

    data->startPosition = fragmentTransform.mapPoint(data->startPosition);
    return true;
}

FloatPoint SVGTextQuery::startPositionOfCharacter(unsigned position) const
{
    if (m_textBoxes.isEmpty())
        return { };

    StartPositionOfCharacterData data(position);
    executeQuery(&data, &SVGTextQuery::startPositionOfCharacterCallback);
    return data.startPosition;
}

// endPositionOfCharacter() implementation
struct EndPositionOfCharacterData : SVGTextQuery::Data {
    EndPositionOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
    {
    }

    unsigned position;
    FloatPoint endPosition;
};

bool SVGTextQuery::endPositionOfCharacterCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    EndPositionOfCharacterData* data = static_cast<EndPositionOfCharacterData*>(queryData);

    unsigned startPosition = data->position;
    unsigned endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    data->endPosition = FloatPoint(fragment.x, fragment.y);

    SVGTextMetrics metrics = SVGTextMetrics::measureCharacterRange(*queryData->textRenderer, fragment.characterOffset, startPosition + 1);
    if (queryData->isVerticalText)
        data->endPosition.move(0, metrics.height());
    else
        data->endPosition.move(metrics.width(), 0);

    AffineTransform fragmentTransform;
    fragment.buildFragmentTransform(fragmentTransform, SVGTextFragment::TransformIgnoringTextLength);
    if (fragmentTransform.isIdentity())
        return true;

    data->endPosition = fragmentTransform.mapPoint(data->endPosition);
    return true;
}

FloatPoint SVGTextQuery::endPositionOfCharacter(unsigned position) const
{
    if (m_textBoxes.isEmpty())
        return { };

    EndPositionOfCharacterData data(position);
    executeQuery(&data, &SVGTextQuery::endPositionOfCharacterCallback);
    return data.endPosition;
}

// rotationOfCharacter() implementation
struct RotationOfCharacterData : SVGTextQuery::Data {
    RotationOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
        , rotation(0)
    {
    }

    unsigned position;
    float rotation;
};

bool SVGTextQuery::rotationOfCharacterCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    RotationOfCharacterData* data = static_cast<RotationOfCharacterData*>(queryData);

    unsigned startPosition = data->position;
    unsigned endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    AffineTransform fragmentTransform;
    fragment.buildFragmentTransform(fragmentTransform, SVGTextFragment::TransformIgnoringTextLength);
    if (fragmentTransform.isIdentity())
        data->rotation = 0;
    else {
        fragmentTransform.scale(1 / fragmentTransform.xScale(), 1 / fragmentTransform.yScale());
        data->rotation = narrowPrecisionToFloat(rad2deg(atan2(fragmentTransform.b(), fragmentTransform.a())));
    }

    return true;
}

float SVGTextQuery::rotationOfCharacter(unsigned position) const
{
    if (m_textBoxes.isEmpty())
        return 0;

    RotationOfCharacterData data(position);
    executeQuery(&data, &SVGTextQuery::rotationOfCharacterCallback);
    return data.rotation;
}

// extentOfCharacter() implementation
struct ExtentOfCharacterData : SVGTextQuery::Data {
    ExtentOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
    {
    }

    unsigned position;
    FloatRect extent;
};

static inline void calculateGlyphBoundaries(SVGTextQuery::Data* queryData, const SVGTextFragment& fragment, unsigned startPosition, FloatRect& extent)
{
    float scalingFactor = queryData->textRenderer->scalingFactor();
    ASSERT(scalingFactor);

    extent.setLocation(FloatPoint(fragment.x, fragment.y - queryData->textRenderer->scaledFont().fontMetrics().floatAscent() / scalingFactor));

    if (startPosition) {
        SVGTextMetrics metrics = SVGTextMetrics::measureCharacterRange(*queryData->textRenderer, fragment.characterOffset, startPosition);
        if (queryData->isVerticalText)
            extent.move(0, metrics.height());
        else
            extent.move(metrics.width(), 0);
    }

    SVGTextMetrics metrics = SVGTextMetrics::measureCharacterRange(*queryData->textRenderer, fragment.characterOffset + startPosition, 1);
    extent.setSize(FloatSize(metrics.width(), metrics.height()));

    AffineTransform fragmentTransform;
    fragment.buildFragmentTransform(fragmentTransform, SVGTextFragment::TransformIgnoringTextLength);
    if (fragmentTransform.isIdentity())
        return;

    extent = fragmentTransform.mapRect(extent);
}

bool SVGTextQuery::extentOfCharacterCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    ExtentOfCharacterData* data = static_cast<ExtentOfCharacterData*>(queryData);

    unsigned startPosition = data->position;
    unsigned endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    calculateGlyphBoundaries(queryData, fragment, startPosition, data->extent);
    return true;
}

FloatRect SVGTextQuery::extentOfCharacter(unsigned position) const
{
    if (m_textBoxes.isEmpty())
        return FloatRect();

    ExtentOfCharacterData data(position);
    executeQuery(&data, &SVGTextQuery::extentOfCharacterCallback);
    return data.extent;
}

// characterNumberAtPosition() implementation
struct CharacterNumberAtPositionData : SVGTextQuery::Data {
    CharacterNumberAtPositionData(const FloatPoint& queryPosition)
        : position(queryPosition)
    {
    }

    FloatPoint position;
};

bool SVGTextQuery::characterNumberAtPositionCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    CharacterNumberAtPositionData* data = static_cast<CharacterNumberAtPositionData*>(queryData);

    FloatRect extent;
    for (unsigned i = 0; i < fragment.length; ++i) {
        unsigned startPosition = data->processedCharacters + i;
        unsigned endPosition = startPosition + 1;
        if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
            continue;

        calculateGlyphBoundaries(queryData, fragment, startPosition, extent);
        if (extent.contains(data->position)) {
            data->processedCharacters += i;
            return true;
        }
    }

    return false;
}

int SVGTextQuery::characterNumberAtPosition(const FloatPoint& position) const
{
    if (m_textBoxes.isEmpty())
        return -1;

    CharacterNumberAtPositionData data(position);
    if (!executeQuery(&data, &SVGTextQuery::characterNumberAtPositionCallback))
        return -1;

    return data.processedCharacters;
}

}
