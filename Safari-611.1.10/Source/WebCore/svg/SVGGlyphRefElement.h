/*
 * Copyright (C) 2011 Leo Yang <leoyang@webkit.org>
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
#include "SVGURIReference.h"

namespace WebCore {

class SVGGlyphRefElement final : public SVGElement, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGGlyphRefElement);
public:
    static Ref<SVGGlyphRefElement> create(const QualifiedName&, Document&);

    bool hasValidGlyphElement(String& glyphName) const;

    float x() const { return m_x; }
    void setX(float);
    float y() const { return m_y; }
    void setY(float);
    float dx() const { return m_dx; }
    void setDx(float);
    float dy() const { return m_dy; }
    void setDy(float);

private:
    SVGGlyphRefElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGGlyphRefElement, SVGElement, SVGURIReference>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    float m_x { 0 };
    float m_y { 0 };
    float m_dx { 0 };
    float m_dy { 0 };
    PropertyRegistry m_propertyRegistry { *this };
};

}
