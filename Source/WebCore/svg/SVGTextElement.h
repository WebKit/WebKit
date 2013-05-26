/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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

#ifndef SVGTextElement_h
#define SVGTextElement_h

#if ENABLE(SVG)
#include "SVGAnimatedTransformList.h"
#include "SVGTextPositioningElement.h"
#include "SVGTransformable.h"

namespace WebCore {

class SVGTextElement FINAL : public SVGTextPositioningElement,
                             public SVGTransformable {
public:
    static PassRefPtr<SVGTextElement> create(const QualifiedName&, Document*);

    virtual SVGElement* nearestViewportElement() const;
    virtual SVGElement* farthestViewportElement() const;

    virtual FloatRect getBBox(StyleUpdateStrategy = AllowStyleUpdate);
    virtual AffineTransform getCTM(StyleUpdateStrategy = AllowStyleUpdate);
    virtual AffineTransform getScreenCTM(StyleUpdateStrategy = AllowStyleUpdate);
    virtual AffineTransform animatedLocalTransform() const;

private:
    SVGTextElement(const QualifiedName&, Document*);

    virtual bool supportsFocus() const OVERRIDE { return true; }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    virtual AffineTransform* supplementalTransform();
    virtual AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope mode) const { return SVGTransformable::localCoordinateSpaceTransform(mode); }

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual bool childShouldCreateRenderer(const NodeRenderingContext&) const;
            
    virtual void svgAttributeChanged(const QualifiedName&);

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGTextElement)
        DECLARE_ANIMATED_TRANSFORM_LIST(Transform, transform)
    END_DECLARE_ANIMATED_PROPERTIES

    // Used by <animateMotion>
    OwnPtr<AffineTransform> m_supplementalTransform;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
