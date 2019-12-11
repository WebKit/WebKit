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

#include "Animation.h"
#include "RenderObject.h"

namespace WebCore {

KeyframeList::~KeyframeList() = default;

void KeyframeList::clear()
{
    m_keyframes.clear();
    m_properties.clear();
}

bool KeyframeList::operator==(const KeyframeList& o) const
{
    if (m_keyframes.size() != o.m_keyframes.size())
        return false;

    auto it2 = o.m_keyframes.begin();
    for (auto it1 = m_keyframes.begin(); it1 != m_keyframes.end(); ++it1, ++it2) {
        if (it1->key() != it2->key())
            return false;
        if (*it1->style() != *it2->style())
            return false;
    }

    return true;
}

void KeyframeList::insert(KeyframeValue&& keyframe)
{
    if (keyframe.key() < 0 || keyframe.key() > 1)
        return;

    bool inserted = false;
    size_t i = 0;
    for (; i < m_keyframes.size(); ++i) {
        if (m_keyframes[i].key() > keyframe.key()) {
            // insert before
            m_keyframes.insert(i, WTFMove(keyframe));
            inserted = true;
            break;
        }
    }
    
    if (!inserted)
        m_keyframes.append(WTFMove(keyframe));

    for (auto& property : m_keyframes[i].properties())
        m_properties.add(property);
}

} // namespace WebCore
