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
#if SVG_SUPPORT
#include "SVGClipPathElement.h"

#include "Attr.h"
#include "Document.h"
#include "RenderCanvas.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGHelper.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "cssstyleselector.h"
#include "ksvg.h"
#include <kcanvas/KCanvasPath.h>
#include <kcanvas/device/KRenderingDevice.h>

using namespace WebCore;

SVGClipPathElement::SVGClipPathElement(const QualifiedName& tagName, Document *doc)
: SVGStyledTransformableElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired()
{
    m_clipper = 0;
}

SVGClipPathElement::~SVGClipPathElement()
{
    delete m_clipper;
}

SVGAnimatedEnumeration *SVGClipPathElement::clipPathUnits() const
{
    if(!m_clipPathUnits)
    {
        lazy_create<SVGAnimatedEnumeration>(m_clipPathUnits, this);
        m_clipPathUnits->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
    }

    return m_clipPathUnits.get();
}

void SVGClipPathElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::clipPathUnitsAttr)
    {
        if(value == "userSpaceOnUse")
            clipPathUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
        else if(value == "objectBoundingBox")
            clipPathUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }
   else
    {
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

KCanvasClipper *SVGClipPathElement::canvasResource()
{
    if (!canvas())
        return 0;
    if (!m_clipper)
        m_clipper = static_cast<KCanvasClipper *>(renderingDevice()->createResource(RS_CLIPPER));
    else
        m_clipper->resetClipData();

    bool bbox = clipPathUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

    RenderStyle *clipPathStyle = createStyleForRenderer(parent()->renderer()); // FIXME: Manual style resolution is a hack
    for (Node *n = firstChild(); n != 0; n = n->nextSibling()) {
        SVGElement *e = svg_dynamic_cast(n);
        if (e && e->isStyled()) {
            SVGStyledElement *styled = static_cast<SVGStyledElement *>(e);
            RenderStyle *pathStyle = document()->styleSelector()->createStyleForElement(styled, clipPathStyle);
            if (KCanvasPath* pathData = styled->toPathData())
                m_clipper->addClipData(pathData, (KCWindRule) pathStyle->svgStyle()->clipRule(), bbox);
            pathStyle->deref(canvas()->renderArena());
        }
    }
    clipPathStyle->deref(canvas()->renderArena());
    return m_clipper;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

