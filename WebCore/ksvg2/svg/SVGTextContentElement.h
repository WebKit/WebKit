/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_SVGTextContentElementImpl_H
#define KSVG_SVGTextContentElementImpl_H
#if SVG_SUPPORT

#include "SVGStyledElement.h"
#include "SVGTests.h"
#include "SVGLangSpace.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore {
    class SVGAnimatedLength;
    class SVGAnimatedEnumeration;

    class SVGTextContentElement : public SVGStyledElement,
                                      public SVGTests,
                                      public SVGLangSpace,
                                      public SVGExternalResourcesRequired
    {
    public:
        SVGTextContentElement(const QualifiedName&, Document*);
        virtual ~SVGTextContentElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGTextContentElement' functions
        SVGAnimatedLength *textLength() const;
        SVGAnimatedEnumeration *lengthAdjust() const;

        long getNumberOfChars() const;
        float getComputedTextLength() const;
        float getSubStringLength(unsigned long charnum, unsigned long nchars) const;
        FloatPoint getStartPositionOfChar(unsigned long charnum) const;
        FloatPoint getEndPositionOfChar(unsigned long charnum) const;
        FloatRect getExtentOfChar(unsigned long charnum) const;
        float getRotationOfChar(unsigned long charnum) const;
        long getCharNumAtPosition(const FloatPoint&) const;
        void selectSubString(unsigned long charnum, unsigned long nchars) const;

        virtual void parseMappedAttribute(MappedAttribute*);

    private:
        mutable RefPtr<SVGAnimatedLength> m_textLength;
        mutable RefPtr<SVGAnimatedEnumeration> m_lengthAdjust;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
