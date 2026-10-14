/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "SVGTransformList.h"

#include "ExceptionOr.h"
#include "SVGParserUtilities.h"
#include "SVGTransform.h"
#include "SVGTransformListInlines.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/StringView.h>

namespace WebCore {

template<typename CharacterType> static constexpr std::array<CharacterType, 5> skewXDesc  { 's', 'k', 'e', 'w', 'X' };
template<typename CharacterType> static constexpr std::array<CharacterType, 5> skewYDesc  { 's', 'k', 'e', 'w', 'Y' };
template<typename CharacterType> static constexpr std::array<CharacterType, 5> scaleDesc  { 's', 'c', 'a', 'l', 'e' };
template<typename CharacterType> static constexpr std::array<CharacterType, 9> translateDesc  { 't', 'r', 'a', 'n', 's', 'l', 'a', 't', 'e' };
template<typename CharacterType> static constexpr std::array<CharacterType, 6> rotateDesc  { 'r', 'o', 't', 'a', 't', 'e' };
template<typename CharacterType> static constexpr std::array<CharacterType, 6> matrixDesc  { 'm', 'a', 't', 'r', 'i', 'x' };

template<typename CharacterType> static std::optional<SVGTransformValue::SVGTransformType> parseTransformTypeGeneric(StringParsingBuffer<CharacterType>& buffer)
{
    if (buffer.atEnd())
        return std::nullopt;

    if (*buffer == 's') {
        if (skipCharactersExactly(buffer, std::span { skewXDesc<CharacterType> }))
            return SVGTransformValue::SVG_TRANSFORM_SKEWX;
        if (skipCharactersExactly(buffer, std::span { skewYDesc<CharacterType> }))
            return SVGTransformValue::SVG_TRANSFORM_SKEWY;
        if (skipCharactersExactly(buffer, std::span { scaleDesc<CharacterType> }))
            return SVGTransformValue::SVG_TRANSFORM_SCALE;
        return std::nullopt;
    }

    if (skipCharactersExactly(buffer, std::span { translateDesc<CharacterType> }))
        return SVGTransformValue::SVG_TRANSFORM_TRANSLATE;
    if (skipCharactersExactly(buffer, std::span { rotateDesc<CharacterType> }))
        return SVGTransformValue::SVG_TRANSFORM_ROTATE;
    if (skipCharactersExactly(buffer, std::span { matrixDesc<CharacterType> }))
        return SVGTransformValue::SVG_TRANSFORM_MATRIX;

    return std::nullopt;
}

std::optional<SVGTransformValue::SVGTransformType> SVGTransformList::parseTransformType(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) {
        return parseTransformType(buffer);
    });
}

std::optional<SVGTransformValue::SVGTransformType> SVGTransformList::parseTransformType(StringParsingBuffer<Latin1Character>& buffer)
{
    return parseTransformTypeGeneric(buffer);
}

std::optional<SVGTransformValue::SVGTransformType> SVGTransformList::parseTransformType(StringParsingBuffer<char16_t>& buffer)
{
    return parseTransformTypeGeneric(buffer);
}

ExceptionOr<RefPtr<SVGTransform>> SVGTransformList::consolidate()
{
    auto result = canAlterList();
    if (result.hasException())
        return result.releaseException();
    ASSERT(result.releaseReturnValue());

    // Spec: If the list was empty, then a value of null is returned.
    if (m_items.isEmpty())
        return nullptr;

    if (m_items.size() == 1)
        return RefPtr { at(0).ptr() };

    auto newItem = SVGTransform::create(*concatenate());
    clearItems();

    auto item = append(WTF::move(newItem));
    commitChange();
    return RefPtr { item.ptr() };
}

std::optional<AffineTransform> SVGTransformList::concatenate() const
{
    if (m_items.isEmpty())
        return std::nullopt;
    auto items = m_items.span();
    AffineTransform result = items.front()->matrix().value();
    for (auto& transform : items.subspan(1))
        result *= transform->matrix().value();
    return result;
}

void SVGTransformList::setTransformValue(SVGTransformValue& transform, const ParsedTransform& parsedTransform)
{
    auto& values = parsedTransform.values;

    switch (parsedTransform.type) {
    case SVGTransformValue::SVG_TRANSFORM_UNKNOWN:
        ASSERT_NOT_REACHED();
        return;

    case SVGTransformValue::SVG_TRANSFORM_SKEWX:
        transform.setSkewX(values[0]);
        return;

    case SVGTransformValue::SVG_TRANSFORM_SKEWY:
        transform.setSkewY(values[0]);
        return;

    case SVGTransformValue::SVG_TRANSFORM_SCALE:
        // Spec: if only one param given, assume uniform scaling.
        if (parsedTransform.valueCount == 1)
            transform.setScale(values[0], values[0]);
        else
            transform.setScale(values[0], values[1]);
        return;

    case SVGTransformValue::SVG_TRANSFORM_TRANSLATE:
        // Spec: if only one param given, assume 2nd param to be 0.
        if (parsedTransform.valueCount == 1)
            transform.setTranslate(values[0], 0);
        else
            transform.setTranslate(values[0], values[1]);
        return;

    case SVGTransformValue::SVG_TRANSFORM_ROTATE:
        if (parsedTransform.valueCount == 1)
            transform.setRotate(values[0], 0, 0);
        else
            transform.setRotate(values[0], values[1], values[2]);
        return;

    case SVGTransformValue::SVG_TRANSFORM_MATRIX:
        transform.setMatrix(AffineTransform(values[0], values[1], values[2], values[3], values[4], values[5]));
        return;
    }
}

template<typename CharacterType> bool SVGTransformList::parseGeneric(StringParsingBuffer<CharacterType>& buffer, ParsedTransforms& transforms)
{
    bool delimParsed = false;
    skipOptionalSVGSpaces(buffer);

    while (buffer.hasCharactersRemaining()) {
        delimParsed = false;

        auto parsedTransformType = parseTransformType(buffer);
        if (!parsedTransformType)
            return false;

        auto parsedTransform = parseTransform(*parsedTransformType, buffer);
        if (!parsedTransform)
            return false;

        transforms.append(*parsedTransform);

        skipOptionalSVGSpaces(buffer);

        if (skipExactly(buffer, ','))
            delimParsed = true;

        skipOptionalSVGSpaces(buffer);
    }

    return !delimParsed;
}

template<typename CharacterType> bool SVGTransformList::parseAndAppend(StringParsingBuffer<CharacterType>& buffer)
{
    ParsedTransforms transforms;
    bool parsingSucceeded = parseGeneric(buffer, transforms);

    // SVGViewSpec stops parsing at the ')' that closes transform(...), so keep
    // the transforms read before the parsing stopped.
    for (auto& parsedTransform : transforms) {
        SVGTransformValue transform;
        setTransformValue(transform, parsedTransform);
        append(SVGTransform::create(WTF::move(transform)));
    }
    return parsingSucceeded;
}

bool SVGTransformList::parse(StringView value)
{
    ParsedTransforms transforms;

    bool parsingSucceeded = readCharactersForParsing(value, [&](auto buffer) {
        return parseGeneric(buffer, transforms);
    });
    if (!parsingSucceeded) {
        clearItems();
        return false;
    }

    // Spec: the steps for synchronizing a list interface object. The existing
    // SVGTransforms, and their SVGMatrix, are reused whatever their transform
    // function, so a value that changes only its numbers costs no memory.
    resize(transforms.size());
    for (size_t index = 0; index < transforms.size(); ++index)
        setTransformValue(protect(m_items[index])->value(), transforms[index]);
    return true;
}

bool SVGTransformList::parse(StringParsingBuffer<Latin1Character>& buffer)
{
    return parseAndAppend(buffer);
}

bool SVGTransformList::parse(StringParsingBuffer<char16_t>& buffer)
{
    return parseAndAppend(buffer);
}

String SVGTransformList::valueAsString() const
{
    StringBuilder builder;
    for (const auto& transform : m_items) {
        if (builder.length())
            builder.append(' ');

        builder.append(transform->value().valueAsString());
    }
    return builder.toString();
}

}
