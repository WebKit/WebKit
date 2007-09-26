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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGCursorElement_h
#define SVGCursorElement_h

#if ENABLE(SVG)

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

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
