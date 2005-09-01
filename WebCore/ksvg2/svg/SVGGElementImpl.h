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

#ifndef KSVG_SVGGElementImpl_H
#define KSVG_SVGGElementImpl_H

#include "SVGStyledElementImpl.h"
#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"
#include "SVGTransformableImpl.h"

namespace KSVG
{
    class SVGGElementImpl : public SVGStyledElementImpl,
                            public SVGTestsImpl,
                            public SVGLangSpaceImpl,
                            public SVGExternalResourcesRequiredImpl,
                            public SVGTransformableImpl
    {
    public:
        SVGGElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix);
        virtual ~SVGGElementImpl();

        virtual void parseAttribute(KDOM::AttributeImpl *attr);

        virtual bool implementsCanvasItem() const { return true; }
        virtual KCanvasItem *createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const;

        virtual void setChanged(bool b = true, bool deep = false);
    };

    class SVGDummyElementImpl : public SVGGElementImpl
    {
    public:
        SVGDummyElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix);
        virtual ~SVGDummyElementImpl();

        // Derived from: 'ElementImpl'
        virtual KDOM::DOMStringImpl *localName() const;
    };
};

#endif

// vim:ts=4:noet
