/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGStyledTransformableElement_h
#define SVGStyledTransformableElement_h

#if ENABLE(SVG)
#include "Path.h"
#include "SVGStyledLocatableElement.h"
#include "SVGTransformable.h"

namespace WebCore {

    extern char SVGStyledTransformableElementIdentifier[];

    class TransformationMatrix;

    class SVGStyledTransformableElement : public SVGStyledLocatableElement,
                                          public SVGTransformable {
    public:
        SVGStyledTransformableElement(const QualifiedName&, Document*);
        virtual ~SVGStyledTransformableElement();
        
        virtual bool isStyledTransformable() const { return true; }

        virtual TransformationMatrix getCTM() const;
        virtual TransformationMatrix getScreenCTM() const;
        virtual SVGElement* nearestViewportElement() const;
        virtual SVGElement* farthestViewportElement() const;
        
        virtual TransformationMatrix animatedLocalTransform() const;
        virtual TransformationMatrix* supplementalTransform();

        virtual FloatRect getBBox() const;

        virtual void parseMappedAttribute(MappedAttribute*);
        bool isKnownAttribute(const QualifiedName&);

        // "base class" methods for all the elements which render as paths
        virtual Path toPathData() const { return Path(); }
        virtual Path toClipPath() const;
        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    protected:
        ANIMATED_PROPERTY_DECLARATIONS(SVGStyledTransformableElement, SVGStyledTransformableElementIdentifier,
                                       SVGNames::transformAttrString, SVGTransformList, Transform, transform)

    private:
        // Used by <animateMotion>
        OwnPtr<TransformationMatrix> m_supplementalTransform;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGStyledTransformableElement_h
