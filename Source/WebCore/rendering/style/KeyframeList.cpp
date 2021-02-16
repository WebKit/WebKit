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
#include "CSSKeyframeRule.h"
#include "RenderObject.h"
#include "StyleResolver.h"

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

bool KeyframeList::hasImplicitKeyframes() const
{
    return size() && (m_keyframes[0].key() || m_keyframes[size() - 1].key() != 1);
}

void KeyframeList::copyKeyframes(KeyframeList& other)
{
    for (auto& keyframe : other.keyframes()) {
        KeyframeValue keyframeValue(keyframe.key(), RenderStyle::clonePtr(*keyframe.style()));
        for (auto propertyId : keyframe.properties())
            keyframeValue.addProperty(propertyId);
        insert(WTFMove(keyframeValue));
    }
}

static const StyleRuleKeyframe& zeroPercentKeyframe()
{
    static LazyNeverDestroyed<Ref<StyleRuleKeyframe>> rule;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        rule.construct(StyleRuleKeyframe::create(MutableStyleProperties::create()));
        rule.get()->setKey(0);
    });
    return rule.get().get();
}

static const StyleRuleKeyframe& hundredPercentKeyframe()
{
    static LazyNeverDestroyed<Ref<StyleRuleKeyframe>> rule;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        rule.construct(StyleRuleKeyframe::create(MutableStyleProperties::create()));
        rule.get()->setKey(1);
    });
    return rule.get().get();
}

void KeyframeList::fillImplicitKeyframes(const Element& element, Style::Resolver& styleResolver, const RenderStyle* elementStyle, const RenderStyle* parentElementStyle)
{
    // If the 0% keyframe is missing, create it (but only if there is at least one other keyframe).
    auto initialSize = size();
    if (initialSize > 0 && m_keyframes[0].key()) {
        KeyframeValue keyframeValue(0, nullptr);
        keyframeValue.setStyle(styleResolver.styleForKeyframe(element, elementStyle, parentElementStyle, &zeroPercentKeyframe(), keyframeValue));
        insert(WTFMove(keyframeValue));
    }

    // If the 100% keyframe is missing, create it (but only if there is at least one other keyframe).
    if (initialSize > 0 && (m_keyframes[size() - 1].key() != 1)) {
        KeyframeValue keyframeValue(1, nullptr);
        keyframeValue.setStyle(styleResolver.styleForKeyframe(element, elementStyle, parentElementStyle, &hundredPercentKeyframe(), keyframeValue));
        insert(WTFMove(keyframeValue));
    }
}

} // namespace WebCore
