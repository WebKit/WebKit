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
#include "RenderObject.h"

namespace WebCore {

KeyframeList::~KeyframeList()
{
    clear();
}

void KeyframeList::clear()
{
    m_keyframes.clear();
    m_properties.clear();
}

bool KeyframeList::operator==(const KeyframeList& o) const
{
    if (m_keyframes.size() != o.m_keyframes.size())
        return false;

    Vector<KeyframeValue>::const_iterator it2 = o.m_keyframes.begin();
    for (Vector<KeyframeValue>::const_iterator it1 = m_keyframes.begin(); it1 != m_keyframes.end(); ++it1) {
        if (it1->m_key != it2->m_key)
            return false;
        const RenderStyle& style1 = *it1->m_style;
        const RenderStyle& style2 = *it2->m_style;
        if (style1 != style2)
            return false;
        ++it2;
    }

    return true;
}

void KeyframeList::insert(float key, PassRefPtr<RenderStyle> style)
{
    if (key < 0 || key > 1)
        return;

    int index = -1;
    
    for (size_t i = 0; i < m_keyframes.size(); ++i) {
        if (m_keyframes[i].m_key == key) {
            index = (int) i;
            break;
        }
        if (m_keyframes[i].m_key > key) {
            // insert before
            m_keyframes.insert(i, KeyframeValue());
            index = (int) i;
            break;
        }
    }
    
    if (index < 0) {
        // append
        index = (int) m_keyframes.size();
        m_keyframes.append(KeyframeValue());
    }
    
    m_keyframes[index].m_key = key;
    m_keyframes[index].m_style = style;
}

} // namespace WebCore
