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

#include "SVGStyledElementImpl.h"
#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

namespace KSVG
{
    class SVGRectImpl;
    class SVGPointImpl;
    class SVGAnimatedLengthImpl;
    class SVGAnimatedEnumerationImpl;

    class SVGTextContentElementImpl : public SVGStyledElementImpl,
                                      public SVGTestsImpl,
                                      public SVGLangSpaceImpl,
                                      public SVGExternalResourcesRequiredImpl
    {
    public:
        SVGTextContentElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id,  KDOM::DOMStringImpl *prefix);
        virtual ~SVGTextContentElementImpl();

        // 'SVGTextContentElement' functions
        SVGAnimatedLengthImpl *textLength() const;
        SVGAnimatedEnumerationImpl *lengthAdjust() const;

        long getNumberOfChars() const;
        float getComputedTextLength() const;
        float getSubStringLength(unsigned long charnum, unsigned long nchars) const;
        SVGPointImpl *getStartPositionOfChar(unsigned long charnum) const;
        SVGPointImpl *getEndPositionOfChar(unsigned long charnum) const;
        SVGRectImpl *getExtentOfChar(unsigned long charnum) const;
        float getRotationOfChar(unsigned long charnum) const;
        long getCharNumAtPosition(SVGPointImpl *point) const;
        void selectSubString(unsigned long charnum, unsigned long nchars) const;

        virtual void parseAttribute(KDOM::AttributeImpl *attr);

    private:
        mutable SVGAnimatedLengthImpl *m_textLength;
        mutable SVGAnimatedEnumerationImpl *m_lengthAdjust;
    };
};

#endif

// vim:ts=4:noet
