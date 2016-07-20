/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGLineElement_h
#define SVGLineElement_h

#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedLength.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGGraphicsElement.h"

namespace WebCore {

class SVGLineElement final : public SVGGraphicsElement,
                             public SVGExternalResourcesRequired {
public:
    static Ref<SVGLineElement> create(const QualifiedName&, Document&);

private:
    SVGLineElement(const QualifiedName&, Document&);
    
    bool isValid() const final { return SVGTests::isValid(); }

    static bool isSupportedAttribute(const QualifiedName&);
    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool supportsMarkers() const final { return true; }

    bool selfHasRelativeLengths() const final;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGLineElement)
        DECLARE_ANIMATED_LENGTH(X1, x1)
        DECLARE_ANIMATED_LENGTH(Y1, y1)
        DECLARE_ANIMATED_LENGTH(X2, x2)
        DECLARE_ANIMATED_LENGTH(Y2, y2)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore

#endif
