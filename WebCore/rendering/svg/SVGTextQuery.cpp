/*
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "SVGTextQuery.h"

#if ENABLE(SVG)
#include "FloatConversion.h"
#include "InlineFlowBox.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "SVGInlineTextBox.h"
#include "VisiblePosition.h"

namespace WebCore {

// Base structure for callback user data
struct SVGTextQuery::Data {
    Data()
        : processedChunkCharacters(0)
    {
    }

    unsigned processedChunkCharacters;
};

static inline InlineFlowBox* flowBoxForRenderer(RenderObject* renderer)
{
    if (!renderer)
        return 0;

    if (renderer->isRenderBlock()) {
        // If we're given a block element, it has to be a RenderSVGText.
        ASSERT(renderer->isSVGText());
        RenderBlock* renderBlock = toRenderBlock(renderer);

        // RenderSVGText only ever contains a single line box.
        InlineFlowBox* flowBox = renderBlock->firstLineBox();
        ASSERT(flowBox == renderBlock->lastLineBox());
        return flowBox;
    }

    if (renderer->isRenderInline()) {
        // We're given a RenderSVGInline or objects that derive from it (RenderSVGTSpan / RenderSVGTextPath)
        RenderInline* renderInline = toRenderInline(renderer);

        // RenderSVGInline only ever contains a single line box.
        InlineFlowBox* flowBox = renderInline->firstLineBox();
        ASSERT(flowBox == renderInline->lastLineBox());
        return flowBox;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static inline float mapLengthThroughChunkTransformation(const SVGInlineTextBox* textBox, bool isVerticalText, float length)
{
    const AffineTransform& transform = textBox->chunkTransformation();
    if (transform.isIdentity())
        return length;
        
    return narrowPrecisionToFloat(static_cast<double>(length) * (isVerticalText ? transform.d() : transform.a()));
}

SVGTextQuery::SVGTextQuery(RenderObject* renderer)
{
    collectTextBoxesInFlowBox(flowBoxForRenderer(renderer));
}

void SVGTextQuery::collectTextBoxesInFlowBox(InlineFlowBox* flowBox)
{
    if (!flowBox)
        return;

    for (InlineBox* child = flowBox->firstChild(); child; child = child->nextOnLine()) {
        if (child->isInlineFlowBox()) {
            // Skip generated content.
            if (!child->renderer()->node())
                continue;

            collectTextBoxesInFlowBox(static_cast<InlineFlowBox*>(child));
            continue;
        }

        ASSERT(child->isSVGInlineTextBox());
        m_textBoxes.append(static_cast<SVGInlineTextBox*>(child));
    }
}

bool SVGTextQuery::executeQuery(Data* queryData, ProcessTextChunkPartCallback chunkPartCallback) const
{
    ASSERT(!m_textBoxes.isEmpty());
    bool finished = false;

    // Loop over all text boxes
    const Vector<SVGInlineTextBox*>::const_iterator end = m_textBoxes.end();
    for (Vector<SVGInlineTextBox*>::const_iterator it = m_textBoxes.begin(); it != end; ++it) {
        const SVGInlineTextBox* textBox = *it;
        const Vector<SVGTextChunkPart>& parts = textBox->svgTextChunkParts();

        int processedCharacters = 0;

        // Loop over all text chunk parts in this text box, firing a callback for each chunk part.
        const Vector<SVGTextChunkPart>::const_iterator partEnd = parts.end();
        for (Vector<SVGTextChunkPart>::const_iterator partIt = parts.begin(); partIt != partEnd; ++partIt) {
            if ((this->*chunkPartCallback)(queryData, textBox, *partIt)) {
                finished = true;
                break;
            }

            processedCharacters += partIt->length;
        }

        if (finished)
            break;

        queryData->processedChunkCharacters += processedCharacters;
    }

    return finished;
}

bool SVGTextQuery::mapStartAndLengthIntoChunkPartCoordinates(Data* queryData, const SVGInlineTextBox* textBox, const SVGTextChunkPart& part, int& startPosition, int& endPosition) const
{
    // Reuse the same logic used for text selection & painting, to map our query start/length into start/endPositions of the current text chunk part.
    startPosition -= queryData->processedChunkCharacters;
    endPosition -= queryData->processedChunkCharacters;
    textBox->mapStartEndPositionsIntoChunkPartCoordinates(startPosition, endPosition, part);

    // If startPosition < endPosition, then the position we're supposed to measure lies in this chunk part.
    return startPosition < endPosition;
}

float SVGTextQuery::measureCharacterRange(const SVGInlineTextBox* textBox, RenderStyle* style, bool isVerticalText, int startPosition, int length) const
{
    // FIXME: Vertical writing mode needs to be handled more accurate.
    if (isVerticalText)
        return length * style->font().height();

    const UChar* startCharacter = textBox->textRenderer()->characters() + textBox->start() + startPosition;
    return style->font().floatWidth(svgTextRunForInlineTextBox(startCharacter, length, style, textBox));
}

// numberOfCharacters() implementation
struct NumberOfCharactersData : SVGTextQuery::Data {
    NumberOfCharactersData()
        : characters(0)
    {
    }

    unsigned characters;
};

bool SVGTextQuery::numberOfCharactersCallback(Data* queryData, const SVGInlineTextBox*, const SVGTextChunkPart& part) const
{
    NumberOfCharactersData* data = static_cast<NumberOfCharactersData*>(queryData);
    data->characters += part.length;
    return false;
}

unsigned SVGTextQuery::numberOfCharacters() const
{
    if (m_textBoxes.isEmpty())
        return 0;

    NumberOfCharactersData data;
    executeQuery(&data, &SVGTextQuery::numberOfCharactersCallback);
    return data.characters;
}

// textLength() implementation
struct TextLengthData : SVGTextQuery::Data {
    TextLengthData()
        : textLength(0.0f)
    {
    }

    float textLength;
};

bool SVGTextQuery::textLengthCallback(Data* queryData, const SVGInlineTextBox* textBox, const SVGTextChunkPart& part) const
{
    TextLengthData* data = static_cast<TextLengthData*>(queryData);

    RenderStyle* style = textBox->textRenderer()->style();
    ASSERT(style);

    bool isVerticalText = isVerticalWritingMode(style->svgStyle());
    float partLength = isVerticalText ? part.height : part.width;

    data->textLength += mapLengthThroughChunkTransformation(textBox, isVerticalText, partLength);
    return false;
}

float SVGTextQuery::textLength() const
{
    if (m_textBoxes.isEmpty())
        return 0.0f;

    TextLengthData data;
    executeQuery(&data, &SVGTextQuery::textLengthCallback);
    return data.textLength;
}

// subStringLength() implementation
struct SubStringLengthData : SVGTextQuery::Data {
    SubStringLengthData(unsigned queryStartPosition, unsigned queryLength)
        : startPosition(queryStartPosition)
        , length(queryLength)
        , subStringLength(0.0f)
    {
    }

    unsigned startPosition;
    unsigned length;

    float subStringLength;
};

bool SVGTextQuery::subStringLengthCallback(Data* queryData, const SVGInlineTextBox* textBox, const SVGTextChunkPart& part) const
{
    SubStringLengthData* data = static_cast<SubStringLengthData*>(queryData);

    int startPosition = data->startPosition;
    int endPosition = startPosition + data->length;
    if (!mapStartAndLengthIntoChunkPartCoordinates(queryData, textBox, part, startPosition, endPosition))
        return false;

    RenderStyle* style = textBox->textRenderer()->style();
    ASSERT(style);

    bool isVerticalText = isVerticalWritingMode(style->svgStyle());
    float partLength = measureCharacterRange(textBox, style, isVerticalWritingMode(style->svgStyle()), part.offset + startPosition, endPosition - startPosition);

    data->subStringLength += mapLengthThroughChunkTransformation(textBox, isVerticalText, partLength);
    return false;
}

float SVGTextQuery::subStringLength(unsigned startPosition, unsigned length) const
{
    if (m_textBoxes.isEmpty())
        return 0.0f;

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

bool SVGTextQuery::startPositionOfCharacterCallback(Data* queryData, const SVGInlineTextBox* textBox, const SVGTextChunkPart& part) const
{
    StartPositionOfCharacterData* data = static_cast<StartPositionOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartAndLengthIntoChunkPartCoordinates(queryData, textBox, part, startPosition, endPosition))
        return false;

    const SVGChar& character = *(part.firstCharacter + startPosition);
    data->startPosition = textBox->chunkTransformation().mapPoint(FloatPoint(character.x, character.y));
    return true;
}

FloatPoint SVGTextQuery::startPositionOfCharacter(unsigned position) const
{
    if (m_textBoxes.isEmpty())
        return FloatPoint();

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

bool SVGTextQuery::endPositionOfCharacterCallback(Data* queryData, const SVGInlineTextBox* textBox, const SVGTextChunkPart& part) const
{
    EndPositionOfCharacterData* data = static_cast<EndPositionOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartAndLengthIntoChunkPartCoordinates(queryData, textBox, part, startPosition, endPosition))
        return false;

    const SVGChar& character = *(part.firstCharacter + startPosition);
    data->endPosition = FloatPoint(character.x, character.y);

    RenderStyle* style = textBox->textRenderer()->style();
    ASSERT(style);

    bool isVerticalText = isVerticalWritingMode(style->svgStyle());
    float glyphAdvance = measureCharacterRange(textBox, style, isVerticalText, part.offset + startPosition, 1);

    if (isVerticalText)
        data->endPosition.move(0.0f, glyphAdvance);
    else
        data->endPosition.move(glyphAdvance, 0.0f);

    data->endPosition = textBox->chunkTransformation().mapPoint(data->endPosition);
    return true;
}

FloatPoint SVGTextQuery::endPositionOfCharacter(unsigned position) const
{
    if (m_textBoxes.isEmpty())
        return FloatPoint();

    EndPositionOfCharacterData data(position);
    executeQuery(&data, &SVGTextQuery::endPositionOfCharacterCallback);
    return data.endPosition;
}

// rotationOfCharacter() implementation
struct RotationOfCharacterData : SVGTextQuery::Data {
    RotationOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
        , rotation(0.0f)
    {
    }

    unsigned position;
    float rotation;
};

bool SVGTextQuery::rotationOfCharacterCallback(Data* queryData, const SVGInlineTextBox* textBox, const SVGTextChunkPart& part) const
{
    RotationOfCharacterData* data = static_cast<RotationOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartAndLengthIntoChunkPartCoordinates(queryData, textBox, part, startPosition, endPosition))
        return false;

    const SVGChar& character = *(part.firstCharacter + startPosition);
    data->rotation = character.angle;
    return true;
}

float SVGTextQuery::rotationOfCharacter(unsigned position) const
{
    if (m_textBoxes.isEmpty())
        return 0.0f;

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

bool SVGTextQuery::extentOfCharacterCallback(Data* queryData, const SVGInlineTextBox* textBox, const SVGTextChunkPart& part) const
{
    ExtentOfCharacterData* data = static_cast<ExtentOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartAndLengthIntoChunkPartCoordinates(queryData, textBox, part, startPosition, endPosition))
        return false;
 
    RenderStyle* style = textBox->textRenderer()->style();
    ASSERT(style);

    const SVGChar& character = *(part.firstCharacter + startPosition);
    data->extent = textBox->calculateGlyphBoundaries(style, part.offset + startPosition, character);
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
        : characterNumber(0)
        , position(queryPosition)
    {
    }

    unsigned characterNumber;
    FloatPoint position;
};

bool SVGTextQuery::characterNumberAtPositionCallback(Data* queryData, const SVGInlineTextBox* textBox, const SVGTextChunkPart& part) const
{
    CharacterNumberAtPositionData* data = static_cast<CharacterNumberAtPositionData*>(queryData);

    RenderStyle* style = textBox->textRenderer()->style();
    ASSERT(style);

    for (int i = 0; i < part.length; ++i) {
        FloatRect extent(textBox->calculateGlyphBoundaries(style, part.offset + i, *(part.firstCharacter + i)));
        if (extent.contains(data->position))
            return true;

        ++data->characterNumber;
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

    return data.characterNumber;
}

}

#endif
