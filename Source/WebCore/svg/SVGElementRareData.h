/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#pragma once

#include "StyleProperties.h"
#include "StyleResolver.h"
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class CSSCursorImageValue;
class SVGCursorElement;
class SVGElement;

class SVGElementRareData {
    WTF_MAKE_NONCOPYABLE(SVGElementRareData); WTF_MAKE_FAST_ALLOCATED;
public:
    SVGElementRareData()
        : m_instancesUpdatesBlocked(false)
        , m_useOverrideComputedStyle(false)
        , m_needsOverrideComputedStyleUpdate(false)
    {
    }

    HashSet<SVGElement*>& instances() { return m_instances; }
    const HashSet<SVGElement*>& instances() const { return m_instances; }

    bool instanceUpdatesBlocked() const { return m_instancesUpdatesBlocked; }
    void setInstanceUpdatesBlocked(bool value) { m_instancesUpdatesBlocked = value; }

    SVGElement* correspondingElement() { return m_correspondingElement; }
    void setCorrespondingElement(SVGElement* correspondingElement) { m_correspondingElement = correspondingElement; }

    MutableStyleProperties* animatedSMILStyleProperties() const { return m_animatedSMILStyleProperties.get(); }
    MutableStyleProperties& ensureAnimatedSMILStyleProperties()
    {
        if (!m_animatedSMILStyleProperties)
            m_animatedSMILStyleProperties = MutableStyleProperties::create(SVGAttributeMode);
        return *m_animatedSMILStyleProperties;
    }

    const RenderStyle* overrideComputedStyle(Element& element, const RenderStyle* parentStyle)
    {
        if (!m_useOverrideComputedStyle)
            return nullptr;
        if (!m_overrideComputedStyle || m_needsOverrideComputedStyleUpdate) {
            // The style computed here contains no CSS Animations/Transitions or SMIL induced rules - this is needed to compute the "base value" for the SMIL animation sandwhich model.
            m_overrideComputedStyle = element.styleResolver().styleForElement(element, parentStyle, nullptr, RuleMatchingBehavior::MatchAllRulesExcludingSMIL).renderStyle;
            m_needsOverrideComputedStyleUpdate = false;
        }
        ASSERT(m_overrideComputedStyle);
        return m_overrideComputedStyle.get();
    }

    bool useOverrideComputedStyle() const { return m_useOverrideComputedStyle; }
    void setUseOverrideComputedStyle(bool value) { m_useOverrideComputedStyle = value; }
    void setNeedsOverrideComputedStyleUpdate() { m_needsOverrideComputedStyleUpdate = true; }

private:
    HashSet<SVGElement*> m_instances;
    SVGElement* m_correspondingElement { nullptr };
    bool m_instancesUpdatesBlocked : 1;
    bool m_useOverrideComputedStyle : 1;
    bool m_needsOverrideComputedStyleUpdate : 1;
    RefPtr<MutableStyleProperties> m_animatedSMILStyleProperties;
    std::unique_ptr<RenderStyle> m_overrideComputedStyle;
};

} // namespace WebCore
