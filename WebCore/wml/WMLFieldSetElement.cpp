/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 *
 */

#include "config.h"

#if ENABLE(WML)
#include "WMLFieldSetElement.h"

#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "RenderFieldset.h"
#include "Text.h"
#include "WMLElementFactory.h"
#include "WMLNames.h"

namespace WebCore {

using namespace WMLNames;

WMLFieldSetElement::WMLFieldSetElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

WMLFieldSetElement::~WMLFieldSetElement()
{
}

void WMLFieldSetElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::titleAttr)
        m_title = parseValueSubstitutingVariableReferences(attr->value());
    else
        WMLElement::parseMappedAttribute(attr);
}

void WMLFieldSetElement::insertedIntoDocument()
{
    WMLElement::insertedIntoDocument();

    if (m_title.isEmpty())
        return;

    m_insertedLegendElement = WMLElementFactory::createWMLElement(insertedLegendTag, document());

    // Insert <dummyLegend> element, as RenderFieldset expect it to be present
    // to layout it's child text content, when rendering <fieldset> elements
    ExceptionCode ec = 0;
    appendChild(m_insertedLegendElement, ec);
    ASSERT(ec == 0);

    // Create text node holding the 'title' attribute value
    m_insertedLegendElement->appendChild(document()->createTextNode(m_title), ec);
    ASSERT(ec == 0);
}

void WMLFieldSetElement::removedFromDocument()
{
    WMLElement::removedFromDocument();
    m_insertedLegendElement.clear();
}

RenderObject* WMLFieldSetElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderFieldset(this);
}

}

#endif
