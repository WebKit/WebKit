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
#include "AccessibilityRenderObject.h"
#include "Document.h"
#include "HTMLNames.h"
#include "RenderBoxModelObject.h"

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityImageMapLink::AccessibilityImageMapLink()
    : m_areaElement(nullptr)
    , m_mapElement(nullptr)
{
}

AccessibilityImageMapLink::~AccessibilityImageMapLink() = default;

Ref<AccessibilityImageMapLink> AccessibilityImageMapLink::create()
{
    return adoptRef(*new AccessibilityImageMapLink());
}

AccessibilityObject* AccessibilityImageMapLink::parentObject() const
{
    if (m_parent)
        return m_parent;
    
    if (!m_mapElement.get() || !m_mapElement->renderer())
        return nullptr;
    
    return m_mapElement->document().axObjectCache()->getOrCreate(m_mapElement->renderer());
}
    
AccessibilityRole AccessibilityImageMapLink::roleValue() const
{
    if (!m_areaElement)
        return AccessibilityRole::WebCoreLink;
    
    const AtomString& ariaRole = getAttribute(roleAttr);
    if (!ariaRole.isEmpty())
        return AccessibilityObject::ariaRoleToWebCoreRole(ariaRole);

    return AccessibilityRole::WebCoreLink;
}
    
Element* AccessibilityImageMapLink::actionElement() const
{
    return anchorElement();
}
    
Element* AccessibilityImageMapLink::anchorElement() const
{
    return m_areaElement.get();
}

URL AccessibilityImageMapLink::url() const
{
    if (!m_areaElement.get())
        return URL();
    
    return m_areaElement->href();
}

void AccessibilityImageMapLink::accessibilityText(Vector<AccessibilityText>& textOrder) const
{
    String description = accessibilityDescription();
    if (!description.isEmpty())
        textOrder.append(AccessibilityText(description, AccessibilityTextSource::Alternative));

    const AtomString& titleText = getAttribute(titleAttr);
    if (!titleText.isEmpty())
        textOrder.append(AccessibilityText(titleText, AccessibilityTextSource::TitleTag));

    const AtomString& summary = getAttribute(summaryAttr);
    if (!summary.isEmpty())
        textOrder.append(AccessibilityText(summary, AccessibilityTextSource::Summary));
}
    
String AccessibilityImageMapLink::accessibilityDescription() const
{
    const AtomString& ariaLabel = getAttribute(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        return ariaLabel;
    const AtomString& alt = getAttribute(altAttr);
    if (!alt.isEmpty())
        return alt;

    return String();
}
    
String AccessibilityImageMapLink::title() const
{
    const AtomString& title = getAttribute(titleAttr);
    if (!title.isEmpty())
        return title;
    const AtomString& summary = getAttribute(summaryAttr);
    if (!summary.isEmpty())
        return summary;

    return String();
}

RenderElement* AccessibilityImageMapLink::imageMapLinkRenderer() const
{
    if (!m_mapElement || !m_areaElement)
        return nullptr;

    RenderElement* renderer = nullptr;
    if (is<AccessibilityRenderObject>(m_parent))
        renderer = downcast<RenderElement>(downcast<AccessibilityRenderObject>(*m_parent).renderer());
    else
        renderer = m_mapElement->renderer();
    
    return renderer;
}

void AccessibilityImageMapLink::detachFromParent()
{
    AccessibilityMockObject::detachFromParent();
    m_areaElement = nullptr;
    m_mapElement = nullptr;
}

Path AccessibilityImageMapLink::elementPath() const
{
    auto renderer = imageMapLinkRenderer();
    if (!renderer)
        return Path();
    
    return m_areaElement->computePath(renderer);
}
    
LayoutRect AccessibilityImageMapLink::elementRect() const
{
    auto renderer = imageMapLinkRenderer();
    if (!renderer)
        return LayoutRect();
    
    return m_areaElement->computeRect(renderer);
}
    
String AccessibilityImageMapLink::stringValueForMSAA() const
{
    return url();
}

String AccessibilityImageMapLink::nameForMSAA() const
{
    return accessibilityDescription();
}

} // namespace WebCore
