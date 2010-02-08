/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2009 Google, Inc.

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

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGTransformableContainer.h"

#include "SVGShadowTreeElements.h"
#include "SVGStyledTransformableElement.h"

namespace WebCore {
    
RenderSVGTransformableContainer::RenderSVGTransformableContainer(SVGStyledTransformableElement* node)
    : RenderSVGContainer(node)
{
}

const AffineTransform& RenderSVGTransformableContainer::localToParentTransform() const
{
    return m_localTransform;
}

AffineTransform RenderSVGTransformableContainer::localTransform() const
{
    return m_localTransform;
}

void RenderSVGTransformableContainer::calculateLocalTransform()
{
    m_localTransform = static_cast<SVGStyledTransformableElement*>(node())->animatedLocalTransform();
    if (!node()->hasTagName(SVGNames::gTag) || !static_cast<SVGGElement*>(node())->isShadowTreeContainerElement())
        return;

    FloatSize translation = static_cast<SVGShadowTreeContainerElement*>(node())->containerTranslation();
    if (translation.width() == 0 && translation.height() == 0)
        return;

    m_localTransform.translate(translation.width(), translation.height());
}

}

#endif // ENABLE(SVG)
