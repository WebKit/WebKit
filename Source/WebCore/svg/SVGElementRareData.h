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

#include "SVGResourceElementClient.h"
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

    void addInstance(SVGElement& element) { m_instances.add(element); }
    void removeInstance(SVGElement& element) { m_instances.remove(element); }
    const WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>& instances() const { return m_instances; }

    bool instanceUpdatesBlocked() const { return m_instancesUpdatesBlocked; }
    void setInstanceUpdatesBlocked(bool value) { m_instancesUpdatesBlocked = value; }

    void addReferencingElement(SVGElement& element) { m_referencingElements.add(element); }
    void removeReferencingElement(SVGElement& element) { m_referencingElements.remove(element); }
    const WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>& referencingElements() const { return m_referencingElements; }
    WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData> takeReferencingElements() { return std::exchange(m_referencingElements, { }); }
    SVGElement* referenceTarget() const { return m_referenceTarget.get(); }
    void setReferenceTarget(WeakPtr<SVGElement, WeakPtrImplWithEventTargetData>&& element) { m_referenceTarget = WTFMove(element); }

    void addReferencingCSSClient(SVGResourceElementClient& client) { m_referencingCSSClients.add(client); }
    void removeReferencingCSSClient(SVGResourceElementClient& client) { m_referencingCSSClients.remove(client); }
    const WeakHashSet<SVGResourceElementClient>& referencingCSSClients() const { return m_referencingCSSClients; }

    SVGElement* correspondingElement() { return m_correspondingElement.get(); }
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
            m_overrideComputedStyle = element.styleResolver().styleForElement(element, { parentStyle }, RuleMatchingBehavior::MatchAllRulesExcludingSMIL).style;
            m_needsOverrideComputedStyleUpdate = false;
        }
        ASSERT(m_overrideComputedStyle);
        return m_overrideComputedStyle.get();
    }

    bool useOverrideComputedStyle() const { return m_useOverrideComputedStyle; }
    void setUseOverrideComputedStyle(bool value) { m_useOverrideComputedStyle = value; }
    void setNeedsOverrideComputedStyleUpdate() { m_needsOverrideComputedStyleUpdate = true; }

private:
    WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData> m_referencingElements;
    WeakPtr<SVGElement, WeakPtrImplWithEventTargetData> m_referenceTarget;

    WeakHashSet<SVGResourceElementClient> m_referencingCSSClients;

    WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData> m_instances;
    WeakPtr<SVGElement, WeakPtrImplWithEventTargetData> m_correspondingElement;
    bool m_instancesUpdatesBlocked : 1;
    bool m_useOverrideComputedStyle : 1;
    bool m_needsOverrideComputedStyleUpdate : 1;
    RefPtr<MutableStyleProperties> m_animatedSMILStyleProperties;
    std::unique_ptr<RenderStyle> m_overrideComputedStyle;
};

} // namespace WebCore
