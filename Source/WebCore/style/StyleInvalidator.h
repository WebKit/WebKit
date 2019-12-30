/*
 * Copyright (C) 2012, 2014, 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "RuleFeature.h"
#include "RuleSet.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>

namespace WebCore {

class Document;
class Element;
class MediaQueryEvaluator;
class Scope;
class SelectorFilter;
class ShadowRoot;
class StyleSheetContents;

namespace Style {

struct InvalidationRuleSet;

class Invalidator {
public:
    Invalidator(const Vector<StyleSheetContents*>&, const MediaQueryEvaluator&);
    Invalidator(const InvalidationRuleSetVector&);

    ~Invalidator();

    bool dirtiesAllStyle() const { return m_dirtiesAllStyle; }
    void invalidateStyle(Document&);
    void invalidateStyle(Scope&);
    void invalidateStyle(ShadowRoot&);
    void invalidateStyle(Element&);

    static void invalidateShadowParts(ShadowRoot&);

    using MatchElementRuleSets = HashMap<MatchElement, InvalidationRuleSetVector, WTF::IntHash<MatchElement>, WTF::StrongEnumHashTraits<MatchElement>>;
    static void addToMatchElementRuleSets(Invalidator::MatchElementRuleSets&, const InvalidationRuleSet&);
    static void invalidateWithMatchElementRuleSets(Element&, const MatchElementRuleSets&);
    static void invalidateAllStyle(Scope&);
    static void invalidateHostAndSlottedStyleIfNeeded(ShadowRoot&);

private:
    enum class CheckDescendants { Yes, No };
    CheckDescendants invalidateIfNeeded(Element&, const SelectorFilter*);
    void invalidateStyleForTree(Element&, SelectorFilter*);
    void invalidateStyleForDescendants(Element&, SelectorFilter*);
    void invalidateInShadowTreeIfNeeded(Element&);
    void invalidateStyleWithMatchElement(Element&, MatchElement);

    struct RuleInformation {
        bool hasSlottedPseudoElementRules { false };
        bool hasHostPseudoClassRules { false };
        bool hasShadowPseudoElementRules { false };
        bool hasPartPseudoElementRules { false };
    };
    RuleInformation collectRuleInformation();

    RefPtr<RuleSet> m_ownedRuleSet;
    const InvalidationRuleSetVector m_ruleSets;

    RuleInformation m_ruleInformation;

    bool m_dirtiesAllStyle { false };
    bool m_didInvalidateHostChildren { false };
};

}
}
