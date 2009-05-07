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

namespace WebCore {

using namespace WMLNames;

WMLOptGroupElement::WMLOptGroupElement(const QualifiedName& tagName, Document* doc)
    : WMLFormControlElement(tagName, doc)
{
}

WMLOptGroupElement::~WMLOptGroupElement()
{
}

const AtomicString& WMLOptGroupElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, optgroup, ("optgroup"));
    return optgroup;
}

bool WMLOptGroupElement::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    bool result = WMLElement::insertBefore(newChild, refChild, ec, shouldLazyAttach);
    if (result)
        recalcSelectOptions();
    return result;
}

bool WMLOptGroupElement::replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    bool result = WMLElement::replaceChild(newChild, oldChild, ec, shouldLazyAttach);
    if (result)
        recalcSelectOptions();
    return result;
}

bool WMLOptGroupElement::removeChild(Node* oldChild, ExceptionCode& ec)
{
    bool result = WMLElement::removeChild(oldChild, ec);
    if (result)
        recalcSelectOptions();
    return result;
}

bool WMLOptGroupElement::appendChild(PassRefPtr<Node> newChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    bool result = WMLElement::appendChild(newChild, ec, shouldLazyAttach);
    if (result)
        recalcSelectOptions();
    return result;
}

bool WMLOptGroupElement::removeChildren()
{
    bool result = WMLElement::removeChildren();
    if (result)
        recalcSelectOptions();
    return result;
}

// FIXME: Activate once WMLSelectElement is available 
#if 0
static inline WMLElement* ownerSelectElement()
{
    Node* select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();

    if (!select)
        return 0;

    return static_cast<WMLSelectElement*>(select);
}
#endif

void WMLOptGroupElement::accessKeyAction(bool)
{
    // FIXME: Activate once WMLSelectElement is available 
#if 0
    WMLSelectElement* select = ownerSelectElement();
    if (!select || select->focused())
        return;

    // send to the parent to bring focus to the list box
    select->accessKeyAction(false);
#endif
}

void WMLOptGroupElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    recalcSelectOptions();
    WMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

void WMLOptGroupElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::titleAttr) {
        m_title = parseValueSubstitutingVariableReferences(attr->value());
        return;
    }

    WMLElement::parseMappedAttribute(attr);
    recalcSelectOptions();
}

void WMLOptGroupElement::attach()
{
    if (parentNode()->renderStyle())
        setRenderStyle(styleForRenderer());
    WMLElement::attach();
}

void WMLOptGroupElement::detach()
{
    m_style.clear();
    WMLElement::detach();
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
    String itemText = document()->displayStringModifiedByEncoding(m_title);

    // In WinIE, leading and trailing whitespace is ignored in options and optgroups. We match this behavior.
    itemText = itemText.stripWhiteSpace();
    // We want to collapse our whitespace too.  This will match other browsers.
    itemText = itemText.simplifyWhiteSpace();

    return itemText;
}

void WMLOptGroupElement::recalcSelectOptions()
{
    // FIXME: Activate once WMLSelectElement is available 
#if 0
    if (WMLSelectElement* select = ownerSelectElement())
        select->setRecalcListItems();
#endif
}

}

#endif
