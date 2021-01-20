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
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGPreserveAspectRatioValue.h"
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/StringView.h>

namespace WebCore {

SVGFitToViewBox::SVGFitToViewBox(SVGElement* contextElement, SVGPropertyAccess access)
    : m_viewBox(SVGAnimatedRect::create(contextElement, access))
    , m_preserveAspectRatio(SVGAnimatedPreserveAspectRatio::create(contextElement, access))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::viewBoxAttr, &SVGFitToViewBox::m_viewBox>();
        PropertyRegistry::registerProperty<SVGNames::preserveAspectRatioAttr, &SVGFitToViewBox::m_preserveAspectRatio>();
    });
}

void SVGFitToViewBox::setViewBox(const FloatRect& viewBox)
{
    m_viewBox->setBaseValInternal(viewBox);
    m_isViewBoxValid = true;
}

void SVGFitToViewBox::resetViewBox()
{
    m_viewBox->setBaseValInternal({ });
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

Optional<FloatRect> SVGFitToViewBox::parseViewBox(StringView value)
{
    return readCharactersForParsing(value, [&](auto buffer) {
        return parseViewBoxGeneric(buffer);
    });
}

Optional<FloatRect> SVGFitToViewBox::parseViewBox(StringParsingBuffer<LChar>& buffer, bool validate)
{
    return parseViewBoxGeneric(buffer, validate);
}

Optional<FloatRect> SVGFitToViewBox::parseViewBox(StringParsingBuffer<UChar>& buffer, bool validate)
{
    return parseViewBoxGeneric(buffer, validate);
}

template<typename CharacterType> Optional<FloatRect> SVGFitToViewBox::parseViewBoxGeneric(StringParsingBuffer<CharacterType>& buffer, bool validate)
{
    StringView stringToParse = buffer.stringViewOfCharactersRemaining();

    skipOptionalSVGSpaces(buffer);

    auto x = parseNumber(buffer);
    auto y = parseNumber(buffer);
    auto width = parseNumber(buffer);
    auto height = parseNumber(buffer, SuffixSkippingPolicy::DontSkip);

    if (validate) {
        Document& document = m_viewBox->contextElement()->document();

        if (!x || !y || !width || !height) {
            document.accessSVGExtensions().reportWarning(makeString("Problem parsing viewBox=\"", stringToParse, "\""));
            return WTF::nullopt;
        }

        // Check that width is positive.
        if (*width < 0.0) {
            document.accessSVGExtensions().reportError("A negative value for ViewBox width is not allowed");
            return WTF::nullopt;
        }

        // Check that height is positive.
        if (*height < 0.0) {
            document.accessSVGExtensions().reportError("A negative value for ViewBox height is not allowed");
            return WTF::nullopt;
        }

        // Nothing should come after the last, fourth number.
        skipOptionalSVGSpaces(buffer);
        if (buffer.hasCharactersRemaining()) {
            document.accessSVGExtensions().reportWarning(makeString("Problem parsing viewBox=\"", stringToParse, "\""));
            return WTF::nullopt;
        }
    }

    return FloatRect { x.valueOr(0), y.valueOr(0), width.valueOr(0), height.valueOr(0) };
}

AffineTransform SVGFitToViewBox::viewBoxToViewTransform(const FloatRect& viewBoxRect, const SVGPreserveAspectRatioValue& preserveAspectRatio, float viewWidth, float viewHeight)
{
    if (!viewBoxRect.width() || !viewBoxRect.height() || !viewWidth || !viewHeight)
        return AffineTransform();

    return preserveAspectRatio.getCTM(viewBoxRect.x(), viewBoxRect.y(), viewBoxRect.width(), viewBoxRect.height(), viewWidth, viewHeight);
}

}
