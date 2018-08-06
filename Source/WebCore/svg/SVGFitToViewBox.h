/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
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

#include "FloatRect.h"
#include "QualifiedName.h"
#include "SVGAttributeRegistry.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include <wtf/HashSet.h>

namespace WebCore {

class AffineTransform;

class SVGFitToViewBox {
    WTF_MAKE_NONCOPYABLE(SVGFitToViewBox);
public:
    static AffineTransform viewBoxToViewTransform(const FloatRect& viewBoxRect, const SVGPreserveAspectRatioValue&, float viewWidth, float viewHeight);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGFitToViewBox>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }

    const FloatRect& viewBox() const { return m_viewBox.currentValue(m_attributeOwnerProxy); }
    const SVGPreserveAspectRatioValue& preserveAspectRatio() const { return m_preserveAspectRatio.currentValue(m_attributeOwnerProxy); }

    RefPtr<SVGAnimatedRect> viewBoxAnimated() { return m_viewBox.animatedProperty(m_attributeOwnerProxy); }
    RefPtr<SVGAnimatedPreserveAspectRatio> preserveAspectRatioAnimated() { return m_preserveAspectRatio.animatedProperty(m_attributeOwnerProxy); }

    void setViewBox(const FloatRect&);
    void resetViewBox();

    void setPreserveAspectRatio(const SVGPreserveAspectRatioValue& preserveAspectRatio) { m_preserveAspectRatio.setValue(preserveAspectRatio); }
    void resetPreserveAspectRatio() { m_preserveAspectRatio.resetValue(); }

    String viewBoxString() const { return m_viewBox.toString(); }
    String preserveAspectRatioString() const { return m_preserveAspectRatio.toString(); }

    bool hasValidViewBox() const { return m_isViewBoxValid; }
    bool hasEmptyViewBox() const { return m_isViewBoxValid && viewBox().isEmpty(); }

protected:
    SVGFitToViewBox(SVGElement* contextElement, AnimatedPropertyState = PropertyIsReadWrite);

    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }

    void reset();
    bool parseAttribute(const QualifiedName&, const AtomicString&);
    bool parseViewBox(const AtomicString& value, FloatRect& viewBox);
    bool parseViewBox(const UChar*& start, const UChar* end, FloatRect& viewBox, bool validate = true);

private:
    static void registerAttributes();

    AttributeOwnerProxy m_attributeOwnerProxy;
    SVGAnimatedRectAttribute m_viewBox;
    SVGAnimatedPreserveAspectRatioAttribute m_preserveAspectRatio;
    bool m_isViewBoxValid { false };
};

} // namespace WebCore
