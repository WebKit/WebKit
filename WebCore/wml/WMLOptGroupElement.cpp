/**
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "WMLOptGroupElement.h"

#include "Document.h"
#include "MappedAttribute.h"
#include "HTMLNames.h"
#include "NodeRenderStyle.h"
#include "RenderStyle.h"
#include "WMLNames.h"
#include "WMLSelectElement.h"

namespace WebCore {

using namespace WMLNames;

WMLOptGroupElement::WMLOptGroupElement(const QualifiedName& tagName, Document* doc)
    : WMLFormControlElement(tagName, doc)
{
}

WMLOptGroupElement::~WMLOptGroupElement()
{
}

String WMLOptGroupElement::title() const
{
    return parseValueSubstitutingVariableReferences(getAttribute(HTMLNames::titleAttr));
}

const AtomicString& WMLOptGroupElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, optgroup, ("optgroup"));
    return optgroup;
}

bool WMLOptGroupElement::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    bool result = WMLFormControlElement::insertBefore(newChild, refChild, ec, shouldLazyAttach);
    if (result)
        recalcSelectOptions();
    return result;
}

bool WMLOptGroupElement::replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    bool result = WMLFormControlElement::replaceChild(newChild, oldChild, ec, shouldLazyAttach);
    if (result)
        recalcSelectOptions();
    return result;
}

bool WMLOptGroupElement::removeChild(Node* oldChild, ExceptionCode& ec)
{
    bool result = WMLFormControlElement::removeChild(oldChild, ec);
    if (result)
        recalcSelectOptions();
    return result;
}

bool WMLOptGroupElement::appendChild(PassRefPtr<Node> newChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    bool result = WMLFormControlElement::appendChild(newChild, ec, shouldLazyAttach);
    if (result)
        recalcSelectOptions();
    return result;
}

bool WMLOptGroupElement::removeChildren()
{
    bool result = WMLFormControlElement::removeChildren();
    if (result)
        recalcSelectOptions();
    return result;
}

static inline WMLSelectElement* ownerSelectElement(Element* element)
{
    Node* select = element->parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();

    if (!select)
        return 0;

    return static_cast<WMLSelectElement*>(select);
}

void WMLOptGroupElement::accessKeyAction(bool)
{
    WMLSelectElement* select = ownerSelectElement(this);
    if (!select || select->focused())
        return;

    // send to the parent to bring focus to the list box
    select->accessKeyAction(false);
}

void WMLOptGroupElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    recalcSelectOptions();
    WMLFormControlElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

void WMLOptGroupElement::parseMappedAttribute(MappedAttribute* attr)
{
    WMLFormControlElement::parseMappedAttribute(attr);
    recalcSelectOptions();
}

void WMLOptGroupElement::attach()
{
    if (parentNode()->renderStyle())
        setRenderStyle(styleForRenderer());
    WMLFormControlElement::attach();
}

void WMLOptGroupElement::detach()
{
    m_style.clear();
    WMLFormControlElement::detach();
}

void WMLOptGroupElement::setRenderStyle(PassRefPtr<RenderStyle> style)
{
    m_style = style;
}

RenderStyle* WMLOptGroupElement::nonRendererRenderStyle() const
{
    return m_style.get();
}

String WMLOptGroupElement::groupLabelText() const
{
    String itemText = document()->displayStringModifiedByEncoding(title());

    // In WinIE, leading and trailing whitespace is ignored in options and optgroups. We match this behavior.
    itemText = itemText.stripWhiteSpace();
    // We want to collapse our whitespace too.  This will match other browsers.
    itemText = itemText.simplifyWhiteSpace();

    return itemText;
}

void WMLOptGroupElement::recalcSelectOptions()
{
    if (WMLSelectElement* select = ownerSelectElement(this))
        select->setRecalcListItems();
}

}

#endif
