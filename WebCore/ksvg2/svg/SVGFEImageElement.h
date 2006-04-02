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

#ifndef KSVG_SVGFEImageElementImpl_H
#define KSVG_SVGFEImageElementImpl_H
#if SVG_SUPPORT

#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGURIReference.h"
#include "SVGLangSpace.h"
#include "SVGExternalResourcesRequired.h"
#include "KCanvasFilters.h"

namespace WebCore {
    class SVGAnimatedPreserveAspectRatio;

    class SVGFEImageElement : public SVGFilterPrimitiveStandardAttributes,
                                  public SVGURIReference,
                                  public SVGLangSpace,
                                  public SVGExternalResourcesRequired,
                                  public CachedObjectClient
    {
    public:
        SVGFEImageElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGFEImageElement();

        // 'SVGFEImageElement' functions
        SVGAnimatedPreserveAspectRatio *preserveAspectRatio() const;

        virtual void parseMappedAttribute(MappedAttribute *attr);
        virtual void notifyFinished(CachedObject *finishedObj);

    protected:
        virtual KCanvasFEImage *filterEffect() const;

    private:
        mutable RefPtr<SVGAnimatedPreserveAspectRatio> m_preserveAspectRatio;
        CachedImage *m_cachedImage;
        mutable KCanvasFEImage *m_filterEffect;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
