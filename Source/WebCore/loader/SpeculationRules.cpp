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
#include "SpeculationRules.h"

#include "Node.h"
#include "ReferrerPolicy.h"
#include <algorithm>
#include <wtf/HashSet.h>
#include <wtf/JSONValues.h>
#include <wtf/URL.h>
#include <wtf/URLHash.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

Ref<SpeculationRules> SpeculationRules::create()
{
    return adoptRef(*new SpeculationRules);
}

SpeculationRules::DocumentPredicate::DocumentPredicate(PredicateVariant&& value)
    : m_value(WTF::move(value))
{
}

const SpeculationRules::DocumentPredicate::PredicateVariant& SpeculationRules::DocumentPredicate::value() const
{
    return m_value;
}

static std::optional<Vector<String>> parseStringOrStringList(JSON::Object& object, const String& key)
{
    Vector<String> result;
    auto value = object.getValue(key);
    if (!value)
        return Vector<String> { };

    if (value->type() == JSON::Value::Type::String) {
        String stringValue = value->asString();
        if (!stringValue.isNull()) {
            result.append(stringValue);
            return result;
        }
    }

    if (RefPtr arrayValue = value->asArray()) {
        for (auto& item : *arrayValue) {
            if (item->type() == JSON::Value::Type::String) {
                String element = item->asString();
                if (element.isNull())
                    return std::nullopt;
                result.append(element);
            } else
                return std::nullopt;
        }
        return result;
    }

    return Vector<String> { };
}

static std::optional<SpeculationRules::DocumentPredicate> parseDocumentPredicate(JSON::Object&);

// https://html.spec.whatwg.org/C#document-rule-predicate
static std::optional<SpeculationRules::DocumentPredicate> parseDocumentPredicate(JSON::Object& object)
{
    auto andValue = object.getValue("and"_s);
    if (andValue && andValue->type() == JSON::Value::Type::Array) {
        if (auto array = andValue->asArray()) {
            SpeculationRules::Conjunction conjunction;
            for (auto& item : *array) {
                auto clauseObject = item->asObject();
                if (!clauseObject)
                    return std::nullopt;
                auto predicate = parseDocumentPredicate(*clauseObject);
                if (!predicate)
                    return std::nullopt;
                conjunction.clauses.append(WTF::move(*predicate));
            }
            return { { Box<SpeculationRules::Conjunction>::create(WTF::move(conjunction)) } };
        }
    }

    auto orValue = object.getValue("or"_s);
    if (orValue && orValue->type() == JSON::Value::Type::Array) {
        if (auto array = orValue->asArray()) {
            SpeculationRules::Disjunction disjunction;
            for (auto& item : *array) {
                auto clauseObject = item->asObject();
                if (!clauseObject)
                    return std::nullopt;
                auto predicate = parseDocumentPredicate(*clauseObject);
                if (!predicate)
                    return std::nullopt;
                disjunction.clauses.append(WTF::move(*predicate));
            }
            return { { Box<SpeculationRules::Disjunction>::create(WTF::move(disjunction)) } };
        }
    }

    auto notValue = object.getValue("not"_s);
    if (notValue && notValue->type() == JSON::Value::Type::Object) {
        if (auto clauseObject = notValue->asObject()) {
            auto predicate = parseDocumentPredicate(*clauseObject);
            if (!predicate)
                return std::nullopt;
            SpeculationRules::Negation negation { Box<SpeculationRules::DocumentPredicate>::create(WTF::move(*predicate)) };
            return { { Box<SpeculationRules::Negation>::create(WTF::move(negation)) } };
        }
    }

    SpeculationRules::URLPatternPredicate urlPredicate;
    auto urlMatches = parseStringOrStringList(object, "url_matches"_s);
    if (urlMatches)
        urlPredicate.patterns.appendVector(*urlMatches);
    auto hrefMatches = parseStringOrStringList(object, "href_matches"_s);
    if (hrefMatches)
        urlPredicate.patterns.appendVector(*hrefMatches);

    SpeculationRules::CSSSelectorPredicate selectorPredicate;
    auto selectorMatches = parseStringOrStringList(object, "selector_matches"_s);
    if (selectorMatches)
        selectorPredicate.selectors.appendVector(*selectorMatches);

    bool hasURLPredicate = !urlPredicate.patterns.isEmpty();
    bool hasSelectorPredicate = !selectorPredicate.selectors.isEmpty();

    if (hasURLPredicate && hasSelectorPredicate) {
        SpeculationRules::Conjunction conjunction;
        conjunction.clauses.append(SpeculationRules::DocumentPredicate { WTF::move(urlPredicate) });
        conjunction.clauses.append(SpeculationRules::DocumentPredicate { WTF::move(selectorPredicate) });
        return { { Box<SpeculationRules::Conjunction>::create(WTF::move(conjunction)) } };
    }

    if (hasURLPredicate)
        return { { WTF::move(urlPredicate) } };

    if (hasSelectorPredicate)
        return { { WTF::move(selectorPredicate) } };

    return std::nullopt;
}

// https://html.spec.whatwg.org/C#parse-a-speculation-rule
static std::optional<SpeculationRules::Rule> parseSingleRule(const JSON::Object& input, const String& rulesetLevelTag, const URL& rulesetBaseURL, const URL& documentBaseURL)
{
    const HashSet<String> allowedKeys = {
        "source"_s, "urls"_s, "where"_s, "requires"_s, "target_hint"_s,
        "referrer_policy"_s, "relative_to"_s, "eagerness"_s,
        "expects_no_vary_search"_s, "tag"_s
    };
    for (const auto& key : input.keys()) {
        if (!allowedKeys.contains(key))
            return std::nullopt;
    }

    String source;
    auto sourceValue = input.getValue("source"_s);
    if (sourceValue && sourceValue->type() == JSON::Value::Type::String)
        source = sourceValue->asString();

    if (source.isEmpty()) {
        bool hasURLs = !!input.getValue("urls"_s);
        bool hasWhere = !!input.getValue("where"_s);
        if (hasURLs && !hasWhere)
            source = "list"_s;
        else if (hasWhere && !hasURLs)
            source = "document"_s;
        else
            return std::nullopt;
    }

    SpeculationRules::Rule rule;

    if (source == "list"_s) {
        if (input.getValue("where"_s))
            return std::nullopt;

        auto urlsValue = input.getValue("urls"_s);
        if (!urlsValue || urlsValue->type() != JSON::Value::Type::Array)
            return std::nullopt;
        auto urlsArray = urlsValue->asArray();
        if (!urlsArray)
            return std::nullopt;

        URL currentBaseURL = rulesetBaseURL;
        auto relativeToValue = input.getValue("relative_to"_s);
        if (relativeToValue && relativeToValue->type() == JSON::Value::Type::String) {
            String relativeTo = relativeToValue->asString();
            if (relativeTo != "ruleset"_s && relativeTo != "document"_s)
                return std::nullopt;
            if (relativeTo == "document"_s)
                currentBaseURL = documentBaseURL;
        }

        for (const auto& urlValue : *urlsArray) {
            if (urlValue->type() != JSON::Value::Type::String)
                return std::nullopt;
            String urlString = urlValue->asString();
            URL parsedURL(currentBaseURL, urlString);
            if (parsedURL.isValid() && (parsedURL.protocolIs("http"_s) || parsedURL.protocolIs("https"_s)))
                rule.urls.append(parsedURL);
        }
        rule.eagerness = SpeculationRules::Eagerness::Immediate;

    } else if (source == "document"_s) {
        if (input.getValue("urls"_s) || input.getValue("relative_to"_s))
            return std::nullopt;

        auto whereValue = input.getValue("where"_s);
        if (whereValue && whereValue->type() == JSON::Value::Type::Object) {
            if (auto whereObject = whereValue->asObject()) {
                auto predicate = parseDocumentPredicate(*whereObject);
                if (!predicate)
                    return std::nullopt;
                rule.predicate = WTF::move(*predicate);
            }
        } else {
            // No "where" means match all links, which is an empty conjunction.
            rule.predicate = { { Box<SpeculationRules::Conjunction>::create() } };
        }
        rule.eagerness = SpeculationRules::Eagerness::Conservative;
    } else
        return std::nullopt;

    auto requiresValue = input.getValue("requires"_s);
    if (requiresValue && requiresValue->type() == JSON::Value::Type::Array) {
        if (auto requiresArray = requiresValue->asArray()) {
            for (const auto& reqValue : *requiresArray) {
                if (reqValue->type() != JSON::Value::Type::String)
                    return std::nullopt;
                String requirement = reqValue->asString();
                if (requirement != "anonymous-client-ip-when-cross-origin"_s)
                    return std::nullopt;
                rule.requirements.append(requirement);
            }
        }
    }

    auto referrerPolicyValue = input.getValue("referrer_policy"_s);
    if (referrerPolicyValue && referrerPolicyValue->type() == JSON::Value::Type::String) {
        String referrerPolicyString = referrerPolicyValue->asString();
        if (!referrerPolicyString.isEmpty()) {
            rule.referrerPolicy = parseReferrerPolicy(referrerPolicyString, ReferrerPolicySource::SpeculationRules);
            // 15.1. If input["referrer_policy"] is not a referrer policy... Return null.
            if (!rule.referrerPolicy)
                return std::nullopt;
        }
    }

    auto eagernessValue = input.getValue("eagerness"_s);
    if (eagernessValue && eagernessValue->type() == JSON::Value::Type::String) {
        String eagernessString = eagernessValue->asString();
        if (eagernessString == "immediate"_s)
            rule.eagerness = SpeculationRules::Eagerness::Immediate;
        else if (eagernessString == "eager"_s)
            rule.eagerness = SpeculationRules::Eagerness::Eager;
        else if (eagernessString == "moderate"_s)
            rule.eagerness = SpeculationRules::Eagerness::Moderate;
        else if (eagernessString == "conservative"_s)
            rule.eagerness = SpeculationRules::Eagerness::Conservative;
        else
            return std::nullopt;
    }

    auto noVarySearchValue = input.getValue("expects_no_vary_search"_s);
    if (noVarySearchValue && noVarySearchValue->type() == JSON::Value::Type::String)
        rule.noVarySearchHint = noVarySearchValue->asString();

    if (!rulesetLevelTag.isNull())
        rule.tags.append(rulesetLevelTag);

    // 18. If input["tag"] exists:
    auto tagValue = input.getValue("tag"_s);
    if (tagValue) {
        // 18.1. If input["tag"] is not a speculation rule tag... return null.
        if (tagValue->type() != JSON::Value::Type::String)
            return std::nullopt;
        String ruleTag = tagValue->asString();
        if (!ruleTag.containsOnlyASCII() || !ruleTag.containsOnly<isASCIIPrintable>())
            return std::nullopt;
        StringBuilder ruleTagBuilder;
        ruleTagBuilder.appendQuotedJSONString(ruleTag);
        String processedRuleTag = ruleTagBuilder.toString();
        // 18.2 Append input["tag"] to tags if it's different from ruleset tag.
        if (processedRuleTag != rulesetLevelTag)
            rule.tags.append(processedRuleTag);
    }

    // 19. If tags is empty, then append null to tags.
    if (rule.tags.isEmpty())
        rule.tags.append(nullAtom());

    return rule;
}

// https://html.spec.whatwg.org/C#parse-a-speculation-rule-set-string step 8
static std::optional<Vector<SpeculationRules::Rule>> parseRules(const JSON::Object& object, const String& key, const String& rulesetLevelTag, const URL& rulesetBaseURL, const URL& documentBaseURL)
{
    auto value = object.getValue(key);
    // 8.1 If parsed[type] exists:
    if (!value || value->type() != JSON::Value::Type::Array)
        return Vector<SpeculationRules::Rule> { };
    auto array = value->asArray();
    if (!array)
        return Vector<SpeculationRules::Rule> { };

    Vector<SpeculationRules::Rule> rules;
    for (auto& item : *array) {
        // 8.1.1 If parsed[type] is a list, then for each rule of parsed[type]:
        auto ruleObject = item->asObject();
        if (!ruleObject)
            return std::nullopt;
        // 8.1.1.1 Let rule be the result of parsing a speculation rule given rule, tag, document, and baseURL.
        if (auto rule = parseSingleRule(*ruleObject, rulesetLevelTag, rulesetBaseURL, documentBaseURL))
            rules.append(WTF::move(*rule));
        // 8.1.1.2. If rule is null, then continue.
    }
    return rules;
}

// https://html.spec.whatwg.org/C#parse-a-speculation-rule-set-string
bool SpeculationRules::parseSpeculationRules(Node& sourceNode, const StringView& text, const URL& rulesetBaseURL, const URL& documentBaseURL)
{
    // 1. Let parsed be the result of parsing a JSON string to an Infra value given input.
    auto jsonValue = JSON::Value::parseJSON(text);
    if (!jsonValue)
        return false;

    // 2. If parsed is not a map, then throw a TypeError indicating that the top-level value needs to be a JSON object.
    auto jsonObject = jsonValue->asObject();
    if (!jsonObject)
        return false;

    String rulesetLevelTag;
    auto tagValue = jsonObject->getValue("tag"_s);
    // 5. If parsed["tag"] exists:
    if (tagValue) {
        // 5.1. If parsed["tag"] is not a speculation rule tag, then throw a TypeError indicating that the speculation rule tag is invalid.
        if (tagValue->type() != JSON::Value::Type::String)
            return false;
        String candidateTag = tagValue->asString();
        if (!candidateTag.containsOnlyASCII() || !candidateTag.containsOnly<isASCIIPrintable>())
            return false;
        StringBuilder ruleTagBuilder;
        ruleTagBuilder.appendQuotedJSONString(candidateTag);
        // 5.2. Set tag to parsed["tag"].
        rulesetLevelTag = ruleTagBuilder.toString();
    }

    auto prefetch = parseRules(*jsonObject, "prefetch"_s, rulesetLevelTag, rulesetBaseURL, documentBaseURL);
    if (prefetch) {
        auto& nodeRules = m_prefetchRulesByNode.ensure(sourceNode, [] {
            return Vector<Rule>();
        }).iterator->value;
        nodeRules.appendVector(WTF::move(*prefetch));
    }
    return true;
}

// https://html.spec.whatwg.org/C#unregister-speculation-rules
Vector<URL> SpeculationRules::unregisterSpeculationRules(Node& sourceNode)
{
    Vector<URL> removedURLs;
    auto it = m_prefetchRulesByNode.find(sourceNode);
    if (it != m_prefetchRulesByNode.end()) {
        for (const auto& rule : it->value) {
            for (const auto& url : rule.urls)
                removedURLs.append(url);
        }
        m_prefetchRulesByNode.remove(it);
    }
    return removedURLs;
}

} // namespace WebCore
