/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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

#if ENABLE(SVG)
#include "SVGTextContentElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "XMLNames.h"

namespace WebCore {

SVGTextContentElement::SVGTextContentElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_textLength(this, LengthModeWidth)
    , m_lengthAdjust(0)
{
}

SVGTextContentElement::~SVGTextContentElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGTextContentElement, SVGLength, Length, length, TextLength, textLength, SVGNames::textLengthAttr.localName(), m_textLength)
ANIMATED_PROPERTY_DEFINITIONS(SVGTextContentElement, int, Enumeration, enumeration, LengthAdjust, lengthAdjust, SVGNames::lengthAdjustAttr.localName(), m_lengthAdjust)

long SVGTextContentElement::getNumberOfChars() const
{
    return 0;
}

float SVGTextContentElement::getComputedTextLength() const
{
    return 0.0f;
}

float SVGTextContentElement::getSubStringLength(unsigned long charnum, unsigned long nchars, ExceptionCode&) const
{
    return 0.0f;
}

FloatPoint SVGTextContentElement::getStartPositionOfChar(unsigned long charnum, ExceptionCode&) const
{
    return FloatPoint();
}

FloatPoint SVGTextContentElement::getEndPositionOfChar(unsigned long charnum, ExceptionCode&) const
{
    return FloatPoint();
}

FloatRect SVGTextContentElement::getExtentOfChar(unsigned long charnum, ExceptionCode&) const
{
    return FloatRect();
}

float SVGTextContentElement::getRotationOfChar(unsigned long charnum, ExceptionCode&) const
{
    return 0.0f;
}

long SVGTextContentElement::getCharNumAtPosition(const FloatPoint& point) const
{
    return 0;
}

void SVGTextContentElement::selectSubString(unsigned long charnum, unsigned long nchars, ExceptionCode&) const
{
}

void SVGTextContentElement::parseMappedAttribute(MappedAttribute* attr)
{
    //if (attr->name() == SVGNames::lengthAdjustAttr)
    //    setXBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    //else
    if (attr->name() == SVGNames::textLengthAttr) {
        setTextLengthBaseValue(SVGLength(this, LengthModeOther, attr->value()));
        if (textLength().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for text attribute <textLength> is not allowed");
    } else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr)) {
            if (attr->name().matches(XMLNames::spaceAttr)) {
                static const AtomicString preserveString("preserve");

                if (attr->value() == preserveString)
                    addCSSProperty(attr, CSS_PROP_WHITE_SPACE, CSS_VAL_PRE);
                else
                    addCSSProperty(attr, CSS_PROP_WHITE_SPACE, CSS_VAL_NOWRAP);
            }
            return;
        }
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
