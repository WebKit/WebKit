/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#include "QualifiedName.h"
#include "SVGAttributeOwnerProxyImpl.h"
#include <wtf/HashSet.h>

namespace WebCore {

class SVGElement;

// Notes on a SVG 1.1 spec discrepancy:
// The SVG DOM defines the attribute externalResourcesRequired as being of type SVGAnimatedBoolean, whereas the 
// SVG language definition says that externalResourcesRequired is not animated. Because the SVG language definition
// states that externalResourcesRequired cannot be animated, the animVal will always be the same as the baseVal.
// FIXME: When implementing animVal support, make sure that animVal==baseVal for externalResourcesRequired
class SVGExternalResourcesRequired {
    WTF_MAKE_NONCOPYABLE(SVGExternalResourcesRequired);
public:
    virtual ~SVGExternalResourcesRequired() = default;

    void parseAttribute(const QualifiedName&, const AtomicString&);
    void svgAttributeChanged(const QualifiedName&);

    static void addSupportedAttributes(HashSet<QualifiedName>&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGExternalResourcesRequired>;
    static auto& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }

    auto externalResourcesRequiredAnimated() { return m_externalResourcesRequired.animatedProperty(attributeOwnerProxy()); }

    bool externalResourcesRequired() const { return m_externalResourcesRequired.value(); }
    void setExternalResourcesRequired(bool externalResourcesRequired) { m_externalResourcesRequired.setValue(externalResourcesRequired); }

protected:
    SVGExternalResourcesRequired(SVGElement* contextElement);

    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }

    virtual void setHaveFiredLoadEvent(bool) { }
    virtual bool isParserInserted() const { return false; }
    virtual bool haveFiredLoadEvent() const { return false; }

    void dispatchLoadEvent();
    void insertedIntoDocument();
    void finishParsingChildren();
    bool haveLoadedRequiredResources() const;

private:
    static void registerAttributes();
    AttributeOwnerProxy attributeOwnerProxy() { return { *this, m_contextElement }; }
    
    SVGElement& m_contextElement;
    SVGAnimatedBooleanAttribute m_externalResourcesRequired;
};

} // namespace WebCore
