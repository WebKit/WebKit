/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "HTMLOptGroupElement.h"

#include "CSSStyleSelector.h"
#include "Document.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "RenderMenuList.h"
#include "NodeRenderStyle.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

HTMLOptGroupElement::HTMLOptGroupElement(const QualifiedName& tagName, Document* doc, HTMLFormElement* f)
    : HTMLFormControlElement(tagName, doc, f)
    , m_style(0)
{
    ASSERT(hasTagName(optgroupTag));
}

bool HTMLOptGroupElement::isFocusable() const
{
    return HTMLElement::isFocusable();
}

const AtomicString& HTMLOptGroupElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, optgroup, ("optgroup"));
    return optgroup;
}

bool HTMLOptGroupElement::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    bool result = HTMLFormControlElement::insertBefore(newChild, refChild, ec, shouldLazyAttach);
    return result;
}

bool HTMLOptGroupElement::replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    bool result = HTMLFormControlElement::replaceChild(newChild, oldChild, ec, shouldLazyAttach);
    return result;
}

bool HTMLOptGroupElement::removeChild(Node* oldChild, ExceptionCode& ec)
{
    bool result = HTMLFormControlElement::removeChild(oldChild, ec);
    return result;
}

bool HTMLOptGroupElement::appendChild(PassRefPtr<Node> newChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    bool result = HTMLFormControlElement::appendChild(newChild, ec, shouldLazyAttach);
    return result;
}

bool HTMLOptGroupElement::removeChildren()
{
    bool result = HTMLFormControlElement::removeChildren();
    return result;
}

void HTMLOptGroupElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    recalcSelectOptions();
    HTMLFormControlElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

void HTMLOptGroupElement::parseMappedAttribute(MappedAttribute* attr)
{
    HTMLFormControlElement::parseMappedAttribute(attr);
    recalcSelectOptions();
}

void HTMLOptGroupElement::recalcSelectOptions()
{
    Node* select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();
    if (select)
        static_cast<HTMLSelectElement*>(select)->setRecalcListItems();
}

String HTMLOptGroupElement::label() const
{
    return getAttribute(labelAttr);
}

void HTMLOptGroupElement::setLabel(const String &value)
{
    setAttribute(labelAttr, value);
}

bool HTMLOptGroupElement::checkDTD(const Node* newChild)
{
    // Make sure to keep this in sync with <select> (other than not allowing an optgroup).
    return newChild->isTextNode() || newChild->hasTagName(HTMLNames::optionTag) || newChild->hasTagName(HTMLNames::hrTag) || newChild->hasTagName(HTMLNames::scriptTag);
}

void HTMLOptGroupElement::attach()
{
    if (parentNode()->renderStyle())
        setRenderStyle(styleForRenderer());
    HTMLFormControlElement::attach();
}

void HTMLOptGroupElement::detach()
{
    m_style.clear();
    HTMLFormControlElement::detach();
}

void HTMLOptGroupElement::setRenderStyle(PassRefPtr<RenderStyle> newStyle)
{
    m_style = newStyle;
}
    
RenderStyle* HTMLOptGroupElement::nonRendererRenderStyle() const 
{ 
    return m_style.get(); 
}

String HTMLOptGroupElement::groupLabelText() const
{
    String itemText = document()->displayStringModifiedByEncoding(getAttribute(labelAttr));
    
    // In WinIE, leading and trailing whitespace is ignored in options and optgroups. We match this behavior.
    itemText = itemText.stripWhiteSpace();
    // We want to collapse our whitespace too.  This will match other browsers.
    itemText = itemText.simplifyWhiteSpace();
        
    return itemText;
}
    
HTMLSelectElement* HTMLOptGroupElement::ownerSelectElement() const
{
    Node* select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();
    
    if (!select)
       return 0;
    
    return static_cast<HTMLSelectElement*>(select);
}

void HTMLOptGroupElement::accessKeyAction(bool)
{
    HTMLSelectElement* select = ownerSelectElement();
    // send to the parent to bring focus to the list box
    if (select && !select->focused())
        select->accessKeyAction(false);
}
    
} // namespace
