/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "ClassChangeInvalidation.h"

#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "SpaceSplitString.h"
#include "StyleInvalidationFunctions.h"
#include <wtf/BitVector.h>

namespace WebCore {
namespace Style {

enum class ClassChangeType : bool { Add, Remove };

struct ClassChange {
    AtomStringImpl* className { };
    ClassChangeType type;
};

constexpr size_t classChangeVectorInlineCapacity = 4;
using ClassChangeVector = Vector<ClassChange, classChangeVectorInlineCapacity>;

static ClassChangeVector collectClasses(const SpaceSplitString& classes, ClassChangeType changeType)
{
    return WTF::map<classChangeVectorInlineCapacity>(classes, [changeType](auto& className) {
        return ClassChange { className.impl(), changeType };
    });
}

static ClassChangeVector computeClassChanges(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses)
{
    unsigned oldSize = oldClasses.size();

    if (!oldSize)
        return collectClasses(newClasses, ClassChangeType::Add);
    if (newClasses.isEmpty())
        return collectClasses(oldClasses, ClassChangeType::Remove);

    ClassChangeVector changedClasses;

    BitVector remainingClassBits;
    remainingClassBits.ensureSize(oldSize);
    // Class vectors tend to be very short. This is faster than using a hash table.
    for (auto& newClass : newClasses) {
        bool foundFromBoth = false;
        for (unsigned i = 0; i < oldSize; ++i) {
            if (newClass == oldClasses[i]) {
                remainingClassBits.quickSet(i);
                foundFromBoth = true;
            }
        }
        if (foundFromBoth)
            continue;
        changedClasses.append({ newClass.impl(), ClassChangeType::Add });
    }
    for (unsigned i = 0; i < oldSize; ++i) {
        // If the bit is not set the corresponding class has been removed.
        if (remainingClassBits.quickGet(i))
            continue;
        changedClasses.append({ oldClasses[i].impl(), ClassChangeType::Remove });
    }

    return changedClasses;
}

struct ClassPrefixChange {
    AtomString prefix;
    ClassChangeType type;
};

using ClassPrefixChangeVector = Vector<ClassPrefixChange, classChangeVectorInlineCapacity>;

// A class prefix selector (".foo-*") can only change state when a class it matches is added or removed.
// The prefixes a class name can match are its leading substrings that end in a hyphen and are followed by
// a non-hyphen character ("foo-bar-baz" matches "foo-" and "foo-bar-"), so enumerate those for each changed
// class and keep the ones whose match state differs between the old and new class lists.
static ClassPrefixChangeVector computeClassPrefixChanges(const ClassChangeVector& classChanges, const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses)
{
    ClassPrefixChangeVector prefixChanges;
    Vector<AtomStringImpl*, classChangeVectorInlineCapacity> checkedPrefixes;

    for (auto& classChange : classChanges) {
        StringView className { classChange.className };
        for (unsigned i = 0; i + 1 < className.length(); ++i) {
            if (className[i] != '-' || className[i + 1] == '-')
                continue;

            auto prefixCharacters = className.left(i + 1);
            // A prefix that isn't an atom can't be the value of any selector.
            auto prefix = prefixCharacters.toExistingAtomString();
            if (prefix.isNull() || checkedPrefixes.contains(prefix.impl()))
                continue;
            checkedPrefixes.append(prefix.impl());

            ASSERT(CSSSelector::classNameMatchesPrefix(className, prefixCharacters));
            bool oldMatches = CSSSelector::hasClassNameMatchingPrefix(oldClasses, prefixCharacters);
            bool newMatches = CSSSelector::hasClassNameMatchingPrefix(newClasses, prefixCharacters);
            if (oldMatches != newMatches)
                prefixChanges.append({ WTF::move(prefix), newMatches ? ClassChangeType::Add : ClassChangeType::Remove });
        }
    }

    return prefixChanges;
}

void ClassChangeInvalidation::computeInvalidation(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses)
{
    auto classChanges = computeClassChanges(oldClasses, newClasses);

    std::optional<ClassPrefixChangeVector> classPrefixChanges;
    auto classPrefixChangesIfNeeded = [&](const RuleFeatureSet& features) -> std::span<const ClassPrefixChange> {
        if (features.classPrefixRules.isEmpty())
            return { };
        if (!classPrefixChanges)
            classPrefixChanges = computeClassPrefixChanges(classChanges, oldClasses, newClasses);
        return classPrefixChanges->span();
    };

    bool shouldInvalidateCurrent = false;
    bool mayAffectStyleInShadowTree = false;

    traverseRuleFeatures(m_element, [&] (const RuleFeatureSet& features, bool mayAffectShadowTree) {
        for (auto& classChange : classChanges) {
            if (mayAffectShadowTree && features.classRules.contains(classChange.className))
                mayAffectStyleInShadowTree = true;
            if (features.classesAffectingHost.contains(classChange.className))
                shouldInvalidateCurrent = true;
        }
        for (auto& prefixChange : classPrefixChangesIfNeeded(features)) {
            if (mayAffectShadowTree && features.classPrefixRules.contains(prefixChange.prefix))
                mayAffectStyleInShadowTree = true;
            if (features.classPrefixesAffectingHost.contains(prefixChange.prefix))
                shouldInvalidateCurrent = true;
        }
    });

    if (mayAffectStyleInShadowTree) {
        // FIXME: We should do fine-grained invalidation for shadow tree.
        m_element->invalidateStyleForSubtree();
    }

    if (shouldInvalidateCurrent)
        m_element->invalidateStyle();

    auto invalidateBeforeAndAfterChange = [](const InvalidationRuleSet& ruleSet) {
        switch (ruleSet.matchElement.relation) {
        case MatchElement::Relation::AnySibling:
        case MatchElement::Relation::ParentAnySibling:
        case MatchElement::Relation::AncestorAnySibling:
            return true;
        case MatchElement::Relation::Subject:
        case MatchElement::Relation::Parent:
        case MatchElement::Relation::Ancestor:
        case MatchElement::Relation::DirectSibling:
        case MatchElement::Relation::IndirectSibling:
        case MatchElement::Relation::ParentSibling:
        case MatchElement::Relation::AncestorSibling:
        case MatchElement::Relation::Host:
        case MatchElement::Relation::HostChild:
            return false;
        }
        ASSERT_NOT_REACHED();
        return false;
    };

    auto invalidateBeforeChange = [&](ClassChangeType type, IsNegation isNegation, const InvalidationRuleSet& ruleSet) {
        if (invalidateBeforeAndAfterChange(ruleSet))
            return true;
        return type == ClassChangeType::Remove ? isNegation == IsNegation::No : isNegation == IsNegation::Yes;
    };

    auto invalidateAfterChange = [&](ClassChangeType type, IsNegation isNegation, const InvalidationRuleSet& ruleSet) {
        if (invalidateBeforeAndAfterChange(ruleSet))
            return true;
        return type == ClassChangeType::Add ? isNegation == IsNegation::No : isNegation == IsNegation::Yes;
    };

    auto collect = [&](auto& ruleSets, std::optional<MatchElement::Relation> onlyRelation = { }) {
        auto collectInvalidationRuleSets = [&](const Vector<InvalidationRuleSet>* invalidationRuleSets, ClassChangeType type) {
            if (!invalidationRuleSets)
                return;
            for (auto& invalidationRuleSet : *invalidationRuleSets) {
                if (onlyRelation && invalidationRuleSet.matchElement.relation != onlyRelation)
                    continue;

                if (invalidateBeforeChange(type, invalidationRuleSet.isNegation, invalidationRuleSet))
                    Invalidator::addToMatchElementRuleSets(m_beforeChangeRuleSets, invalidationRuleSet);
                if (invalidateAfterChange(type, invalidationRuleSet.isNegation, invalidationRuleSet))
                    Invalidator::addToMatchElementRuleSets(m_afterChangeRuleSets, invalidationRuleSet);
            }
        };

        for (auto& classChange : classChanges)
            collectInvalidationRuleSets(ruleSets.classInvalidationRuleSets(classChange.className), classChange.type);

        for (auto& prefixChange : classPrefixChangesIfNeeded(ruleSets.features()))
            collectInvalidationRuleSets(ruleSets.classPrefixInvalidationRuleSets(prefixChange.prefix), prefixChange.type);
    };

    collect(m_element->styleResolver().ruleSets());

    if (RefPtr shadowRoot = m_element->shadowRoot())
        collect(shadowRoot->styleScope().resolver().ruleSets(), MatchElement::Relation::Host);
}

void ClassChangeInvalidation::invalidateBeforeChange()
{
    Invalidator::invalidateWithMatchElementRuleSets(m_element, m_beforeChangeRuleSets);
}

void ClassChangeInvalidation::invalidateAfterChange()
{
    Invalidator::invalidateWithMatchElementRuleSets(m_element, m_afterChangeRuleSets);
}

}
}
