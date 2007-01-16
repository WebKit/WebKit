/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#ifndef SVGStyledLocatableElement_h
#define SVGStyledLocatableElement_h
#ifdef SVG_SUPPORT

#include "SVGLocatable.h"
#include "SVGStyledElement.h"

namespace WebCore {

    class SVGElement;

    class SVGStyledLocatableElement : public SVGStyledElement, virtual public SVGLocatable {
    public:
        SVGStyledLocatableElement(const QualifiedName&, Document*);
        virtual ~SVGStyledLocatableElement();
        
        virtual bool isStyledLocatable() const { return true; }

        // 'SVGStyledLocatableElement' functions
        virtual SVGElement* nearestViewportElement() const;
        virtual SVGElement* farthestViewportElement() const;

        virtual FloatRect getBBox() const;
        virtual AffineTransform getCTM() const;
        virtual AffineTransform getScreenCTM() const;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // SVGStyledLocatableElement_h

// vim:ts=4:noet
