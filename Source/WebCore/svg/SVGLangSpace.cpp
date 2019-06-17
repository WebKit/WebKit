/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"
#include "SVGLangSpace.h"

#include "RenderSVGResource.h"
#include "RenderSVGShape.h"
#include "SVGGeometryElement.h"
#include "XMLNames.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

SVGLangSpace::SVGLangSpace(SVGElement* contextElement)
    : m_contextElement(*contextElement)
{
}

const AtomString& SVGLangSpace::xmlspace() const
{
    if (!m_space) {
        static NeverDestroyed<const AtomString> defaultString("default", AtomString::ConstructFromLiteral);
        return defaultString;
    }
    return m_space;
}
    
bool SVGLangSpace::isKnownAttribute(const QualifiedName& attributeName)
{
    return attributeName.matches(XMLNames::langAttr) || attributeName.matches(XMLNames::spaceAttr);
}

void SVGLangSpace::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name.matches(XMLNames::langAttr))
        setXmllang(value);
    if (name.matches(XMLNames::spaceAttr))
        setXmlspace(value);
}

void SVGLangSpace::svgAttributeChanged(const QualifiedName& attributeName)
{
    if (!isKnownAttribute(attributeName))
        return;

    auto* renderer = m_contextElement.renderer();
    if (!is<RenderSVGShape>(renderer))
        return;

    ASSERT(is<SVGGeometryElement>(m_contextElement));
    SVGElement::InstanceInvalidationGuard guard(m_contextElement);
    RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
}

}
