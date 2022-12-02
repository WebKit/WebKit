/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilitySVGRoot.h"

#include "AXObjectCache.h"
#include "ElementInlines.h"
#include "RenderObject.h"
#include "SVGDescElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGTitleElement.h"
#include "TypedElementDescendantIterator.h"

namespace WebCore {

AccessibilitySVGRoot::AccessibilitySVGRoot(RenderObject* renderer, AXObjectCache* cache)
    : AccessibilitySVGElement(renderer, cache)
{
}

AccessibilitySVGRoot::~AccessibilitySVGRoot() = default;

Ref<AccessibilitySVGRoot> AccessibilitySVGRoot::create(RenderObject* renderer, AXObjectCache* cache)
{
    return adoptRef(*new AccessibilitySVGRoot(renderer, cache));
}

void AccessibilitySVGRoot::setParent(AccessibilityRenderObject* parent)
{
    m_parent = parent;
}
    
AccessibilityObject* AccessibilitySVGRoot::parentObject() const
{
    // If a parent was set because this is a remote SVG resource, use that
    // but otherwise, we should rely on the standard render tree for the parent.
    if (m_parent)
        return m_parent.get();
    
    return AccessibilitySVGElement::parentObject();
}

AccessibilityRole AccessibilitySVGRoot::roleValue() const
{
    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (ariaRole != AccessibilityRole::Unknown)
        return ariaRole;

    return AccessibilityRole::Group;
}

bool AccessibilitySVGRoot::hasAccessibleContent() const
{
    auto* rootElement = this->element();
    if (!rootElement)
        return false;

    auto isAccessibleSVGElement = [] (const Element& element) -> bool {
        if (!is<SVGElement>(element))
            return false;

        // The presence of an SVGTitle or SVGDesc element is enough to deem the SVG hierarchy as accessible.
        if (is<SVGTitleElement>(element)
            || is<SVGDescElement>(element))
            return true;

        // Text content is accessible.
        if (downcast<SVGElement>(element).isTextContent())
            return true;

        // If the role or aria-label attributes are specified, this is accessible.
        if (!element.attributeWithoutSynchronization(HTMLNames::roleAttr).isEmpty()
            || !element.attributeWithoutSynchronization(HTMLNames::aria_labelAttr).isEmpty())
            return true;

        return false;
    };

    if (isAccessibleSVGElement(*rootElement))
        return true;

    // This SVG hierarchy is accessible if any of its descendants is accessible.
    for (const auto& descendant : descendantsOfType<SVGElement>(*rootElement)) {
        if (isAccessibleSVGElement(descendant))
            return true;
    }

    return false;
}

} // namespace WebCore
