/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2006       Alexander Kellett <lypanov@kde.org>

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

#include <kdom/core/AttrImpl.h>

#include "XLinkNames.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGSVGElementImpl.h"
#include "RenderSVGImage.h"
#include "SVGImageElementImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "KCanvasRenderingStyle.h"
#include "SVGAnimatedPreserveAspectRatioImpl.h"
#include "SVGDocumentImpl.h"
#include <ksvg2/KSVGView.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/KCanvasImage.h>
#include <kcanvas/KCanvasPath.h>

#include <kdom/parser/KDOMParser.h>
#include <kdom/core/DOMConfigurationImpl.h>

#include <kxmlcore/Assertions.h>

#include "cssproperties.h"

using namespace KSVG;

SVGImageElementImpl::SVGImageElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGStyledTransformableElementImpl(tagName, doc), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGURIReferenceImpl(), m_imageLoader(this)
{
}

SVGImageElementImpl::~SVGImageElementImpl()
{
}

SVGAnimatedLengthImpl *SVGImageElementImpl::x() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGImageElementImpl::y() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGImageElementImpl::width() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGImageElementImpl::height() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_height, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedPreserveAspectRatioImpl *SVGImageElementImpl::preserveAspectRatio() const
{
    return lazy_create<SVGAnimatedPreserveAspectRatioImpl>(m_preserveAspectRatio, this);
}

void SVGImageElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    const KDOM::AtomicString& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::preserveAspectRatioAttr)
        preserveAspectRatio()->baseVal()->parsePreserveAspectRatio(value.impl());
    else if (attr->name() == SVGNames::widthAttr) {
        width()->baseVal()->setValueAsString(value.impl());
        addCSSProperty(attr, CSS_PROP_WIDTH, value);
    } else if (attr->name() == SVGNames::heightAttr) {
        height()->baseVal()->setValueAsString(value.impl());
        addCSSProperty(attr, CSS_PROP_HEIGHT, value);
    } else {
        if (SVGTestsImpl::parseMappedAttribute(attr))
            return;
        if (SVGLangSpaceImpl::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr))
            return;
        if (SVGURIReferenceImpl::parseMappedAttribute(attr)) {
            if (attr->name().matches(KDOM::XLinkNames::hrefAttr))
                m_imageLoader.updateFromElement();
            return;
        }
        SVGStyledTransformableElementImpl::parseMappedAttribute(attr);
    }
}

khtml::RenderObject *SVGImageElementImpl::createRenderer(RenderArena *arena, khtml::RenderStyle *style)
{
    return new (arena) RenderSVGImage(this);
}

void SVGImageElementImpl::attach()
{
    SVGStyledTransformableElementImpl::attach();
    if (KSVG::RenderSVGImage* imageObj = static_cast<KSVG::RenderSVGImage*>(renderer()))
        imageObj->setImage(m_imageLoader.image());
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

