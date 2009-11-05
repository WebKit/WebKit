/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
#include "SVGSwitchElement.h"

#include "RenderSVGTransformableContainer.h"
#include "SVGNames.h"

namespace WebCore {

SVGSwitchElement::SVGSwitchElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_externalResourcesRequired(this, SVGNames::externalResourcesRequiredAttr, false)
{
}

SVGSwitchElement::~SVGSwitchElement()
{
}

bool SVGSwitchElement::childShouldCreateRenderer(Node* child) const
{
    for (Node* n = firstChild(); n != 0; n = n->nextSibling()) {
        if (n->isSVGElement()) {
            SVGElement* element = static_cast<SVGElement*>(n);
            if (element && element->isValid())
                return (n == child); // Only allow this child if it's the first valid child
        }
    }

    return false;
}

RenderObject* SVGSwitchElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGTransformableContainer(this);
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

