/*
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

#ifndef SelectElement_h
#define SelectElement_h

#include <wtf/Vector.h>

namespace WebCore {

class Element;

class SelectElement {
public:
    virtual ~SelectElement() { }

    virtual bool multiple() const = 0;

    virtual int size() const = 0;
    virtual const Vector<Element*>& listItems() const = 0;

    virtual void listBoxOnChange() = 0;
    virtual void updateListBoxSelection(bool deselectOtherOptions) = 0;

    virtual void menuListOnChange() = 0;

    virtual int activeSelectionStartListIndex() const = 0;
    virtual int activeSelectionEndListIndex() const = 0;

    virtual void setActiveSelectionAnchorIndex(int index) = 0;
    virtual void setActiveSelectionEndIndex(int index) = 0;

    virtual int listToOptionIndex(int listIndex) const = 0;
    virtual int optionToListIndex(int optionIndex) const = 0;

    virtual int selectedIndex() const = 0;
    virtual void setSelectedIndex(int index, bool deselect = true, bool fireOnChange = false) = 0;
};

SelectElement* toSelectElement(Element*);

}

#endif
