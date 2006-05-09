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
#if SVG_SUPPORT
#include "SVGMaskElement.h"

#include "GraphicsContext.h"
#include "KCanvasContainer.h"
#include "KCanvasImage.h"
#include "KCanvasPath.h"
#include "KRenderingDevice.h"
#include "SVGAnimatedLength.h"
#include "SVGHelper.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "cssstyleselector.h"
#include "ksvg.h"
#include "Attr.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

SVGMaskElement::SVGMaskElement(const QualifiedName& tagName, Document *doc) : SVGStyledLocatableElement(tagName, doc), SVGURIReference(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), m_masker(0), m_dirty(true)
{
}

SVGMaskElement::~SVGMaskElement()
{
    delete m_masker;
}

SVGAnimatedLength *SVGMaskElement::x() const
{
    return lazy_create<SVGAnimatedLength>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGMaskElement::y() const
{
    return lazy_create<SVGAnimatedLength>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLength *SVGMaskElement::width() const
{
    return lazy_create<SVGAnimatedLength>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGMaskElement::height() const
{
    return lazy_create<SVGAnimatedLength>(m_height, this, LM_HEIGHT, viewportElement());
}

void SVGMaskElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    IntSize newSize = IntSize(lroundf(width()->baseVal()->value()), lroundf(height()->baseVal()->value()));
    if (!m_masker || !m_masker->mask() || (m_masker->mask()->size() != newSize))
        m_dirty = true;
    SVGStyledLocatableElement::attributeChanged(attr, preserveDecls);
}

void SVGMaskElement::childrenChanged()
{
    m_dirty = true;
    SVGStyledLocatableElement::childrenChanged();
}

void SVGMaskElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::widthAttr)
        width()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::heightAttr)
        height()->baseVal()->setValueAsString(value.impl());
    else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledElement::parseMappedAttribute(attr);
    }
}

KCanvasImage *SVGMaskElement::drawMaskerContent()
{
    KRenderingDevice *device = renderingDevice();
    if (!device->currentContext()) // FIXME: hack for now until Image::lockFocus exists
        return 0;
    if (!renderer())
        return 0;
    KCanvasImage *maskImage = static_cast<KCanvasImage *>(device->createResource(RS_IMAGE));

    IntSize size = IntSize(lroundf(width()->baseVal()->value()), lroundf(height()->baseVal()->value()));
    maskImage->init(size);

    KRenderingDeviceContext *patternContext = device->contextForImage(maskImage);
    device->pushContext(patternContext);

    OwnPtr<GraphicsContext> context(patternContext->createGraphicsContext());

    KCanvasContainer *maskContainer = static_cast<KCanvasContainer *>(renderer());
    RenderObject::PaintInfo info(context.get(), IntRect(), PaintPhaseForeground, 0, 0);
    maskContainer->setDrawsContents(true);
    maskContainer->paint(info, 0, 0);
    maskContainer->setDrawsContents(false);
    
    device->popContext();
    delete patternContext;

    return maskImage;
}

RenderObject* SVGMaskElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    KCanvasContainer* maskContainer = new (arena) KCanvasContainer(this);
    maskContainer->setDrawsContents(false);
    return maskContainer;
}

KCanvasMasker *SVGMaskElement::canvasResource()
{
    if (!m_masker) {
        m_masker = static_cast<KCanvasMasker *>(renderingDevice()->createResource(RS_MASKER));
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
#endif // SVG_SUPPORT

