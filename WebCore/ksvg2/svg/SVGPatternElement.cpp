/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#ifdef SVG_SUPPORT
#include "SVGPatternElement.h"

#include "Document.h"
#include "GraphicsContext.h"
#include "SVGResourceImage.h"
#include "SVGPaintServerPattern.h"
#include "RenderSVGContainer.h"
#include "SVGHelper.h"
#include "SVGLength.h"
#include "SVGMatrix.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGTransformList.h"
#include "SVGTransformable.h"
#include "SVGUnitTypes.h"
#include <math.h>
#include <wtf/OwnPtr.h>
#include <wtf/MathExtras.h>

namespace WebCore {

SVGPatternElement::SVGPatternElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledLocatableElement(tagName, doc)
    , SVGURIReference()
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGFitToViewBox()
    , SVGResourceListener()
    , m_x(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_y(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_width(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_height(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_patternUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_patternContentUnits(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
    , m_patternTransform(new SVGTransformList)
{
    m_ignoreAttributeChanges = false;
}

SVGPatternElement::~SVGPatternElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, int, Enumeration, enumeration, PatternUnits, patternUnits, SVGNames::patternUnitsAttr.localName(), m_patternUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, int, Enumeration, enumeration, PatternContentUnits, patternContentUnits, SVGNames::patternContentUnitsAttr.localName(), m_patternContentUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, SVGLength*, Length, length, X, x, SVGNames::xAttr.localName(), m_x.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, SVGLength*, Length, length, Y, y, SVGNames::yAttr.localName(), m_y.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, SVGLength*, Length, length, Width, width, SVGNames::widthAttr.localName(), m_width.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, SVGLength*, Length, length, Height, height, SVGNames::heightAttr.localName(), m_height.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, SVGTransformList*, TransformList, transformList, PatternTransform, patternTransform, SVGNames::patternTransformAttr.localName(), m_patternTransform.get())

void SVGPatternElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::patternUnitsAttr) {
        if (value == "userSpaceOnUse")
            setPatternUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (value == "objectBoundingBox")
            setPatternUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::patternContentUnitsAttr) {
        if (value == "userSpaceOnUse")
            setPatternContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (value == "objectBoundingBox")
            setPatternContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::patternTransformAttr) {
        SVGTransformList* patternTransforms = patternTransformBaseValue();
        SVGTransformable::parseTransformAttribute(patternTransforms, value);
    } else if (attr->name() == SVGNames::xAttr)
        xBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::yAttr)
        yBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::widthAttr)
        widthBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::heightAttr)
        heightBaseValue()->setValueAsString(value);
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

const SVGStyledElement* SVGPatternElement::pushAttributeContext(const SVGStyledElement* context)
{
    // All attribute's contexts are equal (so just take the one from 'x').
    const SVGStyledElement* restore = x()->context();

    x()->setContext(context);
    y()->setContext(context);
    width()->setContext(context);
    height()->setContext(context);

    return restore;
}

void SVGPatternElement::resourceNotification() const
{
    // We're referenced by a "client", calculate the tile now...
    notifyAttributeChange();
}

void SVGPatternElement::fillAttributesFromReferencePattern(const SVGPatternElement* target, AffineTransform& patternTransformMatrix)
{
    DeprecatedString ref = href().deprecatedString();
    RefPtr<SVGPaintServer> refServer = getPaintServerById(document(), ref.mid(1));

    if (!refServer || refServer->type() != PatternPaintServer)
        return;
    
    RefPtr<SVGPaintServerPattern> refPattern = WTF::static_pointer_cast<SVGPaintServerPattern>(refServer);
    
    if (!hasAttribute(SVGNames::patternUnitsAttr)) {
        const AtomicString& value = target->getAttribute(SVGNames::patternUnitsAttr);
        if (value == "userSpaceOnUse")
            setPatternUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (value == "objectBoundingBox")
            setPatternUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }
    
    if (!hasAttribute(SVGNames::patternContentUnitsAttr)) {
        const AtomicString& value = target->getAttribute(SVGNames::patternContentUnitsAttr);
        if (value == "userSpaceOnUse")
            setPatternContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (value == "objectBoundingBox")
            setPatternContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }

    if (!hasAttribute(SVGNames::patternTransformAttr))
        patternTransformMatrix = refPattern->patternTransform();
}

void SVGPatternElement::drawPatternContentIntoTile(const SVGPatternElement* target, const IntSize& newSize, AffineTransform patternTransformMatrix)
{
    SVGStyledElement* activeElement = static_cast<SVGStyledElement*>(m_paintServer->activeClient()->element());

    bool bbox = (patternUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);

    const SVGStyledElement* savedContext = 0;
    if (bbox) {
        if (width()->unitType() != SVGLength::SVG_LENGTHTYPE_PERCENTAGE)
            width()->newValueSpecifiedUnits(SVGLength::SVG_LENGTHTYPE_PERCENTAGE, width()->value() * 100.);
        if (height()->unitType() != SVGLength::SVG_LENGTHTYPE_PERCENTAGE)
            height()->newValueSpecifiedUnits(SVGLength::SVG_LENGTHTYPE_PERCENTAGE, height()->value() * 100.);
        if (activeElement)
            savedContext = const_cast<SVGPatternElement*>(this)->pushAttributeContext(activeElement);
    }
    
    m_tile = new SVGResourceImage();
    m_tile->init(newSize);

    OwnPtr<GraphicsContext> patternContext(contextForImage(m_tile.get()));

    FloatRect rect(x()->value(), y()->value(), width()->value(), height()->value());
    m_paintServer->setBbox(rect);
    m_paintServer->setPatternTransform(patternTransformMatrix);
    m_paintServer->setTile(m_tile.get());

    for (Node* n = target->firstChild(); n; n = n->nextSibling()) {
        SVGElement* elem = svg_dynamic_cast(n);
        if (!elem || !elem->isStyled())
            continue;
        SVGStyledElement* e = static_cast<SVGStyledElement*>(elem);
        RenderObject* item = e->renderer();
        if (!item)
            continue;
#if 0
        // FIXME: None of this code seems to be necessary
        // to pass the w3c tests (and infact breaks them)
        // However, I'm leaving it #if 0'd for now until
        // I can quiz WildFox on the subject -- ecs 11/24/05
        // It's possible that W3C-SVG-1.1/coords-units-01-b
        // is failing due to lack of this code.
        KCanvasMatrix savedMatrix = item->localTransform();

        const SVGStyledElement* savedContext = 0;
        if (patternContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        {
            if (activeElement)
                savedContext = e->pushAttributeContext(activeElement);
        }

        // Take into account viewportElement's viewBox, if existant...
        if (viewportElement() && viewportElement()->hasTagName(SVGNames::svgTag))
        {
            SVGSVGElement* svgElement = static_cast<SVGSVGElement*>(viewportElement());

            RefPtr<SVGMatrix> svgCTM = svgElement->getCTM();
            RefPtr<SVGMatrix> ctm = getCTM();

            KCanvasMatrix newMatrix(svgCTM->matrix());
            newMatrix.multiply(savedMatrix);
            newMatrix.scale(1.0 / ctm->a(), 1.0 / ctm->d());

            item->setLocalTransform(newMatrix.matrix());
        }
#endif
 
        RenderObject::PaintInfo info(patternContext.get(), IntRect(), PaintPhaseForeground, 0, 0, 0);
        item->paint(info, 0, 0);

#if 0
        if (savedContext)
            e->pushAttributeContext(savedContext);

        item->setLocalTransform(savedMatrix.matrix());
#endif
    }

    if (savedContext)
        const_cast<SVGPatternElement*>(this)->pushAttributeContext(savedContext);
}

void SVGPatternElement::notifyClientsToRepaint() const
{
    const RenderPathList& clients = m_paintServer->clients();

    unsigned size = clients.size();
    for (unsigned i = 0 ; i < size; i++) {
        const RenderPath* current = clients[i];

        SVGStyledElement* styled = (current ? static_cast<SVGStyledElement*>(current->element()) : 0);
        if (styled) {
            styled->setChanged(true);

            if (styled->renderer())
                styled->renderer()->repaint();
        }
    }
}

void SVGPatternElement::notifyAttributeChange() const
{
    if (!m_paintServer || !m_paintServer->activeClient() || m_ignoreAttributeChanges)
        return;

    IntSize newSize = IntSize(lroundf(width()->value()), lroundf(height()->value()));
    if (m_tile && (m_tile->size() == newSize) || newSize.width() < 1 || newSize.height() < 1)
        return;

    m_ignoreAttributeChanges = true;

    // FIXME: This whole "target" idea seems completely broken to me
    // basically it seems we're effectively trying to change the "this" pointer
    // for the rest of the method... why don't we just?  Or better yet, why don't
    // we call some method on the "target" and each target in the chain?  -- ECS 11/21/05

    // Find first pattern def that has children
    const SVGPatternElement* target = this;
    const Node* test = this;
    while (test && !test->hasChildNodes()) {
        DeprecatedString ref = target->href().deprecatedString();
        test = ownerDocument()->getElementById(String(ref.mid(1)).impl());
        if (test && test->hasTagName(SVGNames::patternTag))
            target = static_cast<const SVGPatternElement*>(test);
    }

    unsigned short savedPatternUnits = patternUnits();
    unsigned short savedPatternContentUnits = patternContentUnits();

    AffineTransform patternTransformMatrix;
    if (patternTransform()->numberOfItems() > 0)
        patternTransformMatrix = patternTransform()->consolidate()->matrix()->matrix();


    SVGPatternElement* nonConstThis = const_cast<SVGPatternElement*>(this);

    nonConstThis->fillAttributesFromReferencePattern(target, patternTransformMatrix);   
    nonConstThis->drawPatternContentIntoTile(target, newSize, patternTransformMatrix);
    nonConstThis->setPatternUnitsBaseValue(savedPatternUnits);
    nonConstThis->setPatternContentUnitsBaseValue(savedPatternContentUnits);

    notifyClientsToRepaint();
    
    m_ignoreAttributeChanges = false;
}

RenderObject* SVGPatternElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    RenderSVGContainer* patternContainer = new (arena) RenderSVGContainer(this);
    patternContainer->setDrawsContents(false);
    return patternContainer;
}

SVGResource* SVGPatternElement::canvasResource()
{
    if (!m_paintServer) {
        m_paintServer = new SVGPaintServerPattern();
        m_paintServer->setListener(const_cast<SVGPatternElement*>(this));
    }
    return m_paintServer.get();
}

SVGMatrix* SVGPatternElement::getCTM() const
{
    SVGMatrix* mat = SVGSVGElement::createSVGMatrix();
    if (mat) {
        RefPtr<SVGMatrix> viewBox = viewBoxToViewTransform(width()->value(), height()->value());
        mat->multiply(viewBox.get());
    }

    return mat;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT
