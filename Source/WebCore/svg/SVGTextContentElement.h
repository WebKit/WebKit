/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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

#ifndef SVGTextContentElement_h
#define SVGTextContentElement_h

#if ENABLE(SVG)
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledElement.h"
#include "SVGTests.h"

namespace WebCore {

class SVGTextContentElement : public SVGStyledElement,
                              public SVGTests,
                              public SVGLangSpace,
                              public SVGExternalResourcesRequired {
public:
    enum SVGLengthAdjustType {
        LENGTHADJUST_UNKNOWN            = 0,
        LENGTHADJUST_SPACING            = 1,
        LENGTHADJUST_SPACINGANDGLYPHS   = 2
    };

    unsigned getNumberOfChars() const;
    float getComputedTextLength() const;
    float getSubStringLength(unsigned charnum, unsigned nchars, ExceptionCode&) const;
    FloatPoint getStartPositionOfChar(unsigned charnum, ExceptionCode&) const;
    FloatPoint getEndPositionOfChar(unsigned charnum, ExceptionCode&) const;
    FloatRect getExtentOfChar(unsigned charnum, ExceptionCode&) const;
    float getRotationOfChar(unsigned charnum, ExceptionCode&) const;
    int getCharNumAtPosition(const FloatPoint&) const;
    void selectSubString(unsigned charnum, unsigned nchars, ExceptionCode&) const;

    static SVGTextContentElement* elementFromRenderer(RenderObject*);

    // textLength is not declared using the standard DECLARE_ANIMATED_LENGTH macro
    // as its getter needs special handling (return getComputedTextLength(), instead of m_textLength).
    SVGLength& specifiedTextLength() { return m_specifiedTextLength; }
    PassRefPtr<SVGAnimatedLength> textLengthAnimated();

protected:
    SVGTextContentElement(const QualifiedName&, Document*);

    virtual bool isValid() const { return SVGTests::isValid(); }

    virtual void parseMappedAttribute(Attribute*);
    virtual void svgAttributeChanged(const QualifiedName&);
    virtual void synchronizeProperty(const QualifiedName&);
    void fillPassedAttributeToPropertyTypeMap(AttributeToPropertyTypeMap&);

    virtual bool selfHasRelativeLengths() const;

private:
    virtual bool isTextContent() const { return true; }

    // Animated property declarations
    void synchronizeTextLength();
    SVGLength m_specifiedTextLength;
    mutable SVGSynchronizableAnimatedProperty<SVGLength> m_textLength;
    DECLARE_ANIMATED_ENUMERATION(LengthAdjust, lengthAdjust, SVGLengthAdjustType)

    // SVGExternalResourcesRequired
    DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired) 
};

template<>
struct SVGPropertyTraits<SVGTextContentElement::SVGLengthAdjustType> {
    static SVGTextContentElement::SVGLengthAdjustType highestEnumValue() { return SVGTextContentElement::LENGTHADJUST_SPACINGANDGLYPHS; }

    static String toString(SVGTextContentElement::SVGLengthAdjustType type)
    {
        switch (type) {
        case SVGTextContentElement::LENGTHADJUST_UNKNOWN:
            return emptyString();
        case SVGTextContentElement::LENGTHADJUST_SPACING:
            return "spacing";
        case SVGTextContentElement::LENGTHADJUST_SPACINGANDGLYPHS:
            return "spacingAndGlyphs";
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static SVGTextContentElement::SVGLengthAdjustType fromString(const String& value)
    {
        if (value == "spacingAndGlyphs")
            return SVGTextContentElement::LENGTHADJUST_SPACINGANDGLYPHS;
        if (value == "spacing")
            return SVGTextContentElement::LENGTHADJUST_SPACING;
        return SVGTextContentElement::LENGTHADJUST_UNKNOWN;
    }
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
