/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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
#include "HTMLFormControlElement.h"
#include "SelectElement.h"

namespace WebCore {

class HTMLOptionElement;
class HTMLOptionsCollection;
class KeyboardEvent;

class HTMLSelectElement : public HTMLFormControlElementWithState, public SelectElement {
public:
    HTMLSelectElement(const QualifiedName&, Document*, HTMLFormElement* = 0);

    virtual int selectedIndex() const;
    virtual void setSelectedIndex(int index, bool deselect = true);
    virtual void setSelectedIndexByUser(int index, bool deselect = true, bool fireOnChangeNow = false);

    unsigned length() const;

    virtual int size() const { return m_data.size(); }
    virtual bool multiple() const { return m_data.multiple(); }

    void add(HTMLElement* element, HTMLElement* before, ExceptionCode&);
    void remove(int index);

    String value();
    void setValue(const String&);

    PassRefPtr<HTMLOptionsCollection> options();

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    void setRecalcListItems();

    virtual const Vector<Element*>& listItems() const { return m_data.listItems(this); }

    virtual void accessKeyAction(bool sendToAnyElement);
    void accessKeySetSelectedIndex(int);

    void setMultiple(bool);

    void setSize(int);

    void setOption(unsigned index, HTMLOptionElement*, ExceptionCode&);
    void setLength(unsigned, ExceptionCode&);

    Node* namedItem(const AtomicString& name);
    Node* item(unsigned index);

    CollectionCache* collectionInfo() { return &m_collectionInfo; }

    void scrollToSelection();

private:
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

    virtual bool isEnumeratable() const { return true; }

    virtual bool saveFormControlState(String& value) const;
    virtual void restoreFormControlState(const String&);

    virtual void parseMappedAttribute(MappedAttribute*);

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle *);
    virtual bool appendFormData(FormDataList&, bool);

    virtual int optionToListIndex(int optionIndex) const;
    virtual int listToOptionIndex(int listIndex) const;

    virtual void reset();

    virtual void defaultEventHandler(Event*);

    virtual void setActiveSelectionAnchorIndex(int index);
    virtual void setActiveSelectionEndIndex(int index);
    virtual void updateListBoxSelection(bool deselectOtherOptions);
    virtual void listBoxOnChange();
    virtual void menuListOnChange();
    
    virtual int activeSelectionStartListIndex() const;
    virtual int activeSelectionEndListIndex() const;
    
    void recalcListItems(bool updateSelectedStates = true) const;

    void deselectItems(HTMLOptionElement* excludeElement = 0);
    void typeAheadFind(KeyboardEvent*);
    void saveLastSelection();

    virtual void insertedIntoTree(bool);

    SelectElementData m_data;
    CollectionCache m_collectionInfo;
};

} // namespace

#endif
