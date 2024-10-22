/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2010-2022 Google Inc. All rights reserved.
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
#include "HTMLSelectElement.h"

#include "AXObjectCache.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMFormData.h"
#include "DocumentInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementTraversal.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FormController.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLFormElement.h"
#include "HTMLHRElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionsCollectionInlines.h"
#include "HTMLParserIdioms.h"
#include "KeyboardEvent.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "NodeName.h"
#include "NodeRareData.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "RenderListBox.h"
#include "RenderMenuList.h"
#include "RenderTheme.h"
#include "Settings.h"
#include "SpatialNavigation.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLSelectElement);

using namespace WTF::Unicode;

using namespace HTMLNames;

// https://html.spec.whatwg.org/#dom-htmloptionscollection-length
static constexpr unsigned maxSelectItems = 100000;

HTMLSelectElement::HTMLSelectElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLFormControlElement(tagName, document, form)
    , m_typeAhead(this)
    , m_size(0)
    , m_lastOnChangeIndex(-1)
    , m_activeSelectionAnchorIndex(-1)
    , m_activeSelectionEndIndex(-1)
    , m_isProcessingUserDrivenChange(false)
    , m_multiple(false)
    , m_activeSelectionState(false)
    , m_allowsNonContiguousSelection(false)
    , m_shouldRecalcListItems(false)
{
    ASSERT(hasTagName(selectTag));
}

Ref<HTMLSelectElement> HTMLSelectElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
{
    ASSERT(tagName.matches(selectTag));
    return adoptRef(*new HTMLSelectElement(tagName, document, form));
}

Ref<HTMLSelectElement> HTMLSelectElement::create(Document& document)
{
    return adoptRef(*new HTMLSelectElement(selectTag, document, nullptr));
}

void HTMLSelectElement::didRecalcStyle(Style::Change styleChange)
{
    // Even though the options didn't necessarily change, we will call setOptionsChangedOnRenderer for its side effect
    // of recomputing the width of the element. We need to do that if the style change included a change in zoom level.
    setOptionsChangedOnRenderer();
    HTMLFormControlElement::didRecalcStyle(styleChange);
}

const AtomString& HTMLSelectElement::formControlType() const
{
    static MainThreadNeverDestroyed<const AtomString> selectMultiple("select-multiple"_s);
    static MainThreadNeverDestroyed<const AtomString> selectOne("select-one"_s);
    return m_multiple ? selectMultiple : selectOne;
}

void HTMLSelectElement::optionSelectedByUser(int optionIndex, bool fireOnChangeNow, bool allowMultipleSelection)
{
    // User interaction such as mousedown events can cause list box select elements to send change events.
    // This produces that same behavior for changes triggered by other code running on behalf of the user.
    if (!usesMenuList()) {
        updateSelectedState(optionToListIndex(optionIndex), allowMultipleSelection, false);
        updateValidity();
        if (CheckedPtr renderer = this->renderer())
            renderer->updateFromElement();
        if (fireOnChangeNow)
            listBoxOnChange();
        return;
    }

    // Bail out if this index is already the selected one, to avoid running unnecessary JavaScript that can mess up
    // autofill when there is no actual change (see https://bugs.webkit.org/show_bug.cgi?id=35256 and <rdar://7467917>).
    // The selectOption function does not behave this way, possibly because other callers need a change event even
    // in cases where the selected option is not change.
    if (optionIndex == selectedIndex())
        return;

    selectOption(optionIndex, DeselectOtherOptions | (fireOnChangeNow ? DispatchChangeEvent : 0) | UserDriven);
}

bool HTMLSelectElement::hasPlaceholderLabelOption() const
{
    // The select element has no placeholder label option if it has an attribute "multiple" specified or a display size of non-1.
    // 
    // The condition "size() > 1" is not compliant with the HTML5 spec as of Dec 3, 2010. "size() != 1" is correct.
    // Using "size() > 1" here because size() may be 0 in WebKit.
    // See the discussion at https://bugs.webkit.org/show_bug.cgi?id=43887
    //
    // "0 size()" happens when an attribute "size" is absent or an invalid size attribute is specified.
    // In this case, the display size should be assumed as the default.
    // The default display size is 1 for non-multiple select elements, and 4 for multiple select elements.
    //
    // Finally, if size() == 0 and non-multiple, the display size can be assumed as 1.
    if (multiple() || size() > 1)
        return false;

    int listIndex = optionToListIndex(0);
    ASSERT(listIndex >= 0);
    if (listIndex)
        return false;
    Ref option = downcast<HTMLOptionElement>(*listItems()[listIndex]);
    return option->value().isEmpty();
}

String HTMLSelectElement::validationMessage() const
{
    if (!willValidate())
        return String();

    if (customError())
        return customValidationMessage();

    return valueMissing() ? validationMessageValueMissingForSelectText() : String();
}

bool HTMLSelectElement::valueMissing() const
{
    if (!isRequired())
        return false;

    int firstSelectionIndex = selectedIndex();

    // If a non-placeholer label option is selected (firstSelectionIndex > 0), it's not value-missing.
    return firstSelectionIndex < 0 || (!firstSelectionIndex && hasPlaceholderLabelOption());
}

void HTMLSelectElement::listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow)
{
    if (!multiple())
        optionSelectedByUser(listToOptionIndex(listIndex), fireOnChangeNow, false);
    else {
        updateSelectedState(listIndex, allowMultiplySelections, shift);
        updateValidity();
        if (fireOnChangeNow)
            listBoxOnChange();
    }
}

bool HTMLSelectElement::usesMenuList() const
{
#if !PLATFORM(IOS_FAMILY)
    if (RenderTheme::singleton().delegatesMenuListRendering())
        return true;

    return !m_multiple && m_size <= 1;
#else
    return !m_multiple;
#endif
}

int HTMLSelectElement::activeSelectionStartListIndex() const
{
    if (m_activeSelectionAnchorIndex >= 0)
        return m_activeSelectionAnchorIndex;
    return optionToListIndex(selectedIndex());
}

int HTMLSelectElement::activeSelectionEndListIndex() const
{
    if (m_activeSelectionEndIndex >= 0)
        return m_activeSelectionEndIndex;
    return lastSelectedListIndex();
}

ExceptionOr<void> HTMLSelectElement::add(const OptionOrOptGroupElement& element, const std::optional<HTMLElementOrInt>& before)
{
    RefPtr<HTMLElement> beforeElement;
    Ref<ContainerNode> parent = *this;
    if (before) {
        beforeElement = WTF::switchOn(before.value(),
            [](const RefPtr<HTMLElement>& element) -> HTMLElement* { return element.get(); },
            [this](int index) -> HTMLElement* { return item(index); }
        );
        if (std::holds_alternative<int>(before.value()) && beforeElement && beforeElement->parentNode())
            parent = *beforeElement->parentNode();
    }
    Ref toInsert = WTF::switchOn(element,
        [](const auto& htmlElement) -> HTMLElement& { return *htmlElement; }
    );

    return parent->insertBefore(toInsert, WTFMove(beforeElement));
}

void HTMLSelectElement::remove(int optionIndex)
{
    int listIndex = optionToListIndex(optionIndex);
    if (listIndex < 0)
        return;

    Ref { *listItems()[listIndex] }->remove();
}

String HTMLSelectElement::value() const
{
    if (document().requiresScriptExecutionTelemetry(ScriptTelemetryCategory::FormControls))
        return emptyString();
    for (auto& item : listItems()) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(item.get())) {
            if (option->selected())
                return option->value();
        }
    }
    return emptyString();
}

void HTMLSelectElement::setValue(const String& value)
{
    // Find the option with value() matching the given parameter and make it the current selection.
    unsigned optionIndex = 0;
    for (auto& item : listItems()) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(item.get())) {
            if (option->value() == value) {
                setSelectedIndex(optionIndex);
                return;
            }
            ++optionIndex;
        }
    }

    setSelectedIndex(-1);
}

bool HTMLSelectElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == alignAttr) {
        // Don't map 'align' attribute. This matches what Firefox, Opera and IE do.
        // See http://bugs.webkit.org/show_bug.cgi?id=12072
        return false;
    }

    return HTMLFormControlElement::hasPresentationalHintsForAttribute(name);
}

void HTMLSelectElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::sizeAttr: {
        unsigned oldSize = m_size;
        unsigned size = limitToOnlyHTMLNonNegative(newValue);

        // Ensure that we've determined selectedness of the items at least once prior to changing the size.
        if (oldSize != size)
            updateListItemSelectedStates();

        m_size = size;
        updateValidity();
        if (m_size != oldSize) {
            invalidateStyleAndRenderersForSubtree();
            setRecalcListItems();
            updateValidity();
        }
        break;
    }
    case AttributeNames::multipleAttr:
        parseMultipleAttribute(newValue);
        break;
    default:
        HTMLFormControlElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
        break;
    }
}

int HTMLSelectElement::defaultTabIndex() const
{
    return 0;
}

bool HTMLSelectElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (renderer())
        return isFocusable();
    return HTMLFormControlElement::isKeyboardFocusable(event);
}

bool HTMLSelectElement::isMouseFocusable() const
{
    if (renderer())
        return isFocusable();
    return HTMLFormControlElement::isMouseFocusable();
}

bool HTMLSelectElement::canSelectAll() const
{
    return !usesMenuList();
}

RenderPtr<RenderElement> HTMLSelectElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
#if !PLATFORM(IOS_FAMILY)
    if (usesMenuList())
        return createRenderer<RenderMenuList>(*this, WTFMove(style));
    return createRenderer<RenderListBox>(*this, WTFMove(style));
#else
    return createRenderer<RenderMenuList>(*this, WTFMove(style));
#endif
}

bool HTMLSelectElement::childShouldCreateRenderer(const Node& child) const
{
    if (!HTMLFormControlElement::childShouldCreateRenderer(child))
        return false;
#if !PLATFORM(IOS_FAMILY)
    if (!usesMenuList())
        return is<HTMLOptionElement>(child) || is<HTMLOptGroupElement>(child) || validationMessageShadowTreeContains(child);
#endif
    return validationMessageShadowTreeContains(child);
}

Ref<HTMLCollection> HTMLSelectElement::selectedOptions()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<CollectionType::SelectedOptions>::traversalType>>(*this, CollectionType::SelectedOptions);
}

Ref<HTMLOptionsCollection> HTMLSelectElement::options()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLOptionsCollection>(*this, CollectionType::SelectOptions);
}

void HTMLSelectElement::updateListItemSelectedStates(AllowStyleInvalidation allowStyleInvalidation)
{
    if (m_shouldRecalcListItems)
        recalcListItems(true, allowStyleInvalidation);
}

CompletionHandlerCallingScope HTMLSelectElement::optionToSelectFromChildChangeScope(const ContainerNode::ChildChange& change, HTMLOptGroupElement* parentOptGroup)
{
    if (multiple())
        return { };

    auto getLastSelectedOption = [](HTMLOptGroupElement& optGroup) -> HTMLOptionElement* {
        for (auto* option = Traversal<HTMLOptionElement>::lastChild(optGroup); option; option = Traversal<HTMLOptionElement>::previousSibling(*option)) {
            if (option->selectedWithoutUpdate())
                return option;
        }
        return nullptr;
    };

    RefPtr<HTMLOptionElement> optionToSelect;
    if (change.type == ChildChange::Type::ElementInserted) {
        if (auto* option = dynamicDowncast<HTMLOptionElement>(*change.siblingChanged)) {
            if (option->selectedWithoutUpdate())
                optionToSelect = option;
        } else if (RefPtr optGroup = dynamicDowncast<HTMLOptGroupElement>(change.siblingChanged); !parentOptGroup && optGroup)
            optionToSelect = getLastSelectedOption(*optGroup);
    } else if (parentOptGroup && change.type == ContainerNode::ChildChange::Type::AllChildrenReplaced)
        optionToSelect = getLastSelectedOption(*parentOptGroup);

    return CompletionHandlerCallingScope { [optionToSelect = WTFMove(optionToSelect), isInsertion = change.isInsertion(), select = Ref { *this }] {
        if (optionToSelect)
            select->optionSelectionStateChanged(*optionToSelect, true);
        else if (isInsertion)
            select->scrollToSelection();
    } };
}

void HTMLSelectElement::childrenChanged(const ChildChange& change)
{
    ASSERT(change.affectsElements != ChildChange::AffectsElements::Unknown);

    if (change.affectsElements == ChildChange::AffectsElements::No) {
        HTMLFormControlElement::childrenChanged(change);
        return;
    }

    auto selectOptionIfNecessaryScope = optionToSelectFromChildChangeScope(change);

    setRecalcListItems();
    updateValidity();
    m_lastOnChangeSelection.clear();

    HTMLFormControlElement::childrenChanged(change);
}

void HTMLSelectElement::optionElementChildrenChanged()
{
    setOptionsChangedOnRenderer();
    invalidateStyleForSubtree();
    updateValidity();
    if (CheckedPtr cache = document().existingAXObjectCache())
        cache->childrenChanged(this);
}

void HTMLSelectElement::setSize(unsigned size)
{
    setUnsignedIntegralAttribute(sizeAttr, limitToOnlyHTMLNonNegative(size));
}

HTMLOptionElement* HTMLSelectElement::namedItem(const AtomString& name)
{
    return options()->namedItem(name);
}

HTMLOptionElement* HTMLSelectElement::item(unsigned index)
{
    return options()->item(index);
}

bool HTMLSelectElement::isSupportedPropertyIndex(unsigned index)
{
    return options()->isSupportedPropertyIndex(index);
}

ExceptionOr<void> HTMLSelectElement::setItem(unsigned index, HTMLOptionElement* option)
{
    if (!option) {
        remove(index);
        return { };
    }

    // If we are adding options, we should check 'index > maxSelectItems' first to avoid integer overflow.
    if (index > length() && index >= maxSelectItems) {
        protectedDocument()->addConsoleMessage(MessageSource::Other, MessageLevel::Warning, makeString("Unable to expand the option list and set an option at index. The maximum list length is "_s, maxSelectItems, '.'));
        return { };
    }

    int diff = index - length();
    
    RefPtr<HTMLOptionElement> before;
    // Out of array bounds? First insert empty dummies.
    if (diff > 0) {
        auto result = setLength(index);
        if (result.hasException())
            return result;
        // Replace an existing entry?
    } else if (diff < 0) {
        before = item(index + 1);
        remove(index);
    }

    // Finally add the new element.
    auto result = add(option, HTMLElementOrInt { before.get() });
    if (result.hasException())
        return result;

    if (diff >= 0 && option->selected())
        optionSelectionStateChanged(*option, true);

    return { };
}

ExceptionOr<void> HTMLSelectElement::setLength(unsigned newLength)
{
    // If we are adding options, we should check 'index > maxSelectItems' first to avoid integer overflow.
    if (newLength > length() && newLength > maxSelectItems) {
        protectedDocument()->addConsoleMessage(MessageSource::Other, MessageLevel::Warning, makeString("Unable to expand the option list to length "_s, newLength, " items. The maximum number of items allowed is "_s, maxSelectItems, '.'));
        return { };
    }

    int diff = length() - newLength;

    if (diff < 0) { // Add dummy elements.
        do {
            auto result = add(HTMLOptionElement::create(protectedDocument()).ptr(), std::nullopt);
            if (result.hasException())
                return result;
        } while (++diff);
    } else {
        auto& items = listItems();

        // Removing children fires mutation events, which might mutate the DOM further, so we first copy out a list
        // of elements that we intend to remove then attempt to remove them one at a time.
        Vector<Ref<HTMLOptionElement>> itemsToRemove;
        size_t optionIndex = 0;
        for (auto& item : items) {
            auto* option = dynamicDowncast<HTMLOptionElement>(*item);
            if (option && optionIndex++ >= newLength) {
                ASSERT(item->parentNode());
                itemsToRemove.append(*option);
            }
        }

        // FIXME: Clients can detect what order we remove the options in; is it good to remove them in ascending order?
        // FIXME: This ignores exceptions. A previous version passed through the exception only for the last item removed.
        // What exception behavior do we want?
        for (auto& item : itemsToRemove)
            item->remove();
    }
    return { };
}

bool HTMLSelectElement::isRequiredFormControl() const
{
    return isRequired();
}

bool HTMLSelectElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(editability);
    return !isDisabledFormControl();
#else
    return HTMLFormControlElement::willRespondToMouseClickEventsWithEditability(editability);
#endif
}

// Returns the 1st valid item |skip| items from |listIndex| in direction |direction| if there is one.
// Otherwise, it returns the valid item closest to that boundary which is past |listIndex| if there is one.
// Otherwise, it returns |listIndex|.
// Valid means that it is enabled and an option element.
int HTMLSelectElement::nextValidIndex(int listIndex, SkipDirection direction, int skip) const
{
    ASSERT(direction == -1 || direction == 1);
    auto& listItems = this->listItems();
    int lastGoodIndex = listIndex;
    int size = listItems.size();
    for (listIndex += direction; listIndex >= 0 && listIndex < size; listIndex += direction) {
        --skip;
        if (!listItems[listIndex]->isDisabledFormControl() && is<HTMLOptionElement>(*listItems[listIndex])) {
            lastGoodIndex = listIndex;
            if (skip <= 0)
                break;
        }
    }
    return lastGoodIndex;
}

int HTMLSelectElement::nextSelectableListIndex(int startIndex) const
{
    return nextValidIndex(startIndex, SkipForwards, 1);
}

int HTMLSelectElement::previousSelectableListIndex(int startIndex) const
{
    if (startIndex == -1)
        startIndex = listItems().size();
    return nextValidIndex(startIndex, SkipBackwards, 1);
}

int HTMLSelectElement::firstSelectableListIndex() const
{
    auto& items = listItems();
    int index = nextValidIndex(items.size(), SkipBackwards, INT_MAX);
    if (static_cast<size_t>(index) == items.size())
        return -1;
    return index;
}

int HTMLSelectElement::lastSelectableListIndex() const
{
    return nextValidIndex(-1, SkipForwards, INT_MAX);
}

// Returns the index of the next valid item one page away from |startIndex| in direction |direction|.
int HTMLSelectElement::nextSelectableListIndexPageAway(int startIndex, SkipDirection direction) const
{
    auto& items = listItems();

    // Can't use m_size because renderer forces a minimum size.
    int pageSize = 0;
    if (CheckedPtr renderListBox = dynamicDowncast<RenderListBox>(*renderer()))
        pageSize = renderListBox->size() - 1; // -1 so we still show context.

    // One page away, but not outside valid bounds.
    // If there is a valid option item one page away, the index is chosen.
    // If there is no exact one page away valid option, returns startIndex or the most far index.
    int edgeIndex = direction == SkipForwards ? 0 : items.size() - 1;
    int skipAmount = pageSize + (direction == SkipForwards ? startIndex : edgeIndex - startIndex);
    return nextValidIndex(edgeIndex, direction, skipAmount);
}

void HTMLSelectElement::selectAll()
{
    ASSERT(!usesMenuList());
    if (!renderer() || !m_multiple)
        return;

    // Save the selection so it can be compared to the new selectAll selection
    // when dispatching change events.
    saveLastSelection();

    m_activeSelectionState = true;
    setActiveSelectionAnchorIndex(nextSelectableListIndex(-1));
    setActiveSelectionEndIndex(previousSelectableListIndex(-1));
    if (m_activeSelectionAnchorIndex < 0)
        return;

    updateListBoxSelection(false);
    listBoxOnChange();
    updateValidity();
}

void HTMLSelectElement::saveLastSelection()
{
    if (usesMenuList()) {
        m_lastOnChangeIndex = selectedIndex();
        return;
    }

    m_lastOnChangeSelection = listItems().map([](auto& element) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*element);
        return option && option->selected();
    });
}

void HTMLSelectElement::setActiveSelectionAnchorIndex(int index)
{
    m_activeSelectionAnchorIndex = index;

    // Cache the selection state so we can restore the old selection as the new
    // selection pivots around this anchor index.
    m_cachedStateForActiveSelection = listItems().map([](auto& element) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*element);
        return option && option->selected();
    });
}

void HTMLSelectElement::setActiveSelectionEndIndex(int index)
{
    m_activeSelectionEndIndex = index;
}

void HTMLSelectElement::updateListBoxSelection(bool deselectOtherOptions)
{
    ASSERT(renderer());

#if !PLATFORM(IOS_FAMILY)
    ASSERT(renderer()->isRenderListBox() || m_multiple);
#else
    ASSERT(renderer()->isRenderMenuList() || m_multiple);
#endif

    ASSERT(!listItems().size() || m_activeSelectionAnchorIndex >= 0);

    unsigned start = std::min(m_activeSelectionAnchorIndex, m_activeSelectionEndIndex);
    unsigned end = std::max(m_activeSelectionAnchorIndex, m_activeSelectionEndIndex);

    auto& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        RefPtr element = dynamicDowncast<HTMLOptionElement>(*items[i]);
        if (!element || element->isDisabledFormControl())
            continue;

        if (i >= start && i <= end)
            element->setSelectedState(m_activeSelectionState);
        else if (deselectOtherOptions || i >= m_cachedStateForActiveSelection.size())
            element->setSelectedState(false);
        else
            element->setSelectedState(m_cachedStateForActiveSelection[i]);
    }

    invalidateSelectedItems();
    scrollToSelection();
    updateValidity();
}

void HTMLSelectElement::listBoxOnChange()
{
    ASSERT(!usesMenuList() || m_multiple);

    auto& items = listItems();

    // If the cached selection list is empty, or the size has changed, then fire
    // dispatchFormControlChangeEvent, and return early.
    if (m_lastOnChangeSelection.isEmpty() || m_lastOnChangeSelection.size() != items.size()) {
        dispatchFormControlChangeEvent();
        return;
    }

    // Update m_lastOnChangeSelection and fire dispatchFormControlChangeEvent.
    bool fireOnChange = false;
    for (unsigned i = 0; i < items.size(); ++i) {
        bool selected = [&] {
            RefPtr option = dynamicDowncast<HTMLOptionElement>(*items[i]);
            return option && option->selected();
        }();
        if (selected != m_lastOnChangeSelection[i])
            fireOnChange = true;
        m_lastOnChangeSelection[i] = selected;
    }

    if (fireOnChange) {
        dispatchInputEvent();
        dispatchFormControlChangeEvent();
    }
}

void HTMLSelectElement::dispatchChangeEventForMenuList()
{
    ASSERT(usesMenuList());

    int selected = selectedIndex();
    if (m_lastOnChangeIndex != selected && m_isProcessingUserDrivenChange) {
        m_lastOnChangeIndex = selected;
        m_isProcessingUserDrivenChange = false;
        dispatchInputEvent();
        dispatchFormControlChangeEvent();
    }
}

void HTMLSelectElement::scrollToSelection()
{
#if !PLATFORM(IOS_FAMILY)
    if (usesMenuList())
        return;

    if (CheckedPtr renderer = dynamicDowncast<RenderListBox>(this->renderer()))
        renderer->selectionChanged();
#else
    if (CheckedPtr renderer = this->renderer())
        renderer->repaint();
#endif
}

void HTMLSelectElement::setOptionsChangedOnRenderer()
{
    if (CheckedPtr renderer = this->renderer()) {
#if !PLATFORM(IOS_FAMILY)
        if (auto* renderMenuList = dynamicDowncast<RenderMenuList>(*renderer))
            renderMenuList->setOptionsChanged(true);
        else
            downcast<RenderListBox>(*renderer).setOptionsChanged(true);
#else
        downcast<RenderMenuList>(*renderer).setOptionsChanged(true);
#endif
    }
}

const Vector<WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>>& HTMLSelectElement::listItems() const
{
    if (m_shouldRecalcListItems)
        recalcListItems();
    else {
#if ASSERT_ENABLED
        Vector<WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>> items = m_listItems;
        recalcListItems(false);
        ASSERT(items == m_listItems);
#endif
    }

    return m_listItems;
}

void HTMLSelectElement::invalidateSelectedItems()
{
    if (RefPtr collection = cachedHTMLCollection(CollectionType::SelectedOptions))
        collection->invalidateCache();
}

void HTMLSelectElement::setRecalcListItems()
{
    m_shouldRecalcListItems = true;
    // Manual selection anchor is reset when manipulating the select programmatically.
    m_activeSelectionAnchorIndex = -1;
    setOptionsChangedOnRenderer();
    invalidateStyleForSubtree();
    if (!isConnected()) {
        if (RefPtr collection = cachedHTMLCollection(CollectionType::SelectOptions))
            collection->invalidateCache();
    }
    if (!isConnected())
        invalidateSelectedItems();
    if (CheckedPtr cache = document().existingAXObjectCache())
        cache->childrenChanged(this);

    if (Ref document = this->document(); this == document->focusedElement()) {
        if (RefPtr page = document->page())
            page->chrome().client().focusedSelectElementDidChangeOptions(*this);
    }
}

void HTMLSelectElement::recalcListItems(bool updateSelectedStates, AllowStyleInvalidation allowStyleInvalidation) const
{
    m_listItems.clear();

    m_shouldRecalcListItems = false;

    RefPtr<HTMLOptionElement> foundSelected;
    RefPtr<HTMLOptionElement> firstOption;
    auto handleOptionElement = [&](HTMLOptionElement& option) {
        m_listItems.append(&option);
        if (updateSelectedStates && !m_multiple) {
            if (!firstOption)
                firstOption = &option;
            if (option.selected()) {
                if (foundSelected)
                    foundSelected->setSelectedState(false, allowStyleInvalidation);
                foundSelected = &option;
            } else if (m_size <= 1 && !foundSelected && !option.isDisabledFormControl()) {
                foundSelected = &option;
                foundSelected->setSelectedState(true, allowStyleInvalidation);
            }
        }
    };

    for (Ref child : childrenOfType<HTMLElement>(*const_cast<HTMLSelectElement*>(this))) {
        if (is<HTMLOptGroupElement>(child.get())) {
            m_listItems.append(&child.get());
            for (Ref option : childrenOfType<HTMLOptionElement>(child.get()))
                handleOptionElement(option);
        } else if (RefPtr option = dynamicDowncast<HTMLOptionElement>(child.get()))
            handleOptionElement(*option);
        else if (is<HTMLHRElement>(child.get()))
            m_listItems.append(&child.get());
    }

    if (!foundSelected && m_size <= 1 && firstOption && !firstOption->selected())
        firstOption->setSelectedState(true, allowStyleInvalidation);
}

int HTMLSelectElement::selectedIndex() const
{
    unsigned index = 0;

    // Return the number of the first option selected.
    for (auto& element : listItems()) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*element)) {
            if (option->selected())
                return index;
            ++index;
        }
    }

    return -1;
}

void HTMLSelectElement::setSelectedIndex(int index)
{
    selectOption(index, DeselectOtherOptions);
}

void HTMLSelectElement::optionSelectionStateChanged(HTMLOptionElement& option, bool optionIsSelected)
{
    ASSERT(option.ownerSelectElement() == this);
    if (optionIsSelected)
        selectOption(option.index());
    else if (!usesMenuList())
        selectOption(-1);
    else
        selectOption(nextSelectableListIndex(-1));
}

void HTMLSelectElement::selectOption(int optionIndex, SelectOptionFlags flags)
{
    bool shouldDeselect = !m_multiple || (flags & DeselectOtherOptions);

    auto& items = listItems();
    int listIndex = optionToListIndex(optionIndex);

    RefPtr<HTMLElement> element;
    if (listIndex >= 0)
        element = items[listIndex].get();

    if (shouldDeselect)
        deselectItemsWithoutValidation(element.get());

    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(element)) {
        if (m_activeSelectionAnchorIndex < 0 || shouldDeselect)
            setActiveSelectionAnchorIndex(listIndex);
        if (m_activeSelectionEndIndex < 0 || shouldDeselect)
            setActiveSelectionEndIndex(listIndex);
        option->setSelectedState(true);
    }

    invalidateSelectedItems();
    updateValidity();

    // For the menu list case, this is what makes the selected element appear.
    if (CheckedPtr renderer = this->renderer())
        renderer->updateFromElement();

    scrollToSelection();

    if (usesMenuList()) {
        m_isProcessingUserDrivenChange = flags & UserDriven;
        if (flags & DispatchChangeEvent)
            dispatchChangeEventForMenuList();
        if (CheckedPtr renderer = this->renderer()) {
            if (auto* renderMenuList = dynamicDowncast<RenderMenuList>(*renderer))
                renderMenuList->didSetSelectedIndex(listIndex);
            else
                downcast<RenderListBox>(*renderer).selectionChanged();
        }
    }
}

int HTMLSelectElement::optionToListIndex(int optionIndex) const
{
    auto& items = listItems();
    int listSize = static_cast<int>(items.size());
    if (optionIndex < 0 || optionIndex >= listSize)
        return -1;

    int optionIndex2 = -1;
    for (int listIndex = 0; listIndex < listSize; ++listIndex) {
        if (is<HTMLOptionElement>(*items[listIndex])) {
            ++optionIndex2;
            if (optionIndex2 == optionIndex)
                return listIndex;
        }
    }

    return -1;
}

int HTMLSelectElement::listToOptionIndex(int listIndex) const
{
    auto& items = listItems();
    if (listIndex < 0 || listIndex >= static_cast<int>(items.size()) || !is<HTMLOptionElement>(*items[listIndex]))
        return -1;

    // Actual index of option not counting OPTGROUP entries that may be in list.
    int optionIndex = 0;
    for (int i = 0; i < listIndex; ++i) {
        if (is<HTMLOptionElement>(*items[i]))
            ++optionIndex;
    }

    return optionIndex;
}

void HTMLSelectElement::dispatchFocusEvent(RefPtr<Element>&& oldFocusedElement, const FocusOptions& options)
{
    // Save the selection so it can be compared to the new selection when
    // dispatching change events during blur event dispatch.
    if (usesMenuList())
        saveLastSelection();
    HTMLFormControlElement::dispatchFocusEvent(WTFMove(oldFocusedElement), options);
}

void HTMLSelectElement::dispatchBlurEvent(RefPtr<Element>&& newFocusedElement)
{
    // We only need to fire change events here for menu lists, because we fire
    // change events for list boxes whenever the selection change is actually made.
    // This matches other browsers' behavior.
    if (usesMenuList())
        dispatchChangeEventForMenuList();
    HTMLFormControlElement::dispatchBlurEvent(WTFMove(newFocusedElement));
}

void HTMLSelectElement::deselectItemsWithoutValidation(HTMLElement* excludeElement)
{
    for (auto& element : listItems()) {
        if (element == excludeElement)
            continue;
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*element))
            option->setSelectedState(false);
    }
    invalidateSelectedItems();
}

FormControlState HTMLSelectElement::saveFormControlState() const
{
    FormControlState state;
    auto& items = listItems();
    state.reserveInitialCapacity(items.size());
    for (auto& element : items) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*element);
        if (!option || !option->selected())
            continue;
        state.append(option->value());
        if (!multiple())
            break;
    }
    return state;
}

size_t HTMLSelectElement::searchOptionsForValue(const String& value, size_t listIndexStart, size_t listIndexEnd) const
{
    auto& items = listItems();
    size_t loopEndIndex = std::min(items.size(), listIndexEnd);
    for (size_t i = listIndexStart; i < loopEndIndex; ++i) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*items[i]);
        if (!option)
            continue;
        if (option->value() == value)
            return i;
    }
    return notFound;
}

void HTMLSelectElement::restoreFormControlState(const FormControlState& state)
{
    recalcListItems();

    auto& items = listItems();
    size_t itemsSize = items.size();
    if (!itemsSize)
        return;

    for (auto& element : items) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*element))
            option->setSelectedState(false);
    }

    if (!multiple()) {
        size_t foundIndex = searchOptionsForValue(state[0], 0, itemsSize);
        if (foundIndex != notFound)
            Ref { downcast<HTMLOptionElement>(*items[foundIndex]) }->setSelectedState(true);
    } else {
        size_t startIndex = 0;
        for (auto& value : state) {
            size_t foundIndex = searchOptionsForValue(value, startIndex, itemsSize);
            if (foundIndex == notFound)
                foundIndex = searchOptionsForValue(value, 0, startIndex);
            if (foundIndex == notFound)
                continue;
            Ref { downcast<HTMLOptionElement>(*items[foundIndex]) }->setSelectedState(true);
            startIndex = foundIndex + 1;
        }
    }

    invalidateSelectedItems();
    setOptionsChangedOnRenderer();
    updateValidity();
}

void HTMLSelectElement::parseMultipleAttribute(const AtomString& value)
{
    bool oldUsesMenuList = usesMenuList();
    bool oldMultiple = m_multiple;
    int oldSelectedIndex = selectedIndex();
    m_multiple = !value.isNull();
    updateValidity();
    if (oldUsesMenuList != usesMenuList())
        invalidateStyleAndRenderersForSubtree();
    if (oldMultiple != m_multiple) {
        if (oldSelectedIndex >= 0)
            setSelectedIndex(oldSelectedIndex);
        else
            reset();
    }
}

bool HTMLSelectElement::appendFormData(DOMFormData& formData)
{
    const AtomString& name = this->name();
    if (name.isEmpty())
        return false;

    bool successful = false;
    for (auto& element : listItems()) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*element);
        if (option && option->selected() && !option->isDisabledFormControl()) {
            formData.append(name, option->value());
            successful = true;
        }
    }

    // It's possible that this is a menulist with multiple options and nothing
    // will be submitted (!successful). We won't send a unselected non-disabled
    // option as fallback. This behavior matches to other browsers.
    return successful;
} 

void HTMLSelectElement::reset()
{
    RefPtr<HTMLOptionElement> firstOption;
    RefPtr<HTMLOptionElement> selectedOption;

    for (auto& element : listItems()) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*element);
        if (!option)
            continue;

        if (option->hasAttributeWithoutSynchronization(selectedAttr)) {
            if (selectedOption && !m_multiple)
                selectedOption->setSelectedState(false);
            option->setSelectedState(true);
            selectedOption = option;
        } else
            option->setSelectedState(false);

        if (!firstOption && !option->isDisabledFormControl())
            firstOption = WTFMove(option);
    }

    if (!selectedOption && firstOption && !m_multiple && m_size <= 1)
        firstOption->setSelectedState(true);

    setInteractedWithSinceLastFormSubmitEvent(false);
    invalidateSelectedItems();
    setOptionsChangedOnRenderer();
    invalidateStyleForSubtree();
    updateValidity();
}

#if !PLATFORM(WIN)

bool HTMLSelectElement::platformHandleKeydownEvent(KeyboardEvent* event)
{
    if (!RenderTheme::singleton().popsMenuByArrowKeys())
        return false;

    if (!isSpatialNavigationEnabled(document().frame())) {
        if (event->keyIdentifier() == "Down"_s || event->keyIdentifier() == "Up"_s) {
            focus();
            protectedDocument()->updateStyleIfNeeded();
            // Calling focus() may cause us to lose our renderer. Return true so
            // that our caller doesn't process the event further, but don't set
            // the event as handled.
            WeakPtr renderer = dynamicDowncast<RenderMenuList>(this->renderer());
            if (!renderer)
                return true;

            // Save the selection so it can be compared to the new selection
            // when dispatching change events during selectOption, which
            // gets called from RenderMenuList::valueChanged, which gets called
            // after the user makes a selection from the menu.
            saveLastSelection();
            renderer->showPopup(); // showPopup() may run JS and cause the renderer to get destroyed.
            event->setDefaultHandled();
        }
        return true;
    }

    return false;
}

#endif

void HTMLSelectElement::menuListDefaultEventHandler(Event& event)
{
    ASSERT(renderer());
    ASSERT(renderer()->isRenderMenuList());

    auto& eventNames = WebCore::eventNames();
    if (event.type() == eventNames.keydownEvent) {
        RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event);
        if (!keyboardEvent)
            return;

        if (platformHandleKeydownEvent(keyboardEvent.get()))
            return;

        // When using spatial navigation, we want to be able to navigate away
        // from the select element when the user hits any of the arrow keys,
        // instead of changing the selection.
        if (isSpatialNavigationEnabled(document().frame())) {
            if (!m_activeSelectionState)
                return;
        }

        const String& keyIdentifier = keyboardEvent->keyIdentifier();
        bool handled = true;
        auto& listItems = this->listItems();
        int listIndex = optionToListIndex(selectedIndex());

        // When using caret browsing, we want to be able to move the focus
        // out of the select element when user hits a left or right arrow key.
        if (document().settings().caretBrowsingEnabled()) {
            if (keyIdentifier == "Left"_s || keyIdentifier == "Right"_s)
                return;
        }

        if (keyIdentifier == "Down"_s || keyIdentifier == "Right"_s)
            listIndex = nextValidIndex(listIndex, SkipForwards, 1);
        else if (keyIdentifier == "Up"_s || keyIdentifier == "Left"_s)
            listIndex = nextValidIndex(listIndex, SkipBackwards, 1);
        else if (keyIdentifier == "PageDown"_s)
            listIndex = nextValidIndex(listIndex, SkipForwards, 3);
        else if (keyIdentifier == "PageUp"_s)
            listIndex = nextValidIndex(listIndex, SkipBackwards, 3);
        else if (keyIdentifier == "Home"_s)
            listIndex = nextValidIndex(-1, SkipForwards, 1);
        else if (keyIdentifier == "End"_s)
            listIndex = nextValidIndex(listItems.size(), SkipBackwards, 1);
        else
            handled = false;

        if (handled && static_cast<size_t>(listIndex) < listItems.size())
            selectOption(listToOptionIndex(listIndex), DeselectOtherOptions | DispatchChangeEvent | UserDriven);

        if (handled)
            keyboardEvent->setDefaultHandled();
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (event.type() == eventNames.keypressEvent) {
        RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event);
        if (!keyboardEvent)
            return;

        int keyCode = keyboardEvent->keyCode();
        bool handled = false;

        if (keyCode == ' ' && isSpatialNavigationEnabled(document().frame())) {
            // Use space to toggle arrow key handling for selection change or spatial navigation.
            m_activeSelectionState = !m_activeSelectionState;
            keyboardEvent->setDefaultHandled();
            return;
        }

        if (RenderTheme::singleton().popsMenuBySpaceOrReturn()) {
            if (keyCode == ' ' || keyCode == '\r') {
                focus();
                protectedDocument()->updateStyleIfNeeded();

                // Calling focus() may remove the renderer or change the renderer type.
                WeakPtr renderer = dynamicDowncast<RenderMenuList>(this->renderer());
                if (!renderer)
                    return;

                // Save the selection so it can be compared to the new selection
                // when dispatching change events during selectOption, which
                // gets called from RenderMenuList::valueChanged, which gets called
                // after the user makes a selection from the menu.
                saveLastSelection();
                renderer->showPopup(); // showPopup() may run JS and cause the renderer to get destroyed.
                handled = true;
            }
        } else if (RenderTheme::singleton().popsMenuByArrowKeys()) {
            if (keyCode == ' ') {
                focus();
                protectedDocument()->updateStyleIfNeeded();

                // Calling focus() may remove the renderer or change the renderer type.
                WeakPtr renderer = dynamicDowncast<RenderMenuList>(this->renderer());
                if (!renderer)
                    return;

                // Save the selection so it can be compared to the new selection
                // when dispatching change events during selectOption, which
                // gets called from RenderMenuList::valueChanged, which gets called
                // after the user makes a selection from the menu.
                saveLastSelection();
                renderer->showPopup(); // showPopup() may run JS and cause the renderer to get destroyed.
                handled = true;
            } else if (keyCode == '\r') {
                if (RefPtr form = this->form())
                    form->submitImplicitly(*keyboardEvent, false);
                dispatchChangeEventForMenuList();
                handled = true;
            }
        }

        if (handled)
            keyboardEvent->setDefaultHandled();
    }

    if (RefPtr mouseEvent = dynamicDowncast<MouseEvent>(event); event.type() == eventNames.mousedownEvent && mouseEvent && mouseEvent->button() == MouseButton::Left) {
        focus();
#if !PLATFORM(IOS_FAMILY)
        protectedDocument()->updateStyleIfNeeded();

        if (WeakPtr menuList = dynamicDowncast<RenderMenuList>(renderer())) {
            ASSERT(!menuList->popupIsVisible());
            // Save the selection so it can be compared to the new
            // selection when we call onChange during selectOption,
            // which gets called from RenderMenuList::valueChanged,
            // which gets called after the user makes a selection from
            // the menu.
            saveLastSelection();
            menuList->showPopup(); // showPopup() may run JS and cause the renderer to get destroyed.
        }
#endif
        event.setDefaultHandled();
    }

#if !PLATFORM(IOS_FAMILY)
    if (event.type() == eventNames.blurEvent && !focused()) {
        CheckedRef menuList = downcast<RenderMenuList>(*renderer());
        if (menuList->popupIsVisible())
            menuList->hidePopup();
    }
#endif
}

void HTMLSelectElement::updateSelectedState(int listIndex, bool multi, bool shift)
{
    auto& items = listItems();
    int listSize = static_cast<int>(items.size());
    if (listIndex < 0 || listIndex >= listSize)
        return;

    Ref clickedElement = *items[listIndex];
    if (is<HTMLOptGroupElement>(clickedElement))
        return;

    // Save the selection so it can be compared to the new selection when
    // dispatching change events during mouseup, or after autoscroll finishes.
    saveLastSelection();

    m_activeSelectionState = true;

    bool shiftSelect = m_multiple && shift;
    bool multiSelect = m_multiple && multi && !shift;

    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(clickedElement)) {
        // Keep track of whether an active selection (like during drag
        // selection), should select or deselect.
        if (option->selected() && multiSelect)
            m_activeSelectionState = false;
        if (!m_activeSelectionState)
            option->setSelectedState(false);
    }

    // If we're not in any special multiple selection mode, then deselect all
    // other items, excluding the clicked option. If no option was clicked, then
    // this will deselect all items in the list.
    if (!shiftSelect && !multiSelect)
        deselectItemsWithoutValidation(clickedElement.ptr());

    // If the anchor hasn't been set, and we're doing a single selection or a
    // shift selection, then initialize the anchor to the first selected index.
    if (m_activeSelectionAnchorIndex < 0 && !multiSelect)
        setActiveSelectionAnchorIndex(selectedIndex());

    // Set the selection state of the clicked option.
    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(clickedElement); option && !option->isDisabledFormControl())
        option->setSelectedState(true);

    // If there was no selectedIndex() for the previous initialization, or If
    // we're doing a single selection, or a multiple selection (using cmd or
    // ctrl), then initialize the anchor index to the listIndex that just got
    // clicked.
    if (m_activeSelectionAnchorIndex < 0 || !shiftSelect)
        setActiveSelectionAnchorIndex(listIndex);

    invalidateSelectedItems();
    setActiveSelectionEndIndex(listIndex);
    updateListBoxSelection(!multiSelect);
}

void HTMLSelectElement::listBoxDefaultEventHandler(Event& event)
{
    auto& listItems = this->listItems();

    auto& eventNames = WebCore::eventNames();
    RefPtr mouseEvent = dynamicDowncast<MouseEvent>(event);
    if (event.type() == eventNames.mousedownEvent && mouseEvent && mouseEvent->button() == MouseButton::Left) {
        focus();
        protectedDocument()->updateStyleIfNeeded();

        // Calling focus() may remove or change our renderer, in which case we don't want to handle the event further.
        CheckedPtr renderListBox = dynamicDowncast<RenderListBox>(this->renderer());
        if (!renderListBox)
            return;

        // Convert to coords relative to the list box if needed.
        IntPoint localOffset = roundedIntPoint(renderListBox->absoluteToLocal(mouseEvent->absoluteLocation(), UseTransforms));
        int listIndex = renderListBox->listIndexAtOffset(toIntSize(localOffset));
        if (listIndex >= 0) {
            if (!isDisabledFormControl()) {
#if PLATFORM(COCOA)
                updateSelectedState(listIndex, mouseEvent->metaKey(), mouseEvent->shiftKey());
#else
                updateSelectedState(listIndex, mouseEvent->ctrlKey(), mouseEvent->shiftKey());
#endif
            }
            if (RefPtr frame = document().frame())
                frame->eventHandler().setMouseDownMayStartAutoscroll();

            mouseEvent->setDefaultHandled();
        }
    } else if (event.type() == eventNames.mousemoveEvent && mouseEvent && !downcast<RenderListBox>(*renderer()).canBeScrolledAndHasScrollableArea()) {
        if (mouseEvent->button() != MouseButton::Left || !mouseEvent->buttonDown())
            return;

        CheckedRef renderListBox = downcast<RenderListBox>(*renderer());
        IntPoint localOffset = roundedIntPoint(renderListBox->absoluteToLocal(mouseEvent->absoluteLocation(), UseTransforms));
        int listIndex = renderListBox->listIndexAtOffset(toIntSize(localOffset));
        if (listIndex >= 0) {
            if (!isDisabledFormControl()) {
                if (m_multiple) {
                    // Only extend selection if there is something selected.
                    if (m_activeSelectionAnchorIndex < 0)
                        return;

                    setActiveSelectionEndIndex(listIndex);
                    updateListBoxSelection(false);
                } else {
                    setActiveSelectionAnchorIndex(listIndex);
                    setActiveSelectionEndIndex(listIndex);
                    updateListBoxSelection(true);
                }
            }
            mouseEvent->setDefaultHandled();
        }
    } else if (event.type() == eventNames.mouseupEvent && mouseEvent && mouseEvent->button() == MouseButton::Left && document().frame()->eventHandler().autoscrollRenderer() != renderer()) {
        // This click or drag event was not over any of the options.
        if (m_lastOnChangeSelection.isEmpty())
            return;
        // This makes sure we fire dispatchFormControlChangeEvent for a single
        // click. For drag selection, onChange will fire when the autoscroll
        // timer stops.
        listBoxOnChange();
    } else if (event.type() == eventNames.keydownEvent) {
        RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event);
        if (!keyboardEvent)
            return;

        CheckedPtr renderer = this->renderer();
        bool isHorizontalWritingMode = renderer ? renderer->writingMode().isHorizontal() : true;
        bool isBlockFlipped = renderer ? renderer->writingMode().isBlockFlipped() : false;

        auto nextKeyIdentifier = isHorizontalWritingMode ? "Down"_s : "Right"_s;
        auto previousKeyIdentifier = isHorizontalWritingMode ? "Up"_s : "Left"_s;
        if (isBlockFlipped)
            std::swap(nextKeyIdentifier, previousKeyIdentifier);

        const String& keyIdentifier = keyboardEvent->keyIdentifier();

        bool handled = false;
        int endIndex = 0;
        if (m_activeSelectionEndIndex < 0) {
            // Initialize the end index
            if (keyIdentifier == nextKeyIdentifier || keyIdentifier == "PageDown"_s) {
                int startIndex = lastSelectedListIndex();
                handled = true;
                if (keyIdentifier == nextKeyIdentifier)
                    endIndex = nextSelectableListIndex(startIndex);
                else
                    endIndex = nextSelectableListIndexPageAway(startIndex, SkipForwards);
            } else if (keyIdentifier == previousKeyIdentifier || keyIdentifier == "PageUp"_s) {
                int startIndex = optionToListIndex(selectedIndex());
                handled = true;
                if (keyIdentifier == previousKeyIdentifier)
                    endIndex = previousSelectableListIndex(startIndex);
                else
                    endIndex = nextSelectableListIndexPageAway(startIndex, SkipBackwards);
            }
        } else {
            // Set the end index based on the current end index.
            if (keyIdentifier == nextKeyIdentifier) {
                endIndex = nextSelectableListIndex(m_activeSelectionEndIndex);
                handled = true;
            } else if (keyIdentifier == previousKeyIdentifier) {
                endIndex = previousSelectableListIndex(m_activeSelectionEndIndex);
                handled = true;
            } else if (keyIdentifier == "PageDown"_s) {
                endIndex = nextSelectableListIndexPageAway(m_activeSelectionEndIndex, SkipForwards);
                handled = true;
            } else if (keyIdentifier == "PageUp"_s) {
                endIndex = nextSelectableListIndexPageAway(m_activeSelectionEndIndex, SkipBackwards);
                handled = true;
            }
        }
        if (keyIdentifier == "Home"_s) {
            endIndex = firstSelectableListIndex();
            handled = true;
        } else if (keyIdentifier == "End"_s) {
            endIndex = lastSelectableListIndex();
            handled = true;
        }

        if (isSpatialNavigationEnabled(document().frame()))
            // Check if the selection moves to the boundary.
            if (keyIdentifier == "Left"_s || keyIdentifier == "Right"_s || ((keyIdentifier == "Down"_s || keyIdentifier == "Up"_s) && endIndex == m_activeSelectionEndIndex))
                return;

        if (endIndex >= 0 && handled) {
            // Save the selection so it can be compared to the new selection
            // when dispatching change events immediately after making the new
            // selection.
            saveLastSelection();

            ASSERT_UNUSED(listItems, !listItems.size() || static_cast<size_t>(endIndex) < listItems.size());
            setActiveSelectionEndIndex(endIndex);

#if PLATFORM(COCOA)
            m_allowsNonContiguousSelection = m_multiple && isSpatialNavigationEnabled(document().frame());
#else
            m_allowsNonContiguousSelection = m_multiple && (isSpatialNavigationEnabled(document().frame()) || keyboardEvent->ctrlKey());
#endif
            bool selectNewItem = keyboardEvent->shiftKey() || !m_allowsNonContiguousSelection;

            if (selectNewItem)
                m_activeSelectionState = true;
            // If the anchor is unitialized, or if we're going to deselect all
            // other options, then set the anchor index equal to the end index.
            bool deselectOthers = !m_multiple || (!keyboardEvent->shiftKey() && selectNewItem);
            if (m_activeSelectionAnchorIndex < 0 || deselectOthers) {
                if (deselectOthers)
                    deselectItemsWithoutValidation();
                setActiveSelectionAnchorIndex(m_activeSelectionEndIndex);
            }

            downcast<RenderListBox>(*renderer).scrollToRevealElementAtListIndex(endIndex);
            if (selectNewItem) {
                updateListBoxSelection(deselectOthers);
                listBoxOnChange();
            } else
                scrollToSelection();

            keyboardEvent->setDefaultHandled();
        }
    } else if (event.type() == eventNames.keypressEvent) {
        RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event);
        if (!keyboardEvent)
            return;

        int keyCode = keyboardEvent->keyCode();
        if (keyCode == '\r') {
            if (RefPtr form = this->form())
                form->submitImplicitly(*keyboardEvent, false);
            keyboardEvent->setDefaultHandled();
        } else if (m_multiple && keyCode == ' ' && m_allowsNonContiguousSelection) {
            // Use space to toggle selection change.
            m_activeSelectionState = !m_activeSelectionState;
            ASSERT(m_activeSelectionEndIndex >= 0);
            ASSERT(m_activeSelectionEndIndex < static_cast<int>(listItems.size()));
            ASSERT(is<HTMLOptionElement>(*listItems[m_activeSelectionEndIndex]));
            updateSelectedState(m_activeSelectionEndIndex, true /*multi*/, false /*shift*/);
            listBoxOnChange();
            keyboardEvent->setDefaultHandled();
        }
    }
}

void HTMLSelectElement::defaultEventHandler(Event& event)
{
#if !PLATFORM(IOS_FAMILY)
    bool rendererIsMenuList = false;
#endif
    {
        CheckedPtr renderer = this->renderer();
        if (!renderer)
            return;

#if !PLATFORM(IOS_FAMILY)
        rendererIsMenuList = renderer->isRenderMenuList();
#endif
    }

#if !PLATFORM(IOS_FAMILY)
    if (isDisabledFormControl()) {
        HTMLFormControlElement::defaultEventHandler(event);
        return;
    }

    if (rendererIsMenuList)
        menuListDefaultEventHandler(event);
    else 
        listBoxDefaultEventHandler(event);
#else
    menuListDefaultEventHandler(event);
#endif
    if (event.defaultHandled())
        return;

    if (event.type() == eventNames().keypressEvent) {
        if (RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event)) {
            if (!keyboardEvent->ctrlKey() && !keyboardEvent->altKey() && !keyboardEvent->metaKey() && u_isprint(keyboardEvent->charCode())) {
                typeAheadFind(*keyboardEvent);
                event.setDefaultHandled();
                return;
            }
        }
    }
    HTMLFormControlElement::defaultEventHandler(event);
}

int HTMLSelectElement::lastSelectedListIndex() const
{
    auto& items = listItems();
    for (size_t i = items.size(); i;) {
        RefPtr element = dynamicDowncast<HTMLOptionElement>(*items[--i]);
        if (element && element->selected())
            return i;
    }
    return -1;
}

int HTMLSelectElement::indexOfSelectedOption() const
{
    return optionToListIndex(selectedIndex());
}

int HTMLSelectElement::optionCount() const
{
    return listItems().size();
}

String HTMLSelectElement::optionAtIndex(int index) const
{
    RefPtr option = dynamicDowncast<HTMLOptionElement>(*listItems()[index]);
    if (!option || option->isDisabledFormControl())
        return { };
    return option->textIndentedToRespectGroupLabel();
}

void HTMLSelectElement::typeAheadFind(KeyboardEvent& event)
{
    int index = m_typeAhead.handleEvent(&event, TypeAhead::MatchPrefix | TypeAhead::CycleFirstChar);
    if (index < 0)
        return;
    selectOption(listToOptionIndex(index), DeselectOtherOptions | DispatchChangeEvent | UserDriven);
    if (!usesMenuList())
        listBoxOnChange();
}

void HTMLSelectElement::accessKeySetSelectedIndex(int index)
{    
    // First bring into focus the list box.
    if (!focused())
        accessKeyAction(false);
    
    // If this index is already selected, unselect. otherwise update the selected index.
    auto& items = listItems();
    int listIndex = optionToListIndex(index);
    if (listIndex >= 0) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*items[listIndex])) {
            if (option->selected())
                option->setSelectedState(false);
            else
                selectOption(index, DispatchChangeEvent | UserDriven);
        }
    }

    if (usesMenuList())
        dispatchChangeEventForMenuList();
    else
        listBoxOnChange();

    scrollToSelection();
}

unsigned HTMLSelectElement::length() const
{
    unsigned options = 0;

    auto& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        if (is<HTMLOptionElement>(*items[i]))
            ++options;
    }

    return options;
}

ExceptionOr<void> HTMLSelectElement::showPicker()
{
    RefPtr frame = document().frame();
    if (!frame)
        return { };

    if (!isMutable())
        return Exception { ExceptionCode::InvalidStateError, "Select showPicker() cannot be used on immutable controls."_s };

    // In cross-origin iframes it should throw a "SecurityError" DOMException. In same-origin iframes it should work fine.
    RefPtr localTopFrame = dynamicDowncast<LocalFrame>(frame->tree().top());
    if (!localTopFrame || !frame->document()->securityOrigin().isSameOriginAs(localTopFrame->document()->securityOrigin()))
        return Exception { ExceptionCode::SecurityError, "Select showPicker() called from cross-origin iframe."_s };

    RefPtr window = frame->window();
    if (!window || !window->hasTransientActivation())
        return Exception { ExceptionCode::NotAllowedError, "Select showPicker() requires a user gesture."_s };

#if !PLATFORM(IOS_FAMILY)
    if (WeakPtr renderMenuList = dynamicDowncast<RenderMenuList>(renderer()))
        renderMenuList->showPopup(); // showPopup() may run JS and cause the renderer to get destroyed.
#endif

    return { };
}

} // namespace
