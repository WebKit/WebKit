/*
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
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

#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedRect.h"
#include "SVGFitToViewBox.h"
#include "SVGTransformListValues.h"
#include "SVGZoomAndPan.h"

namespace WebCore {

class SVGElement;
class SVGTransformList;

class SVGViewSpec final : public RefCounted<SVGViewSpec>, public SVGFitToViewBox, public SVGZoomAndPan {
public:
    static Ref<SVGViewSpec> create(SVGElement& contextElement)
    {
        return adoptRef(*new SVGViewSpec(contextElement));
    }

    bool parseViewSpec(const String&);
    void reset();
    void resetContextElement() { m_contextElement = nullptr; }

    SVGElement* viewTarget() const;
    const String& viewTargetString() const { return m_viewTargetString; }

    String transformString() const { return m_transform.toString(); }
    RefPtr<SVGTransformList> transform();
    SVGTransformListValues transformValue() const { return m_transform.value(); }

    const WeakPtr<SVGElement>& contextElementConcurrently() const { return m_contextElement; }

private:
    explicit SVGViewSpec(SVGElement&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGViewSpec, SVGFitToViewBox, SVGZoomAndPan>;
    static void registerAttributes();

    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }

    WeakPtr<SVGElement> m_contextElement;
    String m_viewTargetString;
    AttributeOwnerProxy m_attributeOwnerProxy;
    SVGAnimatedTransformListAttribute m_transform;
};

} // namespace WebCore
