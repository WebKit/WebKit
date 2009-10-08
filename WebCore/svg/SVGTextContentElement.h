/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>

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

#ifndef SVGTextContentElement_h
#define SVGTextContentElement_h

#if ENABLE(SVG)
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledElement.h"
#include "SVGTests.h"

namespace WebCore {

    extern char SVGTextContentElementIdentifier[];

    class SVGLength;

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

        SVGTextContentElement(const QualifiedName&, Document*);
        virtual ~SVGTextContentElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }
        virtual bool isTextContent() const { return true; }

        unsigned getNumberOfChars() const;
        float getComputedTextLength() const;
        float getSubStringLength(unsigned charnum, unsigned nchars, ExceptionCode&) const;
        FloatPoint getStartPositionOfChar(unsigned charnum, ExceptionCode&) const;
        FloatPoint getEndPositionOfChar(unsigned charnum, ExceptionCode&) const;
        FloatRect getExtentOfChar(unsigned charnum, ExceptionCode&) const;
        float getRotationOfChar(unsigned charnum, ExceptionCode&) const;
        int getCharNumAtPosition(const FloatPoint&) const;
        void selectSubString(unsigned charnum, unsigned nchars, ExceptionCode&) const;

        virtual void parseMappedAttribute(MappedAttribute*);

        bool isKnownAttribute(const QualifiedName&);

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGTextContentElement, SVGTextContentElementIdentifier, SVGNames::textLengthAttrString, SVGLength, TextLength, textLength)
        ANIMATED_PROPERTY_DECLARATIONS(SVGTextContentElement, SVGTextContentElementIdentifier, SVGNames::lengthAdjustAttrString, int, LengthAdjust, lengthAdjust)

        // SVGExternalResourcesRequired
        ANIMATED_PROPERTY_DECLARATIONS(SVGExternalResourcesRequired, SVGExternalResourcesRequiredIdentifier,
                                       SVGNames::externalResourcesRequiredAttrString, bool,
                                       ExternalResourcesRequired, externalResourcesRequired) 
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
