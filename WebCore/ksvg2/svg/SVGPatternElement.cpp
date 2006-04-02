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
#include "SVGPatternElement.h"

#include "GraphicsContext.h"
#include "Document.h"
#include "KCanvasContainer.h"
#include "KCanvasCreator.h"
#include "KCanvasImage.h"
#include "KCanvasMatrix.h"
#include "KCanvasRenderingStyle.h"
#include "KRenderingDevice.h"
#include "KRenderingPaintServerPattern.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedTransformList.h"
#include "SVGDOMImplementation.h"
#include "SVGHelper.h"
#include "SVGMatrix.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGTransformList.h"
#include "SVGTransformable.h"
#include "ksvg.h"
#include "Attr.h"

namespace WebCore {

SVGPatternElement::SVGPatternElement(const QualifiedName& tagName, Document *doc) : SVGStyledLocatableElement(tagName, doc), SVGURIReference(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGFitToViewBox(), KCanvasResourceListener()
{
    m_tile = 0;
    m_paintServer = 0;
    m_ignoreAttributeChanges = false;
}

SVGPatternElement::~SVGPatternElement()
{
    delete m_paintServer;
}

SVGAnimatedEnumeration *SVGPatternElement::patternUnits() const
{
    if (!m_patternUnits) {
        lazy_create<SVGAnimatedEnumeration>(m_patternUnits, this);
        m_patternUnits->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }

    return m_patternUnits.get();
}

SVGAnimatedEnumeration *SVGPatternElement::patternContentUnits() const
{
    if (!m_patternContentUnits) {
        lazy_create<SVGAnimatedEnumeration>(m_patternContentUnits, this);
        m_patternContentUnits->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
    }

    return m_patternContentUnits.get();
}

SVGAnimatedLength *SVGPatternElement::x() const
{
    return lazy_create<SVGAnimatedLength>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGPatternElement::y() const
{
    return lazy_create<SVGAnimatedLength>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLength *SVGPatternElement::width() const
{
    return lazy_create<SVGAnimatedLength>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGPatternElement::height() const
{
    return lazy_create<SVGAnimatedLength>(m_height, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedTransformList *SVGPatternElement::patternTransform() const
{
    return lazy_create<SVGAnimatedTransformList>(m_patternTransform, this);
}

void SVGPatternElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString &value = attr->value();
    if (attr->name() == SVGNames::patternUnitsAttr) {
        if (value == "userSpaceOnUse")
            patternUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (value == "objectBoundingBox")
            patternUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::patternContentUnitsAttr) {
        if (value == "userSpaceOnUse")
            patternContentUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (value == "objectBoundingBox")
            patternContentUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::patternTransformAttr) {
        SVGTransformList *patternTransforms = patternTransform()->baseVal();
        SVGTransformable::parseTransformAttribute(patternTransforms, value);
    } else if (attr->name() == SVGNames::xAttr)
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
        if (SVGFitToViewBox::parseMappedAttribute(attr))
            return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

const SVGStyledElement *SVGPatternElement::pushAttributeContext(const SVGStyledElement *context)
{
    // All attribute's contexts are equal (so just take the one from 'x').
    const SVGStyledElement *restore = x()->baseVal()->context();

    x()->baseVal()->setContext(context);
    y()->baseVal()->setContext(context);
    width()->baseVal()->setContext(context);
    height()->baseVal()->setContext(context);

    return restore;
}

void SVGPatternElement::resourceNotification() const
{
    // We're referenced by a "client", calculate the tile now...
    notifyAttributeChange();
}

void SVGPatternElement::fillAttributesFromReferencePattern(const SVGPatternElement *target, KCanvasMatrix &patternTransformMatrix) const
{
    DeprecatedString ref = String(href()->baseVal()).deprecatedString();
    KRenderingPaintServer *refServer = getPaintServerById(document(), ref.mid(1));

    if(!refServer || refServer->type() != PS_PATTERN)
        return;
    
    KRenderingPaintServerPattern *refPattern = static_cast<KRenderingPaintServerPattern *>(refServer);
    
    if(!hasAttribute(SVGNames::patternUnitsAttr))
    {
        const AtomicString& value = target->getAttribute(SVGNames::patternUnitsAttr);
        if(value == "userSpaceOnUse")
            patternUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
        else if(value == "objectBoundingBox")
            patternUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }
    
    if(!hasAttribute(SVGNames::patternContentUnitsAttr))
    {
        const AtomicString& value = target->getAttribute(SVGNames::patternContentUnitsAttr);
        if(value == "userSpaceOnUse")
            patternContentUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
        else if(value == "objectBoundingBox")
            patternContentUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }

    if(!hasAttribute(SVGNames::patternTransformAttr))
        patternTransformMatrix = refPattern->patternTransform();
}

void SVGPatternElement::drawPatternContentIntoTile(const SVGPatternElement *target, const IntSize &newSize, KCanvasMatrix patternTransformMatrix) const
{
    KRenderingDevice *device = renderingDevice();
    
    SVGStyledElement *activeElement = static_cast<SVGStyledElement *>(m_paintServer->activeClient()->element());

    bool bbox = (patternUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);

    const SVGStyledElement *savedContext = 0;
    if(bbox)
    {
        if(width()->baseVal()->unitType() != SVG_LENGTHTYPE_PERCENTAGE)
            width()->baseVal()->newValueSpecifiedUnits(SVG_LENGTHTYPE_PERCENTAGE, width()->baseVal()->value() * 100.);
        if(height()->baseVal()->unitType() != SVG_LENGTHTYPE_PERCENTAGE)
            height()->baseVal()->newValueSpecifiedUnits(SVG_LENGTHTYPE_PERCENTAGE, height()->baseVal()->value() * 100.);
        if(activeElement)
            savedContext = const_cast<SVGPatternElement *>(this)->pushAttributeContext(activeElement);
    }
    
    delete m_tile;
    m_tile = static_cast<KCanvasImage *>(device->createResource(RS_IMAGE));
    m_tile->init(newSize);

    KRenderingDeviceContext *patternContext = device->contextForImage(m_tile);
    device->pushContext(patternContext);
    FloatRect rect(x()->baseVal()->value(), y()->baseVal()->value(), width()->baseVal()->value(), height()->baseVal()->value());
    m_paintServer->setBbox(rect);
    m_paintServer->setPatternTransform(patternTransformMatrix);
    m_paintServer->setTile(m_tile);

    for(Node *n = target->firstChild(); n != 0; n = n->nextSibling())
    {
        SVGElement *elem = svg_dynamic_cast(n);
        if (!elem || !elem->isStyled())
            continue;
        SVGStyledElement *e = static_cast<SVGStyledElement *>(elem);
        RenderObject *item = e->renderer();
        if(!item)
            continue;
#if 0
        // FIXME: None of this code seems to be necessary
        // to pass the w3c tests (and infact breaks them)
        // However, I'm leaving it #if 0'd for now until
        // I can quiz WildFox on the subject -- ecs 11/24/05
        // It's possible that W3C-SVG-1.1/coords-units-01-b
        // is failing due to lack of this code.
        KCanvasMatrix savedMatrix = item->localTransform();

        const SVGStyledElement *savedContext = 0;
        if(patternContentUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        {
            if(activeElement)
                savedContext = e->pushAttributeContext(activeElement);
        }

        // Take into account viewportElement's viewBox, if existant...
        if(viewportElement() && viewportElement()->hasTagName(SVGNames::svgTag))
        {
            SVGSVGElement *svgElement = static_cast<SVGSVGElement *>(viewportElement());

            RefPtr<SVGMatrix> svgCTM = svgElement->getCTM();
            RefPtr<SVGMatrix> ctm = getCTM();

            KCanvasMatrix newMatrix(svgCTM->qmatrix());
            newMatrix.multiply(savedMatrix);
            newMatrix.scale(1.0 / ctm->a(), 1.0 / ctm->d());

            item->setLocalTransform(newMatrix.qmatrix());
        }
#endif

        GraphicsContext p;
        RenderObject::PaintInfo info(&p, IntRect(), PaintPhaseForeground, 0, 0);
        item->paint(info, 0, 0);
#if 0
        if(savedContext)
            e->pushAttributeContext(savedContext);

        item->setLocalTransform(savedMatrix.qmatrix());
#endif
    }

    if(savedContext)
        const_cast<SVGPatternElement *>(this)->pushAttributeContext(savedContext);

    device->popContext();
    delete patternContext;
}

void SVGPatternElement::notifyClientsToRepaint() const
{
    const KCanvasItemList &clients = m_paintServer->clients();

    KCanvasItemList::ConstIterator it = clients.begin();
    KCanvasItemList::ConstIterator end = clients.end();

    for(; it != end; ++it)
    {
        const RenderPath *current = (*it);

        SVGStyledElement *styled = (current ? static_cast<SVGStyledElement *>(current->element()) : 0);
        if(styled)
        {
            styled->setChanged(true);

            if(styled->renderer())
                styled->renderer()->repaint();
        }
    }
}

void SVGPatternElement::notifyAttributeChange() const
{
    if(!m_paintServer || !m_paintServer->activeClient() || m_ignoreAttributeChanges)
        return;

    IntSize newSize = IntSize(lroundf(width()->baseVal()->value()), lroundf(height()->baseVal()->value()));
    if(m_tile && (m_tile->size() == newSize) || newSize.width() < 1 || newSize.height() < 1)
        return;

    m_ignoreAttributeChanges = true;

    // FIXME: This whole "target" idea seems completely broken to me
    // basically it seems we're effectively trying to change the "this" pointer
    // for the rest of the method... why don't we just?  Or better yet, why don't
    // we call some method on the "target" and each target in the chain?  -- ECS 11/21/05

    // Find first pattern def that has children
    const SVGPatternElement *target = this;
    const Node *test = this;
    while(test && !test->hasChildNodes())
    {
        DeprecatedString ref = String(target->href()->baseVal()).deprecatedString();
        test = ownerDocument()->getElementById(String(ref.mid(1)).impl());
        if(test && test->hasTagName(SVGNames::patternTag))
            target = static_cast<const SVGPatternElement *>(test);
    }

    unsigned short savedPatternUnits = patternUnits()->baseVal();
    unsigned short savedPatternContentUnits = patternContentUnits()->baseVal();

    KCanvasMatrix patternTransformMatrix;
    if(patternTransform()->baseVal()->numberOfItems() > 0)
        patternTransformMatrix = KCanvasMatrix(patternTransform()->baseVal()->consolidate()->matrix()->qmatrix());

    fillAttributesFromReferencePattern(target, patternTransformMatrix);
    
    drawPatternContentIntoTile(target, newSize, patternTransformMatrix);
    
    patternUnits()->setBaseVal(savedPatternUnits);
    patternContentUnits()->setBaseVal(savedPatternContentUnits);

    notifyClientsToRepaint();
    
    m_ignoreAttributeChanges = false;
}

RenderObject *SVGPatternElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    KCanvasContainer *patternContainer = renderingDevice()->createContainer(arena, style, this);
    patternContainer->setDrawsContents(false);
    return patternContainer;
}

KRenderingPaintServerPattern *SVGPatternElement::canvasResource()
{
    if (!m_paintServer) {
        KRenderingPaintServer *pserver = renderingDevice()->createPaintServer(KCPaintServerType(PS_PATTERN));
        m_paintServer = static_cast<KRenderingPaintServerPattern *>(pserver);
        m_paintServer->setListener(const_cast<SVGPatternElement *>(this));
    }
    return m_paintServer;
}

SVGMatrix *SVGPatternElement::getCTM() const
{
    SVGMatrix *mat = SVGSVGElement::createSVGMatrix();
    if(mat)
    {
        RefPtr<SVGMatrix> viewBox = viewBoxToViewTransform(width()->baseVal()->value(),
                                                        height()->baseVal()->value());

        mat->multiply(viewBox.get());
    }

    return mat;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT
