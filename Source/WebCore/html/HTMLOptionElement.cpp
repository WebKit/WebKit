/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2017 Google Inc. All rights reserved.
 * Copyright (C) 2011 Motorola Mobility, Inc. All rights reserved.
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

#include "AXObjectCache.h"
#include "ContainerNodeInlines.h"
#include "Document.h"
#include "ElementAncestorIteratorInlines.h"
#include "HTMLDataListElement.h"
#include "HTMLHRElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLSelectElement.h"
#include "NodeName.h"
#include "NodeRenderStyle.h"
#include "NodeTraversal.h"
#include "PseudoClassChangeInvalidation.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderTheme.h"
#include "ScriptElement.h"
#include "StyleResolver.h"
#include "Text.h"
#include <wtf/Ref.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLOptionElement);

using namespace HTMLNames;

HTMLOptionElement::HTMLOptionElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document, TypeFlag::HasCustomStyleResolveCallbacks)
{
    ASSERT(hasTagName(optionTag));
}

Ref<HTMLOptionElement> HTMLOptionElement::create(Document& document)
{
    return adoptRef(*new HTMLOptionElement(optionTag, document));
}

Ref<HTMLOptionElement> HTMLOptionElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLOptionElement(tagName, document));
}

ExceptionOr<Ref<HTMLOptionElement>> HTMLOptionElement::createForLegacyFactoryFunction(Document& document, String&& text, const AtomString& value, bool defaultSelected, bool selected)
{
    auto element = create(document);

    if (!text.isEmpty()) {
        auto appendResult = element->appendChild(Text::create(document, WTF::move(text)));
        if (appendResult.hasException())
            return appendResult.releaseException();
    }

    if (!value.isNull())
        element->setAttributeWithoutSynchronization(valueAttr, value);
    if (defaultSelected)
        element->setAttributeWithoutSynchronization(selectedAttr, emptyAtom());
    element->setSelected(selected);

    return element;
}

auto HTMLOptionElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree) -> InsertedIntoAncestorResult
{
    auto result = HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);

    if (!document().settings().htmlEnhancedSelectParsingEnabled() || m_ownerSelect)
        return result;

    if (RefPtr select = HTMLSelectElement::findOwnerSelect(protectedParentNode().get(), HTMLSelectElement::ExcludeOptGroup::No)) {
        m_ownerSelect = select.get();
        select->setRecalcListItems();
    }

    return result;
}

void HTMLOptionElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (!document().settings().htmlEnhancedSelectParsingEnabled() || !m_ownerSelect)
        return;

    if (RefPtr select = HTMLSelectElement::findOwnerSelect(protectedParentNode().get(), HTMLSelectElement::ExcludeOptGroup::No)) {
        ASSERT_UNUSED(select, select == m_ownerSelect.get());
        return;
    }

    if (RefPtr select = std::exchange(m_ownerSelect, nullptr).get())
        select->setRecalcListItems();
}

bool HTMLOptionElement::isFocusable() const
{
    RefPtr select = ownerSelectElement();
    if (select && select->usesMenuList())
        return false;
    return HTMLElement::isFocusable();
}

bool HTMLOptionElement::matchesDefaultPseudoClass() const
{
    return m_isDefault;
}

String HTMLOptionElement::text() const
{
    String text = collectOptionInnerText();

    // FIXME: Is displayStringModifiedByEncoding helpful here?
    // If it's correct here, then isn't it needed in the value and label functions too?
    return protectedDocument()->displayStringModifiedByEncoding(text).trim(isASCIIWhitespace).simplifyWhiteSpace(isASCIIWhitespace);
}

void HTMLOptionElement::setText(String&& text)
{
    Ref protectedThis { *this };

    // Changing the text causes a recalc of a select's items, which will reset the selected
    // index to the first item if the select is single selection with a menu list. We attempt to
    // preserve the selected item.
    RefPtr select = ownerSelectElement();
    bool selectIsMenuList = select && select->usesMenuList();
    int oldSelectedIndex = selectIsMenuList ? select->selectedIndex() : -1;

    setTextContent(WTF::move(text));
    
    if (selectIsMenuList && select->selectedIndex() != oldSelectedIndex)
        select->setSelectedIndex(oldSelectedIndex);
}

bool HTMLOptionElement::accessKeyAction(bool)
{
    RefPtr select = ownerSelectElement();
    if (select) {
        select->accessKeySetSelectedIndex(index());
        return true;
    }
    return false;
}

HTMLFormElement* HTMLOptionElement::form() const
{
    if (RefPtr selectElement = ownerSelectElement())
        return selectElement->form();
    return nullptr;
}

RefPtr<HTMLFormElement> HTMLOptionElement::formForBindings() const
{
    // FIXME: The downcast should be unnecessary, but the WPT was written before https://github.com/WICG/webcomponents/issues/1072 was resolved. Update once the WPT has been updated.
    return dynamicDowncast<HTMLFormElement>(retargetReferenceTargetForBindings(form()));
}

int HTMLOptionElement::index() const
{
    // It would be faster to cache the index, but harder to get it right in all cases.

    RefPtr selectElement = ownerSelectElement();
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

void HTMLOptionElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::disabledAttr: {
        bool newDisabled = !newValue.isNull();
        if (m_disabled != newDisabled) {
            Style::PseudoClassChangeInvalidation disabledInvalidation(*this, { { CSSSelector::PseudoClass::Disabled, newDisabled },  { CSSSelector::PseudoClass::Enabled, !newDisabled } });
            m_disabled = newDisabled;
            if (CheckedPtr renderer = this->renderer(); renderer && renderer->style().hasUsedAppearance())
                renderer->repaint();
        }
        break;
    }
    case AttributeNames::selectedAttr: {
        // FIXME: Use PseudoClassChangeInvalidation in other elements that implement matchesDefaultPseudoClass().
        Style::PseudoClassChangeInvalidation defaultInvalidation(*this, CSSSelector::PseudoClass::Default, !newValue.isNull());
        m_isDefault = !newValue.isNull();

        // FIXME: WebKit still need to implement 'dirtiness'. See: https://bugs.webkit.org/show_bug.cgi?id=258073
        // https://html.spec.whatwg.org/multipage/form-elements.html#concept-option-selectedness
        if (oldValue.isNull() != newValue.isNull())
            setSelected(!newValue.isNull());
        break;
    }
    case AttributeNames::labelAttr: {
        if (RefPtr select = ownerSelectElement())
            select->optionElementChildrenChanged();
        break;
    }
    case AttributeNames::valueAttr:
        for (Ref dataList : ancestorsOfType<HTMLDataListElement>(*this))
            dataList->optionElementChildrenChanged();
        break;
    default:
        HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
        break;
    }
}

String HTMLOptionElement::value() const
{
    const AtomString& value = attributeWithoutSynchronization(valueAttr);
    if (!value.isNull())
        return value;
    return collectOptionInnerTextCollapsingWhitespace();
}

bool HTMLOptionElement::selected(AllowStyleInvalidation allowStyleInvalidation) const
{
    if (RefPtr select = ownerSelectElement())
        select->updateListItemSelectedStates(allowStyleInvalidation);
    return m_isSelected;
}

void HTMLOptionElement::setSelected(bool selected)
{
    if (m_isSelected == selected)
        return;

    setSelectedState(selected);

    if (RefPtr select = ownerSelectElement())
        select->optionSelectionStateChanged(*this, selected);
}

void HTMLOptionElement::setSelectedState(bool selected, AllowStyleInvalidation allowStyleInvalidation)
{
    if (m_isSelected == selected)
        return;

    std::optional<Style::PseudoClassChangeInvalidation> checkedInvalidation;
    if (allowStyleInvalidation == AllowStyleInvalidation::Yes)
        emplace(checkedInvalidation, *this, { { CSSSelector::PseudoClass::Checked, selected } });

    m_isSelected = selected;

    if (CheckedPtr cache = protectedDocument()->existingAXObjectCache())
        cache->onSelectedOptionChanged(*this);
}

void HTMLOptionElement::childrenChanged(const ChildChange& change)
{
    Vector<Ref<HTMLDataListElement>> ancestors;
    for (Ref dataList : ancestorsOfType<HTMLDataListElement>(*this))
        ancestors.append(WTF::move(dataList));
    for (auto& dataList : ancestors)
        dataList->optionElementChildrenChanged();
    if (change.source != ChildChange::Source::Clone) {
        if (RefPtr select = ownerSelectElement())
            select->optionElementChildrenChanged();
    }
    HTMLElement::childrenChanged(change);
}

HTMLSelectElement* HTMLOptionElement::ownerSelectElement() const
{
    if (document().settings().htmlEnhancedSelectParsingEnabled())
        return m_ownerSelect.get();

    if (auto* parent = parentElement()) {
        if (auto* select = dynamicDowncast<HTMLSelectElement>(*parent))
            return select;
        if (auto* optGroup = dynamicDowncast<HTMLOptGroupElement>(*parent))
            return optGroup->ownerSelectElement();
    }
    return nullptr;
}

String HTMLOptionElement::label() const
{
    String label = attributeWithoutSynchronization(labelAttr);
    if (!label.isNull())
        return label;
    return collectOptionInnerTextCollapsingWhitespace();
}

// Same as label() but ignores the label content attribute in quirks mode for compatibility with other browsers.
String HTMLOptionElement::displayLabel() const
{
    if (document().inQuirksMode())
        return collectOptionInnerTextCollapsingWhitespace();
    return label();
}

void HTMLOptionElement::willResetComputedStyle()
{
    // FIXME: This is nasty, we ask our owner select to repaint even if the new
    // style is exactly the same.
    if (RefPtr select = ownerSelectElement()) {
        if (CheckedPtr renderer = select->renderer())
            renderer->repaint();
    }
}

String HTMLOptionElement::textIndentedToRespectGroupLabel() const
{
    if (!document().settings().htmlEnhancedSelectParsingEnabled()) {
        if (is<HTMLOptGroupElement>(parentNode()))
            return makeString("    "_s, label());
        return label();
    }

    for (Ref ancestor : ancestorsOfType<HTMLElement>(*this)) {
        if (is<HTMLOptGroupElement>(ancestor))
            return makeString("    "_s, label());
        if (is<HTMLDataListElement>(ancestor) || is<HTMLSelectElement>(ancestor) || is<HTMLOptionElement>(ancestor) || is<HTMLHRElement>(ancestor))
            return label();
    }
    return label();
}

bool HTMLOptionElement::isDisabledFormControl() const
{
    if (ownElementDisabled())
        return true;

    if (!document().settings().htmlEnhancedSelectParsingEnabled()) {
        auto* parentOptGroup = dynamicDowncast<HTMLOptGroupElement>(parentNode());
        return parentOptGroup && parentOptGroup->isDisabledFormControl();
    }

    for (Ref ancestor : ancestorsOfType<HTMLElement>(*this)) {
        if (RefPtr optGroup = dynamicDowncast<HTMLOptGroupElement>(ancestor))
            return optGroup->isDisabledFormControl();
        if (is<HTMLDataListElement>(ancestor) || is<HTMLSelectElement>(ancestor) || is<HTMLOptionElement>(ancestor) || is<HTMLHRElement>(ancestor))
            return false;
    }
    return false;
}

String HTMLOptionElement::collectOptionInnerText() const
{
    StringBuilder text;
    // Text nodes inside script elements are not part of the option text.
    for (RefPtr node = firstChild(); node; node = isScriptElement(*node) ? NodeTraversal::nextSkippingChildren(*node, this) : NodeTraversal::next(*node, this)) {
        if (auto* textNode = dynamicDowncast<Text>(*node))
            text.append(textNode->data());
    }
    return text.toString();
}

String HTMLOptionElement::collectOptionInnerTextCollapsingWhitespace() const
{
    return collectOptionInnerText().trim(isASCIIWhitespace).simplifyWhiteSpace(isASCIIWhitespace);
}

} // namespace
