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

#ifndef KSVG_SVGGradientElementImpl_H
#define KSVG_SVGGradientElementImpl_H
#if SVG_SUPPORT

#include "SVGURIReference.h"
#include "SVGStyledElement.h"
#include "SVGExternalResourcesRequired.h"

#include "KRenderingPaintServerGradient.h"

namespace WebCore {
    class SVGGradientElement;
    class SVGAnimatedEnumeration;
    class SVGAnimatedTransformList;
    class SVGGradientElement : public SVGStyledElement,
                                   public SVGURIReference,
                                   public SVGExternalResourcesRequired,
                                   public KCanvasResourceListener
    {
    public:
        SVGGradientElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGGradientElement();

        // 'SVGGradientElement' functions
        SVGAnimatedEnumeration *gradientUnits() const;
        SVGAnimatedTransformList *gradientTransform() const;
        SVGAnimatedEnumeration *spreadMethod() const;

        virtual void parseMappedAttribute(MappedAttribute *attr);
        virtual void notifyAttributeChange() const;
        
        virtual KRenderingPaintServerGradient *canvasResource();
        virtual void resourceNotification() const;

    protected:
        virtual void buildGradient(KRenderingPaintServerGradient *grad) const = 0;
        virtual KCPaintServerType gradientType() const = 0;
        void rebuildStops() const;

    protected:
        mutable RefPtr<SVGAnimatedEnumeration> m_spreadMethod;
        mutable RefPtr<SVGAnimatedEnumeration> m_gradientUnits;
        mutable RefPtr<SVGAnimatedTransformList> m_gradientTransform;
        mutable KRenderingPaintServerGradient *m_resource;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
