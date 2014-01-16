/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "Event.h"
#include "HTMLFormControlElementWithState.h"
#include "TypeAhead.h"
#include <wtf/Vector.h>

namespace WebCore {

class HTMLOptionElement;
class HTMLOptionsCollection;

class HTMLSelectElement : public HTMLFormControlElementWithState, public TypeAheadDataSource {
public:
    static PassRefPtr<HTMLSelectElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    int selectedIndex() const;
    void setSelectedIndex(int);

    void optionSelectedByUser(int index, bool dispatchChangeEvent, bool allowMultipleSelection = false);

    // For ValidityState
    virtual String validationMessage() const override;
    virtual bool valueMissing() const override;

    unsigned length() const;

    int size() const { return m_size; }
    bool multiple() const { return m_multiple; }

    bool usesMenuList() const;

    void add(HTMLElement*, HTMLElement* beforeElement, ExceptionCode&);

    using Node::remove;
    // Should be remove(int) but it conflicts with Node::remove(ExceptionCode&).
    void removeByIndex(int);
    void remove(HTMLOptionElement*);

    String value() const;
    void setValue(const String&);

    PassRefPtr<HTMLOptionsCollection> options();
    PassRefPtr<HTMLCollection> selectedOptions();

    void optionElementChildrenChanged();

    void setRecalcListItems();
    void invalidateSelectedItems();
    void updateListItemSelectedStates();

    const Vector<HTMLElement*>& listItems() const;

    virtual void accessKeyAction(bool sendMouseEvents) override;
    void accessKeySetSelectedIndex(int);

    void setMultiple(bool);

    void setSize(int);

    void setOption(unsigned index, HTMLOptionElement*, ExceptionCode&);
    void setLength(unsigned, ExceptionCode&);

    Node* namedItem(const AtomicString& name);
    Node* item(unsigned index);

    void scrollToSelection();

    void listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow = true);

#if PLATFORM(IOS)
    virtual bool willRespondToMouseClickEvents() override;
#endif

    bool canSelectAll() const;
    void selectAll();
    int listToOptionIndex(int listIndex) const;
    void listBoxOnChange();
    int optionToListIndex(int optionIndex) const;
    int activeSelectionStartListIndex() const;
    int activeSelectionEndListIndex() const;
    void setActiveSelectionAnchorIndex(int);
    void setActiveSelectionEndIndex(int);
    void updateListBoxSelection(bool deselectOtherOptions);
    
    // For use in the implementation of HTMLOptionElement.
    void optionSelectionStateChanged(HTMLOptionElement*, bool optionIsSelected);
    bool allowsNonContiguousSelection() const { return m_allowsNonContiguousSelection; };

protected:
    HTMLSelectElement(const QualifiedName&, Document&, HTMLFormElement*);

private:
    virtual const AtomicString& formControlType() const override;
    
    virtual bool isKeyboardFocusable(KeyboardEvent*) const override;
    virtual bool isMouseFocusable() const override;

    virtual void dispatchFocusEvent(PassRefPtr<Element> oldFocusedElement, FocusDirection) override final;
    virtual void dispatchBlurEvent(PassRefPtr<Element> newFocusedElement) override final;
    
    virtual bool canStartSelection() const override { return false; }

    virtual bool isEnumeratable() const override { return true; }
    virtual bool supportLabels() const override { return true; }

    virtual FormControlState saveFormControlState() const override;
    virtual void restoreFormControlState(const FormControlState&) override;

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual bool isPresentationAttribute(const QualifiedName&) const override;

    virtual bool childShouldCreateRenderer(const Node&) const override;
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual bool appendFormData(FormDataList&, bool) override;

    virtual void reset() override;

    virtual void defaultEventHandler(Event*) override;

    void dispatchChangeEventForMenuList();

    virtual void didRecalcStyle(Style::Change) override final;

    void recalcListItems(bool updateSelectedStates = true) const;

    void deselectItems(HTMLOptionElement* excludeElement = 0);
    void typeAheadFind(KeyboardEvent*);
    void saveLastSelection();

    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;

    virtual bool isOptionalFormControl() const override { return !isRequiredFormControl(); }
    virtual bool isRequiredFormControl() const override;

    bool hasPlaceholderLabelOption() const;

    enum SelectOptionFlag {
        DeselectOtherOptions = 1 << 0,
        DispatchChangeEvent = 1 << 1,
        UserDriven = 1 << 2,
    };
    typedef unsigned SelectOptionFlags;
    void selectOption(int optionIndex, SelectOptionFlags = 0);
    void deselectItemsWithoutValidation(HTMLElement* elementToExclude = 0);
    void parseMultipleAttribute(const AtomicString&);
    int lastSelectedListIndex() const;
    void updateSelectedState(int listIndex, bool multi, bool shift);
    void menuListDefaultEventHandler(Event*);
    bool platformHandleKeydownEvent(KeyboardEvent*);
    void listBoxDefaultEventHandler(Event*);
    void setOptionsChangedOnRenderer();
    size_t searchOptionsForValue(const String&, size_t listIndexStart, size_t listIndexEnd) const;

    enum SkipDirection {
        SkipBackwards = -1,
        SkipForwards = 1
    };
    int nextValidIndex(int listIndex, SkipDirection, int skip) const;
    int nextSelectableListIndex(int startIndex) const;
    int previousSelectableListIndex(int startIndex) const;
    int firstSelectableListIndex() const;
    int lastSelectableListIndex() const;
    int nextSelectableListIndexPageAway(int startIndex, SkipDirection) const;

    virtual void childrenChanged(const ChildChange&) override;
    virtual bool areAuthorShadowsAllowed() const override { return false; }

    // TypeAheadDataSource functions.
    virtual int indexOfSelectedOption() const override;
    virtual int optionCount() const override;
    virtual String optionAtIndex(int index) const override;

    // m_listItems contains HTMLOptionElement, HTMLOptGroupElement, and HTMLHRElement objects.
    mutable Vector<HTMLElement*> m_listItems;
    Vector<bool> m_lastOnChangeSelection;
    Vector<bool> m_cachedStateForActiveSelection;
    TypeAhead m_typeAhead;
    int m_size;
    int m_lastOnChangeIndex;
    int m_activeSelectionAnchorIndex;
    int m_activeSelectionEndIndex;
    bool m_isProcessingUserDrivenChange;
    bool m_multiple;
    bool m_activeSelectionState;
    bool m_allowsNonContiguousSelection;
    mutable bool m_shouldRecalcListItems;
};

NODE_TYPE_CASTS(HTMLSelectElement)

} // namespace

#endif
