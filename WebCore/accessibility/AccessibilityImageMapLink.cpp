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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "AccessibilityRenderObject.h"
#include "AXObjectCache.h"
#include "Document.h"
#include "HTMLNames.h"
#include "IntRect.h"
#include "RenderObject.h"

using namespace std;

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityImageMapLink::AccessibilityImageMapLink()
    : m_areaElement(0), 
      m_mapElement(0)
{
}

AccessibilityImageMapLink::~AccessibilityImageMapLink()
{
}    

PassRefPtr<AccessibilityImageMapLink> AccessibilityImageMapLink::create()
{
    return adoptRef(new AccessibilityImageMapLink());
}

AccessibilityObject* AccessibilityImageMapLink::parentObject() const
{
    if (m_parent)
        return m_parent;
    
    if (!m_mapElement || !m_mapElement->renderer())
        return 0;
    
    return m_mapElement->document()->axObjectCache()->getOrCreate(m_mapElement->renderer());
}
    
Element* AccessibilityImageMapLink::actionElement() const
{
    return anchorElement();
}
    
Element* AccessibilityImageMapLink::anchorElement() const
{
    return m_areaElement;
}

KURL AccessibilityImageMapLink::url() const
{
    if (!m_areaElement)
        return KURL();
    
    return m_areaElement->href();
}
    
String AccessibilityImageMapLink::accessibilityDescription() const
{
    if (!m_areaElement)
        return String();

    const AtomicString& alt = m_areaElement->getAttribute(altAttr);
    if (!alt.isEmpty())
        return alt;

    return String();
}
    
String AccessibilityImageMapLink::title() const
{
    if (!m_areaElement)
        return String();
    
    const AtomicString& title = m_areaElement->getAttribute(titleAttr);
    if (!title.isEmpty())
        return title;
    const AtomicString& summary = m_areaElement->getAttribute(summaryAttr);
    if (!summary.isEmpty())
        return summary;

    return String();
}
    
IntRect AccessibilityImageMapLink::elementRect() const
{
    if (!m_mapElement || !m_areaElement)
        return IntRect();

    RenderObject* renderer;
    if (m_parent && m_parent->isAccessibilityRenderObject())
        renderer = static_cast<AccessibilityRenderObject*>(m_parent)->renderer();
    else
        renderer = m_mapElement->renderer();
    
    if (!renderer)
        return IntRect();
    
    return m_areaElement->getRect(renderer);
}
    
IntSize AccessibilityImageMapLink::size() const
{
    return elementRect().size();
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
