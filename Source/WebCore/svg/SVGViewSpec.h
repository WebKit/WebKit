/*
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
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

#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedRect.h"
#include "SVGFitToViewBox.h"
#include "SVGTransformListValues.h"
#include "SVGZoomAndPan.h"

namespace WebCore {

class SVGElement;
class SVGTransformList;

class SVGViewSpec final : public RefCounted<SVGViewSpec>, public SVGZoomAndPan, public SVGFitToViewBox {
public:
    static Ref<SVGViewSpec> create(SVGElement& contextElement)
    {
        return adoptRef(*new SVGViewSpec(contextElement));
    }

    bool parseViewSpec(const String&);
    void reset();

    SVGElement* viewTarget() const;

    String transformString() const;
    String viewBoxString() const;
    String preserveAspectRatioString() const;
    const String& viewTargetString() const { return m_viewTargetString; }

    SVGZoomAndPanType zoomAndPan() const { return m_zoomAndPan; }
    ExceptionOr<void> setZoomAndPan(unsigned short);
    void setZoomAndPanBaseValue(unsigned short zoomAndPan) { m_zoomAndPan = SVGZoomAndPan::parseFromNumber(zoomAndPan); }

    void resetContextElement() { m_contextElement = nullptr; }

    // Custom non-animated 'transform' property.
    RefPtr<SVGTransformList> transform();
    SVGTransformListValues transformBaseValue() const { return m_transform; }

    // Custom animated 'viewBox' property.
    RefPtr<SVGAnimatedRect> viewBoxAnimated();
    FloatRect& viewBox() { return m_viewBox; }
    void setViewBoxBaseValue(const FloatRect& viewBox) { m_viewBox = viewBox; }

    // Custom animated 'preserveAspectRatio' property.
    RefPtr<SVGAnimatedPreserveAspectRatio> preserveAspectRatioAnimated();
    SVGPreserveAspectRatioValue& preserveAspectRatio() { return m_preserveAspectRatio; }
    void setPreserveAspectRatioBaseValue(const SVGPreserveAspectRatioValue& preserveAspectRatio) { m_preserveAspectRatio = preserveAspectRatio; }

private:
    explicit SVGViewSpec(SVGElement&);

    static const SVGPropertyInfo* transformPropertyInfo();
    static const SVGPropertyInfo* viewBoxPropertyInfo();
    static const SVGPropertyInfo* preserveAspectRatioPropertyInfo();

    static const AtomicString& transformIdentifier();
    static const AtomicString& viewBoxIdentifier();
    static const AtomicString& preserveAspectRatioIdentifier();

    static Ref<SVGAnimatedProperty> lookupOrCreateTransformWrapper(SVGViewSpec* contextElement);
    static Ref<SVGAnimatedProperty> lookupOrCreateViewBoxWrapper(SVGViewSpec* contextElement);
    static Ref<SVGAnimatedProperty> lookupOrCreatePreserveAspectRatioWrapper(SVGViewSpec* contextElement);

    SVGElement* m_contextElement;
    SVGZoomAndPanType m_zoomAndPan { SVGZoomAndPanMagnify };
    SVGTransformListValues m_transform;
    FloatRect m_viewBox;
    SVGPreserveAspectRatioValue m_preserveAspectRatio;
    String m_viewTargetString;
};

} // namespace WebCore
