/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Motorola Mobility, Inc.  All rights reserved.
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
#include "HTMLOptionElement.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "HTMLDataListElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLSelectElement.h"
#include "NodeRenderStyle.h"
#include "NodeTraversal.h"
#include "RenderMenuList.h"
#include "RenderTheme.h"
#include "ScriptElement.h"
#include "StyleResolver.h"
#include "Text.h"
#include <wtf/Ref.h>

namespace WebCore {

using namespace HTMLNames;

HTMLOptionElement::HTMLOptionElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_disabled(false)
    , m_isSelected(false)
{
    ASSERT(hasTagName(optionTag));
    setHasCustomStyleResolveCallbacks();
}

Ref<HTMLOptionElement> HTMLOptionElement::create(Document& document)
{
    return adoptRef(*new HTMLOptionElement(optionTag, document));
}

Ref<HTMLOptionElement> HTMLOptionElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLOptionElement(tagName, document));
}

RefPtr<HTMLOptionElement> HTMLOptionElement::createForJSConstructor(Document& document, const String& data, const String& value,
        bool defaultSelected, bool selected, ExceptionCode& ec)
{
    RefPtr<HTMLOptionElement> element = adoptRef(new HTMLOptionElement(optionTag, document));

    Ref<Text> text = Text::create(document, data.isNull() ? "" : data);

    ec = 0;
    element->appendChild(WTFMove(text), ec);
    if (ec)
        return nullptr;

    if (!value.isNull())
        element->setValue(value);
    if (defaultSelected)
        element->setAttribute(selectedAttr, emptyAtom);
    element->setSelected(selected);

    return element;
}

bool HTMLOptionElement::isFocusable() const
{
    if (!supportsFocus())
        return false;
    // Option elements do not have a renderer.
    auto* style = const_cast<HTMLOptionElement&>(*this).computedStyle();
    return style && style->display() != NONE;
}

String HTMLOptionElement::text() const
{
    String text = collectOptionInnerText();

    // FIXME: Is displayStringModifiedByEncoding helpful here?
    // If it's correct here, then isn't it needed in the value and label functions too?
    return document().displayStringModifiedByEncoding(text).stripWhiteSpace(isHTMLSpace).simplifyWhiteSpace(isHTMLSpace);
}

void HTMLOptionElement::setText(const String &text, ExceptionCode& ec)
{
    Ref<HTMLOptionElement> protectFromMutationEvents(*this);

    // Changing the text causes a recalc of a select's items, which will reset the selected
    // index to the first item if the select is single selection with a menu list. We attempt to
    // preserve the selected item.
    RefPtr<HTMLSelectElement> select = ownerSelectElement();
    bool selectIsMenuList = select && select->usesMenuList();
    int oldSelectedIndex = selectIsMenuList ? select->selectedIndex() : -1;

    // Handle the common special case where there's exactly 1 child node, and it's a text node.
    Node* child = firstChild();
    if (is<Text>(child) && !child->nextSibling())
        downcast<Text>(*child).setData(text, ec);
    else {
        removeChildren();
        appendChild(Text::create(document(), text), ec);
    }
    
    if (selectIsMenuList && select->selectedIndex() != oldSelectedIndex)
        select->setSelectedIndex(oldSelectedIndex);
}

void HTMLOptionElement::accessKeyAction(bool)
{
    HTMLSelectElement* select = ownerSelectElement();
    if (select)
        select->accessKeySetSelectedIndex(index());
}

int HTMLOptionElement::index() const
{
    // It would be faster to cache the index, but harder to get it right in all cases.

    HTMLSelectElement* selectElement = ownerSelectElement();
    if (!selectElement)
        return 0;

    int optionIndex = 0;

    for (auto& item : selectElement->listItems()) {
        if (!is<HTMLOptionElement>(*item))
            continue;
        if (item == this)
            return optionIndex;
        ++optionIndex;
    }

    return 0;
}

void HTMLOptionElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
#if ENABLE(DATALIST_ELEMENT)
    if (name == valueAttr) {
        if (HTMLDataListElement* dataList = ownerDataListElement())
            dataList->optionElementChildrenChanged();
    } else
#endif
    if (name == disabledAttr) {
        bool oldDisabled = m_disabled;
        m_disabled = !value.isNull();
        if (oldDisabled != m_disabled) {
            setNeedsStyleRecalc();
            if (renderer() && renderer()->style().hasAppearance())
                renderer()->theme().stateChanged(*renderer(), ControlStates::EnabledState);
        }
    } else if (name == selectedAttr) {
        // FIXME: This doesn't match what the HTML specification says.
        // The specification implies that removing the selected attribute or
        // changing the value of a selected attribute that is already present
        // has no effect on whether the element is selected. Further, it seems
        // that we need to do more than just set m_isSelected to select in that
        // case; we'd need to do the other work from the setSelected function.
        m_isSelected = !value.isNull();
    } else
        HTMLElement::parseAttribute(name, value);
}

String HTMLOptionElement::value() const
{
    const AtomicString& value = fastGetAttribute(valueAttr);
    if (!value.isNull())
        return value;
    return collectOptionInnerText().stripWhiteSpace(isHTMLSpace).simplifyWhiteSpace(isHTMLSpace);
}

void HTMLOptionElement::setValue(const String& value)
{
    setAttribute(valueAttr, value);
}

bool HTMLOptionElement::selected()
{
    if (HTMLSelectElement* select = ownerSelectElement())
        select->updateListItemSelectedStates();
    return m_isSelected;
}

void HTMLOptionElement::setSelected(bool selected)
{
    if (m_isSelected == selected)
        return;

    setSelectedState(selected);

    if (HTMLSelectElement* select = ownerSelectElement())
        select->optionSelectionStateChanged(this, selected);
}

void HTMLOptionElement::setSelectedState(bool selected)
{
    if (m_isSelected == selected)
        return;

    m_isSelected = selected;
    setNeedsStyleRecalc();

    if (HTMLSelectElement* select = ownerSelectElement())
        select->invalidateSelectedItems();
}

void HTMLOptionElement::childrenChanged(const ChildChange& change)
{
#if ENABLE(DATALIST_ELEMENT)
    if (HTMLDataListElement* dataList = ownerDataListElement())
        dataList->optionElementChildrenChanged();
    else
#endif
    if (HTMLSelectElement* select = ownerSelectElement())
        select->optionElementChildrenChanged();
    HTMLElement::childrenChanged(change);
}

#if ENABLE(DATALIST_ELEMENT)
HTMLDataListElement* HTMLOptionElement::ownerDataListElement() const
{
    for (ContainerNode* parent = parentNode(); parent ; parent = parent->parentNode()) {
        if (is<HTMLDataListElement>(*parent))
            return downcast<HTMLDataListElement>(parent);
    }
    return nullptr;
}
#endif

HTMLSelectElement* HTMLOptionElement::ownerSelectElement() const
{
    ContainerNode* select = parentNode();
    while (select && !is<HTMLSelectElement>(*select))
        select = select->parentNode();

    if (!select)
        return nullptr;

    return downcast<HTMLSelectElement>(select);
}

String HTMLOptionElement::label() const
{
    String label = fastGetAttribute(labelAttr);
    if (!label.isNull())
        return label.stripWhiteSpace(isHTMLSpace);
    return collectOptionInnerText().stripWhiteSpace(isHTMLSpace).simplifyWhiteSpace(isHTMLSpace);
}

void HTMLOptionElement::setLabel(const String& label)
{
    setAttribute(labelAttr, label);
}

void HTMLOptionElement::willResetComputedStyle()
{
    // FIXME: This is nasty, we ask our owner select to repaint even if the new
    // style is exactly the same.
    if (auto select = ownerSelectElement()) {
        if (auto renderer = select->renderer())
            renderer->repaint();
    }
}

String HTMLOptionElement::textIndentedToRespectGroupLabel() const
{
    ContainerNode* parent = parentNode();
    if (is<HTMLOptGroupElement>(parent))
        return "    " + label();
    return label();
}

bool HTMLOptionElement::isDisabledFormControl() const
{
    if (ownElementDisabled())
        return true;

    if (!is<HTMLOptGroupElement>(parentNode()))
        return false;

    return downcast<HTMLOptGroupElement>(*parentNode()).isDisabledFormControl();
}

Node::InsertionNotificationRequest HTMLOptionElement::insertedInto(ContainerNode& insertionPoint)
{
    if (HTMLSelectElement* select = ownerSelectElement()) {
        select->setRecalcListItems();
        // Do not call selected() since calling updateListItemSelectedStates()
        // at this time won't do the right thing. (Why, exactly?)
        // FIXME: Might be better to call this unconditionally, always passing m_isSelected,
        // rather than only calling it if we are selected.
        if (m_isSelected)
            select->optionSelectionStateChanged(this, true);
        select->scrollToSelection();
    }

    return HTMLElement::insertedInto(insertionPoint);
}

String HTMLOptionElement::collectOptionInnerText() const
{
    StringBuilder text;
    for (Node* node = firstChild(); node; ) {
        if (is<Text>(*node))
            text.append(node->nodeValue());
        // Text nodes inside script elements are not part of the option text.
        if (is<Element>(*node) && toScriptElementIfPossible(downcast<Element>(node)))
            node = NodeTraversal::nextSkippingChildren(*node, this);
        else
            node = NodeTraversal::next(*node, this);
    }
    return text.toString();
}

} // namespace
