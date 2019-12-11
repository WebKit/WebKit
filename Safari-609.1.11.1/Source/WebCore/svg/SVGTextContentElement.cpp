/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "SVGTextContentElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "DOMPoint.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "RenderObject.h"
#include "RenderSVGResource.h"
#include "RenderSVGText.h"
#include "SVGNames.h"
#include "SVGPoint.h"
#include "SVGRect.h"
#include "SVGTextQuery.h"
#include "XMLNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGTextContentElement);
 
SVGTextContentElement::SVGTextContentElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::textLengthAttr, &SVGTextContentElement::m_textLength>();
        PropertyRegistry::registerProperty<SVGNames::lengthAdjustAttr, SVGLengthAdjustType, &SVGTextContentElement::m_lengthAdjust>();
    });
}

unsigned SVGTextContentElement::getNumberOfChars()
{
    document().updateLayoutIgnorePendingStylesheets();
    return SVGTextQuery(renderer()).numberOfCharacters();
}

float SVGTextContentElement::getComputedTextLength()
{
    document().updateLayoutIgnorePendingStylesheets();
    return SVGTextQuery(renderer()).textLength();
}

ExceptionOr<float> SVGTextContentElement::getSubStringLength(unsigned charnum, unsigned nchars)
{
    unsigned numberOfChars = getNumberOfChars();
    if (charnum >= numberOfChars)
        return Exception { IndexSizeError };

    nchars = std::min(nchars, numberOfChars - charnum);
    return SVGTextQuery(renderer()).subStringLength(charnum, nchars);
}

ExceptionOr<Ref<SVGPoint>> SVGTextContentElement::getStartPositionOfChar(unsigned charnum)
{
    if (charnum > getNumberOfChars())
        return Exception { IndexSizeError };

    return SVGPoint::create(SVGTextQuery(renderer()).startPositionOfCharacter(charnum));
}

ExceptionOr<Ref<SVGPoint>> SVGTextContentElement::getEndPositionOfChar(unsigned charnum)
{
    if (charnum > getNumberOfChars())
        return Exception { IndexSizeError };

    return SVGPoint::create(SVGTextQuery(renderer()).endPositionOfCharacter(charnum));
}

ExceptionOr<Ref<SVGRect>> SVGTextContentElement::getExtentOfChar(unsigned charnum)
{
    if (charnum > getNumberOfChars())
        return Exception { IndexSizeError };

    return SVGRect::create(SVGTextQuery(renderer()).extentOfCharacter(charnum));
}

ExceptionOr<float> SVGTextContentElement::getRotationOfChar(unsigned charnum)
{
    if (charnum > getNumberOfChars())
        return Exception { IndexSizeError };

    return SVGTextQuery(renderer()).rotationOfCharacter(charnum);
}

int SVGTextContentElement::getCharNumAtPosition(DOMPointInit&& pointInit)
{
    document().updateLayoutIgnorePendingStylesheets();
    FloatPoint transformPoint {static_cast<float>(pointInit.x), static_cast<float>(pointInit.y)};
    return SVGTextQuery(renderer()).characterNumberAtPosition(transformPoint);
}

ExceptionOr<void> SVGTextContentElement::selectSubString(unsigned charnum, unsigned nchars)
{
    unsigned numberOfChars = getNumberOfChars();
    if (charnum >= numberOfChars)
        return Exception { IndexSizeError };

    nchars = std::min(nchars, numberOfChars - charnum);

    ASSERT(document().frame());

    FrameSelection& selection = document().frame()->selection();

    // Find selection start
    VisiblePosition start(firstPositionInNode(const_cast<SVGTextContentElement*>(this)));
    for (unsigned i = 0; i < charnum; ++i)
        start = start.next();

    // Find selection end
    VisiblePosition end(start);
    for (unsigned i = 0; i < nchars; ++i)
        end = end.next();

    selection.setSelection(VisibleSelection(start, end));

    return { };
}

bool SVGTextContentElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name.matches(XMLNames::spaceAttr))
        return true;
    return SVGGraphicsElement::isPresentationAttribute(name);
}

void SVGTextContentElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name.matches(XMLNames::spaceAttr)) {
        if (value == "preserve")
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWhiteSpace, CSSValuePre);
        else
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWhiteSpace, CSSValueNowrap);
        return;
    }

    SVGGraphicsElement::collectStyleForPresentationAttribute(name, value, style);
}

void SVGTextContentElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::lengthAdjustAttr) {
        auto propertyValue = SVGPropertyTraits<SVGLengthAdjustType>::fromString(value);
        if (propertyValue > 0)
            m_lengthAdjust->setBaseValInternal<SVGLengthAdjustType>(propertyValue);
    } else if (name == SVGNames::textLengthAttr)
        m_textLength->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Other, value, parseError, SVGLengthNegativeValuesMode::Forbid));

    reportAttributeParsingError(parseError, name, value);

    SVGGraphicsElement::parseAttribute(name, value);
}

void SVGTextContentElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        if (attrName == SVGNames::textLengthAttr)
            m_specifiedTextLength = m_textLength->baseVal()->value();

        if (auto renderer = this->renderer()) {
            InstanceInvalidationGuard guard(*this);
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

SVGAnimatedLength& SVGTextContentElement::textLengthAnimated()
{
    static NeverDestroyed<SVGLengthValue> defaultTextLength(SVGLengthMode::Other);
    if (m_textLength->baseVal()->value() == defaultTextLength)
        m_textLength->baseVal()->value() = { getComputedTextLength(), SVGLengthType::Number };
    return m_textLength;
}

bool SVGTextContentElement::selfHasRelativeLengths() const
{
    // Any element of the <text> subtree is advertized as using relative lengths.
    // On any window size change, we have to relayout the text subtree, as the
    // effective 'on-screen' font size may change.
    return true;
}

SVGTextContentElement* SVGTextContentElement::elementFromRenderer(RenderObject* renderer)
{
    if (!renderer)
        return nullptr;

    if (!renderer->isSVGText() && !renderer->isSVGInline())
        return nullptr;

    SVGElement* element = downcast<SVGElement>(renderer->node());
    ASSERT(element);

    if (!is<SVGTextContentElement>(element))
        return nullptr;

    return downcast<SVGTextContentElement>(element);
}

}
