/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
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
#include "SVGTextContentElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Frame.h"
#include "RenderObject.h"
#include "SVGTextQuery.h"
#include "SelectionController.h"
#include "XMLNames.h"

namespace WebCore {

SVGTextContentElement::SVGTextContentElement(const QualifiedName& tagName, Document* document)
    : SVGStyledElement(tagName, document)
    , m_textLength(LengthModeOther)
    , m_lengthAdjust(LENGTHADJUST_SPACING)
{
}

unsigned SVGTextContentElement::getNumberOfChars() const
{
    document()->updateLayoutIgnorePendingStylesheets();
    return SVGTextQuery(renderer()).numberOfCharacters();
}

float SVGTextContentElement::getComputedTextLength() const
{
    document()->updateLayoutIgnorePendingStylesheets();
    return SVGTextQuery(renderer()).textLength();
}

float SVGTextContentElement::getSubStringLength(unsigned charnum, unsigned nchars, ExceptionCode& ec) const
{
    document()->updateLayoutIgnorePendingStylesheets();

    unsigned numberOfChars = getNumberOfChars();
    if (charnum >= numberOfChars) {
        ec = INDEX_SIZE_ERR;
        return 0.0f;
    }

    return SVGTextQuery(renderer()).subStringLength(charnum, nchars);
}

FloatPoint SVGTextContentElement::getStartPositionOfChar(unsigned charnum, ExceptionCode& ec) const
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (charnum > getNumberOfChars()) {
        ec = INDEX_SIZE_ERR;
        return FloatPoint();
    }

    return SVGTextQuery(renderer()).startPositionOfCharacter(charnum);
}

FloatPoint SVGTextContentElement::getEndPositionOfChar(unsigned charnum, ExceptionCode& ec) const
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (charnum > getNumberOfChars()) {
        ec = INDEX_SIZE_ERR;
        return FloatPoint();
    }

    return SVGTextQuery(renderer()).endPositionOfCharacter(charnum);
}

FloatRect SVGTextContentElement::getExtentOfChar(unsigned charnum, ExceptionCode& ec) const
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (charnum > getNumberOfChars()) {
        ec = INDEX_SIZE_ERR;
        return FloatRect();
    }

    return SVGTextQuery(renderer()).extentOfCharacter(charnum);
}

float SVGTextContentElement::getRotationOfChar(unsigned charnum, ExceptionCode& ec) const
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (charnum > getNumberOfChars()) {
        ec = INDEX_SIZE_ERR;
        return 0.0f;
    }

    return SVGTextQuery(renderer()).rotationOfCharacter(charnum);
}

int SVGTextContentElement::getCharNumAtPosition(const FloatPoint& point) const
{
    document()->updateLayoutIgnorePendingStylesheets();
    return SVGTextQuery(renderer()).characterNumberAtPosition(point);
}

void SVGTextContentElement::selectSubString(unsigned charnum, unsigned nchars, ExceptionCode& ec) const
{
    unsigned numberOfChars = getNumberOfChars();
    if (charnum >= numberOfChars) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (nchars > numberOfChars - charnum)
        nchars = numberOfChars - charnum;

    ASSERT(document());
    ASSERT(document()->frame());

    SelectionController* controller = document()->frame()->selection();
    if (!controller)
        return;

    // Find selection start
    VisiblePosition start(const_cast<SVGTextContentElement*>(this), 0, SEL_DEFAULT_AFFINITY);
    for (unsigned i = 0; i < charnum; ++i)
        start = start.next();

    // Find selection end
    VisiblePosition end(start);
    for (unsigned i = 0; i < nchars; ++i)
        end = end.next();

    controller->setSelection(VisibleSelection(start, end));
}

void SVGTextContentElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == SVGNames::lengthAdjustAttr) {
        if (attr->value() == "spacing")
            setLengthAdjustBaseValue(LENGTHADJUST_SPACING);
        else if (attr->value() == "spacingAndGlyphs")
            setLengthAdjustBaseValue(LENGTHADJUST_SPACINGANDGLYPHS);
    } else if (attr->name() == SVGNames::textLengthAttr) {
        setTextLengthBaseValue(SVGLength(LengthModeOther, attr->value()));
        if (textLengthBaseValue().value(this) < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for text attribute <textLength> is not allowed");
    } else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr)) {
            if (attr->name().matches(XMLNames::spaceAttr)) {
                DEFINE_STATIC_LOCAL(const AtomicString, preserveString, ("preserve"));

                if (attr->value() == preserveString)
                    addCSSProperty(attr, CSSPropertyWhiteSpace, CSSValuePre);
                else
                    addCSSProperty(attr, CSSPropertyWhiteSpace, CSSValueNowrap);
            }
            return;
        }
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

void SVGTextContentElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGStyledElement::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeLengthAdjust();
        synchronizeTextLength();
        synchronizeExternalResourcesRequired();
        return;
    }

    if (attrName == SVGNames::lengthAdjustAttr)
        synchronizeLengthAdjust();
    else if (attrName == SVGNames::textLengthAttr)
        synchronizeTextLength();
    else if (SVGExternalResourcesRequired::isKnownAttribute(attrName))
        synchronizeExternalResourcesRequired();
}

bool SVGTextContentElement::isKnownAttribute(const QualifiedName& attrName)
{
    return (attrName.matches(SVGNames::lengthAdjustAttr) ||
            attrName.matches(SVGNames::textLengthAttr) ||
            SVGTests::isKnownAttribute(attrName) ||
            SVGLangSpace::isKnownAttribute(attrName) ||
            SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
            SVGStyledElement::isKnownAttribute(attrName));
}

bool SVGTextContentElement::selfHasRelativeLengths() const
{
    return textLength().isRelative();
}

SVGTextContentElement* SVGTextContentElement::elementFromRenderer(RenderObject* renderer)
{
    if (!renderer)
        return 0;

    if (!renderer->isSVGText() && !renderer->isSVGInline())
        return 0;

    Node* node = renderer->node();
    ASSERT(node);
    ASSERT(node->isSVGElement());

    if (!node->hasTagName(SVGNames::textTag)
        && !node->hasTagName(SVGNames::tspanTag)
#if ENABLE(SVG_FONTS)
        && !node->hasTagName(SVGNames::altGlyphTag)
#endif
        && !node->hasTagName(SVGNames::trefTag)
        && !node->hasTagName(SVGNames::textPathTag))
        return 0;

    return static_cast<SVGTextContentElement*>(node);
}

}

#endif // ENABLE(SVG)
