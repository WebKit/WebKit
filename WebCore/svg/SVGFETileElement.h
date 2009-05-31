/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGFETileElement_h
#define SVGFETileElement_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGFETile.h"

namespace WebCore {

    class SVGFETileElement : public SVGFilterPrimitiveStandardAttributes {
    public:
        SVGFETileElement(const QualifiedName&, Document*);
        virtual ~SVGFETileElement();

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual bool build(SVGResourceFilter*);

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFETileElement, SVGNames::feTileTagString, SVGNames::inAttrString, String, In1, in1)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
