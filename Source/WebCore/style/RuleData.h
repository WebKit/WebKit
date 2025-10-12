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

#include "PropertyAllowlist.h"
#include "SelectorFilter.h"
#include "StyleRule.h"
#include <wtf/CompactRefPtrTuple.h>

namespace WebCore {
namespace Style {

enum class MatchBasedOnRuleHash : unsigned {
    None,
    Universal,
    ClassA,
    ClassB,
    ClassC
};

enum class UsedRuleType : uint8_t {
    StartingStyle = 1 << 0,
    BaseAppearance = 1 << 1
};

class RuleData {
public:
    static const unsigned maximumSelectorComponentCount = 8192;

    RuleData(const StyleRule&, unsigned selectorIndex, unsigned selectorListIndex, unsigned position, OptionSet<UsedRuleType>);

    unsigned position() const { return m_position; }

    const StyleRule& styleRule() const { return *m_styleRuleWithSelectorIndex.pointer(); }

    const CSSSelector* selector() const
    { 
        return styleRule().selectorList().selectorAt(selectorIndex());
    }

#if ENABLE(CSS_SELECTOR_JIT)
    CompiledSelector& compiledSelector() const { return styleRule().compiledSelectorForListIndex(m_selectorListIndex); }
#endif
    
    unsigned selectorIndex() const { return m_styleRuleWithSelectorIndex.type(); }
    unsigned selectorListIndex() const { return m_selectorListIndex; }

    bool canMatchPseudoElement() const { return m_canMatchPseudoElement; }
    MatchBasedOnRuleHash matchBasedOnRuleHash() const { return static_cast<MatchBasedOnRuleHash>(m_matchBasedOnRuleHash); }
    unsigned linkMatchType() const { return m_linkMatchType; }
    void setLinkMatchType(unsigned value) { m_linkMatchType = value; }
    PropertyAllowlist propertyAllowlist() const { return static_cast<PropertyAllowlist>(m_propertyAllowlist); }
    OptionSet<UsedRuleType> usedRuleTypes() const { return OptionSet<UsedRuleType>::fromRaw(m_usedRuleTypes); }
    bool isEnabled() const { return m_isEnabled; }
    void setEnabled(bool value) { m_isEnabled = value; }

    const SelectorFilter::Hashes& descendantSelectorIdentifierHashes() const { return m_descendantSelectorIdentifierHashes; }

    void disableSelectorFiltering() { m_descendantSelectorIdentifierHashes[0] = 0; }

private:
    // Keep in sync with RuleFeature's selectorIndex and selectorListIndex size.
    CompactRefPtrTuple<const StyleRule, uint16_t> m_styleRuleWithSelectorIndex;
    unsigned m_selectorListIndex : 16;
    unsigned m_matchBasedOnRuleHash : 3;
    unsigned m_canMatchPseudoElement : 1;
    unsigned m_linkMatchType : 2; //  SelectorChecker::LinkMatchMask
    unsigned m_propertyAllowlist : 2;
    unsigned m_usedRuleTypes : 2;
    unsigned m_isEnabled : 1;
    // If we have more rules than 2^bitcount here we'll get confused about rule order.
    unsigned m_position : 21;
    SelectorFilter::Hashes m_descendantSelectorIdentifierHashes;
};

} // namespace Style
} // namespace WebCore

namespace WTF {

// RuleData is simple enough that initializing to 0 and moving with memcpy will totally work.
template<> struct VectorTraits<WebCore::Style::RuleData> : SimpleClassVectorTraits { };

} // namespace WTF

