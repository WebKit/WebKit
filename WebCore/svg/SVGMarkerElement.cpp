/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGMarkerElement.h"

#include "MappedAttribute.h"
#include "PlatformString.h"
#include "RenderSVGViewportContainer.h"
#include "SVGAngle.h"
#include "SVGFitToViewBox.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"

namespace WebCore {

char SVGOrientTypeAttrIdentifier[] = "SVGOrientTypeAttr";
char SVGOrientAngleAttrIdentifier[] = "SVGOrientAngleAttr";

SVGMarkerElement::SVGMarkerElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGFitToViewBox()
    , m_refX(this, SVGNames::refXAttr, LengthModeWidth)
    , m_refY(this, SVGNames::refYAttr, LengthModeHeight)
    , m_markerWidth(this, SVGNames::markerWidthAttr, LengthModeWidth, "3")
    , m_markerHeight(this, SVGNames::markerHeightAttr, LengthModeHeight, "3") 
    , m_markerUnits(this, SVGNames::markerUnitsAttr, SVG_MARKERUNITS_STROKEWIDTH)
    , m_orientType(this, SVGNames::orientAttr, SVG_MARKER_ORIENT_ANGLE)
    , m_orientAngle(this, SVGNames::orientAttr, SVGAngle::create())
    , m_externalResourcesRequired(this, SVGNames::externalResourcesRequiredAttr, false)
    , m_viewBox(this, SVGNames::viewBoxAttr)
    , m_preserveAspectRatio(this, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio::create())
{
    // Spec: If the markerWidth/markerHeight attribute is not specified, the effect is as if a value of "3" were specified.
}

SVGMarkerElement::~SVGMarkerElement()
{
    // Call detach() here because if we wait until ~Node() calls it, we crash during
    // RenderSVGViewportContainer destruction, as the renderer assumes that the element
    // is still fully constructed. See <https://bugs.webkit.org/show_bug.cgi?id=21293>.
    if (renderer())
        detach();
}

TransformationMatrix SVGMarkerElement::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    return SVGFitToViewBox::viewBoxToViewTransform(viewBox(), preserveAspectRatio(), viewWidth, viewHeight);
}

void SVGMarkerElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::markerUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setMarkerUnitsBaseValue(SVG_MARKERUNITS_USERSPACEONUSE);
        else if (attr->value() == "strokeWidth")
            setMarkerUnitsBaseValue(SVG_MARKERUNITS_STROKEWIDTH);
    } else if (attr->name() == SVGNames::refXAttr)
        setRefXBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::refYAttr)
        setRefYBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::markerWidthAttr)
        setMarkerWidthBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::markerHeightAttr)
        setMarkerHeightBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::orientAttr) {
        RefPtr<SVGAngle> angle = SVGAngle::create();

        if (attr->value() == "auto")
            setOrientTypeBaseValue(SVG_MARKER_ORIENT_AUTO);
        else {
            angle->setValueAsString(attr->value());
            setOrientTypeBaseValue(SVG_MARKER_ORIENT_ANGLE);
        }

        setOrientAngleBaseValue(angle.get());
    } else {
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        if (SVGFitToViewBox::parseMappedAttribute(attr))
            return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

void SVGMarkerElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledElement::svgAttributeChanged(attrName);

    if (!m_marker)
        return;

    if (attrName == SVGNames::markerUnitsAttr || attrName == SVGNames::refXAttr ||
        attrName == SVGNames::refYAttr || attrName == SVGNames::markerWidthAttr ||
        attrName == SVGNames::markerHeightAttr || attrName == SVGNames::orientAttr ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGFitToViewBox::isKnownAttribute(attrName) ||
        SVGStyledElement::isKnownAttribute(attrName)) {
        if (renderer())
            renderer()->setNeedsLayout(true);

        m_marker->invalidate();
    }
}

void SVGMarkerElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGStyledElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (!m_marker)
        return;

    if (renderer())
        renderer()->setNeedsLayout(true);

    m_marker->invalidate();
}

void SVGMarkerElement::setOrientToAuto()
{
    setOrientTypeBaseValue(SVG_MARKER_ORIENT_AUTO);

    RefPtr<SVGAngle> angle = SVGAngle::create();
    setOrientAngleBaseValue(angle.get());

    if (!m_marker)
        return;

    if (renderer())
        renderer()->setNeedsLayout(true);

    m_marker->invalidate();
}

void SVGMarkerElement::setOrientToAngle(PassRefPtr<SVGAngle> angle)
{
    setOrientTypeBaseValue(SVG_MARKER_ORIENT_ANGLE);
    setOrientAngleBaseValue(angle.get());

    if (!m_marker)
        return;

    if (renderer())
        renderer()->setNeedsLayout(true);

    m_marker->invalidate();
}

SVGResource* SVGMarkerElement::canvasResource()
{
    if (!m_marker)
        m_marker = SVGResourceMarker::create();

    m_marker->setMarker(toRenderSVGViewportContainer(renderer()));

    if (orientType() == SVG_MARKER_ORIENT_ANGLE) {
        if (orientAngle())
            m_marker->setAngle(orientAngle()->value());
    } else
        m_marker->setAutoAngle();

    m_marker->setRef(refX().value(this), refY().value(this));
    m_marker->setUseStrokeWidth(markerUnits() == SVG_MARKERUNITS_STROKEWIDTH);

    return m_marker.get();
}

RenderObject* SVGMarkerElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    RenderSVGViewportContainer* markerContainer = new (arena) RenderSVGViewportContainer(this);
    markerContainer->setDrawsContents(false); // Marker contents will be explicitly drawn.
    return markerContainer;
}

}

#endif // ENABLE(SVG)
