/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGGraphicsElement_h
#define SVGGraphicsElement_h

#if ENABLE(SVG)
#include "SVGAnimatedTransformList.h"
#include "SVGElement.h"
#include "SVGTests.h"
#include "SVGTransformable.h"

namespace WebCore {

class AffineTransform;
class Path;

class SVGGraphicsElement : public SVGElement, public SVGTransformable, public SVGTests {
public:
    virtual ~SVGGraphicsElement();

    virtual AffineTransform getCTM(StyleUpdateStrategy = AllowStyleUpdate) OVERRIDE;
    virtual AffineTransform getScreenCTM(StyleUpdateStrategy = AllowStyleUpdate) OVERRIDE;
    virtual SVGElement* nearestViewportElement() const OVERRIDE;
    virtual SVGElement* farthestViewportElement() const OVERRIDE;

    virtual AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope mode) const OVERRIDE { return SVGTransformable::localCoordinateSpaceTransform(mode); }
    virtual AffineTransform animatedLocalTransform() const OVERRIDE;
    virtual AffineTransform* supplementalTransform() OVERRIDE;

    virtual FloatRect getBBox(StyleUpdateStrategy = AllowStyleUpdate) OVERRIDE;

    // "base class" methods for all the elements which render as paths
    virtual void toClipPath(Path&);
    virtual RenderElement* createRenderer(RenderArena&, RenderStyle&) OVERRIDE;

protected:
    SVGGraphicsElement(const QualifiedName&, Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGGraphicsElement)
        DECLARE_ANIMATED_TRANSFORM_LIST(Transform, transform)
    END_DECLARE_ANIMATED_PROPERTIES

private:
    virtual bool isSVGGraphicsElement() const OVERRIDE { return true; }

    // SVGTests
    virtual void synchronizeRequiredFeatures() OVERRIDE { SVGTests::synchronizeRequiredFeatures(this); }
    virtual void synchronizeRequiredExtensions() OVERRIDE { SVGTests::synchronizeRequiredExtensions(this); }
    virtual void synchronizeSystemLanguage() OVERRIDE { SVGTests::synchronizeSystemLanguage(this); }

    // Used by <animateMotion>
    OwnPtr<AffineTransform> m_supplementalTransform;
};

void isSVGGraphicsElement(const SVGGraphicsElement&); // Catch unnecessary runtime check of type known at compile time.
inline bool isSVGGraphicsElement(const SVGElement& element) { return element.isSVGGraphicsElement(); }
inline bool isSVGGraphicsElement(const Node& node) { return node.isSVGElement() && toSVGElement(node).isSVGGraphicsElement(); }
NODE_TYPE_CASTS(SVGGraphicsElement)

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGGraphicsElement_h
