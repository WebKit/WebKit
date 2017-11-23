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
#include "TypeAhead.h"

#include "KeyboardEvent.h"
#include <wtf/unicode/CharacterNames.h>


namespace WebCore {
using namespace WTF::Unicode;

TypeAhead::TypeAhead(TypeAheadDataSource* dataSource)
    : m_dataSource(dataSource)
    , m_repeatingChar(0)
{
}

static const Seconds typeAheadTimeout { 1_s };

static String stripLeadingWhiteSpace(const String& string)
{
    unsigned length = string.length();
    unsigned i;
    for (i = 0; i < length; ++i) {
        if (string[i] != noBreakSpace && !isSpaceOrNewline(string[i]))
            break;
    }
    return string.substring(i, length - i);
}

int TypeAhead::handleEvent(KeyboardEvent* event, MatchModeFlags matchMode)
{
    if (event->timeStamp() < m_lastTypeTime)
        return -1;

    int optionCount = m_dataSource->optionCount();
    Seconds delta = event->timeStamp() - m_lastTypeTime;
    m_lastTypeTime = event->timeStamp();

    UChar c = event->charCode();

    if (delta > typeAheadTimeout)
        m_buffer.clear();
    m_buffer.append(c);

    if (optionCount < 1)
        return -1;

    int searchStartOffset = 1;
    String prefix;
    if (matchMode & CycleFirstChar && c == m_repeatingChar) {
        // The user is likely trying to cycle through all the items starting
        // with this character, so just search on the character.
        prefix = String(&c, 1);
        m_repeatingChar = c;
    } else if (matchMode & MatchPrefix) {
        prefix = m_buffer.toString();
        if (m_buffer.length() > 1) {
            m_repeatingChar = 0;
            searchStartOffset = 0;
        } else
            m_repeatingChar = c;
    }

    if (!prefix.isEmpty()) {
        int selected = m_dataSource->indexOfSelectedOption();
        int index = (selected < 0 ? 0 : selected) + searchStartOffset;
        index %= optionCount;

        String prefixWithCaseFolded(prefix.foldCase());
        for (int i = 0; i < optionCount; ++i, index = (index + 1) % optionCount) {
            // Fold the option string and check if its prefix is equal to the folded prefix.
            String text = m_dataSource->optionAtIndex(index);
            if (stripLeadingWhiteSpace(text).foldCase().startsWith(prefixWithCaseFolded))
                return index;
        }
    }

    if (matchMode & MatchIndex) {
        bool ok = false;
        int index = m_buffer.toString().toInt(&ok);
        if (index > 0 && index <= optionCount)
            return index - 1;
    }
    return -1;
}

} // namespace WebCore
