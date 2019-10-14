/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"
#include "SVGGraphicsElement.h"

#include "RenderSVGPath.h"
#include "RenderSVGResource.h"
#include "SVGMatrix.h"
#include "SVGNames.h"
#include "SVGPathData.h"
#include "SVGRect.h"
#include "SVGSVGElement.h"
#include "SVGStringList.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGGraphicsElement);

SVGGraphicsElement::SVGGraphicsElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , SVGTests(this)
    , m_shouldIsolateBlending(false)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::transformAttr, &SVGGraphicsElement::m_transform>();
    });
}

SVGGraphicsElement::~SVGGraphicsElement() = default;

Ref<SVGMatrix> SVGGraphicsElement::getCTMForBindings()
{
    return SVGMatrix::create(getCTM());
}

AffineTransform SVGGraphicsElement::getCTM(StyleUpdateStrategy styleUpdateStrategy)
{
    return SVGLocatable::computeCTM(this, SVGLocatable::NearestViewportScope, styleUpdateStrategy);
}

Ref<SVGMatrix> SVGGraphicsElement::getScreenCTMForBindings()
{
    return SVGMatrix::create(getScreenCTM());
}

AffineTransform SVGGraphicsElement::getScreenCTM(StyleUpdateStrategy styleUpdateStrategy)
{
    return SVGLocatable::computeCTM(this, SVGLocatable::ScreenScope, styleUpdateStrategy);
}

AffineTransform SVGGraphicsElement::animatedLocalTransform() const
{
    AffineTransform matrix;
    auto* style = renderer() ? &renderer()->style() : nullptr;

    // If CSS property was set, use that, otherwise fallback to attribute (if set).
    if (style && style->hasTransform()) {
        
        FloatRect boundingBox;
        switch (style->transformBox()) {
        case TransformBox::FillBox:
            boundingBox = renderer()->objectBoundingBox();
            break;
        case TransformBox::BorderBox:
            // For SVG elements without an associated CSS layout box, the used value for border-box is view-box.
        case TransformBox::ViewBox: {
            FloatSize viewportSize;
            SVGLengthContext(this).determineViewport(viewportSize);
            boundingBox.setSize(viewportSize);
            break;
            }
        }
        
        // Note: objectBoundingBox is an emptyRect for elements like pattern or clipPath.
        // See the "Object bounding box units" section of http://dev.w3.org/csswg/css3-transforms/
        TransformationMatrix transform;
        style->applyTransform(transform, boundingBox);

        // Flatten any 3D transform.
        matrix = transform.toAffineTransform();
        // CSS bakes the zoom factor into lengths, including translation components.
        // In order to align CSS & SVG transforms, we need to invert this operation.
        float zoom = style->effectiveZoom();
        if (zoom != 1) {
            matrix.setE(matrix.e() / zoom);
            matrix.setF(matrix.f() / zoom);
        }

    } else
        matrix = transform().concatenate();

    if (m_supplementalTransform)
        return *m_supplementalTransform * matrix;
    return matrix;
}

AffineTransform* SVGGraphicsElement::supplementalTransform()
{
    if (!m_supplementalTransform)
        m_supplementalTransform = makeUnique<AffineTransform>();
    return m_supplementalTransform.get();
}

void SVGGraphicsElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::transformAttr) {
        m_transform->baseVal()->parse(value);
        return;
    }

    SVGElement::parseAttribute(name, value);
    SVGTests::parseAttribute(name, value);
}

void SVGGraphicsElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::transformAttr) {
        InstanceInvalidationGuard guard(*this);

        if (auto renderer = this->renderer()) {
            renderer->setNeedsTransformUpdate();
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }

        return;
    }

    SVGElement::svgAttributeChanged(attrName);
    SVGTests::svgAttributeChanged(attrName);
}

SVGElement* SVGGraphicsElement::nearestViewportElement() const
{
    return SVGTransformable::nearestViewportElement(this);
}

SVGElement* SVGGraphicsElement::farthestViewportElement() const
{
    return SVGTransformable::farthestViewportElement(this);
}

Ref<SVGRect> SVGGraphicsElement::getBBoxForBindings()
{
    return SVGRect::create(getBBox());
}

FloatRect SVGGraphicsElement::getBBox(StyleUpdateStrategy styleUpdateStrategy)
{
    return SVGTransformable::getBBox(this, styleUpdateStrategy);
}

RenderPtr<RenderElement> SVGGraphicsElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGPath>(*this, WTFMove(style));
}

Path SVGGraphicsElement::toClipPath()
{
    Path path = pathFromGraphicsElement(this);
    // FIXME: How do we know the element has done a layout?
    path.transform(animatedLocalTransform());
    return path;
}

}
