/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "KeyframeList.h"

namespace WebCore {

bool KeyframeList::operator==(const KeyframeList& o) const
{
    if (m_keyframes.size() != o.m_keyframes.size())
        return false;

    Vector<KeyframeValue>::const_iterator it2 = o.m_keyframes.begin();
    for (Vector<KeyframeValue>::const_iterator it1 = m_keyframes.begin(); it1 != m_keyframes.end(); ++it1) {
        if (it1->key != it2->key)
            return false;
        const RenderStyle& style1 = it1->style;
        const RenderStyle& style2 = it2->style;
        if (!(style1 == style2))
            return false;
        ++it2;
    }

    return true;
}

void KeyframeList::insert(float inKey, const RenderStyle& inStyle)
{
    if (inKey < 0 || inKey > 1)
        return;

    for (size_t i = 0; i < m_keyframes.size(); ++i) {
        if (m_keyframes[i].key == inKey) {
            m_keyframes[i].style = inStyle;
            return;
        }
        if (m_keyframes[i].key > inKey) {
            // insert before
            m_keyframes.insert(i, KeyframeValue());
            m_keyframes[i].key = inKey;
            m_keyframes[i].style = inStyle;
            return;
        }
    }

    // append
    m_keyframes.append(KeyframeValue());
    m_keyframes[m_keyframes.size()-1].key = inKey;
    m_keyframes[m_keyframes.size()-1].style = inStyle;
}

} // namespace WebCore
