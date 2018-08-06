/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedString.h"
#include "SVGElement.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGTests.h"
#include "SVGURIReference.h"

namespace WebCore {

class CSSCursorImageValue;

class SVGCursorElement final : public SVGElement, public SVGExternalResourcesRequired, public SVGTests, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGCursorElement);
public:
    static Ref<SVGCursorElement> create(const QualifiedName&, Document&);

    virtual ~SVGCursorElement();

    void addClient(CSSCursorImageValue&);
    void removeClient(CSSCursorImageValue&);

    const SVGLengthValue& x() const { return m_x.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& y() const { return m_y.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedLength> xAnimated() { return m_x.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> yAnimated() { return m_y.animatedProperty(attributeOwnerProxy()); }

private:
    SVGCursorElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGCursorElement, SVGElement, SVGExternalResourcesRequired, SVGTests, SVGURIReference>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool isValid() const final { return SVGTests::isValid(); }
    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const final;

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedLengthAttribute m_x { LengthModeWidth };
    SVGAnimatedLengthAttribute m_y { LengthModeHeight };
    HashSet<CSSCursorImageValue*> m_clients;
};

} // namespace WebCore
