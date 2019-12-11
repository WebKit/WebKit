/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "StyleResolver.h"
#include "StyleScopeRuleSets.h"
#include <wtf/Forward.h>

namespace WebCore {

class StyleRulePage;

namespace Style {

class PageRuleCollector {
public:
    PageRuleCollector(Resolver::State& state, ScopeRuleSets& ruleSets)
        : m_state(state)
        , m_ruleSets(ruleSets) { }

    void matchAllPageRules(int pageIndex);
    const MatchResult& matchResult() const { return m_result; }

private:
    bool isLeftPage(int pageIndex) const;
    bool isRightPage(int pageIndex) const { return !isLeftPage(pageIndex); }
    bool isFirstPage(int pageIndex) const;
    String pageName(int pageIndex) const;

    void matchPageRules(RuleSet* rules, bool isLeftPage, bool isFirstPage, const String& pageName);
    void matchPageRulesForList(Vector<StyleRulePage*>& matchedRules, const Vector<StyleRulePage*>& rules, bool isLeftPage, bool isFirstPage, const String& pageName);

    const Resolver::State& m_state;
    ScopeRuleSets& m_ruleSets;

    MatchResult m_result;
};

} // namespace Style
} // namespace WebCore
