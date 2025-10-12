/*
 * Copyright (C) 2025 Shopify Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SpeculationRulesMatcher.h"

#include "Document.h"
#include "Element.h"
#include "HTMLAnchorElement.h"
#include "JSDOMGlobalObject.h"
#include "ScriptController.h"
#include "SelectorQuery.h"
#include "SpeculationRules.h"
#include "URLPattern.h"
#include "URLPatternOptions.h"

namespace WebCore {

static bool matches(const SpeculationRules::DocumentPredicate&, Document&, HTMLAnchorElement&);

static bool matches(const SpeculationRules::URLPatternPredicate& predicate, HTMLAnchorElement& anchor)
{
    for (const auto& patternString : predicate.patterns) {
        ExceptionOr<Ref<URLPattern>> exceptionOrPattern = URLPattern::create(anchor.protectedDocument(), patternString, String(anchor.document().baseURL().string()), URLPatternOptions());
        if (exceptionOrPattern.hasException())
            continue;

        Ref<URLPattern> pattern = exceptionOrPattern.returnValue();
        auto result = pattern->test(anchor.protectedDocument(), anchor.href().string(), String(anchor.document().baseURL().string()));
        if (!result.hasException() && result.returnValue())
            return true;
    }
    return false;
}

static bool matches(const SpeculationRules::CSSSelectorPredicate& predicate, Element& element)
{
    for (const auto& selectorString : predicate.selectors) {
        auto query = element.protectedDocument()->selectorQueryForString(selectorString);
        if (query.hasException())
            continue;
        if (query.returnValue().matches(element))
            return true;
    }
    return false;
}

static bool matches(const Box<SpeculationRules::Conjunction>& predicate, Document& document, HTMLAnchorElement& anchor)
{
    for (const auto& clause : predicate->clauses) {
        if (!matches(clause, document, anchor))
            return false;
    }
    return true;
}

static bool matches(const Box<SpeculationRules::Disjunction>& predicate, Document& document, HTMLAnchorElement& anchor)
{
    if (predicate->clauses.isEmpty())
        return false;

    for (const auto& clause : predicate->clauses) {
        if (matches(clause, document, anchor))
            return true;
    }
    return false;
}

static bool matches(const Box<SpeculationRules::Negation>& predicate, Document& document, HTMLAnchorElement& anchor)
{
    return !matches(*predicate->clause, document, anchor);
}

static bool matches(const SpeculationRules::DocumentPredicate& predicate, Document& document, HTMLAnchorElement& anchor)
{
    return WTF::switchOn(predicate.value(),
        [&] (const SpeculationRules::URLPatternPredicate& p) { return matches(p, anchor); },
        [&] (const SpeculationRules::CSSSelectorPredicate& p) { return matches(p, anchor); },
        [&] (const Box<SpeculationRules::Conjunction>& p) { return matches(p, document, anchor); },
        [&] (const Box<SpeculationRules::Disjunction>& p) { return matches(p, document, anchor); },
        [&] (const Box<SpeculationRules::Negation>& p) { return matches(p, document, anchor); }
    );
}

// https://wicg.github.io/nav-speculation/speculation-rules.html#document-rule-predicate-matching
std::optional<PrefetchRule> SpeculationRulesMatcher::hasMatchingRule(Document& document, HTMLAnchorElement& anchor)
{
    const auto& speculationRules = document.speculationRules();
    const auto& url = anchor.href();

    for (const auto& rule : speculationRules->prefetchRules()) {
        for (const auto& href : rule.urls) {
            if (href == url)
                return PrefetchRule { rule.tags, rule.referrerPolicy, rule.eagerness == SpeculationRules::Eagerness::Conservative };
        }

        if (rule.predicate && matches(rule.predicate.value(), document, anchor))
            return PrefetchRule { rule.tags, rule.referrerPolicy, rule.eagerness == SpeculationRules::Eagerness::Conservative || rule.eagerness == SpeculationRules::Eagerness::Moderate };
    }

    return std::nullopt;
}

} // namespace WebCore
