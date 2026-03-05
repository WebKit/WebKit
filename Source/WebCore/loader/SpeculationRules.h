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

#pragma once

#include <WebCore/EventTarget.h>
#include <wtf/Box.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Node;
enum class ReferrerPolicy : uint8_t;

// https://wicg.github.io/nav-speculation/speculation-rules.html
class SpeculationRules : public RefCounted<SpeculationRules> {
public:
    // https://wicg.github.io/nav-speculation/speculation-rules.html#valid-eagerness-strings
    enum class Eagerness : uint8_t {
        Immediate,
        Eager,
        Moderate,
        Conservative,
    };

    // https://wicg.github.io/nav-speculation/speculation-rules.html#document-rule-predicate
    struct DocumentPredicate;

    struct URLPatternPredicate {
        Vector<String> patterns;
    };

    struct CSSSelectorPredicate {
        Vector<String> selectors;
    };

    struct Conjunction {
        Vector<DocumentPredicate> clauses;
    };

    struct Disjunction {
        Vector<DocumentPredicate> clauses;
    };

    struct Negation {
        Box<DocumentPredicate> clause;
    };

    struct DocumentPredicate {
        WTF_MAKE_TZONE_ALLOCATED(DocumentPredicate);
    public:
        using PredicateVariant = WTF::Variant<URLPatternPredicate, CSSSelectorPredicate, Box<Conjunction>, Box<Disjunction>, Box<Negation>>;

        DocumentPredicate(PredicateVariant&& value);
        DocumentPredicate(DocumentPredicate&&) = default;
        DocumentPredicate& operator=(DocumentPredicate&&) = default;

        const PredicateVariant& NODELETE value() const;

    private:
        PredicateVariant m_value;
    };

    // https://wicg.github.io/nav-speculation/speculation-rules.html#speculation-rule
    struct Rule {
        Vector<URL> urls;
        std::optional<DocumentPredicate> predicate;
        Eagerness eagerness;
        std::optional<ReferrerPolicy> referrerPolicy;
        Vector<String> tags;
        Vector<String> requirements;
        String noVarySearchHint;
    };

    static Ref<SpeculationRules> create();

    // https://wicg.github.io/nav-speculation/speculation-rules.html#parse-speculation-rules
    WEBCORE_EXPORT bool parseSpeculationRules(Node& sourceNode, const StringView&, const URL& rulesetBaseURL, const URL& documentBaseURL);

    // https://html.spec.whatwg.org/multipage/webappapis.html#unregister-speculation-rules
    Vector<URL> unregisterSpeculationRules(Node& sourceNode);

    const WeakHashMap<Node, Vector<Rule>, WeakPtrImplWithEventTargetData>& prefetchRules() const { return m_prefetchRulesByNode; }

private:
    SpeculationRules() = default;

    WeakHashMap<Node, Vector<Rule>, WeakPtrImplWithEventTargetData> m_prefetchRulesByNode;
};

} // namespace WebCore
