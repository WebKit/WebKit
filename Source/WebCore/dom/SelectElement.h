/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http//www.torchmobile.com/)
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

#include "DOMTimeStamp.h"
#include "PlatformString.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

class Element;

// HTMLSelectElement hold this struct as member variable
// and pass it to the static helper functions.
class SelectElementData {
public:
    SelectElementData();
    ~SelectElementData();

    bool multiple() const { return m_multiple; }
    void setMultiple(bool value) { m_multiple = value; }

    int size() const { return m_size; }
    void setSize(int value) { m_size = value; }

    bool usesMenuList() const
    {
#if ENABLE(NO_LISTBOX_RENDERING)
        return true;
#else
        return !m_multiple && m_size <= 1;
#endif
    }

    int lastOnChangeIndex() const { return m_lastOnChangeIndex; }
    void setLastOnChangeIndex(int value) { m_lastOnChangeIndex = value; }

    bool userDrivenChange() const { return m_userDrivenChange; }
    void setUserDrivenChange(bool value) { m_userDrivenChange = value; }

    Vector<bool>& lastOnChangeSelection() { return m_lastOnChangeSelection; }

    bool activeSelectionState() const { return m_activeSelectionState; }
    void setActiveSelectionState(bool value) { m_activeSelectionState = value; }

    int activeSelectionAnchorIndex() const { return m_activeSelectionAnchorIndex; }
    void setActiveSelectionAnchorIndex(int value) { m_activeSelectionAnchorIndex = value; }

    int activeSelectionEndIndex() const { return m_activeSelectionEndIndex; }
    void setActiveSelectionEndIndex(int value) { m_activeSelectionEndIndex = value; }

    Vector<bool>& cachedStateForActiveSelection() { return m_cachedStateForActiveSelection; }

    UChar repeatingChar() const { return m_repeatingChar; }
    void setRepeatingChar(const UChar& value) { m_repeatingChar = value; }

    DOMTimeStamp lastCharTime() const { return m_lastCharTime; }
    void setLastCharTime(const DOMTimeStamp& value) { m_lastCharTime = value; }

    String& typedString() { return m_typedString; }
    void setTypedString(const String& value) { m_typedString = value; }

private:
    void checkListItems(const Element*) const;

    Vector<bool> m_lastOnChangeSelection;
    Vector<bool> m_cachedStateForActiveSelection;

    // Instance variables for type-ahead find
    DOMTimeStamp m_lastCharTime;
    String m_typedString;
    UChar m_repeatingChar;

    int m_size;
    int m_lastOnChangeIndex;
    int m_activeSelectionAnchorIndex;
    int m_activeSelectionEndIndex;

    bool m_userDrivenChange;
    bool m_multiple;
    bool m_activeSelectionState;
};

}

#endif
