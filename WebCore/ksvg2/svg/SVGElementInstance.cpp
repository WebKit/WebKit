/*
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"

#ifdef SVG_SUPPORT
#include "SVGElementInstance.h"

#include "SVGElementInstanceList.h"
#include "SVGUseElement.h"

#include <wtf/Assertions.h>

namespace WebCore {

SVGElementInstance::SVGElementInstance(PassRefPtr<SVGUseElement> useElement, PassRefPtr<SVGElement> clonedElement, PassRefPtr<SVGElement> originalElement)
    : m_useElement(useElement)
    , m_element(originalElement)
    , m_clonedElement(clonedElement)
    , m_previousSibling(0)
    , m_nextSibling(0)
    , m_firstChild(0)
    , m_lastChild(0)
{
    ASSERT(m_useElement);
    ASSERT(m_element);
    ASSERT(m_clonedElement);

    // Register as instance for passed element.
    SVGDocumentExtensions* extensions = (m_element->document() ? m_element->document()->accessSVGExtensions() : 0);
    if (extensions)
        extensions->mapInstanceToElement(this, m_element.get());
}

SVGElementInstance::~SVGElementInstance()
{
    // Deregister as instance for passed element.
    SVGDocumentExtensions* extensions = (m_element->document() ? m_element->document()->accessSVGExtensions() : 0);
    if (extensions)
        extensions->removeInstanceMapping(this, m_element.get());
}

SVGElement* SVGElementInstance::clonedElement() const
{
    return m_clonedElement.get();
}

SVGElement* SVGElementInstance::correspondingElement() const
{
    return m_element.get();
}

SVGUseElement* SVGElementInstance::correspondingUseElement() const
{
    return m_useElement.get();
}

SVGElementInstance* SVGElementInstance::parentNode() const
{
    return parent();
}

PassRefPtr<SVGElementInstanceList> SVGElementInstance::childNodes()
{
    return new SVGElementInstanceList(this);
}

SVGElementInstance* SVGElementInstance::previousSibling() const
{
    return m_previousSibling;
}

SVGElementInstance* SVGElementInstance::nextSibling() const
{
    return m_nextSibling;
}

SVGElementInstance* SVGElementInstance::firstChild() const
{
    return m_firstChild;
}

SVGElementInstance* SVGElementInstance::lastChild() const
{
    return m_lastChild;
}

void SVGElementInstance::appendChild(PassRefPtr<SVGElementInstance> child)
{
    child->setParent(this);

    if (m_lastChild) {
        child->m_previousSibling = m_lastChild;
        m_lastChild->m_nextSibling = child.get();
    } else
        m_firstChild = child.get();

    m_lastChild = child.get();
}

void SVGElementInstance::updateInstance(SVGElement* element)
{
    ASSERT(element == m_element);

    RefPtr<Node> clone = m_element->cloneNode(false);
    SVGElement* svgClone = svg_dynamic_cast(clone.get());
    ASSERT(svgClone);

    // Replace node in the hidden use tree.
    ExceptionCode ec = 0;
    m_clonedElement->parentNode()->replaceChild(clone, m_clonedElement.get(), ec);
    ASSERT(ec == 0);

    m_clonedElement = svgClone;
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
