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

#pragma once

#include "HTMLFormControlElementWithState.h"
#include "TypeAhead.h"

namespace WebCore {

class HTMLOptionsCollection;

class HTMLSelectElement : public HTMLFormControlElementWithState, private TypeAheadDataSource {
    WTF_MAKE_ISO_ALLOCATED(HTMLSelectElement);
public:
    static Ref<HTMLSelectElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    WEBCORE_EXPORT int selectedIndex() const;
    WEBCORE_EXPORT void setSelectedIndex(int);

    WEBCORE_EXPORT void optionSelectedByUser(int index, bool dispatchChangeEvent, bool allowMultipleSelection = false);

    String validationMessage() const final;
    bool valueMissing() const final;

    WEBCORE_EXPORT unsigned length() const;

    unsigned size() const { return m_size; }
    bool multiple() const { return m_multiple; }

    bool usesMenuList() const;

    using OptionOrOptGroupElement = Variant<RefPtr<HTMLOptionElement>, RefPtr<HTMLOptGroupElement>>;
    using HTMLElementOrInt = Variant<RefPtr<HTMLElement>, int>;
    WEBCORE_EXPORT ExceptionOr<void> add(const OptionOrOptGroupElement&, const std::optional<HTMLElementOrInt>& before);

    using Node::remove;
    WEBCORE_EXPORT void remove(int);

    WEBCORE_EXPORT String value() const;
    WEBCORE_EXPORT void setValue(const String&);

    WEBCORE_EXPORT Ref<HTMLOptionsCollection> options();
    Ref<HTMLCollection> selectedOptions();

    void optionElementChildrenChanged();

    void setRecalcListItems();
    void invalidateSelectedItems();
    void updateListItemSelectedStates();

    WEBCORE_EXPORT const Vector<HTMLElement*>& listItems() const;

    bool accessKeyAction(bool sendMouseEvents) final;
    void accessKeySetSelectedIndex(int);

    WEBCORE_EXPORT void setMultiple(bool);

    WEBCORE_EXPORT void setSize(unsigned);

    // Called by the bindings for the unnamed index-setter.
    ExceptionOr<void> setItem(unsigned index, HTMLOptionElement*);
    ExceptionOr<void> setLength(unsigned);

    WEBCORE_EXPORT HTMLOptionElement* namedItem(const AtomString& name);
    WEBCORE_EXPORT HTMLOptionElement* item(unsigned index);

    void scrollToSelection();

    void listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow = true);

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
    void optionSelectionStateChanged(HTMLOptionElement&, bool optionIsSelected);
    bool allowsNonContiguousSelection() const { return m_allowsNonContiguousSelection; };

protected:
    HTMLSelectElement(const QualifiedName&, Document&, HTMLFormElement*);

private:
    const AtomString& formControlType() const final;

    int defaultTabIndex() const final;
    bool isKeyboardFocusable(KeyboardEvent*) const final;
    bool isMouseFocusable() const final;

    void dispatchFocusEvent(RefPtr<Element>&& oldFocusedElement, FocusDirection) final;
    void dispatchBlurEvent(RefPtr<Element>&& newFocusedElement) final;
    
    bool canStartSelection() const final { return false; }

    bool isEnumeratable() const final { return true; }
    bool supportLabels() const final { return true; }

    bool isInteractiveContent() const final { return true; }

    FormControlState saveFormControlState() const final;
    void restoreFormControlState(const FormControlState&) final;

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const final;

    bool childShouldCreateRenderer(const Node&) const final;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool appendFormData(DOMFormData&, bool) final;

    void reset() final;

    void defaultEventHandler(Event&) final;
    bool willRespondToMouseClickEvents() final;

    void dispatchChangeEventForMenuList();

    void didRecalcStyle(Style::Change) final;

    void recalcListItems(bool updateSelectedStates = true) const;

    void deselectItems(HTMLOptionElement* excludeElement = nullptr);
    void typeAheadFind(KeyboardEvent&);
    void saveLastSelection();

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;

    bool isOptionalFormControl() const final { return !isRequiredFormControl(); }
    bool isRequiredFormControl() const final;

    bool hasPlaceholderLabelOption() const;

    enum SelectOptionFlag {
        DeselectOtherOptions = 1 << 0,
        DispatchChangeEvent = 1 << 1,
        UserDriven = 1 << 2,
    };
    typedef unsigned SelectOptionFlags;
    void selectOption(int optionIndex, SelectOptionFlags = 0);
    void deselectItemsWithoutValidation(HTMLElement* elementToExclude = nullptr);
    void parseMultipleAttribute(const AtomString&);
    int lastSelectedListIndex() const;
    void updateSelectedState(int listIndex, bool multi, bool shift);
    void menuListDefaultEventHandler(Event&);
    bool platformHandleKeydownEvent(KeyboardEvent*);
    void listBoxDefaultEventHandler(Event&);
    void setOptionsChangedOnRenderer();
    size_t searchOptionsForValue(const String&, size_t listIndexStart, size_t listIndexEnd) const;

    enum SkipDirection { SkipBackwards = -1, SkipForwards = 1 };
    int nextValidIndex(int listIndex, SkipDirection, int skip) const;
    int nextSelectableListIndex(int startIndex) const;
    int previousSelectableListIndex(int startIndex) const;
    int firstSelectableListIndex() const;
    int lastSelectableListIndex() const;
    int nextSelectableListIndexPageAway(int startIndex, SkipDirection) const;

    void childrenChanged(const ChildChange&) final;

    // TypeAheadDataSource functions.
    int indexOfSelectedOption() const final;
    int optionCount() const final;
    String optionAtIndex(int index) const final;


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
