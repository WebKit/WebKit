/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#include "Document.h"
#include "ElementAncestorIterator.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "RenderMenuList.h"
#include "NodeRenderStyle.h"
#include "StyleResolver.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLOptGroupElement);

using namespace HTMLNames;

inline HTMLOptGroupElement::HTMLOptGroupElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(optgroupTag));
}

Ref<HTMLOptGroupElement> HTMLOptGroupElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLOptGroupElement(tagName, document));
}

bool HTMLOptGroupElement::isDisabledFormControl() const
{
    return hasAttributeWithoutSynchronization(disabledAttr);
}

bool HTMLOptGroupElement::isFocusable() const
{
    if (!supportsFocus())
        return false;
    // Optgroup elements do not have a renderer.
    auto* style = const_cast<HTMLOptGroupElement&>(*this).computedStyle();
    return style && style->display() != DisplayType::None;
}

const AtomString& HTMLOptGroupElement::formControlType() const
{
    static NeverDestroyed<const AtomString> optgroup("optgroup", AtomString::ConstructFromLiteral);
    return optgroup;
}

void HTMLOptGroupElement::childrenChanged(const ChildChange& change)
{
    recalcSelectOptions();
    HTMLElement::childrenChanged(change);
}

void HTMLOptGroupElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    HTMLElement::parseAttribute(name, value);
    recalcSelectOptions();

    if (name == disabledAttr)
        invalidateStyleForSubtree();
}

void HTMLOptGroupElement::recalcSelectOptions()
{
    if (auto selectElement = makeRefPtr(ancestorsOfType<HTMLSelectElement>(*this).first())) {
        selectElement->setRecalcListItems();
        selectElement->updateValidity();
    }
}

String HTMLOptGroupElement::groupLabelText() const
{
    String itemText = document().displayStringModifiedByEncoding(attributeWithoutSynchronization(labelAttr));
    
    // In WinIE, leading and trailing whitespace is ignored in options and optgroups. We match this behavior.
    itemText = itemText.stripWhiteSpace();
    // We want to collapse our whitespace too.  This will match other browsers.
    itemText = itemText.simplifyWhiteSpace();
        
    return itemText;
}
    
HTMLSelectElement* HTMLOptGroupElement::ownerSelectElement() const
{
    RefPtr<ContainerNode> select = parentNode();
    while (select && !is<HTMLSelectElement>(*select))
        select = select->parentNode();
    
    if (!select)
        return nullptr;
    
    return downcast<HTMLSelectElement>(select.get());
}

void HTMLOptGroupElement::accessKeyAction(bool)
{
    RefPtr<HTMLSelectElement> select = ownerSelectElement();
    // send to the parent to bring focus to the list box
    if (select && !select->focused())
        select->accessKeyAction(false);
}

} // namespace
