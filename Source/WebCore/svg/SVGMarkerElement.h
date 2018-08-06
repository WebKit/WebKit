/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "SVGAnimatedAngle.h"
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedRect.h"
#include "SVGElement.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGMarkerTypes.h"

namespace WebCore {

class SVGMarkerElement final : public SVGElement, public SVGExternalResourcesRequired, public SVGFitToViewBox {
    WTF_MAKE_ISO_ALLOCATED(SVGMarkerElement);
public:
    // Forward declare enumerations in the W3C naming scheme, for IDL generation.
    enum {
        SVG_MARKERUNITS_UNKNOWN = SVGMarkerUnitsUnknown,
        SVG_MARKERUNITS_USERSPACEONUSE = SVGMarkerUnitsUserSpaceOnUse,
        SVG_MARKERUNITS_STROKEWIDTH = SVGMarkerUnitsStrokeWidth
    };

    enum {
        SVG_MARKER_ORIENT_UNKNOWN = SVGMarkerOrientUnknown,
        SVG_MARKER_ORIENT_AUTO = SVGMarkerOrientAuto,
        SVG_MARKER_ORIENT_ANGLE = SVGMarkerOrientAngle,
        SVG_MARKER_ORIENT_AUTOSTARTREVERSE = SVGMarkerOrientAutoStartReverse
    };

    static Ref<SVGMarkerElement> create(const QualifiedName&, Document&);

    AffineTransform viewBoxToViewTransform(float viewWidth, float viewHeight) const;

    void setOrientToAuto();
    void setOrientToAngle(SVGAngle&);

    const SVGLengthValue& refX() const { return m_refX.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& refY() const { return m_refY.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& markerWidth() const { return m_markerWidth.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& markerHeight() const { return m_markerHeight.currentValue(attributeOwnerProxy()); }
    SVGMarkerUnitsType markerUnits() const { return m_markerUnits.currentValue(attributeOwnerProxy()); }
    const SVGAngleValue& orientAngle() const { return m_orientAngle.currentValue(attributeOwnerProxy()); }
    SVGMarkerOrientType orientType() const { return m_orientType.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedLength> refXAnimated() { return m_refX.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> refYAnimated() { return m_refY.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> markerWidthAnimated() { return m_markerWidth.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> markerHeightAnimated() { return m_markerHeight.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> markerUnitsAnimated() { return m_markerUnits.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedAngle> orientAngleAnimated() { return m_orientAngle.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> orientTypeAnimated() { return m_orientType.animatedProperty(attributeOwnerProxy()); }

private:
    SVGMarkerElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGMarkerElement, SVGElement, SVGExternalResourcesRequired, SVGFitToViewBox>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static bool isAnimatedLengthAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isAnimatedLengthAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;
    void childrenChanged(const ChildChange&) override;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool rendererIsNeeded(const RenderStyle&) override { return true; }

    bool needsPendingResourceHandling() const override { return false; }

    bool selfHasRelativeLengths() const override;

    void setOrient(SVGMarkerOrientType, const SVGAngleValue&);

    static const AtomicString& orientTypeIdentifier();
    static const AtomicString& orientAngleIdentifier();

    AttributeOwnerProxy m_attributeOwnerProxy { *this };

    class SVGAnimatedCustomOrientTypeAttribute : public SVGAnimatedEnumerationAttribute<SVGMarkerOrientType> {
    public:
        using Base = SVGAnimatedEnumerationAttribute<SVGMarkerOrientType>;

        SVGAnimatedCustomOrientTypeAttribute(SVGMarkerOrientType baseValue)
            : Base(baseValue)
        {
        }
        void synchronize(Element& element, const QualifiedName& attributeName)
        {
            static const NeverDestroyed<String> autoString = MAKE_STATIC_STRING_IMPL("auto");
            static const NeverDestroyed<String> autoStartReverseString = MAKE_STATIC_STRING_IMPL("auto-start-reverse");

            if (!shouldSynchronize())
                return;
            if (value() == SVGMarkerOrientAuto)
                element.setSynchronizedLazyAttribute(attributeName, autoString.get());
            else if (value() == SVGMarkerOrientAutoStartReverse)
                element.setSynchronizedLazyAttribute(attributeName, autoStartReverseString.get());
        }
    };

    using SVGAnimatedCustomAngleAttributeAccessor = SVGAnimatedPairAttributeAccessor<SVGMarkerElement, SVGAnimatedAngleAttribute, AnimatedAngle, SVGAnimatedCustomOrientTypeAttribute, AnimatedEnumeration>;

    SVGAnimatedLengthAttribute m_refX { LengthModeWidth };
    SVGAnimatedLengthAttribute m_refY { LengthModeHeight };
    SVGAnimatedLengthAttribute m_markerWidth { LengthModeWidth, "3" };
    SVGAnimatedLengthAttribute m_markerHeight { LengthModeHeight, "3" };
    SVGAnimatedEnumerationAttribute<SVGMarkerUnitsType> m_markerUnits { SVGMarkerUnitsStrokeWidth };
    SVGAnimatedAngleAttribute m_orientAngle;
    SVGAnimatedCustomOrientTypeAttribute m_orientType { SVGMarkerOrientAngle };
};

} // namespace WebCore
