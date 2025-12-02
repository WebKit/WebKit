/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGFitToViewBox.h"

#include "AffineTransform.h"
#include "Document.h"
#include "FloatRect.h"
#include "NodeDocument.h"
#include "NodeInlines.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGPreserveAspectRatioValue.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/StringView.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGFitToViewBox);

SVGFitToViewBox::SVGFitToViewBox(SVGElement* contextElement, SVGPropertyAccess access)
    : m_viewBox(SVGAnimatedRect::create(contextElement, access))
    , m_preserveAspectRatio(SVGAnimatedPreserveAspectRatio::create(contextElement, access))
{
    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::viewBoxAttr, &SVGFitToViewBox::m_viewBox>();
        PropertyRegistry::registerProperty<SVGNames::preserveAspectRatioAttr, &SVGFitToViewBox::m_preserveAspectRatio>();
    }
}

void SVGFitToViewBox::setViewBox(const FloatRect& viewBox)
{
    Ref { m_viewBox }->setBaseValInternal(viewBox);
    m_isViewBoxValid = true;
}

void SVGFitToViewBox::resetViewBox()
{
    Ref { m_viewBox }->setBaseValInternal({ });
    m_isViewBoxValid = false;
}

void SVGFitToViewBox::reset()
{
    resetViewBox();
    resetPreserveAspectRatio();
}

bool SVGFitToViewBox::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::viewBoxAttr) {
        if (!value.isNull()) {
            if (auto result = parseViewBox(value)) {
                setViewBox(WTFMove(*result));
                return true;
            }
        }
        resetViewBox();
        return true;
    }

    if (name == SVGNames::preserveAspectRatioAttr) {
        setPreserveAspectRatio(SVGPreserveAspectRatioValue { value });
        return true;
    }

    return false;
}

std::optional<FloatRect> SVGFitToViewBox::parseViewBox(StringView value)
{
    return readCharactersForParsing(value, [&](auto buffer) {
        return parseViewBoxGeneric(buffer);
    });
}

std::optional<FloatRect> SVGFitToViewBox::parseViewBox(StringParsingBuffer<Latin1Character>& buffer, bool validate)
{
    return parseViewBoxGeneric(buffer, validate);
}

std::optional<FloatRect> SVGFitToViewBox::parseViewBox(StringParsingBuffer<char16_t>& buffer, bool validate)
{
    return parseViewBoxGeneric(buffer, validate);
}

template<typename CharacterType> std::optional<FloatRect> SVGFitToViewBox::parseViewBoxGeneric(StringParsingBuffer<CharacterType>& buffer, bool validate)
{
    StringView stringToParse = buffer.stringViewOfCharactersRemaining();

    skipOptionalSVGSpaces(buffer);

    auto x = parseNumber(buffer);
    auto y = parseNumber(buffer);
    auto width = parseNumber(buffer);
    auto height = parseNumber(buffer, SuffixSkippingPolicy::DontSkip);

    if (validate) {
        Ref document = Ref { m_viewBox }->contextElement()->document();

        if (!x || !y || !width || !height) {
            document->checkedSVGExtensions()->reportWarning(makeString("Problem parsing viewBox=\""_s, stringToParse, "\""_s));
            return std::nullopt;
        }

        // Check that width is positive.
        if (*width < 0.0) {
            document->checkedSVGExtensions()->reportError("A negative value for ViewBox width is not allowed"_s);
            return std::nullopt;
        }

        // Check that height is positive.
        if (*height < 0.0) {
            document->checkedSVGExtensions()->reportError("A negative value for ViewBox height is not allowed"_s);
            return std::nullopt;
        }

        // Nothing should come after the last, fourth number.
        skipOptionalSVGSpaces(buffer);
        if (buffer.hasCharactersRemaining()) {
            document->checkedSVGExtensions()->reportWarning(makeString("Problem parsing viewBox=\""_s, stringToParse, "\""_s));
            return std::nullopt;
        }
    }

    return FloatRect { x.value_or(0), y.value_or(0), width.value_or(0), height.value_or(0) };
}

AffineTransform SVGFitToViewBox::viewBoxToViewTransform(const FloatRect& viewBoxRect, const SVGPreserveAspectRatioValue& preserveAspectRatio, float viewWidth, float viewHeight)
{
    if (!viewBoxRect.width() || !viewBoxRect.height() || !viewWidth || !viewHeight)
        return AffineTransform();

    return preserveAspectRatio.getCTM(viewBoxRect.x(), viewBoxRect.y(), viewBoxRect.width(), viewBoxRect.height(), viewWidth, viewHeight);
}

}
