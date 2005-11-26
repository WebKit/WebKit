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

#include "SVGStyledTransformableElementImpl.h"
#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

namespace KSVG
{
    class SVGGElementImpl : public SVGStyledTransformableElementImpl,
                            public SVGTestsImpl,
                            public SVGLangSpaceImpl,
                            public SVGExternalResourcesRequiredImpl
    {
    public:
        SVGGElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGGElementImpl();
        
        virtual bool isValid() const { return SVGTestsImpl::isValid(); }

        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

        virtual bool rendererIsNeeded(khtml::RenderStyle *) { return true; }
        virtual khtml::RenderObject *createRenderer(RenderArena *arena, khtml::RenderStyle *style);

        virtual void setChanged(bool b = true, bool deep = false);
    };

    class SVGDummyElementImpl : public SVGGElementImpl
    {
    public:
        SVGDummyElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGDummyElementImpl();

        // Derived from: 'ElementImpl'
        virtual const KDOM::AtomicString& localName() const;
    private:
        KDOM::AtomicString m_localName;
    };
};

#endif

// vim:ts=4:noet
