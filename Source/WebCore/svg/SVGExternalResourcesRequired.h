/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "SVGPropertyOwnerRegistry.h"
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

    void parseAttribute(const QualifiedName&, const AtomString&);
    void svgAttributeChanged(const QualifiedName&);

    static void addSupportedAttributes(HashSet<QualifiedName>&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGExternalResourcesRequired>;

    bool externalResourcesRequired() const { return m_externalResourcesRequired->currentValue(); }
    SVGAnimatedBoolean& externalResourcesRequiredAnimated() { return m_externalResourcesRequired; }

protected:
    SVGExternalResourcesRequired(SVGElement* contextElement);

    static bool isKnownAttribute(const QualifiedName& attributeName) { return PropertyRegistry::isKnownAttribute(attributeName); }

    virtual void setHaveFiredLoadEvent(bool) { }
    virtual bool isParserInserted() const { return false; }
    virtual bool haveFiredLoadEvent() const { return false; }

    void dispatchLoadEvent();
    void insertedIntoDocument();
    void finishParsingChildren();
    bool haveLoadedRequiredResources() const;

private:
    SVGElement& m_contextElement;
    Ref<SVGAnimatedBoolean> m_externalResourcesRequired;
};

} // namespace WebCore
