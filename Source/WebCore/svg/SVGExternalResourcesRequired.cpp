/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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
#include "SVGExternalResourcesRequired.h"

#include "RenderSVGResource.h"
#include "RenderSVGShape.h"
#include "SVGElement.h"
#include "SVGNames.h"

namespace WebCore {

SVGExternalResourcesRequired::SVGExternalResourcesRequired(SVGElement* contextElement)
    : m_contextElement(*contextElement)
    , m_externalResourcesRequired(SVGAnimatedBoolean::create(contextElement))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::externalResourcesRequiredAttr, &SVGExternalResourcesRequired::m_externalResourcesRequired>();
    });
}

void SVGExternalResourcesRequired::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::externalResourcesRequiredAttr)
        m_externalResourcesRequired->setBaseValInternal(value == "true");
}

void SVGExternalResourcesRequired::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isKnownAttribute(attrName))
        return;
    if (!m_contextElement.isConnected())
        return;

    // Handle dynamic updates of the 'externalResourcesRequired' attribute. Only possible case: changing from 'true' to 'false'
    // causes an immediate dispatch of the SVGLoad event. If the attribute value was 'false' before inserting the script element
    // in the document, the SVGLoad event has already been dispatched.
    if (!externalResourcesRequiredAnimated().isAnimating() && !externalResourcesRequired() && !haveFiredLoadEvent() && !isParserInserted()) {
        setHaveFiredLoadEvent(true);

        ASSERT(m_contextElement.haveLoadedRequiredResources());
        m_contextElement.sendSVGLoadEventIfPossible();
    }

    auto* renderer = m_contextElement.renderer();
    if (renderer && is<RenderSVGShape>(renderer)) {
        SVGElement::InstanceInvalidationGuard guard(m_contextElement);
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
    }
}

void SVGExternalResourcesRequired::addSupportedAttributes(HashSet<QualifiedName>& supportedAttributes)
{
    supportedAttributes.add(SVGNames::externalResourcesRequiredAttr);
}

void SVGExternalResourcesRequired::dispatchLoadEvent()
{
    if (isParserInserted())
        ASSERT(externalResourcesRequired() != haveFiredLoadEvent());
    else if (haveFiredLoadEvent())
        return;

    // HTML and SVG differ completely in the 'onload' event handling of <script> elements.
    // HTML fires the 'load' event after it sucessfully loaded a remote resource, otherwise an error event.
    // SVG fires the SVGLoad event immediately after parsing the <script> element, if externalResourcesRequired
    // is set to 'false', otherwise it dispatches the 'SVGLoad' event just after loading the remote resource.
    if (!externalResourcesRequired())
        return;

    ASSERT(!haveFiredLoadEvent());

    // Dispatch SVGLoad event
    setHaveFiredLoadEvent(true);
    ASSERT(m_contextElement.haveLoadedRequiredResources());

    m_contextElement.sendSVGLoadEventIfPossible();
}

void SVGExternalResourcesRequired::insertedIntoDocument()
{
    if (isParserInserted())
        return;

    // Eventually send SVGLoad event now for the dynamically inserted script element.
    if (externalResourcesRequired())
        return;
    setHaveFiredLoadEvent(true);
    m_contextElement.sendSVGLoadEventIfPossibleAsynchronously();
}

void SVGExternalResourcesRequired::finishParsingChildren()
{
    // A SVGLoad event has been fired by SVGElement::finishParsingChildren.
    if (!externalResourcesRequired())
        setHaveFiredLoadEvent(true);
}

bool SVGExternalResourcesRequired::haveLoadedRequiredResources() const
{
    return !externalResourcesRequired() || haveFiredLoadEvent();
}

}
