/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGStringList.h"
#include "SVGZoomAndPan.h"

namespace WebCore {

class SVGViewElement final : public SVGElement, public SVGExternalResourcesRequired, public SVGFitToViewBox, public SVGZoomAndPan {
    WTF_MAKE_ISO_ALLOCATED(SVGViewElement);
public:
    static Ref<SVGViewElement> create(const QualifiedName&, Document&);

    using SVGElement::ref;
    using SVGElement::deref;

    Ref<SVGStringList> viewTarget() { return m_viewTarget.copyRef(); }

private:
    SVGViewElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGViewElement, SVGElement, SVGExternalResourcesRequired, SVGFitToViewBox>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    // FIXME(webkit.org/b/196554): svgAttributeChanged missing.
    void parseAttribute(const QualifiedName&, const AtomString&) final;

    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGStringList> m_viewTarget { SVGStringList::create(this) };
};

} // namespace WebCore
