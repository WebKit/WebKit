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

#pragma once

#include "CSSPropertyNames.h"
#include <wtf/Vector.h>
#include <wtf/HashSet.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class Element;
class RenderStyle;
class TimingFunction;

namespace Style {
class Resolver;
}

class KeyframeValue {
public:
    KeyframeValue(double key, std::unique_ptr<RenderStyle> style)
        : m_key(key)
        , m_style(WTFMove(style))
    {
    }

    void addProperty(CSSPropertyID prop) { m_properties.add(prop); }
    bool containsProperty(CSSPropertyID prop) const { return m_properties.contains(prop); }
    const HashSet<CSSPropertyID>& properties() const { return m_properties; }

    double key() const { return m_key; }
    void setKey(double key) { m_key = key; }

    const RenderStyle* style() const { return m_style.get(); }
    void setStyle(std::unique_ptr<RenderStyle> style) { m_style = WTFMove(style); }

    TimingFunction* timingFunction() const { return m_timingFunction.get(); }
    void setTimingFunction(const RefPtr<TimingFunction>& timingFunction) { m_timingFunction = timingFunction; }

private:
    double m_key;
    HashSet<CSSPropertyID> m_properties; // The properties specified in this keyframe.
    std::unique_ptr<RenderStyle> m_style;
    RefPtr<TimingFunction> m_timingFunction;
};

class KeyframeList {
public:
    explicit KeyframeList(const AtomString& animationName)
        : m_animationName(animationName)
    {
    }
    ~KeyframeList();
        
    KeyframeList& operator=(KeyframeList&&) = default;
    bool operator==(const KeyframeList& o) const;
    bool operator!=(const KeyframeList& o) const { return !(*this == o); }

    const AtomString& animationName() const { return m_animationName; }
    
    void insert(KeyframeValue&&);
    
    void addProperty(CSSPropertyID prop) { m_properties.add(prop); }
    bool containsProperty(CSSPropertyID prop) const { return m_properties.contains(prop); }
    const HashSet<CSSPropertyID>& properties() const { return m_properties; }
    
    void clear();
    bool isEmpty() const { return m_keyframes.isEmpty(); }
    size_t size() const { return m_keyframes.size(); }
    const KeyframeValue& operator[](size_t index) const { return m_keyframes[index]; }
    const Vector<KeyframeValue>& keyframes() const { return m_keyframes; }

    void copyKeyframes(KeyframeList&);
    bool hasImplicitKeyframes() const;
    void fillImplicitKeyframes(const Element&, Style::Resolver&, const RenderStyle* elementStyle, const RenderStyle* parentElementStyle);

private:
    AtomString m_animationName;
    Vector<KeyframeValue> m_keyframes; // Kept sorted by key.
    HashSet<CSSPropertyID> m_properties; // The properties being animated.
};

} // namespace WebCore
