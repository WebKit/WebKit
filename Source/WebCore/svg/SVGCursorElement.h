/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "SVGElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"

namespace WebCore {

class StyleCursorImage;

class SVGCursorElement final : public SVGElement, public SVGTests, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGCursorElement);
public:
    static Ref<SVGCursorElement> create(const QualifiedName&, Document&);

    virtual ~SVGCursorElement();

    void addClient(StyleCursorImage&);
    void removeClient(StyleCursorImage&);

    const SVGLengthValue& x() const { return m_x->currentValue(); }
    const SVGLengthValue& y() const { return m_y->currentValue(); }

    SVGAnimatedLength& xAnimated() { return m_x; }
    SVGAnimatedLength& yAnimated() { return m_y; }

private:
    SVGCursorElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGCursorElement, SVGElement, SVGTests, SVGURIReference>;
    
    void parseAttribute(const QualifiedName&, const AtomString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool isValid() const final { return SVGTests::isValid(); }
    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const final;

    Ref<SVGAnimatedLength> m_x { SVGAnimatedLength::create(this, SVGLengthMode::Width) };
    Ref<SVGAnimatedLength> m_y { SVGAnimatedLength::create(this, SVGLengthMode::Height) };
    HashSet<StyleCursorImage*> m_clients;
};

} // namespace WebCore
