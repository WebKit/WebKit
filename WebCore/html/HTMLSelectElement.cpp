/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "HTMLSelectElement.h"

#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "CharacterNames.h"
#include "Document.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "RenderListBox.h"
#include "RenderMenuList.h"
#include <math.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#define ARROW_KEYS_POP_MENU 1
#else
#define ARROW_KEYS_POP_MENU 0
#endif

using namespace std;
using namespace WTF;
using namespace Unicode;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

static const DOMTimeStamp typeAheadTimeout = 1000;

HTMLSelectElement::HTMLSelectElement(Document* doc, HTMLFormElement* f)
    : HTMLFormControlElementWithState(selectTag, doc, f)
    , m_minwidth(0)
    , m_size(0)
    , m_multiple(false)
    , m_recalcListItems(false)
    , m_lastOnChangeIndex(-1)
    , m_activeSelectionAnchorIndex(-1)
    , m_activeSelectionEndIndex(-1)
    , m_activeSelectionState(false)
    , m_repeatingChar(0)
    , m_lastCharTime(0)
{
}

HTMLSelectElement::HTMLSelectElement(const QualifiedName& tagName, Document* doc, HTMLFormElement* f)
    : HTMLFormControlElementWithState(tagName, doc, f)
    , m_minwidth(0)
    , m_size(0)
    , m_multiple(false)
    , m_recalcListItems(false)
    , m_lastOnChangeIndex(-1)
    , m_activeSelectionAnchorIndex(-1)
    , m_activeSelectionEndIndex(-1)
    , m_activeSelectionState(false)
    , m_repeatingChar(0)
    , m_lastCharTime(0)
{
}

bool HTMLSelectElement::checkDTD(const Node* newChild)
{
    // Make sure to keep <optgroup> in sync with this.
    return newChild->isTextNode() || newChild->hasTagName(optionTag) || newChild->hasTagName(optgroupTag) || newChild->hasTagName(hrTag) ||
           newChild->hasTagName(scriptTag);
}

void HTMLSelectElement::recalcStyle( StyleChange ch )
{
    if (hasChangedChild() && renderer()) {
        if (usesMenuList())
            static_cast<RenderMenuList*>(renderer())->setOptionsChanged(true);
        else
            static_cast<RenderListBox*>(renderer())->setOptionsChanged(true);
    } else if (m_recalcListItems)
        recalcListItems();

    HTMLFormControlElementWithState::recalcStyle(ch);
}

const AtomicString& HTMLSelectElement::type() const
{
    static const AtomicString selectMultiple("select-multiple");
    static const AtomicString selectOne("select-one");
    return m_multiple ? selectMultiple : selectOne;
}

int HTMLSelectElement::selectedIndex() const
{
    // return the number of the first option selected
    unsigned index = 0;
    const Vector<HTMLElement*>& items = listItems();
    for (unsigned int i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            if (static_cast<HTMLOptionElement*>(items[i])->selected())
                return index;
            index++;
        }
    }
    return -1;
}

int HTMLSelectElement::lastSelectedListIndex() const
{
    // return the number of the last option selected
    unsigned index = 0;
    bool found = false;
    const Vector<HTMLElement*>& items = listItems();
    for (unsigned int i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            if (static_cast<HTMLOptionElement*>(items[i])->selected()) {
                index = i;
                found = true;
            }
        }
    }
    return found ? (int) index : -1;
}

void HTMLSelectElement::deselectItems(HTMLOptionElement* excludeElement)
{
    const Vector<HTMLElement*>& items = listItems();
    unsigned i;
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag) && (items[i] != excludeElement)) {
            HTMLOptionElement* element = static_cast<HTMLOptionElement*>(items[i]);
            element->setSelectedState(false);
        }
    }
}

void HTMLSelectElement::setSelectedIndex(int optionIndex, bool deselect, bool fireOnChange)
{
    const Vector<HTMLElement*>& items = listItems();
    int listIndex = optionToListIndex(optionIndex);
    HTMLOptionElement* element = 0;

    if (!multiple())
        deselect = true;

    if (listIndex >= 0) {
        if (m_activeSelectionAnchorIndex < 0 || deselect)
            setActiveSelectionAnchorIndex(listIndex);
        if (m_activeSelectionEndIndex < 0 || deselect)
            setActiveSelectionEndIndex(listIndex);
        element = static_cast<HTMLOptionElement*>(items[listIndex]);
        element->setSelectedState(true);
    }

    if (deselect)
        deselectItems(element);

    scrollToSelection();

    // This only gets called with fireOnChange for menu lists. 
    if (fireOnChange && usesMenuList())
        menuListOnChange();
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

unsigned HTMLSelectElement::length() const
{
    unsigned len = 0;
    const Vector<HTMLElement*>& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        if (items[i]->hasLocalName(optionTag))
            ++len;
    }
    return len;
}

void HTMLSelectElement::add(HTMLElement *element, HTMLElement *before, ExceptionCode& ec)
{
    RefPtr<HTMLElement> protectNewChild(element); // make sure the element is ref'd and deref'd so we don't leak it

    if (!element || !(element->hasLocalName(optionTag) || element->hasLocalName(hrTag)))
        return;

    ec = 0;
    insertBefore(element, before, ec);
    if (!ec)
        setRecalcListItems();
}

void HTMLSelectElement::remove(int index)
{
    ExceptionCode ec = 0;
    int listIndex = optionToListIndex(index);

    const Vector<HTMLElement*>& items = listItems();
    if (listIndex < 0 || index >= int(items.size()))
        return; // ### what should we do ? remove the last item?

    Element *item = items[listIndex];
    ASSERT(item->parentNode());
    item->parentNode()->removeChild(item, ec);
    if (!ec)
        setRecalcListItems();
}

String HTMLSelectElement::value()
{
    unsigned i;
    const Vector<HTMLElement*>& items = listItems();
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag) && static_cast<HTMLOptionElement*>(items[i])->selected())
            return static_cast<HTMLOptionElement*>(items[i])->value();
    }
    return String("");
}

void HTMLSelectElement::setValue(const String &value)
{
    if (value.isNull())
        return;
    // find the option with value() matching the given parameter
    // and make it the current selection.
    const Vector<HTMLElement*>& items = listItems();
    unsigned optionIndex = 0;
    for (unsigned i = 0; i < items.size(); i++)
        if (items[i]->hasLocalName(optionTag)) {
            if (static_cast<HTMLOptionElement*>(items[i])->value() == value) {
                setSelectedIndex(optionIndex, true);
                return;
            }
            optionIndex++;
        }
}

bool HTMLSelectElement::saveState(String& value) const
{
    const Vector<HTMLElement*>& items = listItems();
    int l = items.size();
    Vector<char, 1024> characters(l);
    for (int i = 0; i < l; ++i) {
        HTMLElement* e = items[i];
        bool selected = e->hasLocalName(optionTag) && static_cast<HTMLOptionElement*>(e)->selected();
        characters[i] = selected ? 'X' : '.';
    }
    value = String(characters.data(), l);
    return true;
}

void HTMLSelectElement::restoreState(const String& state)
{
    recalcListItems();
    
    const Vector<HTMLElement*>& items = listItems();
    int l = items.size();
    for (int i = 0; i < l; i++)
        if (items[i]->hasLocalName(optionTag))
            static_cast<HTMLOptionElement*>(items[i])->setSelectedState(state[i] == 'X');
            
    setChanged();
}

bool HTMLSelectElement::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode& ec)
{
    bool result = HTMLFormControlElementWithState::insertBefore(newChild, refChild, ec);
    if (result)
        setRecalcListItems();
    return result;
}

bool HTMLSelectElement::replaceChild(PassRefPtr<Node> newChild, Node *oldChild, ExceptionCode& ec)
{
    bool result = HTMLFormControlElementWithState::replaceChild(newChild, oldChild, ec);
    if (result)
        setRecalcListItems();
    return result;
}

bool HTMLSelectElement::removeChild(Node* oldChild, ExceptionCode& ec)
{
    bool result = HTMLFormControlElementWithState::removeChild(oldChild, ec);
    if (result)
        setRecalcListItems();
    return result;
}

bool HTMLSelectElement::appendChild(PassRefPtr<Node> newChild, ExceptionCode& ec)
{
    bool result = HTMLFormControlElementWithState::appendChild(newChild, ec);
    if (result)
        setRecalcListItems();
    return result;
}

bool HTMLSelectElement::removeChildren()
{
    bool result = HTMLFormControlElementWithState::removeChildren();
    if (result)
        setRecalcListItems();
    return result;
}

void HTMLSelectElement::parseMappedAttribute(MappedAttribute *attr)
{
    bool oldUsesMenuList = usesMenuList();
    if (attr->name() == sizeAttr) {
        int oldSize = m_size;
        // Set the attribute value to a number.
        // This is important since the style rules for this attribute can determine the appearance property.
        int size = attr->value().toInt();
        String attrSize = String::number(size);
        if (attrSize != attr->value())
            attr->setValue(attrSize);

        m_size = max(size, 1);
        if ((oldUsesMenuList != usesMenuList() || !oldUsesMenuList && m_size != oldSize) && attached()) {
            detach();
            attach();
            setRecalcListItems();
        }
    } else if (attr->name() == widthAttr) {
        m_minwidth = max(attr->value().toInt(), 0);
    } else if (attr->name() == multipleAttr) {
        m_multiple = (!attr->isNull());
        if (oldUsesMenuList != usesMenuList() && attached()) {
            detach();
            attach();
        }
    } else if (attr->name() == accesskeyAttr) {
        // FIXME: ignore for the moment
    } else if (attr->name() == alignAttr) {
        // Don't map 'align' attribute.  This matches what Firefox, Opera and IE do.
        // See http://bugs.webkit.org/show_bug.cgi?id=12072
    } else if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else if (attr->name() == onchangeAttr) {
        setHTMLEventListener(changeEvent, attr);
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
    return !usesMenuList(); 
}

void HTMLSelectElement::selectAll()
{
    ASSERT(!usesMenuList());
    if (!renderer() || !multiple())
        return;
    
    // Save the selection so it can be compared to the new selectAll selection when we call onChange
    saveLastSelection();
    
    m_activeSelectionState = true;
    setActiveSelectionAnchorIndex(nextSelectableListIndex(-1));
    setActiveSelectionEndIndex(previousSelectableListIndex(-1));
    
    updateListBoxSelection(false);
    listBoxOnChange();
}

RenderObject *HTMLSelectElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    if (usesMenuList())
        return new (arena) RenderMenuList(this);
    return new (arena) RenderListBox(this);
}

bool HTMLSelectElement::appendFormData(FormDataList& list, bool)
{
    bool successful = false;
    const Vector<HTMLElement*>& items = listItems();

    unsigned i;
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            HTMLOptionElement *option = static_cast<HTMLOptionElement*>(items[i]);
            if (option->selected()) {
                list.appendData(name(), option->value());
                successful = true;
            }
        }
    }

    // ### this case should not happen. make sure that we select the first option
    // in any case. otherwise we have no consistency with the DOM interface. FIXME!
    // we return the first one if it was a combobox select
    if (!successful && !m_multiple && m_size <= 1 && items.size() &&
        (items[0]->hasLocalName(optionTag))) {
        HTMLOptionElement *option = static_cast<HTMLOptionElement*>(items[0]);
        if (option->value().isNull())
            list.appendData(name(), option->text().stripWhiteSpace());
        else
            list.appendData(name(), option->value());
        successful = true;
    }

    return successful;
}

int HTMLSelectElement::optionToListIndex(int optionIndex) const
{
    const Vector<HTMLElement*>& items = listItems();
    int listSize = (int)items.size();
    if (optionIndex < 0 || optionIndex >= listSize)
        return -1;

    int optionIndex2 = -1;
    for (int listIndex = 0; listIndex < listSize; listIndex++) {
        if (items[listIndex]->hasLocalName(optionTag)) {
            optionIndex2++;
            if (optionIndex2 == optionIndex)
                return listIndex;
        }
    }
    return -1;
}

int HTMLSelectElement::listToOptionIndex(int listIndex) const
{
    const Vector<HTMLElement*>& items = listItems();
    if (listIndex < 0 || listIndex >= int(items.size()) ||
        !items[listIndex]->hasLocalName(optionTag))
        return -1;

    int optionIndex = 0; // actual index of option not counting OPTGROUP entries that may be in list
    for (int i = 0; i < listIndex; i++)
        if (items[i]->hasLocalName(optionTag))
            optionIndex++;
    return optionIndex;
}

PassRefPtr<HTMLOptionsCollection> HTMLSelectElement::options()
{
    return new HTMLOptionsCollection(this);
}

void HTMLSelectElement::recalcListItems(bool updateSelectedStates) const
{
    m_listItems.clear();
    HTMLOptionElement* foundSelected = 0;
    for (Node* current = firstChild(); current; current = current->traverseNextSibling(this)) {
        if (current->hasTagName(optgroupTag) && current->firstChild()) {
            // FIXME: It doesn't make sense to add an optgroup to the list items,
            // when it has children, but not to add it if it happens to have,
            // children (say some comment nodes or text nodes), yet that's what
            // this code does!
            m_listItems.append(static_cast<HTMLElement*>(current));
            current = current->firstChild();
            // FIXME: It doesn't make sense to handle an <optgroup> inside another <optgroup>
            // if it's not the first child, but not handle it if it happens to be the first
            // child, yet that's what this code does!
        }

        if (current->hasTagName(optionTag)) {
            m_listItems.append(static_cast<HTMLElement*>(current));
            if (updateSelectedStates) {
                if (!foundSelected && (usesMenuList() || (!m_multiple && static_cast<HTMLOptionElement*>(current)->selected()))) {
                    foundSelected = static_cast<HTMLOptionElement*>(current);
                    foundSelected->setSelectedState(true);
                } else if (foundSelected && !m_multiple && static_cast<HTMLOptionElement*>(current)->selected()) {
                    foundSelected->setSelectedState(false);
                    foundSelected = static_cast<HTMLOptionElement*>(current);
                }
            }
        }
        if (current->hasTagName(hrTag))
            m_listItems.append(static_cast<HTMLElement*>(current));
    }
    m_recalcListItems = false;
}

void HTMLSelectElement::childrenChanged(bool changedByParser)
{
    setRecalcListItems();
    HTMLFormControlElementWithState::childrenChanged(changedByParser);
}

void HTMLSelectElement::setRecalcListItems()
{
    m_recalcListItems = true;
    if (renderer()) {
        if (usesMenuList())
            static_cast<RenderMenuList*>(renderer())->setOptionsChanged(true);
        else
            static_cast<RenderListBox*>(renderer())->setOptionsChanged(true);
    }
    if (!inDocument())
        m_collectionInfo.reset();
    setChanged();
}

void HTMLSelectElement::reset()
{
    bool optionSelected = false;
    HTMLOptionElement* firstOption = 0;
    const Vector<HTMLElement*>& items = listItems();
    unsigned i;
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            HTMLOptionElement *option = static_cast<HTMLOptionElement*>(items[i]);
            if (!option->getAttribute(selectedAttr).isNull()) {
                option->setSelectedState(true);
                optionSelected = true;
            } else
                option->setSelectedState(false);
            if (!firstOption)
                firstOption = option;
        }
    }
    if (!optionSelected && firstOption && usesMenuList())
        firstOption->setSelectedState(true);
    
    setChanged();
}

void HTMLSelectElement::dispatchFocusEvent()
{
    if (usesMenuList())
        // Save the selection so it can be compared to the new selection when we call onChange during dispatchBlurEvent.
        saveLastSelection();
    HTMLFormControlElementWithState::dispatchFocusEvent();
}

void HTMLSelectElement::dispatchBlurEvent()
{
    // We only need to fire onChange here for menu lists, because we fire onChange for list boxes whenever the selection change is actually made.
    // This matches other browsers' behavior.
    if (usesMenuList())
        menuListOnChange();
    HTMLFormControlElementWithState::dispatchBlurEvent();
}

void HTMLSelectElement::defaultEventHandler(Event* evt)
{
    if (!renderer())
        return;
    
    if (usesMenuList())
        menuListDefaultEventHandler(evt);
    else 
        listBoxDefaultEventHandler(evt);
    
    if (evt->defaultHandled())
        return;

    if (evt->type() == keypressEvent && evt->isKeyboardEvent()) {
        KeyboardEvent* keyboardEvent = static_cast<KeyboardEvent*>(evt);
    
        if (!keyboardEvent->ctrlKey() && !keyboardEvent->altKey() && !keyboardEvent->metaKey() &&
            isPrintableChar(keyboardEvent->charCode())) {
            typeAheadFind(keyboardEvent);
            evt->setDefaultHandled();
            return;
        }
    }

    HTMLFormControlElementWithState::defaultEventHandler(evt);
}

void HTMLSelectElement::menuListDefaultEventHandler(Event* evt)
{
    RenderMenuList* menuList = static_cast<RenderMenuList*>(renderer());

    if (evt->type() == keydownEvent) {
        if (!renderer() || !evt->isKeyboardEvent())
            return;
        String keyIdentifier = static_cast<KeyboardEvent*>(evt)->keyIdentifier();
        bool handled = false;
#if ARROW_KEYS_POP_MENU
        if (keyIdentifier == "Down" || keyIdentifier == "Up") {
            focus();
            // Save the selection so it can be compared to the new selection when we call onChange during setSelectedIndex,
            // which gets called from RenderMenuList::valueChanged, which gets called after the user makes a selection from the menu.
            saveLastSelection();
            menuList->showPopup();
            handled = true;
        }
#else
        int listIndex = optionToListIndex(selectedIndex());
        if (keyIdentifier == "Down" || keyIdentifier == "Right") {
            int size = listItems().size();
            for (listIndex += 1;
                 listIndex >= 0 && listIndex < size && (listItems()[listIndex]->disabled() || !listItems()[listIndex]->hasTagName(optionTag));
                 ++listIndex);
            
            if (listIndex >= 0 && listIndex < size)
                setSelectedIndex(listToOptionIndex(listIndex));
            handled = true;
        } else if (keyIdentifier == "Up" || keyIdentifier == "Left") {
            int size = listItems().size();
            for (listIndex -= 1;
                 listIndex >= 0 && listIndex < size && (listItems()[listIndex]->disabled() || !listItems()[listIndex]->hasTagName(optionTag));
                 --listIndex);
            
            if (listIndex >= 0 && listIndex < size)
                setSelectedIndex(listToOptionIndex(listIndex));
            handled = true;
        }
#endif
        if (handled)
            evt->setDefaultHandled();
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (evt->type() == keypressEvent) {
        if (!renderer() || !evt->isKeyboardEvent())
            return;
        int keyCode = static_cast<KeyboardEvent*>(evt)->keyCode();
        bool handled = false;
#if ARROW_KEYS_POP_MENU
        if (keyCode == ' ') {
            focus();
            // Save the selection so it can be compared to the new selection when we call onChange during setSelectedIndex,
            // which gets called from RenderMenuList::valueChanged, which gets called after the user makes a selection from the menu.
            saveLastSelection();
            menuList->showPopup();
            handled = true;
        }
        if (keyCode == '\r') {
            menuListOnChange();
            if (form())
                form()->submitClick(evt);
            handled = true;
        }
#else
        int listIndex = optionToListIndex(selectedIndex());
        if (keyCode == '\r') {
            // listIndex should already be selected, but this will fire the onchange handler.
            setSelectedIndex(listToOptionIndex(listIndex), true, true);
            handled = true;
        }
#endif
        if (handled)
            evt->setDefaultHandled();
    }

    if (evt->type() == mousedownEvent && evt->isMouseEvent() && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        focus();
        if (menuList->popupIsVisible())
            menuList->hidePopup();
        else {
            // Save the selection so it can be compared to the new selection when we call onChange during setSelectedIndex,
            // which gets called from RenderMenuList::valueChanged, which gets called after the user makes a selection from the menu.
            saveLastSelection();
            menuList->showPopup();
        }
        evt->setDefaultHandled();
    }
}

void HTMLSelectElement::listBoxDefaultEventHandler(Event* evt)
{
    if (evt->type() == mousedownEvent && evt->isMouseEvent() && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        focus();
        
        MouseEvent* mEvt = static_cast<MouseEvent*>(evt);
        int listIndex = static_cast<RenderListBox*>(renderer())->listIndexAtOffset(mEvt->offsetX(), mEvt->offsetY());
        if (listIndex >= 0) {
            // Save the selection so it can be compared to the new selection when we call onChange during mouseup, or after autoscroll finishes.
            saveLastSelection();

            m_activeSelectionState = true;
            
            bool multiSelectKeyPressed = false;
#if PLATFORM(MAC)
            multiSelectKeyPressed = mEvt->metaKey();
#else
            multiSelectKeyPressed = mEvt->ctrlKey();
#endif

            bool shiftSelect = multiple() && mEvt->shiftKey();
            bool multiSelect = multiple() && multiSelectKeyPressed && !mEvt->shiftKey();
            
            HTMLElement* clickedElement = listItems()[listIndex];            
            HTMLOptionElement* option = 0;
            if (clickedElement->hasLocalName(optionTag)) {
                option = static_cast<HTMLOptionElement*>(clickedElement);
                
                // Keep track of whether an active selection (like during drag selection), should select or deselect
                if (option->selected() && multiSelectKeyPressed)
                    m_activeSelectionState = false;

                if (!m_activeSelectionState)
                    option->setSelectedState(false);
            }
            
            // If we're not in any special multiple selection mode, then deselect all other items, excluding the clicked option.
            // If no option was clicked, then this will deselect all items in the list.
            if (!shiftSelect && !multiSelect)
                deselectItems(option);

            // If the anchor hasn't been set, and we're doing a single selection or a shift selection, then initialize the anchor to the first selected index.
            if (m_activeSelectionAnchorIndex < 0 && !multiSelect)
                setActiveSelectionAnchorIndex(selectedIndex());

            // Set the selection state of the clicked option
            if (option && !option->disabled())
                option->setSelectedState(true);
            
            // If there was no selectedIndex() for the previous initialization, or
            // If we're doing a single selection, or a multiple selection (using cmd or ctrl), then initialize the anchor index to the listIndex that just got clicked.
            if (listIndex >= 0 && (m_activeSelectionAnchorIndex < 0 || !shiftSelect))
                setActiveSelectionAnchorIndex(listIndex);
            
            setActiveSelectionEndIndex(listIndex);
            updateListBoxSelection(!multiSelect);

            if (Frame* frame = document()->frame())
                frame->eventHandler()->setMouseDownMayStartAutoscroll();

            evt->setDefaultHandled();
        }
    } else if (evt->type() == mouseupEvent && evt->isMouseEvent() && static_cast<MouseEvent*>(evt)->button() == LeftButton && document()->frame()->eventHandler()->autoscrollRenderer() != renderer())
        // This makes sure we fire onChange for a single click.  For drag selection, onChange will fire when the autoscroll timer stops.
        listBoxOnChange();
    else if (evt->type() == keydownEvent) {
        if (!evt->isKeyboardEvent())
            return;
        String keyIdentifier = static_cast<KeyboardEvent*>(evt)->keyIdentifier();

        int endIndex = 0;        
        if (m_activeSelectionEndIndex < 0) {
            // Initialize the end index
            if (keyIdentifier == "Down")
                endIndex = nextSelectableListIndex(lastSelectedListIndex());
            else if (keyIdentifier == "Up")
                endIndex = previousSelectableListIndex(optionToListIndex(selectedIndex()));
        } else {
            // Set the end index based on the current end index
            if (keyIdentifier == "Down")
                endIndex = nextSelectableListIndex(m_activeSelectionEndIndex);
            else if (keyIdentifier == "Up")
                endIndex = previousSelectableListIndex(m_activeSelectionEndIndex);    
        }
        
        if (keyIdentifier == "Down" || keyIdentifier == "Up") {
            // Save the selection so it can be compared to the new selection when we call onChange immediately after making the new selection.
            saveLastSelection();

            ASSERT(endIndex >= 0 && (unsigned)endIndex < listItems().size()); 
            setActiveSelectionEndIndex(endIndex);
            
            // If the anchor is unitialized, or if we're going to deselect all other options, then set the anchor index equal to the end index.
            bool deselectOthers = !multiple() || !static_cast<KeyboardEvent*>(evt)->shiftKey();
            if (m_activeSelectionAnchorIndex < 0 || deselectOthers) {
                m_activeSelectionState = true;
                if (deselectOthers)
                    deselectItems();
                setActiveSelectionAnchorIndex(m_activeSelectionEndIndex);
            }

            static_cast<RenderListBox*>(renderer())->scrollToRevealElementAtListIndex(endIndex);
            updateListBoxSelection(deselectOthers);
            listBoxOnChange();
            evt->setDefaultHandled();
        }
    } else if (evt->type() == keypressEvent) {
        if (!evt->isKeyboardEvent())
            return;
        int keyCode = static_cast<KeyboardEvent*>(evt)->keyCode();

        if (keyCode == '\r') {
            if (form())
                form()->submitClick(evt);
            evt->setDefaultHandled();
            return;
        }
    }
}

void HTMLSelectElement::setActiveSelectionAnchorIndex(int index)
{
    m_activeSelectionAnchorIndex = index;
    
    // Cache the selection state so we can restore the old selection as the new selection pivots around this anchor index
    const Vector<HTMLElement*>& items = listItems();
    m_cachedStateForActiveSelection.clear();
    for (unsigned i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            HTMLOptionElement* option = static_cast<HTMLOptionElement*>(items[i]);
            m_cachedStateForActiveSelection.append(option->selected());
        } else
            m_cachedStateForActiveSelection.append(false);
    }
}

void HTMLSelectElement::updateListBoxSelection(bool deselectOtherOptions)
{
    ASSERT(renderer() && renderer()->isListBox());
    
    unsigned start;
    unsigned end;
    ASSERT(m_activeSelectionAnchorIndex >= 0);
    start = min(m_activeSelectionAnchorIndex, m_activeSelectionEndIndex);
    end = max(m_activeSelectionAnchorIndex, m_activeSelectionEndIndex);

    const Vector<HTMLElement*>& items = listItems();
    for (unsigned i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            HTMLOptionElement* option = static_cast<HTMLOptionElement*>(items[i]);
            if (!option->disabled()) {
                if (i >= start && i <= end)
                    option->setSelectedState(m_activeSelectionState);
                else if (deselectOtherOptions)
                    option->setSelectedState(false);
                else
                    option->setSelectedState(m_cachedStateForActiveSelection[i]);
            }
        }
    }

    scrollToSelection();
}

void HTMLSelectElement::menuListOnChange()
{
    ASSERT(usesMenuList());
    int selected = selectedIndex();
    if (m_lastOnChangeIndex != selected) {
        m_lastOnChangeIndex = selected;
        onChange();
    }
}

void HTMLSelectElement::listBoxOnChange()
{
    ASSERT(!usesMenuList());

    const Vector<HTMLElement*>& items = listItems();
    
    // If the cached selection list is empty, or the size has changed, then fire onChange, and return early.
    if (m_lastOnChangeSelection.isEmpty() || m_lastOnChangeSelection.size() != items.size()) {
        onChange();
        return;
    }
    
    // Update m_lastOnChangeSelection and fire onChange
    bool fireOnChange = false;
    for (unsigned i = 0; i < items.size(); i++) {
        bool selected = false;
        if (items[i]->hasLocalName(optionTag))
            selected = static_cast<HTMLOptionElement*>(items[i])->selected();
        if (selected != m_lastOnChangeSelection[i])      
            fireOnChange = true;
        m_lastOnChangeSelection[i] = selected;
    }
    if (fireOnChange)
        onChange();
}

void HTMLSelectElement::saveLastSelection()
{
    const Vector<HTMLElement*>& items = listItems();

    if (usesMenuList()) {
        m_lastOnChangeIndex = selectedIndex();
        return;
    }
    
    m_lastOnChangeSelection.clear();
    for (unsigned i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            HTMLOptionElement* option = static_cast<HTMLOptionElement*>(items[i]);
            m_lastOnChangeSelection.append(option->selected());
        } else
            m_lastOnChangeSelection.append(false);
    }
}

static String stripLeadingWhiteSpace(const String& string)
{
    int length = string.length();
    int i;
    for (i = 0; i < length; ++i)
        if (string[i] != noBreakSpace &&
            (string[i] <= 0x7F ? !isASCIISpace(string[i]) : (direction(string[i]) != WhiteSpaceNeutral)))
            break;

    return string.substring(i, length - i);
}

void HTMLSelectElement::typeAheadFind(KeyboardEvent* event)
{
    if (event->timeStamp() < m_lastCharTime)
        return;

    DOMTimeStamp delta = event->timeStamp() - m_lastCharTime;

    m_lastCharTime = event->timeStamp();

    UChar c = event->charCode();

    String prefix;
    int searchStartOffset = 1;
    if (delta > typeAheadTimeout) {
        m_typedString = prefix = String(&c, 1);
        m_repeatingChar = c;
    } else {
        m_typedString.append(c);

        if (c == m_repeatingChar)
            // The user is likely trying to cycle through all the items starting with this character, so just search on the character
            prefix = String(&c, 1);
        else {
            m_repeatingChar = 0;
            prefix = m_typedString;
            searchStartOffset = 0;
        }
    }

    const Vector<HTMLElement*>& items = listItems();
    int itemCount = items.size();
    if (itemCount < 1)
        return;

    int index = (optionToListIndex(selectedIndex()) + searchStartOffset) % itemCount;
    for (int i = 0; i < itemCount; i++, index = (index + 1) % itemCount) {
        if (!items[index]->hasTagName(optionTag) || items[index]->disabled())
            continue;

        if (stripLeadingWhiteSpace(static_cast<HTMLOptionElement*>(items[index])->optionText()).startsWith(prefix, false)) {
            setSelectedIndex(listToOptionIndex(index));
            setChanged();
            return;
        }
    }
}

int HTMLSelectElement::nextSelectableListIndex(int startIndex)
{
    const Vector<HTMLElement*>& items = listItems();
    int index = startIndex + 1;
    while (index >= 0 && (unsigned)index < items.size() && (!items[index]->hasLocalName(optionTag) || items[index]->disabled()))
        index++;
    if ((unsigned) index == items.size())
        return startIndex;
    return index;
}

int HTMLSelectElement::previousSelectableListIndex(int startIndex)
{
    const Vector<HTMLElement*>& items = listItems();
    if (startIndex == -1)
        startIndex = items.size();
    int index = startIndex - 1;
    while (index >= 0 && (unsigned)index < items.size() && (!items[index]->hasLocalName(optionTag) || items[index]->disabled()))
        index--;
    if (index == -1)
        return startIndex;
    return index;
}

void HTMLSelectElement::accessKeyAction(bool sendToAnyElement)
{
    focus();
    dispatchSimulatedClick(0, sendToAnyElement);
}

void HTMLSelectElement::setMultiple(bool multiple)
{
    setAttribute(multipleAttr, multiple ? "" : 0);
}

void HTMLSelectElement::setSize(int size)
{
    setAttribute(sizeAttr, String::number(size));
}

Node* HTMLSelectElement::namedItem(const String &name, bool caseSensitive)
{
    return options()->namedItem(name, caseSensitive);
}

Node* HTMLSelectElement::item(unsigned index)
{
    return options()->item(index);
}

void HTMLSelectElement::setOption(unsigned index, HTMLOptionElement* option, ExceptionCode& ec)
{
    ec = 0;
    if (index > INT_MAX)
        index = INT_MAX;
    int diff = index  - length();
    HTMLElement* before = 0;
    // out of array bounds ? first insert empty dummies
    if (diff > 0) {
        setLength(index, ec);
        // replace an existing entry ?
    } else if (diff < 0) {
        before = static_cast<HTMLElement*>(options()->item(index+1));
        remove(index);
    }
    // finally add the new element
    if (!ec) {
        add(option, before, ec);
        if (diff >= 0 && option->selected())
            setSelectedIndex(index, !m_multiple);
    }
}

void HTMLSelectElement::setLength(unsigned newLen, ExceptionCode& ec)
{
    ec = 0;
    if (newLen > INT_MAX)
        newLen = INT_MAX;
    int diff = length() - newLen;

    if (diff < 0) { // add dummy elements
        do {
            RefPtr<Element> option = document()->createElement("option", ec);
            if (!option)
                break;
            add(static_cast<HTMLElement*>(option.get()), 0, ec);
            if (ec)
                break;
        } while (++diff);
    }
    else // remove elements
        while (diff-- > 0)
            remove(newLen);
}

void HTMLSelectElement::scrollToSelection()
{
    if (renderer() && !usesMenuList())
        static_cast<RenderListBox*>(renderer())->selectionChanged();
}

#ifndef NDEBUG

void HTMLSelectElement::checkListItems() const
{
    Vector<HTMLElement*> items = m_listItems;
    recalcListItems(false);
    ASSERT(items == m_listItems);
}

#endif

} // namespace
