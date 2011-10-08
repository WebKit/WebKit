/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "Attribute.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "EventNames.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "OptionGroupElement.h"
#include "Page.h"
#include "RenderListBox.h"
#include "RenderMenuList.h"
#include "ScriptEventListener.h"
#include "SpatialNavigation.h"
#include <wtf/unicode/Unicode.h>

using namespace std;
using namespace WTF::Unicode;

namespace WebCore {

using namespace HTMLNames;

// Upper limit agreed upon with representatives of Opera and Mozilla.
static const unsigned maxSelectItems = 10000;

// Configure platform-specific behavior when focused pop-up receives arrow/space/return keystroke.
// (PLATFORM(MAC) and PLATFORM(GTK) are always false in Chromium, hence the extra tests.)
#if PLATFORM(MAC) || (PLATFORM(CHROMIUM) && OS(DARWIN))
#define ARROW_KEYS_POP_MENU 1
#define SPACE_OR_RETURN_POP_MENU 0
#elif PLATFORM(GTK) || (PLATFORM(CHROMIUM) && OS(UNIX))
#define ARROW_KEYS_POP_MENU 0
#define SPACE_OR_RETURN_POP_MENU 1
#else
#define ARROW_KEYS_POP_MENU 0
#define SPACE_OR_RETURN_POP_MENU 0
#endif

static const DOMTimeStamp typeAheadTimeout = 1000;

enum SkipDirection {
    SkipBackwards = -1,
    SkipForwards = 1
};

HTMLSelectElement::HTMLSelectElement(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
    : HTMLFormControlElementWithState(tagName, document, form)
{
    ASSERT(hasTagName(selectTag));
}

PassRefPtr<HTMLSelectElement> HTMLSelectElement::create(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
{
    ASSERT(tagName.matches(selectTag));
    return adoptRef(new HTMLSelectElement(tagName, document, form));
}

const AtomicString& HTMLSelectElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, selectMultiple, ("select-multiple"));
    DEFINE_STATIC_LOCAL(const AtomicString, selectOne, ("select-one"));
    return m_data.multiple() ? selectMultiple : selectOne;
}

int HTMLSelectElement::selectedIndex() const
{
    return selectedIndex(m_data, this);
}

void HTMLSelectElement::deselectItems(HTMLOptionElement* excludeElement)
{
    deselectItems(m_data, this, excludeElement);
    setNeedsValidityCheck();
}

void HTMLSelectElement::setSelectedIndex(int optionIndex, bool deselect)
{
    setSelectedIndex(m_data, this, optionIndex, deselect, false, false);
}

void HTMLSelectElement::setSelectedIndexByUser(int optionIndex, bool deselect, bool fireOnChangeNow, bool allowMultipleSelection)
{
    // List box selects can fire onchange events through user interaction, such as
    // mousedown events. This allows that same behavior programmatically.
    if (!m_data.usesMenuList()) {
        updateSelectedState(m_data, this, optionIndex, allowMultipleSelection, false);
        setNeedsValidityCheck();
        if (fireOnChangeNow)
            listBoxOnChange();
        return;
    }

    // Bail out if this index is already the selected one, to avoid running unnecessary JavaScript that can mess up
    // autofill, when there is no actual change (see https://bugs.webkit.org/show_bug.cgi?id=35256 and rdar://7467917 ).
    // Perhaps this logic could be moved into SelectElement, but some callers of SelectElement::setSelectedIndex()
    // seem to expect it to fire its change event even when the index was already selected.
    if (optionIndex == selectedIndex())
        return;
    
    setSelectedIndex(m_data, this, optionIndex, deselect, fireOnChangeNow, true);
}

bool HTMLSelectElement::hasPlaceholderLabelOption() const
{
    // The select element has no placeholder label option if it has an attribute "multiple" specified or a display size of non-1.
    // 
    // The condition "size() > 1" is actually not compliant with the HTML5 spec as of Dec 3, 2010. "size() != 1" is correct.
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
    if (listIndex < 0)
        return false;
    HTMLOptionElement* option = static_cast<HTMLOptionElement*>(listItems()[listIndex]);
    return !listIndex && option->value().isEmpty();
}

bool HTMLSelectElement::valueMissing() const
{
    if (!isRequiredFormControl())
        return false;

    int firstSelectionIndex = selectedIndex();

    // If a non-placeholer label option is selected (firstSelectionIndex > 0), it's not value-missing.
    return firstSelectionIndex < 0 || (!firstSelectionIndex && hasPlaceholderLabelOption());
}

void HTMLSelectElement::listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow)
{
    if (!multiple())
        setSelectedIndexByUser(listToOptionIndex(listIndex), true, fireOnChangeNow);
    else {
        updateSelectedState(m_data, this, listIndex, allowMultiplySelections, shift);
        setNeedsValidityCheck();
        if (fireOnChangeNow)
            listBoxOnChange();
    }
}

int HTMLSelectElement::activeSelectionStartListIndex() const
{
    if (m_data.activeSelectionAnchorIndex() >= 0)
        return m_data.activeSelectionAnchorIndex();
    return optionToListIndex(selectedIndex());
}

int HTMLSelectElement::activeSelectionEndListIndex() const
{
    if (m_data.activeSelectionEndIndex() >= 0)
        return m_data.activeSelectionEndIndex();
    return lastSelectedListIndex(m_data, this);
}

void HTMLSelectElement::add(HTMLElement* element, HTMLElement* before, ExceptionCode& ec)
{
    RefPtr<HTMLElement> protectNewChild(element); // make sure the element is ref'd and deref'd so we don't leak it

    if (!element || !(element->hasLocalName(optionTag) || element->hasLocalName(hrTag)))
        return;

    insertBefore(element, before, ec);
    setNeedsValidityCheck();
}

void HTMLSelectElement::remove(int optionIndex)
{
    int listIndex = optionToListIndex(optionIndex);
    if (listIndex < 0)
        return;

    ExceptionCode ec;
    listItems()[listIndex]->remove(ec);
}

void HTMLSelectElement::remove(HTMLOptionElement* option)
{
    if (option->ownerSelectElement() != this)
        return;

    ExceptionCode ec;
    option->remove(ec);
}

String HTMLSelectElement::value() const
{
    const Vector<Element*>& items = listItems();
    for (unsigned i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag) && static_cast<HTMLOptionElement*>(items[i])->selected())
            return static_cast<HTMLOptionElement*>(items[i])->value();
    }
    return "";
}

void HTMLSelectElement::setValue(const String &value)
{
    if (value.isNull())
        return;
    // find the option with value() matching the given parameter
    // and make it the current selection.
    const Vector<Element*>& items = listItems();
    unsigned optionIndex = 0;
    for (unsigned i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            if (static_cast<HTMLOptionElement*>(items[i])->value() == value) {
                setSelectedIndex(optionIndex, true);
                return;
            }
            optionIndex++;
        }
    }
}

void HTMLSelectElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == sizeAttr) {
        int oldSize = m_data.size();
        // Set the attribute value to a number.
        // This is important since the style rules for this attribute can determine the appearance property.
        int size = attr->value().toInt();
        String attrSize = String::number(size);
        if (attrSize != attr->value())
            attr->setValue(attrSize);
        size = max(size, 1);

        // Ensure that we've determined selectedness of the items at least once prior to changing the size.
        if (oldSize != size)
            recalcListItemsIfNeeded();

        m_data.setSize(size);
        setNeedsValidityCheck();
        if (m_data.size() != oldSize && attached()) {
            reattach();
            setRecalcListItems();
        }
    } else if (attr->name() == multipleAttr)
        parseMultipleAttribute(attr);
    else if (attr->name() == accesskeyAttr) {
        // FIXME: ignore for the moment
    } else if (attr->name() == alignAttr) {
        // Don't map 'align' attribute.  This matches what Firefox, Opera and IE do.
        // See http://bugs.webkit.org/show_bug.cgi?id=12072
    } else if (attr->name() == onchangeAttr) {
        setAttributeEventListener(eventNames().changeEvent, createAttributeEventListener(this, attr));
    } else
        HTMLFormControlElementWithState::parseMappedAttribute(attr);
}

bool HTMLSelectElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (renderer())
        return isFocusable();
    return HTMLFormControlElementWithState::isKeyboardFocusable(event);
}

bool HTMLSelectElement::isMouseFocusable() const
{
    if (renderer())
        return isFocusable();
    return HTMLFormControlElementWithState::isMouseFocusable();
}

bool HTMLSelectElement::canSelectAll() const
{
    return !m_data.usesMenuList(); 
}

RenderObject* HTMLSelectElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    if (m_data.usesMenuList())
        return new (arena) RenderMenuList(this);
    return new (arena) RenderListBox(this);
}

int HTMLSelectElement::optionToListIndex(int optionIndex) const
{
    return optionToListIndex(m_data, this, optionIndex);
}

int HTMLSelectElement::listToOptionIndex(int listIndex) const
{
    return listToOptionIndex(m_data, this, listIndex);
}

PassRefPtr<HTMLOptionsCollection> HTMLSelectElement::options()
{
    return HTMLOptionsCollection::create(this);
}

void HTMLSelectElement::recalcListItems(bool updateSelectedStates) const
{
    recalcListItems(const_cast<SelectElementData&>(m_data), this, updateSelectedStates);
}

void HTMLSelectElement::recalcListItemsIfNeeded()
{
    if (m_data.shouldRecalcListItems())
        recalcListItems();
}

void HTMLSelectElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    setRecalcListItems();
    setNeedsValidityCheck();
    HTMLFormControlElementWithState::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    
    if (AXObjectCache::accessibilityEnabled() && renderer())
        renderer()->document()->axObjectCache()->childrenChanged(renderer());
}

void HTMLSelectElement::setRecalcListItems()
{
    setRecalcListItems(m_data, this);

    if (!inDocument())
        m_collectionInfo.reset();
}

void HTMLSelectElement::defaultEventHandler(Event* event)
{
    defaultEventHandler(m_data, this, event, form());
    if (event->defaultHandled())
        return;
    HTMLFormControlElementWithState::defaultEventHandler(event);
}

void HTMLSelectElement::setActiveSelectionAnchorIndex(int index)
{
    setActiveSelectionAnchorIndex(m_data, this, index);
}

void HTMLSelectElement::setActiveSelectionEndIndex(int index)
{
    setActiveSelectionEndIndex(m_data, index);
}

void HTMLSelectElement::updateListBoxSelection(bool deselectOtherOptions)
{
    updateListBoxSelection(m_data, this, deselectOtherOptions);
    setNeedsValidityCheck();
}

void HTMLSelectElement::menuListOnChange()
{
    menuListOnChange(m_data, this);
}

void HTMLSelectElement::listBoxOnChange()
{
    listBoxOnChange(m_data, this);
}

void HTMLSelectElement::saveLastSelection()
{
    saveLastSelection(m_data, this);
}

void HTMLSelectElement::accessKeyAction(bool sendToAnyElement)
{
    focus();
    dispatchSimulatedClick(0, sendToAnyElement);
}

void HTMLSelectElement::setMultiple(bool multiple)
{
    bool oldMultiple = this->multiple();
    int oldSelectedIndex = selectedIndex();
    setAttribute(multipleAttr, multiple ? "" : 0);

    // Restore selectedIndex after changing the multiple flag to preserve
    // selection as single-line and multi-line has different defaults.
    if (oldMultiple != this->multiple())
        setSelectedIndex(oldSelectedIndex);
}

void HTMLSelectElement::setSize(int size)
{
    setAttribute(sizeAttr, String::number(size));
}

Node* HTMLSelectElement::namedItem(const AtomicString& name)
{
    return options()->namedItem(name);
}

Node* HTMLSelectElement::item(unsigned index)
{
    return options()->item(index);
}

void HTMLSelectElement::setOption(unsigned index, HTMLOptionElement* option, ExceptionCode& ec)
{
    ec = 0;
    if (index > maxSelectItems - 1)
        index = maxSelectItems - 1;
    int diff = index  - length();
    HTMLElement* before = 0;
    // out of array bounds ? first insert empty dummies
    if (diff > 0) {
        setLength(index, ec);
        // replace an existing entry ?
    } else if (diff < 0) {
        before = toHTMLElement(options()->item(index+1));
        remove(index);
    }
    // finally add the new element
    if (!ec) {
        add(option, before, ec);
        if (diff >= 0 && option->selected())
            setSelectedIndex(index, !m_data.multiple());
    }
}

void HTMLSelectElement::setLength(unsigned newLen, ExceptionCode& ec)
{
    ec = 0;
    if (newLen > maxSelectItems)
        newLen = maxSelectItems;
    int diff = length() - newLen;

    if (diff < 0) { // add dummy elements
        do {
            RefPtr<Element> option = document()->createElement(optionTag, false);
            ASSERT(option);
            add(toHTMLElement(option.get()), 0, ec);
            if (ec)
                break;
        } while (++diff);
    } else {
        const Vector<Element*>& items = listItems();

        // Removing children fires mutation events, which might mutate the DOM further, so we first copy out a list
        // of elements that we intend to remove then attempt to remove them one at a time.
        Vector<RefPtr<Element> > itemsToRemove;
        size_t optionIndex = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            Element* item = items[i];
            if (item->hasLocalName(optionTag) && optionIndex++ >= newLen) {
                ASSERT(item->parentNode());
                itemsToRemove.append(item);
            }
        }

        for (size_t i = 0; i < itemsToRemove.size(); ++i) {
            Element* item = itemsToRemove[i].get();
            if (item->parentNode()) {
                item->parentNode()->removeChild(item, ec);
            }
        }
    }
    setNeedsValidityCheck();
}

void HTMLSelectElement::scrollToSelection()
{
    scrollToSelection(m_data, this);
}

bool HTMLSelectElement::isRequiredFormControl() const
{
    return required();
}

// Returns the 1st valid item |skip| items from |listIndex| in direction |direction| if there is one.
// Otherwise, it returns the valid item closest to that boundary which is past |listIndex| if there is one.
// Otherwise, it returns |listIndex|.
// Valid means that it is enabled and an option element.
static int nextValidIndex(const Vector<Element*>& listItems, int listIndex, SkipDirection direction, int skip)
{
    ASSERT(direction == -1 || direction == 1);
    int lastGoodIndex = listIndex;
    int size = listItems.size();
    for (listIndex += direction; listIndex >= 0 && listIndex < size; listIndex += direction) {
        --skip;
        if (!listItems[listIndex]->disabled() && isOptionElement(listItems[listIndex])) {
            lastGoodIndex = listIndex;
            if (skip <= 0)
                break;
        }
    }
    return lastGoodIndex;
}

static int nextSelectableListIndex(SelectElementData& data, Element* element, int startIndex)
{
    return nextValidIndex(data.listItems(element), startIndex, SkipForwards, 1);
}

static int previousSelectableListIndex(SelectElementData& data, Element* element, int startIndex)
{
    if (startIndex == -1)
        startIndex = data.listItems(element).size();
    return nextValidIndex(data.listItems(element), startIndex, SkipBackwards, 1);
}

static int firstSelectableListIndex(SelectElementData& data, Element* element)
{
    const Vector<Element*>& items = data.listItems(element);
    int index = nextValidIndex(items, items.size(), SkipBackwards, INT_MAX);
    if (static_cast<unsigned>(index) == items.size())
        return -1;
    return index;
}

static int lastSelectableListIndex(SelectElementData& data, Element* element)
{
    return nextValidIndex(data.listItems(element), -1, SkipForwards, INT_MAX);
}

// Returns the index of the next valid item one page away from |startIndex| in direction |direction|.
static int nextSelectableListIndexPageAway(SelectElementData& data, Element* element, int startIndex, SkipDirection direction)
{
    const Vector<Element*>& items = data.listItems(element);
    // Can't use data->size() because renderer forces a minimum size.
    int pageSize = 0;
    if (element->renderer()->isListBox())
        pageSize = toRenderListBox(element->renderer())->size() - 1; // -1 so we still show context

    // One page away, but not outside valid bounds.
    // If there is a valid option item one page away, the index is chosen.
    // If there is no exact one page away valid option, returns startIndex or the most far index.
    int edgeIndex = (direction == SkipForwards) ? 0 : (items.size() - 1);
    int skipAmount = pageSize + ((direction == SkipForwards) ? startIndex : (edgeIndex - startIndex));
    return nextValidIndex(items, edgeIndex, direction, skipAmount);
}

void HTMLSelectElement::selectAll()
{
    ASSERT(!m_data.usesMenuList());
    if (!renderer() || !m_data.multiple())
        return;

    // Save the selection so it can be compared to the new selectAll selection
    // when dispatching change events
    saveLastSelection(m_data, this);

    m_data.setActiveSelectionState(true);
    setActiveSelectionAnchorIndex(m_data, this, nextSelectableListIndex(m_data, this, -1));
    setActiveSelectionEndIndex(m_data, previousSelectableListIndex(m_data, this, -1));

    updateListBoxSelection(m_data, this, false);
    listBoxOnChange(m_data, this);
    setNeedsValidityCheck();
}

void HTMLSelectElement::saveLastSelection(SelectElementData& data, Element* element)
{
    if (data.usesMenuList()) {
        data.setLastOnChangeIndex(selectedIndex(data, element));
        return;
    }

    Vector<bool>& lastOnChangeSelection = data.lastOnChangeSelection(); 
    lastOnChangeSelection.clear();

    const Vector<Element*>& items = data.listItems(element);
    for (unsigned i = 0; i < items.size(); ++i) {
        OptionElement* optionElement = toOptionElement(items[i]);
        lastOnChangeSelection.append(optionElement && optionElement->selected());
    }
}

void HTMLSelectElement::setActiveSelectionAnchorIndex(SelectElementData& data, Element* element, int index)
{
    data.setActiveSelectionAnchorIndex(index);

    // Cache the selection state so we can restore the old selection as the new
    // selection pivots around this anchor index
    Vector<bool>& cachedStateForActiveSelection = data.cachedStateForActiveSelection(); 
    cachedStateForActiveSelection.clear();

    const Vector<Element*>& items = data.listItems(element);
    for (unsigned i = 0; i < items.size(); ++i) {
        OptionElement* optionElement = toOptionElement(items[i]);
        cachedStateForActiveSelection.append(optionElement && optionElement->selected());
    }
}

void HTMLSelectElement::setActiveSelectionEndIndex(SelectElementData& data, int index)
{
    data.setActiveSelectionEndIndex(index);
}

void HTMLSelectElement::updateListBoxSelection(SelectElementData& data, Element* element, bool deselectOtherOptions)
{
    ASSERT(element->renderer() && (element->renderer()->isListBox() || data.multiple()));
    ASSERT(!data.listItems(element).size() || data.activeSelectionAnchorIndex() >= 0);

    unsigned start = min(data.activeSelectionAnchorIndex(), data.activeSelectionEndIndex());
    unsigned end = max(data.activeSelectionAnchorIndex(), data.activeSelectionEndIndex());
    Vector<bool>& cachedStateForActiveSelection = data.cachedStateForActiveSelection();

    const Vector<Element*>& items = data.listItems(element);
    for (unsigned i = 0; i < items.size(); ++i) {
        OptionElement* optionElement = toOptionElement(items[i]);
        if (!optionElement || items[i]->disabled())
            continue;

        if (i >= start && i <= end)
            optionElement->setSelectedState(data.activeSelectionState());
        else if (deselectOtherOptions || i >= cachedStateForActiveSelection.size())
            optionElement->setSelectedState(false);
        else
            optionElement->setSelectedState(cachedStateForActiveSelection[i]);
    }

    toSelectElement(element)->updateValidity();
    scrollToSelection(data, element);
}

void HTMLSelectElement::listBoxOnChange(SelectElementData& data, Element* element)
{
    ASSERT(!data.usesMenuList() || data.multiple());

    Vector<bool>& lastOnChangeSelection = data.lastOnChangeSelection(); 
    const Vector<Element*>& items = data.listItems(element);

    // If the cached selection list is empty, or the size has changed, then fire
    // dispatchFormControlChangeEvent, and return early.
    if (lastOnChangeSelection.isEmpty() || lastOnChangeSelection.size() != items.size()) {
        element->dispatchFormControlChangeEvent();
        return;
    }

    // Update lastOnChangeSelection and fire dispatchFormControlChangeEvent
    bool fireOnChange = false;
    for (unsigned i = 0; i < items.size(); ++i) {
        OptionElement* optionElement = toOptionElement(items[i]);
        bool selected = optionElement && optionElement->selected();
        if (selected != lastOnChangeSelection[i])
            fireOnChange = true;
        lastOnChangeSelection[i] = selected;
    }

    if (fireOnChange)
        element->dispatchFormControlChangeEvent();
}

void HTMLSelectElement::menuListOnChange(SelectElementData& data, Element* element)
{
    ASSERT(data.usesMenuList());

    int selected = selectedIndex(data, element);
    if (data.lastOnChangeIndex() != selected && data.userDrivenChange()) {
        data.setLastOnChangeIndex(selected);
        data.setUserDrivenChange(false);
        element->dispatchFormControlChangeEvent();
    }
}

void HTMLSelectElement::scrollToSelection(SelectElementData& data, Element* element)
{
    if (data.usesMenuList())
        return;

    if (RenderObject* renderer = element->renderer())
        toRenderListBox(renderer)->selectionChanged();
}

void HTMLSelectElement::setOptionsChangedOnRenderer(SelectElementData& data, Element* element)
{
    if (RenderObject* renderer = element->renderer()) {
        if (data.usesMenuList())
            toRenderMenuList(renderer)->setOptionsChanged(true);
        else
            toRenderListBox(renderer)->setOptionsChanged(true);
    }
}

void HTMLSelectElement::setRecalcListItems(SelectElementData& data, Element* element)
{
    data.setShouldRecalcListItems(true);
    // Manual selection anchor is reset when manipulating the select programmatically.
    data.setActiveSelectionAnchorIndex(-1);
    setOptionsChangedOnRenderer(data, element);
    element->setNeedsStyleRecalc();
}

void HTMLSelectElement::recalcListItems(SelectElementData& data, const Element* element, bool updateSelectedStates)
{
    Vector<Element*>& listItems = data.rawListItems();
    listItems.clear();

    data.setShouldRecalcListItems(false);

    OptionElement* foundSelected = 0;
    for (Node* currentNode = element->firstChild(); currentNode;) {
        if (!currentNode->isElementNode()) {
            currentNode = currentNode->traverseNextSibling(element);
            continue;
        }

        Element* current = static_cast<Element*>(currentNode);

        // optgroup tags may not nest. However, both FireFox and IE will
        // flatten the tree automatically, so we follow suit.
        // (http://www.w3.org/TR/html401/interact/forms.html#h-17.6)
        if (isOptionGroupElement(current)) {
            listItems.append(current);
            if (current->firstChild()) {
                currentNode = current->firstChild();
                continue;
            }
        }

        if (OptionElement* optionElement = toOptionElement(current)) {
            listItems.append(current);

            if (updateSelectedStates && !data.multiple()) {
                if (!foundSelected && (data.size() <= 1 || optionElement->selected())) {
                    foundSelected = optionElement;
                    foundSelected->setSelectedState(true);
                } else if (foundSelected && optionElement->selected()) {
                    foundSelected->setSelectedState(false);
                    foundSelected = optionElement;
                }
            }
        }

        if (current->hasTagName(HTMLNames::hrTag))
            listItems.append(current);

        // In conforming HTML code, only <optgroup> and <option> will be found
        // within a <select>. We call traverseNextSibling so that we only step
        // into those tags that we choose to. For web-compat, we should cope
        // with the case where odd tags like a <div> have been added but we
        // handle this because such tags have already been removed from the
        // <select>'s subtree at this point.
        currentNode = currentNode->traverseNextSibling(element);
    }
}

int HTMLSelectElement::selectedIndex(const SelectElementData& data, const Element* element)
{
    unsigned index = 0;

    // return the number of the first option selected
    const Vector<Element*>& items = data.listItems(element);
    for (size_t i = 0; i < items.size(); ++i) {
        if (OptionElement* optionElement = toOptionElement(items[i])) {
            if (optionElement->selected())
                return index;
            ++index;
        }
    }

    return -1;
}

void HTMLSelectElement::setSelectedIndex(SelectElementData& data, Element* element, int optionIndex, bool deselect, bool fireOnChangeNow, bool userDrivenChange)
{
    if (optionIndex == -1 && !deselect && !data.multiple())
        optionIndex = nextSelectableListIndex(data, element, -1);
    if (!data.multiple())
        deselect = true;

    const Vector<Element*>& items = data.listItems(element);
    int listIndex = optionToListIndex(data, element, optionIndex);

    Element* excludeElement = 0;
    if (OptionElement* optionElement = (listIndex >= 0 ? toOptionElement(items[listIndex]) : 0)) {
        excludeElement = items[listIndex];
        if (data.activeSelectionAnchorIndex() < 0 || deselect)
            setActiveSelectionAnchorIndex(data, element, listIndex);
        if (data.activeSelectionEndIndex() < 0 || deselect)
            setActiveSelectionEndIndex(data, listIndex);
        optionElement->setSelectedState(true);
    }

    if (deselect)
        deselectItems(data, element, excludeElement);

    // For the menu list case, this is what makes the selected element appear.
    if (RenderObject* renderer = element->renderer())
        renderer->updateFromElement();

    scrollToSelection(data, element);

    // This only gets called with fireOnChangeNow for menu lists. 
    if (data.usesMenuList()) {
        data.setUserDrivenChange(userDrivenChange);
        if (fireOnChangeNow)
            menuListOnChange(data, element);
        RenderObject* renderer = element->renderer();
        if (renderer) {
            if (data.usesMenuList())
                toRenderMenuList(renderer)->didSetSelectedIndex(listIndex);
            else if (renderer->isListBox())
                toRenderListBox(renderer)->selectionChanged();
        }
    }

    toSelectElement(element)->updateValidity();
    if (Frame* frame = element->document()->frame())
        frame->page()->chrome()->client()->formStateDidChange(element);
}

int HTMLSelectElement::optionToListIndex(const SelectElementData& data, const Element* element, int optionIndex)
{
    const Vector<Element*>& items = data.listItems(element);
    int listSize = (int) items.size();
    if (optionIndex < 0 || optionIndex >= listSize)
        return -1;

    int optionIndex2 = -1;
    for (int listIndex = 0; listIndex < listSize; ++listIndex) {
        if (isOptionElement(items[listIndex])) {
            ++optionIndex2;
            if (optionIndex2 == optionIndex)
                return listIndex;
        }
    }

    return -1;
}

int HTMLSelectElement::listToOptionIndex(const SelectElementData& data, const Element* element, int listIndex)
{
    const Vector<Element*>& items = data.listItems(element);
    if (listIndex < 0 || listIndex >= int(items.size()) || !isOptionElement(items[listIndex]))
        return -1;

    // Actual index of option not counting OPTGROUP entries that may be in list.
    int optionIndex = 0;
    for (int i = 0; i < listIndex; ++i) {
        if (isOptionElement(items[i]))
            ++optionIndex;
    }

    return optionIndex;
}

void HTMLSelectElement::dispatchFocusEvent(PassRefPtr<Node> oldFocusedNode)
{
    // Save the selection so it can be compared to the new selection when
    // dispatching change events during blur event dispatchal
    if (m_data.usesMenuList())
        saveLastSelection(m_data, this);
    HTMLFormControlElementWithState::dispatchFocusEvent(oldFocusedNode);
}

void HTMLSelectElement::dispatchBlurEvent(PassRefPtr<Node> newFocusedNode)
{
    // We only need to fire change events here for menu lists, because we fire
    // change events for list boxes whenever the selection change is actually made.
    // This matches other browsers' behavior.
    if (m_data.usesMenuList())
        menuListOnChange(m_data, this);
    HTMLFormControlElementWithState::dispatchBlurEvent(newFocusedNode);
}

void HTMLSelectElement::deselectItems(SelectElementData& data, Element* element, Element* excludeElement)
{
    const Vector<Element*>& items = data.listItems(element);
    for (unsigned i = 0; i < items.size(); ++i) {
        if (items[i] == excludeElement)
            continue;

        if (OptionElement* optionElement = toOptionElement(items[i]))
            optionElement->setSelectedState(false);
    }
}

bool HTMLSelectElement::saveFormControlState(String& value) const
{
    const Vector<Element*>& items = m_data.listItems(this);
    int length = items.size();

    // FIXME: Change this code to use the new StringImpl::createUninitialized code path.
    Vector<char, 1024> characters(length);
    for (int i = 0; i < length; ++i) {
        OptionElement* optionElement = toOptionElement(items[i]);
        bool selected = optionElement && optionElement->selected();
        characters[i] = selected ? 'X' : '.';
    }

    value = String(characters.data(), length);
    return true;
}

void HTMLSelectElement::restoreFormControlState(const String& state)
{
    recalcListItems(m_data, this);

    const Vector<Element*>& items = m_data.listItems(this);
    int length = items.size();

    for (int i = 0; i < length; ++i) {
        if (OptionElement* optionElement = toOptionElement(items[i]))
            optionElement->setSelectedState(state[i] == 'X');
    }

    setOptionsChangedOnRenderer(m_data, this);
    setNeedsValidityCheck();
}

void HTMLSelectElement::parseMultipleAttribute(const Attribute* attribute)
{
    bool oldUsesMenuList = m_data.usesMenuList();
    m_data.setMultiple(!attribute->isNull());
    updateValidity();
    if (oldUsesMenuList != m_data.usesMenuList())
        reattachIfAttached();
}

bool HTMLSelectElement::appendFormData(FormDataList& list, bool)
{
    const AtomicString& name = formControlName();
    if (name.isEmpty())
        return false;

    bool successful = false;
    const Vector<Element*>& items = m_data.listItems(this);

    for (unsigned i = 0; i < items.size(); ++i) {
        OptionElement* optionElement = toOptionElement(items[i]);
        if (optionElement && optionElement->selected() && !optionElement->disabled()) {
            list.appendData(name, optionElement->value());
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
    OptionElement* firstOption = 0;
    OptionElement* selectedOption = 0;

    const Vector<Element*>& items = m_data.listItems(this);
    for (unsigned i = 0; i < items.size(); ++i) {
        OptionElement* optionElement = toOptionElement(items[i]);
        if (!optionElement)
            continue;

        if (items[i]->fastHasAttribute(selectedAttr)) {
            if (selectedOption && !m_data.multiple())
                selectedOption->setSelectedState(false);
            optionElement->setSelectedState(true);
            selectedOption = optionElement;
        } else
            optionElement->setSelectedState(false);

        if (!firstOption)
            firstOption = optionElement;
    }

    if (!selectedOption && firstOption && !m_data.multiple() && m_data.size() <= 1)
        firstOption->setSelectedState(true);

    setOptionsChangedOnRenderer(m_data, this);
    setNeedsStyleRecalc();
    setNeedsValidityCheck();
}

#if !PLATFORM(WIN) || OS(WINCE)
bool HTMLSelectElement::platformHandleKeydownEvent(SelectElementData& data, Element* element, KeyboardEvent* event)
{
#if ARROW_KEYS_POP_MENU
    if (!isSpatialNavigationEnabled(element->document()->frame())) {
        if (event->keyIdentifier() == "Down" || event->keyIdentifier() == "Up") {
            element->focus();

            // Calling focus() may cause us to lose our renderer. Return true so
            // that our caller doesn't process the event further, but don't set
            // the event as handled.
            if (!element->renderer())
                return true;

            // Save the selection so it can be compared to the new selection
            // when dispatching change events during setSelectedIndex, which
            // gets called from RenderMenuList::valueChanged, which gets called
            // after the user makes a selection from the menu.
            saveLastSelection(data, element);
            if (RenderMenuList* menuList = toRenderMenuList(element->renderer()))
                menuList->showPopup();
            event->setDefaultHandled();
        }
        return true;
    }
#endif
    return false;
}
#endif

void HTMLSelectElement::menuListDefaultEventHandler(SelectElementData& data, Element* element, Event* event, HTMLFormElement* htmlForm)
{
    if (event->type() == eventNames().keydownEvent) {
        if (!element->renderer() || !event->isKeyboardEvent())
            return;

        if (platformHandleKeydownEvent(data, element, static_cast<KeyboardEvent*>(event)))
            return;

        // When using spatial navigation, we want to be able to navigate away
        // from the select element when the user hits any of the arrow keys,
        // instead of changing the selection.
        if (isSpatialNavigationEnabled(element->document()->frame())) {
            if (!data.activeSelectionState())
                return;
        }

        const String& keyIdentifier = static_cast<KeyboardEvent*>(event)->keyIdentifier();
        bool handled = true;

        UNUSED_PARAM(htmlForm);
        const Vector<Element*>& listItems = data.listItems(element);

        int listIndex = optionToListIndex(data, element, selectedIndex(data, element));

        if (keyIdentifier == "Down" || keyIdentifier == "Right")
            listIndex = nextValidIndex(listItems, listIndex, SkipForwards, 1);
        else if (keyIdentifier == "Up" || keyIdentifier == "Left")
            listIndex = nextValidIndex(listItems, listIndex, SkipBackwards, 1);
        else if (keyIdentifier == "PageDown")
            listIndex = nextValidIndex(listItems, listIndex, SkipForwards, 3);
        else if (keyIdentifier == "PageUp")
            listIndex = nextValidIndex(listItems, listIndex, SkipBackwards, 3);
        else if (keyIdentifier == "Home")
            listIndex = nextValidIndex(listItems, -1, SkipForwards, 1);
        else if (keyIdentifier == "End")
            listIndex = nextValidIndex(listItems, listItems.size(), SkipBackwards, 1);
        else
            handled = false;

        if (handled && listIndex >= 0 && static_cast<unsigned>(listIndex) < listItems.size())
            setSelectedIndex(data, element, listToOptionIndex(data, element, listIndex));

        if (handled)
            event->setDefaultHandled();
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (event->type() == eventNames().keypressEvent) {
        if (!element->renderer() || !event->isKeyboardEvent())
            return;

        int keyCode = static_cast<KeyboardEvent*>(event)->keyCode();
        bool handled = false;

        if (keyCode == ' ' && isSpatialNavigationEnabled(element->document()->frame())) {
            // Use space to toggle arrow key handling for selection change or spatial navigation.
            data.setActiveSelectionState(!data.activeSelectionState());
            event->setDefaultHandled();
            return;
        }

#if SPACE_OR_RETURN_POP_MENU
        if (keyCode == ' ' || keyCode == '\r') {
            element->focus();

            // Calling focus() may cause us to lose our renderer, in which case
            // do not want to handle the event.
            if (!element->renderer())
                return;

            // Save the selection so it can be compared to the new selection
            // when dispatching change events during setSelectedIndex, which
            // gets called from RenderMenuList::valueChanged, which gets called
            // after the user makes a selection from the menu.
            saveLastSelection(data, element);
            if (RenderMenuList* menuList = toRenderMenuList(element->renderer()))
                menuList->showPopup();
            handled = true;
        }
#elif ARROW_KEYS_POP_MENU
        if (keyCode == ' ') {
            element->focus();

            // Calling focus() may cause us to lose our renderer, in which case
            // do not want to handle the event.
            if (!element->renderer())
                return;

            // Save the selection so it can be compared to the new selection
            // when dispatching change events during setSelectedIndex, which
            // gets called from RenderMenuList::valueChanged, which gets called
            // after the user makes a selection from the menu.
            saveLastSelection(data, element);
            if (RenderMenuList* menuList = toRenderMenuList(element->renderer()))
                menuList->showPopup();
            handled = true;
        } else if (keyCode == '\r') {
            if (htmlForm)
                htmlForm->submitImplicitly(event, false);
            menuListOnChange(data, element);
            handled = true;
        }
#else
        int listIndex = optionToListIndex(data, element, selectedIndex(data, element));
        if (keyCode == '\r') {
            // listIndex should already be selected, but this will fire the onchange handler.
            setSelectedIndex(data, element, listToOptionIndex(data, element, listIndex), true, true);
            handled = true;
        }
#endif
        if (handled)
            event->setDefaultHandled();
    }

    if (event->type() == eventNames().mousedownEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton) {
        element->focus();
        if (element->renderer() && element->renderer()->isMenuList()) {
            if (RenderMenuList* menuList = toRenderMenuList(element->renderer())) {
                if (menuList->popupIsVisible())
                    menuList->hidePopup();
                else {
                    // Save the selection so it can be compared to the new
                    // selection when we call onChange during setSelectedIndex,
                    // which gets called from RenderMenuList::valueChanged,
                    // which gets called after the user makes a selection from
                    // the menu.
                    saveLastSelection(data, element);
                    menuList->showPopup();
                }
            }
        }
        event->setDefaultHandled();
    }
}

void HTMLSelectElement::updateSelectedState(SelectElementData& data, Element* element, int listIndex, bool multi, bool shift)
{
    ASSERT(listIndex >= 0);

    // Save the selection so it can be compared to the new selection when
    // dispatching change events during mouseup, or after autoscroll finishes.
    saveLastSelection(data, element);

    data.setActiveSelectionState(true);

    bool shiftSelect = data.multiple() && shift;
    bool multiSelect = data.multiple() && multi && !shift;

    Element* clickedElement = data.listItems(element)[listIndex];
    OptionElement* option = toOptionElement(clickedElement);
    if (option) {
        // Keep track of whether an active selection (like during drag
        // selection), should select or deselect
        if (option->selected() && multi)
            data.setActiveSelectionState(false);

        if (!data.activeSelectionState())
            option->setSelectedState(false);
    }

    // If we're not in any special multiple selection mode, then deselect all
    // other items, excluding the clicked option. If no option was clicked, then
    // this will deselect all items in the list.
    if (!shiftSelect && !multiSelect)
        deselectItems(data, element, clickedElement);

    // If the anchor hasn't been set, and we're doing a single selection or a
    // shift selection, then initialize the anchor to the first selected index.
    if (data.activeSelectionAnchorIndex() < 0 && !multiSelect)
        setActiveSelectionAnchorIndex(data, element, selectedIndex(data, element));

    // Set the selection state of the clicked option
    if (option && !clickedElement->disabled())
        option->setSelectedState(true);

    // If there was no selectedIndex() for the previous initialization, or If
    // we're doing a single selection, or a multiple selection (using cmd or
    // ctrl), then initialize the anchor index to the listIndex that just got
    // clicked.
    if (data.activeSelectionAnchorIndex() < 0 || !shiftSelect)
        setActiveSelectionAnchorIndex(data, element, listIndex);

    setActiveSelectionEndIndex(data, listIndex);
    updateListBoxSelection(data, element, !multiSelect);
}

void HTMLSelectElement::listBoxDefaultEventHandler(SelectElementData& data, Element* element, Event* event, HTMLFormElement* htmlForm)
{
    const Vector<Element*>& listItems = data.listItems(element);

    if (event->type() == eventNames().mousedownEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton) {
        element->focus();

        // Calling focus() may cause us to lose our renderer, in which case do not want to handle the event.
        if (!element->renderer())
            return;

        // Convert to coords relative to the list box if needed.
        MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
        IntPoint localOffset = roundedIntPoint(element->renderer()->absoluteToLocal(mouseEvent->absoluteLocation(), false, true));
        int listIndex = toRenderListBox(element->renderer())->listIndexAtOffset(toSize(localOffset));
        if (listIndex >= 0) {
#if PLATFORM(MAC) || (PLATFORM(CHROMIUM) && OS(DARWIN))
            updateSelectedState(data, element, listIndex, mouseEvent->metaKey(), mouseEvent->shiftKey());
#else
            updateSelectedState(data, element, listIndex, mouseEvent->ctrlKey(), mouseEvent->shiftKey());
#endif
            if (Frame* frame = element->document()->frame())
                frame->eventHandler()->setMouseDownMayStartAutoscroll();

            event->setDefaultHandled();
        }
    } else if (event->type() == eventNames().mouseupEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton && element->document()->frame()->eventHandler()->autoscrollRenderer() != element->renderer()) {
        // This makes sure we fire dispatchFormControlChangeEvent for a single
        // click. For drag selection, onChange will fire when the autoscroll
        // timer stops.
        listBoxOnChange(data, element);
    } else if (event->type() == eventNames().keydownEvent) {
        if (!event->isKeyboardEvent())
            return;
        const String& keyIdentifier = static_cast<KeyboardEvent*>(event)->keyIdentifier();

        bool handled = false;
        int endIndex = 0;
        if (data.activeSelectionEndIndex() < 0) {
            // Initialize the end index
            if (keyIdentifier == "Down" || keyIdentifier == "PageDown") {
                int startIndex = lastSelectedListIndex(data, element);
                handled = true;
                if (keyIdentifier == "Down")
                    endIndex = nextSelectableListIndex(data, element, startIndex);
                else
                    endIndex = nextSelectableListIndexPageAway(data, element, startIndex, SkipForwards);
            } else if (keyIdentifier == "Up" || keyIdentifier == "PageUp") {
                int startIndex = optionToListIndex(data, element, selectedIndex(data, element));
                handled = true;
                if (keyIdentifier == "Up")
                    endIndex = previousSelectableListIndex(data, element, startIndex);
                else
                    endIndex = nextSelectableListIndexPageAway(data, element, startIndex, SkipBackwards);
            }
        } else {
            // Set the end index based on the current end index
            if (keyIdentifier == "Down") {
                endIndex = nextSelectableListIndex(data, element, data.activeSelectionEndIndex());
                handled = true;
            } else if (keyIdentifier == "Up") {
                endIndex = previousSelectableListIndex(data, element, data.activeSelectionEndIndex());
                handled = true;
            } else if (keyIdentifier == "PageDown") {
                endIndex = nextSelectableListIndexPageAway(data, element, data.activeSelectionEndIndex(), SkipForwards);
                handled = true;
            } else if (keyIdentifier == "PageUp") {
                endIndex = nextSelectableListIndexPageAway(data, element, data.activeSelectionEndIndex(), SkipBackwards);
                handled = true;
            }
        }
        if (keyIdentifier == "Home") {
            endIndex = firstSelectableListIndex(data, element);
            handled = true;
        } else if (keyIdentifier == "End") {
            endIndex = lastSelectableListIndex(data, element);
            handled = true;
        }

        if (isSpatialNavigationEnabled(element->document()->frame()))
            // Check if the selection moves to the boundary.
            if (keyIdentifier == "Left" || keyIdentifier == "Right" || ((keyIdentifier == "Down" || keyIdentifier == "Up") && endIndex == data.activeSelectionEndIndex()))
                return;

        if (endIndex >= 0 && handled) {
            // Save the selection so it can be compared to the new selection
            // when dispatching change events immediately after making the new
            // selection.
            saveLastSelection(data, element);

            ASSERT_UNUSED(listItems, !listItems.size() || (endIndex >= 0 && static_cast<unsigned>(endIndex) < listItems.size()));
            setActiveSelectionEndIndex(data, endIndex);

            bool selectNewItem = !data.multiple() || static_cast<KeyboardEvent*>(event)->shiftKey() || !isSpatialNavigationEnabled(element->document()->frame());
            if (selectNewItem)
                data.setActiveSelectionState(true);
            // If the anchor is unitialized, or if we're going to deselect all
            // other options, then set the anchor index equal to the end index.
            bool deselectOthers = !data.multiple() || (!static_cast<KeyboardEvent*>(event)->shiftKey() && selectNewItem);
            if (data.activeSelectionAnchorIndex() < 0 || deselectOthers) {
                if (deselectOthers)
                    deselectItems(data, element);
                setActiveSelectionAnchorIndex(data, element, data.activeSelectionEndIndex());
            }

            toRenderListBox(element->renderer())->scrollToRevealElementAtListIndex(endIndex);
            if (selectNewItem) {
                updateListBoxSelection(data, element, deselectOthers);
                listBoxOnChange(data, element);
            } else
                scrollToSelection(data, element);

            event->setDefaultHandled();
        }
    } else if (event->type() == eventNames().keypressEvent) {
        if (!event->isKeyboardEvent())
            return;
        int keyCode = static_cast<KeyboardEvent*>(event)->keyCode();

        if (keyCode == '\r') {
            if (htmlForm)
                htmlForm->submitImplicitly(event, false);
            event->setDefaultHandled();
        } else if (data.multiple() && keyCode == ' ' && isSpatialNavigationEnabled(element->document()->frame())) {
            // Use space to toggle selection change.
            data.setActiveSelectionState(!data.activeSelectionState());
            updateSelectedState(data, element, listToOptionIndex(data, element, data.activeSelectionEndIndex()), true /*multi*/, false /*shift*/);
            listBoxOnChange(data, element);
            event->setDefaultHandled();
        }
    }
}

void HTMLSelectElement::defaultEventHandler(SelectElementData& data, Element* element, Event* event, HTMLFormElement* htmlForm)
{
    if (!element->renderer())
        return;

    if (data.usesMenuList())
        menuListDefaultEventHandler(data, element, event, htmlForm);
    else 
        listBoxDefaultEventHandler(data, element, event, htmlForm);

    if (event->defaultHandled())
        return;

    if (event->type() == eventNames().keypressEvent && event->isKeyboardEvent()) {
        KeyboardEvent* keyboardEvent = static_cast<KeyboardEvent*>(event);
        if (!keyboardEvent->ctrlKey() && !keyboardEvent->altKey() && !keyboardEvent->metaKey() && isPrintableChar(keyboardEvent->charCode())) {
            typeAheadFind(data, element, keyboardEvent);
            event->setDefaultHandled();
            return;
        }
    }
}

int HTMLSelectElement::lastSelectedListIndex(const SelectElementData& data, const Element* element)
{
    // FIXME: We should iterate the listItems in the reverse order.
    unsigned index = 0;
    bool found = false;
    const Vector<Element*>& items = data.listItems(element);
    for (size_t i = 0; i < items.size(); ++i) {
        if (OptionElement* optionElement = toOptionElement(items[i])) {
            if (optionElement->selected()) {
                index = i;
                found = true;
            }
        }
    }

    return found ? (int) index : -1;
}

static String stripLeadingWhiteSpace(const String& string)
{
    int length = string.length();

    int i;
    for (i = 0; i < length; ++i) {
        if (string[i] != noBreakSpace && (string[i] <= 0x7F ? !isASCIISpace(string[i]) : (direction(string[i]) != WhiteSpaceNeutral)))
            break;
    }

    return string.substring(i, length - i);
}

void HTMLSelectElement::typeAheadFind(SelectElementData& data, Element* element, KeyboardEvent* event)
{
    if (event->timeStamp() < data.lastCharTime())
        return;

    DOMTimeStamp delta = event->timeStamp() - data.lastCharTime();
    data.setLastCharTime(event->timeStamp());

    UChar c = event->charCode();

    String prefix;
    int searchStartOffset = 1;
    if (delta > typeAheadTimeout) {
        prefix = String(&c, 1);
        data.setTypedString(prefix);
        data.setRepeatingChar(c);
    } else {
        data.typedString().append(c);

        if (c == data.repeatingChar()) {
            // The user is likely trying to cycle through all the items starting
            // with this character, so just search on the character
            prefix = String(&c, 1);
        } else {
            data.setRepeatingChar(0);
            prefix = data.typedString();
            searchStartOffset = 0;
        }
    }

    const Vector<Element*>& items = data.listItems(element);
    int itemCount = items.size();
    if (itemCount < 1)
        return;

    int selected = selectedIndex(data, element);
    int index = (optionToListIndex(data, element, selected >= 0 ? selected : 0) + searchStartOffset) % itemCount;
    ASSERT(index >= 0);

    // Compute a case-folded copy of the prefix string before beginning the search for
    // a matching element. This code uses foldCase to work around the fact that
    // String::startWith does not fold non-ASCII characters. This code can be changed
    // to use startWith once that is fixed.
    String prefixWithCaseFolded(prefix.foldCase());
    for (int i = 0; i < itemCount; ++i, index = (index + 1) % itemCount) {
        OptionElement* optionElement = toOptionElement(items[index]);
        if (!optionElement || items[index]->disabled())
            continue;

        // Fold the option string and check if its prefix is equal to the folded prefix.
        String text = optionElement->textIndentedToRespectGroupLabel();
        if (stripLeadingWhiteSpace(text).foldCase().startsWith(prefixWithCaseFolded)) {
            setSelectedIndex(data, element, listToOptionIndex(data, element, index));
            if (!data.usesMenuList())
                listBoxOnChange(data, element);

            setOptionsChangedOnRenderer(data, element);
            element->setNeedsStyleRecalc();
            return;
        }
    }
}

void HTMLSelectElement::insertedIntoTree(bool deep)
{
    // When the element is created during document parsing, it won't have any
    // items yet - but for innerHTML and related methods, this method is called
    // after the whole subtree is constructed.
    recalcListItems(m_data, this, true);
    HTMLFormControlElementWithState::insertedIntoTree(deep);
}

void HTMLSelectElement::accessKeySetSelectedIndex(int index)
{    
    // First bring into focus the list box.
    if (!focused())
        accessKeyAction(false);
    
    // if this index is already selected, unselect. otherwise update the selected index
    const Vector<Element*>& items = m_data.listItems(this);
    int listIndex = optionToListIndex(m_data, this, index);
    if (OptionElement* optionElement = (listIndex >= 0 ? toOptionElement(items[listIndex]) : 0)) {
        if (optionElement->selected())
            optionElement->setSelectedState(false);
        else
            setSelectedIndex(m_data, this, index, false, true);
    }

    if (m_data.usesMenuList())
        menuListOnChange(m_data, this);
    else
        listBoxOnChange(m_data, this);

    scrollToSelection(m_data, this);
}

unsigned HTMLSelectElement::length() const
{
    unsigned options = 0;

    const Vector<Element*>& items = m_data.listItems(this);
    for (unsigned i = 0; i < items.size(); ++i) {
        if (isOptionElement(items[i]))
            ++options;
    }

    return options;
}

HTMLSelectElement* toSelectElement(Element* element)
{
    if (element->isHTMLElement() && element->hasTagName(HTMLNames::selectTag))
        return static_cast<HTMLSelectElement*>(element);
    // FIXME: toFooClass() function should not return 0.
    return 0;
}

} // namespace
