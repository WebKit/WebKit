/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "SelectElement.h"

namespace WebCore {

class HTMLOptionElement;
class HTMLOptionsCollection;

class HTMLSelectElement : public HTMLFormControlElementWithState {
public:
    static PassRefPtr<HTMLSelectElement> create(const QualifiedName&, Document*, HTMLFormElement*);

    int selectedIndex() const;
    void setSelectedIndex(int index, bool deselect = true);
    void setSelectedIndexByUser(int index, bool deselect = true, bool fireOnChangeNow = false, bool allowMultipleSelection = false);

    // For ValidityState
    bool valueMissing() const;

    unsigned length() const;

    int size() const { return m_data.size(); }
    bool multiple() const { return m_data.multiple(); }

    void add(HTMLElement*, HTMLElement* beforeElement, ExceptionCode&);
    void remove(int index);
    void remove(HTMLOptionElement*);

    String value() const;
    void setValue(const String&);

    PassRefPtr<HTMLOptionsCollection> options();

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    void setRecalcListItems();
    void recalcListItemsIfNeeded();

    const Vector<Element*>& listItems() const { return m_data.listItems(this); }

    virtual void accessKeyAction(bool sendToAnyElement);
    void accessKeySetSelectedIndex(int);

    void setMultiple(bool);

    void setSize(int);

    void setOption(unsigned index, HTMLOptionElement*, ExceptionCode&);
    void setLength(unsigned, ExceptionCode&);

    Node* namedItem(const AtomicString& name);
    Node* item(unsigned index);

    CollectionCache* collectionInfo() { m_collectionInfo.checkConsistency(); return &m_collectionInfo; }

    void scrollToSelection();

    void listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow = true);

    void updateValidity() { setNeedsValidityCheck(); }

    virtual bool canSelectAll() const;
    virtual void selectAll();
    int listToOptionIndex(int listIndex) const;
    void listBoxOnChange();
    int optionToListIndex(int optionIndex) const;
    int activeSelectionStartListIndex() const;
    int activeSelectionEndListIndex() const;
    void setActiveSelectionAnchorIndex(int);
    void setActiveSelectionEndIndex(int);
    void updateListBoxSelection(bool deselectOtherOptions);
    
protected:
    HTMLSelectElement(const QualifiedName&, Document*, HTMLFormElement*);

private:
    virtual const AtomicString& formControlType() const;
    
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;

    virtual void dispatchFocusEvent(PassRefPtr<Node> oldFocusedNode);
    virtual void dispatchBlurEvent(PassRefPtr<Node> newFocusedNode);
    
    virtual bool canStartSelection() const { return false; }

    virtual bool isEnumeratable() const { return true; }

    virtual bool saveFormControlState(String& value) const;
    virtual void restoreFormControlState(const String&);

    virtual void parseMappedAttribute(Attribute*);

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle *);
    virtual bool appendFormData(FormDataList&, bool);

    virtual void reset();

    virtual void defaultEventHandler(Event*);

    void menuListOnChange();
    
    void recalcListItems(bool updateSelectedStates = true) const;

    void deselectItems(HTMLOptionElement* excludeElement = 0);
    void typeAheadFind(KeyboardEvent*);
    void saveLastSelection();

    virtual void insertedIntoTree(bool);

    virtual bool isOptionalFormControl() const { return !isRequiredFormControl(); }
    virtual bool isRequiredFormControl() const;

    bool hasPlaceholderLabelOption() const;

    // FIXME: Fold some of the following functions.
    static void selectAll(SelectElementData&, Element*);
    static void saveLastSelection(SelectElementData&, Element*);
    static void setActiveSelectionAnchorIndex(SelectElementData&, Element*, int index);
    static void setActiveSelectionEndIndex(SelectElementData&, int index);
    static void updateListBoxSelection(SelectElementData&, Element*, bool deselectOtherOptions);
    static void listBoxOnChange(SelectElementData&, Element*);
    static void menuListOnChange(SelectElementData&, Element*);
    static void scrollToSelection(SelectElementData&, Element*);
    static void setRecalcListItems(SelectElementData&, Element*);
    static void recalcListItems(SelectElementData&, const Element*, bool updateSelectedStates = true);
    static int selectedIndex(const SelectElementData&, const Element*);
    static void setSelectedIndex(SelectElementData&, Element*, int optionIndex, bool deselect = true, bool fireOnChangeNow = false, bool userDrivenChange = true);
    static int optionToListIndex(const SelectElementData&, const Element*, int optionIndex);
    static int listToOptionIndex(const SelectElementData&, const Element*, int listIndex);
    static void dispatchFocusEvent(SelectElementData&, Element*);
    static void dispatchBlurEvent(SelectElementData&, Element*);
    static void deselectItems(SelectElementData&, Element*, Element* excludeElement = 0);
    static bool saveFormControlState(const SelectElementData&, const Element*, String& state);
    static void restoreFormControlState(SelectElementData&, Element*, const String& state);
    static void parseMultipleAttribute(SelectElementData&, Element*, Attribute*);
    static bool appendFormData(SelectElementData&, Element*, FormDataList&);
    static void reset(SelectElementData&, Element*);
    static void defaultEventHandler(SelectElementData&, Element*, Event*, HTMLFormElement*);
    static int lastSelectedListIndex(const SelectElementData&, const Element*);
    static void typeAheadFind(SelectElementData&, Element*, KeyboardEvent*);
    static void insertedIntoTree(SelectElementData&, Element*);
    static void accessKeySetSelectedIndex(SelectElementData&, Element*, int index);
    static unsigned optionCount(const SelectElementData&, const Element*);

    static void updateSelectedState(SelectElementData&, Element*, int listIndex, bool multi, bool shift);
 
    static void menuListDefaultEventHandler(SelectElementData&, Element*, Event*, HTMLFormElement*);
    static bool platformHandleKeydownEvent(SelectElementData&, Element*, KeyboardEvent*);
    static void listBoxDefaultEventHandler(SelectElementData&, Element*, Event*, HTMLFormElement*);
    static void setOptionsChangedOnRenderer(SelectElementData&, Element*);
    friend class SelectElementData;

    SelectElementData m_data;
    CollectionCache m_collectionInfo;
};

HTMLSelectElement* toSelectElement(Element*);

} // namespace

#endif
