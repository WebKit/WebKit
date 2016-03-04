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
#include "HTMLOptionElement.h"
#include "TypeAhead.h"
#include <wtf/Vector.h>

namespace WebCore {

class HTMLOptionsCollection;

class HTMLSelectElement : public HTMLFormControlElementWithState, public TypeAheadDataSource {
public:
    static Ref<HTMLSelectElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    WEBCORE_EXPORT int selectedIndex() const;
    void setSelectedIndex(int);

    WEBCORE_EXPORT void optionSelectedByUser(int index, bool dispatchChangeEvent, bool allowMultipleSelection = false);

    // For ValidityState
    String validationMessage() const override;
    bool valueMissing() const override;

    unsigned length() const;

    unsigned size() const { return m_size; }
    bool multiple() const { return m_multiple; }

    bool usesMenuList() const;

    void add(HTMLElement*, HTMLElement* beforeElement, ExceptionCode&);
    void add(HTMLElement*, int beforeIndex, ExceptionCode&);

    using Node::remove;
    // Should be remove(int) but it conflicts with Node::remove(ExceptionCode&).
    void removeByIndex(int);
    void remove(HTMLOptionElement*);

    WEBCORE_EXPORT String value() const;
    void setValue(const String&);

    Ref<HTMLOptionsCollection> options();
    Ref<HTMLCollection> selectedOptions();

    void optionElementChildrenChanged();

    void setRecalcListItems();
    void invalidateSelectedItems();
    void updateListItemSelectedStates();

    WEBCORE_EXPORT const Vector<HTMLElement*>& listItems() const;

    void accessKeyAction(bool sendMouseEvents) override;
    void accessKeySetSelectedIndex(int);

    void setMultiple(bool);

    void setSize(unsigned);

    void setOption(unsigned index, HTMLOptionElement*, ExceptionCode&);
    void setLength(unsigned, ExceptionCode&);

    HTMLOptionElement* namedItem(const AtomicString& name);
    HTMLOptionElement* item(unsigned index);

    void scrollToSelection();

    void listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow = true);

#if PLATFORM(IOS)
    bool willRespondToMouseClickEvents() override;
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
    const AtomicString& formControlType() const override;
    
    bool isKeyboardFocusable(KeyboardEvent*) const override;
    bool isMouseFocusable() const override;

    void dispatchFocusEvent(RefPtr<Element>&& oldFocusedElement, FocusDirection) override final;
    void dispatchBlurEvent(RefPtr<Element>&& newFocusedElement) override final;
    
    bool canStartSelection() const override { return false; }

    bool canHaveUserAgentShadowRoot() const override final { return true; }

    bool isEnumeratable() const override { return true; }
    bool supportLabels() const override { return true; }

    FormControlState saveFormControlState() const override;
    void restoreFormControlState(const FormControlState&) override;

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;

    bool childShouldCreateRenderer(const Node&) const override;
    RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;
    bool appendFormData(FormDataList&, bool) override;

    void reset() override;

    void defaultEventHandler(Event*) override;

    void dispatchChangeEventForMenuList();

    void didRecalcStyle(Style::Change) override final;

    void recalcListItems(bool updateSelectedStates = true) const;

    void deselectItems(HTMLOptionElement* excludeElement = 0);
    void typeAheadFind(KeyboardEvent&);
    void saveLastSelection();

    InsertionNotificationRequest insertedInto(ContainerNode&) override;

    bool isOptionalFormControl() const override { return !isRequiredFormControl(); }
    bool isRequiredFormControl() const override;

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

    void childrenChanged(const ChildChange&) override;

    // TypeAheadDataSource functions.
    int indexOfSelectedOption() const override;
    int optionCount() const override;
    String optionAtIndex(int index) const override;

    // m_listItems contains HTMLOptionElement, HTMLOptGroupElement, and HTMLHRElement objects.
    mutable Vector<HTMLElement*> m_listItems;
    Vector<bool> m_lastOnChangeSelection;
    Vector<bool> m_cachedStateForActiveSelection;
    TypeAhead m_typeAhead;
    unsigned m_size;
    int m_lastOnChangeIndex;
    int m_activeSelectionAnchorIndex;
    int m_activeSelectionEndIndex;
    bool m_isProcessingUserDrivenChange;
    bool m_multiple;
    bool m_activeSelectionState;
    bool m_allowsNonContiguousSelection;
    mutable bool m_shouldRecalcListItems;
};

} // namespace

#endif
