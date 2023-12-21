/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009-2016 Google, Inc.
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
#include "LegacyRenderSVGTransformableContainer.h"

#include "RenderElementInlines.h"
#include "RenderStyleInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGGElement.h"
#include "SVGGraphicsElement.h"
#include "SVGUseElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRenderSVGTransformableContainer);

LegacyRenderSVGTransformableContainer::LegacyRenderSVGTransformableContainer(SVGGraphicsElement& element, RenderStyle&& style)
    : LegacyRenderSVGContainer(Type::LegacySVGTransformableContainer, element, WTFMove(style))
    , m_needsTransformUpdate(true)
    , m_didTransformToRootUpdate(false)
{
    ASSERT(isLegacyRenderSVGTransformableContainer());
}

bool LegacyRenderSVGTransformableContainer::calculateLocalTransform()
{
    SVGGraphicsElement& element = graphicsElement();

    // If we're either the renderer for a <use> element, or for any <g> element inside the shadow
    // tree, that was created during the use/symbol/svg expansion in SVGUseElement. These containers
    // need to respect the translations induced by their corresponding use elements x/y attributes.
    auto* useElement = dynamicDowncast<SVGUseElement>(element);
    if (!useElement && element.isInShadowTree() && is<SVGGElement>(element)) {
        if (auto* correspondingElement = dynamicDowncast<SVGUseElement>(element.correspondingElement()))
            useElement = correspondingElement;
    }

    if (useElement) {
        SVGLengthContext lengthContext(&element);
        FloatSize translation(useElement->x().value(lengthContext), useElement->y().value(lengthContext));
        if (translation != m_lastTranslation)
            m_needsTransformUpdate = true;
        m_lastTranslation = translation;
    }

    auto referenceBoxRect = transformReferenceBoxRect();
    if (referenceBoxRect != m_lastTransformReferenceBoxRect) {
        m_lastTransformReferenceBoxRect = referenceBoxRect;
        m_needsTransformUpdate = true;
    }

    m_didTransformToRootUpdate = m_needsTransformUpdate || SVGRenderSupport::transformToRootChanged(parent());
    if (!m_needsTransformUpdate)
        return false;

    m_localTransform = element.animatedLocalTransform();
    m_localTransform.translate(m_lastTranslation);
    m_needsTransformUpdate = false;
    return true;
}

SVGGraphicsElement& LegacyRenderSVGTransformableContainer::graphicsElement()
{
    return downcast<SVGGraphicsElement>(LegacyRenderSVGContainer::element());
}

}
