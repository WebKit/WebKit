/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef HTMLSelectElement_h
#define HTMLSelectElement_h

#include "CollectionCache.h"
#include "Event.h"
#include "HTMLFormControlElement.h"
#include <wtf/Vector.h>

namespace WebCore {

class HTMLOptionElement;
class HTMLOptionsCollection;
class KeyboardEvent;

class HTMLSelectElement : public HTMLFormControlElementWithState {
public:
    HTMLSelectElement(const QualifiedName&, Document*, HTMLFormElement* = 0);

    virtual int tagPriority() const { return 6; }
    virtual bool checkDTD(const Node* newChild);

    virtual const AtomicString& formControlType() const;
    
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;
    virtual bool canSelectAll() const;
    virtual void selectAll();

    virtual void recalcStyle(StyleChange);

    virtual void dispatchFocusEvent();
    virtual void dispatchBlurEvent();
    
    virtual bool canStartSelection() const { return false; }

    int selectedIndex() const;
    void setSelectedIndex(int index, bool deselect = true, bool fireOnChange = false);
    int lastSelectedListIndex() const;

    virtual bool isEnumeratable() const { return true; }

    unsigned length() const;

    int minWidth() const { return m_minwidth; }

    int size() const { return m_size; }

    bool multiple() const { return m_multiple; }

    void add(HTMLElement* element, HTMLElement* before, ExceptionCode&);
    void remove(int index);

    String value();
    void setValue(const String&);
    
    PassRefPtr<HTMLOptionsCollection> options();

    virtual bool saveFormControlState(String& value) const;
    virtual void restoreFormControlState(const String&);

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    virtual void parseMappedAttribute(MappedAttribute*);

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle *);
    virtual bool appendFormData(FormDataList&, bool);

    // get the actual listbox index of the optionIndexth option
    int optionToListIndex(int optionIndex) const;
    // reverse of optionToListIndex - get optionIndex from listboxIndex
    int listToOptionIndex(int listIndex) const;

    void setRecalcListItems();

    const Vector<HTMLElement*>& listItems() const
    {
        if (m_recalcListItems)
            recalcListItems();
        else
            checkListItems();
        return m_listItems;
    }
    virtual void reset();

    virtual void defaultEventHandler(Event*);
    virtual void accessKeyAction(bool sendToAnyElement);
    void accessKeySetSelectedIndex(int);

    void setMultiple(bool);

    void setSize(int);

    void setOption(unsigned index, HTMLOptionElement*, ExceptionCode&);
    void setLength(unsigned, ExceptionCode&);

    Node* namedItem(const AtomicString& name);
    Node* item(unsigned index);

    CollectionCache* collectionInfo() { return &m_collectionInfo; }
    
    void setActiveSelectionAnchorIndex(int index);
    void setActiveSelectionEndIndex(int index) { m_activeSelectionEndIndex = index; }
    void updateListBoxSelection(bool deselectOtherOptions);
    void listBoxOnChange();
    void menuListOnChange();
    
    int activeSelectionStartListIndex() const;
    int activeSelectionEndListIndex() const;
    
    void scrollToSelection();

private:
    void recalcListItems(bool updateSelectedStates = true) const;
    void checkListItems() const;

    void deselectItems(HTMLOptionElement* excludeElement = 0);
    bool usesMenuList() const { return !m_multiple && m_size <= 1; }
    int nextSelectableListIndex(int startIndex);
    int previousSelectableListIndex(int startIndex);
    void menuListDefaultEventHandler(Event*);
    void listBoxDefaultEventHandler(Event*);
    void typeAheadFind(KeyboardEvent*);
    void saveLastSelection();

    virtual void insertedIntoTree(bool);

    mutable Vector<HTMLElement*> m_listItems;
    Vector<bool> m_cachedStateForActiveSelection;
    Vector<bool> m_lastOnChangeSelection;
    int m_minwidth;
    int m_size;
    bool m_multiple;
    mutable bool m_recalcListItems;
    mutable int m_lastOnChangeIndex;

    int m_activeSelectionAnchorIndex;
    int m_activeSelectionEndIndex;
    bool m_activeSelectionState;

    // Instance variables for type-ahead find
    UChar m_repeatingChar;
    DOMTimeStamp m_lastCharTime;
    String m_typedString;

    CollectionCache m_collectionInfo;
};

#ifdef NDEBUG

inline void HTMLSelectElement::checkListItems() const
{
}

#endif

} // namespace

#endif
