/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "AccessibilityImageMapLink.h"

#include "AXObjectCache.h"
#include "ContainerNodeInlines.h"
#include "ElementAncestorIteratorInlines.h"
#include "HTMLImageElement.h"

namespace WebCore {

using namespace HTMLNames;

AccessibilityImageMapLink::AccessibilityImageMapLink(AXID axID, HTMLAreaElement& element, AXObjectCache& cache)
    : AccessibilityNodeObject(axID, &element, cache)
{
}

AccessibilityImageMapLink::~AccessibilityImageMapLink() = default;

Ref<AccessibilityImageMapLink> AccessibilityImageMapLink::create(AXID axID, HTMLAreaElement& element, AXObjectCache& cache)
{
    return adoptRef(*new AccessibilityImageMapLink(axID, element, cache));
}

AccessibilityObject* AccessibilityImageMapLink::parentObject() const
{
    RefPtr node = this->node();
    if (!node)
        return nullptr;

    RefPtr map = ancestorsOfType<HTMLMapElement>(*node).first();
    WeakPtr cache = axObjectCache();
    return map && cache ? cache->getOrCreate(map->imageElement().get()) : nullptr;
}

AccessibilityRole AccessibilityImageMapLink::determineAccessibilityRole()
{
    if (m_ariaRole != AccessibilityRole::Unknown)
        return m_ariaRole;

    return !url().isEmpty() ? AccessibilityRole::Link : AccessibilityRole::Generic;
}

bool AccessibilityImageMapLink::computeIsIgnored() const
{
    if (!node())
        return true;
    return defaultObjectInclusion() == AccessibilityObjectInclusion::IgnoreObject ? true : false;
}

Element* AccessibilityImageMapLink::actionElement() const
{
    return anchorElement();
}

Element* AccessibilityImageMapLink::anchorElement() const
{
    ASSERT(!node() || is<HTMLAreaElement>(node()));
    return dynamicDowncast<Element>(node());
}

URL AccessibilityImageMapLink::url() const
{
    RefPtr areaElement = downcast<HTMLAreaElement>(node());
    return areaElement ? areaElement->href() : URL();
}

void AccessibilityImageMapLink::accessibilityText(Vector<AccessibilityText>& textOrder) const
{
    String description = this->description();
    if (!description.isEmpty())
        textOrder.append(AccessibilityText(WTFMove(description), AccessibilityTextSource::Alternative));

    const AtomString& titleText = getAttribute(titleAttr);
    if (!titleText.isEmpty())
        textOrder.append(AccessibilityText(titleText, AccessibilityTextSource::TitleTag));

    const AtomString& summary = getAttribute(summaryAttr);
    if (!summary.isEmpty())
        textOrder.append(AccessibilityText(summary, AccessibilityTextSource::Summary));
}

String AccessibilityImageMapLink::description() const
{
    auto ariaLabel = getAttributeTrimmed(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        return ariaLabel;

    return altTextFromAttributeOrStyle();
}

RenderElement* AccessibilityImageMapLink::imageMapLinkRenderer() const
{
    RefPtr node = this->node();
    if (!node)
        return nullptr;
    RefPtr map = ancestorsOfType<HTMLMapElement>(*node).first();
    return map ? map->renderer() : nullptr;
}

Path AccessibilityImageMapLink::elementPath() const
{
    CheckedPtr renderer = imageMapLinkRenderer();
    RefPtr areaElement = dynamicDowncast<HTMLAreaElement>(node());
    return renderer && areaElement ? areaElement->computePath(*renderer) : Path();
}

LayoutRect AccessibilityImageMapLink::elementRect() const
{
    CheckedPtr renderer = imageMapLinkRenderer();
    RefPtr areaElement = dynamicDowncast<HTMLAreaElement>(node());
    return renderer && areaElement ? areaElement->computeRect(renderer.get()) : LayoutRect();
}

} // namespace WebCore
