/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef KeyframeList_h
#define KeyframeList_h

#include "AtomicString.h"
#include "RenderStyle.h"
#include <wtf/Vector.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class KeyframeValue {
public:
    KeyframeValue()
        : key(-1)
    {
    }

    float key;
    RenderStyle style;
};

class KeyframeList : public RefCounted<KeyframeList> {
public:
    static PassRefPtr<KeyframeList> create(const AtomicString& animationName) { return adoptRef(new KeyframeList(animationName)); }

    bool operator==(const KeyframeList& o) const;
    bool operator!=(const KeyframeList& o) const { return !(*this == o); }
    
    const AtomicString& animationName() const { return m_animationName; }
    
    void insert(float inKey, const RenderStyle& inStyle);
    
    void addProperty(int prop) { m_properties.add(prop); }
    bool containsProperty(int prop) const { return m_properties.contains(prop); }
    HashSet<int>::const_iterator beginProperties() { return m_properties.begin(); }
    HashSet<int>::const_iterator endProperties() { return m_properties.end(); }
    
    void clear() { m_keyframes.clear(); m_properties.clear(); }
    bool isEmpty() const { return m_keyframes.isEmpty(); }
    size_t size() const { return m_keyframes.size(); }
    Vector<KeyframeValue>::const_iterator beginKeyframes() const { return m_keyframes.begin(); }
    Vector<KeyframeValue>::const_iterator endKeyframes() const { return m_keyframes.end(); }

private:
    KeyframeList(const AtomicString& animationName)
        : m_animationName(animationName)
    {
        insert(0, RenderStyle());
        insert(1, RenderStyle());
    }
        
    AtomicString m_animationName;
    Vector<KeyframeValue> m_keyframes;
    HashSet<int> m_properties;       // the properties being animated
};

} // namespace WebCore

#endif // KeyframeList_h
