/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Alexander Kellett <lypanov@kde.org>

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
#include <kcanvas/KCanvasImage.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kdom/css/CSSStyleSelector.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGMaskElementImpl.h"
#include "SVGAnimatedLengthImpl.h"


namespace KSVG {

SVGMaskElementImpl::SVGMaskElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : SVGStyledLocatableElementImpl(tagName, doc), SVGURIReferenceImpl(), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), m_masker(0), m_dirty(true)
{
}

SVGMaskElementImpl::~SVGMaskElementImpl()
{
    delete m_masker;
}

SVGAnimatedLengthImpl *SVGMaskElementImpl::x() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGMaskElementImpl::y() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGMaskElementImpl::width() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGMaskElementImpl::height() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_height, this, LM_HEIGHT, viewportElement());
}

void SVGMaskElementImpl::attributeChanged(KDOM::AttributeImpl* attr, bool preserveDecls)
{
    IntSize newSize = IntSize(lroundf(width()->baseVal()->value()), lroundf(height()->baseVal()->value()));
    if (!m_masker || !m_masker->mask() || (m_masker->mask()->size() != newSize))
        m_dirty = true;
    SVGStyledLocatableElementImpl::attributeChanged(attr, preserveDecls);
}

void SVGMaskElementImpl::childrenChanged()
{
    m_dirty = true;
    SVGStyledLocatableElementImpl::childrenChanged();
}

void SVGMaskElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::widthAttr)
        width()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::heightAttr)
        height()->baseVal()->setValueAsString(value.impl());
    else {
        if (SVGURIReferenceImpl::parseMappedAttribute(attr))
            return;
        if (SVGTestsImpl::parseMappedAttribute(attr))
            return;
        if (SVGLangSpaceImpl::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr))
            return;
        SVGStyledElementImpl::parseMappedAttribute(attr);
    }
}

KCanvasImage *SVGMaskElementImpl::drawMaskerContent()
{
    KRenderingDevice *device = QPainter::renderingDevice();
    if (!device->currentContext()) // FIXME: hack for now until QPixmap::lockFocus exists
        return 0;
    if (!renderer())
        return 0;
    KCanvasImage *maskImage = static_cast<KCanvasImage *>(device->createResource(RS_IMAGE));

    IntSize size = IntSize(lroundf(width()->baseVal()->value()), lroundf(height()->baseVal()->value()));
    maskImage->init(size);

    KRenderingDeviceContext *patternContext = device->contextForImage(maskImage);
    device->pushContext(patternContext);

    KCanvasContainer *maskContainer = static_cast<KCanvasContainer *>(renderer());
    QPainter p;
    khtml::RenderObject::PaintInfo info(&p, IntRect(), PaintActionForeground, 0);
    maskContainer->setDrawsContents(true);
    maskContainer->paint(info, 0, 0);
    maskContainer->setDrawsContents(false);
    
    device->popContext();
    delete patternContext;

    return maskImage;
}

khtml::RenderObject *SVGMaskElementImpl::createRenderer(RenderArena *arena, khtml::RenderStyle *style)
{
    KCanvasContainer *maskContainer = QPainter::renderingDevice()->createContainer(arena, style, this);
    maskContainer->setDrawsContents(false);
    return maskContainer;
}

KCanvasMasker *SVGMaskElementImpl::canvasResource()
{
    if (!m_masker) {
        m_masker = static_cast<KCanvasMasker *>(QPainter::renderingDevice()->createResource(RS_MASKER));
        m_dirty = true;
    }
    if (m_dirty) {
        KCanvasImage *newMaskImage = drawMaskerContent();
        m_masker->setMask(newMaskImage);
        m_dirty = (newMaskImage != 0);
    }
    return m_masker;
}

}

// vim:ts=4:noet
