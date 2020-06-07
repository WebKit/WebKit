/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#include "SelectorFilter.h"
#include "StyleRule.h"

namespace WebCore {
namespace Style {

enum PropertyWhitelistType {
    PropertyWhitelistNone   = 0,
    PropertyWhitelistMarker,
#if ENABLE(VIDEO)
    PropertyWhitelistCue
#endif
};

enum class MatchBasedOnRuleHash : unsigned {
    None,
    Universal,
    ClassA,
    ClassB,
    ClassC
};

class RuleData {
public:
    static const unsigned maximumSelectorComponentCount = 8192;

    RuleData(const StyleRule&, unsigned selectorIndex, unsigned selectorListIndex, unsigned position);

    unsigned position() const { return m_position; }

    const StyleRule& styleRule() const { return *m_styleRule; }

    const CSSSelector* selector() const { return m_styleRule->selectorList().selectorAt(m_selectorIndex); }
#if ENABLE(CSS_SELECTOR_JIT)
    CompiledSelector& compiledSelector() const { return m_styleRule->compiledSelectorForListIndex(m_selectorListIndex); }
#endif
    
    unsigned selectorIndex() const { return m_selectorIndex; }
    unsigned selectorListIndex() const { return m_selectorListIndex; }

    bool canMatchPseudoElement() const { return m_canMatchPseudoElement; }
    MatchBasedOnRuleHash matchBasedOnRuleHash() const { return static_cast<MatchBasedOnRuleHash>(m_matchBasedOnRuleHash); }
    bool containsUncommonAttributeSelector() const { return m_containsUncommonAttributeSelector; }
    unsigned linkMatchType() const { return m_linkMatchType; }
    PropertyWhitelistType propertyWhitelistType() const { return static_cast<PropertyWhitelistType>(m_propertyWhitelistType); }
    bool isEnabled() const { return m_isEnabled; }
    void setEnabled(bool value) { m_isEnabled = value; }

    const SelectorFilter::Hashes& descendantSelectorIdentifierHashes() const { return m_descendantSelectorIdentifierHashes; }

    void disableSelectorFiltering() { m_descendantSelectorIdentifierHashes[0] = 0; }

private:
    RefPtr<const StyleRule> m_styleRule;
    // Keep in sync with RuleFeature's selectorIndex and selectorListIndex size.
    unsigned m_selectorIndex : 16;
    unsigned m_selectorListIndex : 16;
    // If we have more rules than 2^bitcount here we'll get confused about rule order.
    unsigned m_position : 22;
    unsigned m_matchBasedOnRuleHash : 3;
    unsigned m_canMatchPseudoElement : 1;
    unsigned m_containsUncommonAttributeSelector : 1;
    unsigned m_linkMatchType : 2; //  SelectorChecker::LinkMatchMask
    unsigned m_propertyWhitelistType : 2;
    unsigned m_isEnabled : 1;
    SelectorFilter::Hashes m_descendantSelectorIdentifierHashes;
};

} // namespace Style
} // namespace WebCore

namespace WTF {

// RuleData is simple enough that initializing to 0 and moving with memcpy will totally work.
template<> struct VectorTraits<WebCore::Style::RuleData> : SimpleClassVectorTraits { };

} // namespace WTF

