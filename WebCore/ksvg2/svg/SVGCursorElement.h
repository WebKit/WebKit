/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
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

#ifndef SVGCursorElement_H
#define SVGCursorElement_H

#ifdef SVG_SUPPORT

#include "Image.h"

#include "SVGLength.h"
#include "SVGElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"
#include "SVGExternalResourcesRequired.h"
#include "CachedResourceClient.h"

namespace WebCore
{
    class SVGCursorElement : public SVGElement,
                                 public SVGTests,
                                 public SVGExternalResourcesRequired,
                                 public SVGURIReference,
                                 public CachedResourceClient
    {
    public:
        SVGCursorElement(const QualifiedName&, Document*);
        virtual ~SVGCursorElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // 'SVGCursorElement' functions
        virtual void parseMappedAttribute(MappedAttribute *attr);

        CachedImage* cachedImage() const { return m_cachedImage; }

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGURIReference, String, Href, href)

        ANIMATED_PROPERTY_DECLARATIONS(SVGCursorElement, SVGLength, SVGLength, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGCursorElement, SVGLength, SVGLength, Y, y)

        CachedImage *m_cachedImage;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
