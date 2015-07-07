/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"
#include "SVGSwitchElement.h"

#include "ElementIterator.h"
#include "RenderSVGTransformableContainer.h"
#include "SVGNames.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_BOOLEAN(SVGSwitchElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGSwitchElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGSwitchElement::SVGSwitchElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::switchTag));
    registerAnimatedPropertiesForSVGSwitchElement();
}

PassRefPtr<SVGSwitchElement> SVGSwitchElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new SVGSwitchElement(tagName, document));
}

bool SVGSwitchElement::childShouldCreateRenderer(const Node& child) const
{
    // We create a renderer for the first valid SVG element child.
    // FIXME: The renderer must be updated after dynamic change of the requiredFeatures, requiredExtensions and systemLanguage attributes (https://bugs.webkit.org/show_bug.cgi?id=74749).
    for (auto& element : childrenOfType<SVGElement>(*this)) {
        if (!element.isValid())
            continue;
        return &element == &child; // Only allow this child if it's the first valid child
    }

    return false;
}

RenderPtr<RenderElement> SVGSwitchElement::createElementRenderer(PassRef<RenderStyle> style)
{
    return createRenderer<RenderSVGTransformableContainer>(*this, WTF::move(style));
}

}
