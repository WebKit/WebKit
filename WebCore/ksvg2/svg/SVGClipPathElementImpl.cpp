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

#include "config.h"
#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasPath.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGClipPathElementImpl.h"
#include "SVGAnimatedEnumerationImpl.h"

using namespace KSVG;

SVGClipPathElementImpl::SVGClipPathElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGStyledTransformableElementImpl(tagName, doc), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl()
{
    m_clipper = 0;
}

SVGClipPathElementImpl::~SVGClipPathElementImpl()
{
}

SVGAnimatedEnumerationImpl *SVGClipPathElementImpl::clipPathUnits() const
{
    if(!m_clipPathUnits)
    {
        lazy_create<SVGAnimatedEnumerationImpl>(m_clipPathUnits, this);
        m_clipPathUnits->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
    }

    return m_clipPathUnits.get();
}

void SVGClipPathElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
    if (attr->name() == SVGNames::clipPathUnitsAttr)
    {
        if(value == "userSpaceOnUse")
            clipPathUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
        else if(value == "objectBoundingBox")
            clipPathUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }
   else
    {
        if(SVGTestsImpl::parseMappedAttribute(attr)) return;
        if(SVGLangSpaceImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElementImpl::parseMappedAttribute(attr);
    }
}

KCanvasClipper *SVGClipPathElementImpl::canvasResource()
{
    if(!canvas())
        return 0;
    if(!m_clipper)
        m_clipper = static_cast<KCanvasClipper *>(canvas()->renderingDevice()->createResource(RS_CLIPPER));
    else
        m_clipper->resetClipData();

    bool bbox = clipPathUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

    for(KDOM::NodeImpl *n = firstChild(); n != 0; n = n->nextSibling())
    {
        SVGElementImpl *e = svg_dynamic_cast(n);
        if(e && e->isStyled())
        {
            SVGStyledElementImpl *styled = static_cast<SVGStyledElementImpl *>(e);
            SVGRenderStyle *style = styleForRenderer(parentNode()->renderer())->svgStyle();
            m_clipper->addClipData(styled->toPathData(), (KCWindRule) style->clipRule(), bbox);
        }
    }
    return m_clipper;
}

// vim:ts=4:noet
