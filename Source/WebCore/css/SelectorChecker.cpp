/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2014 Yusuke Suzuki <utatane.tea@gmail.com>
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

#include "config.h"
#include "SelectorChecker.h"

#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CommonAtomStrings.h"
#include "Document.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "ElementTraversal.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLSlotElement.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "RenderElement.h"
#include "RuleFeature.h"
#include "SelectorCheckerTestFunctions.h"
#include "ShadowRoot.h"
#include "StyleRule.h"
#include "StyleScope.h"
#include "Text.h"
#include "TypedElementDescendantIterator.h"

namespace WebCore {

using namespace HTMLNames;

enum class VisitedMatchType : unsigned char {
    Disabled, Enabled
};

struct SelectorChecker::LocalContext {
    LocalContext(const CSSSelector& selector, const Element& element, VisitedMatchType visitedMatchType, PseudoId pseudoId)
        : selector(&selector)
        , element(&element)
        , visitedMatchType(visitedMatchType)
        , firstSelectorOfTheFragment(&selector)
        , pseudoId(pseudoId)
    { }

    const CSSSelector* selector;
    const Element* element;
    VisitedMatchType visitedMatchType;
    const CSSSelector* firstSelectorOfTheFragment;
    PseudoId pseudoId;
    bool isMatchElement { true };
    bool isSubjectOrAdjacentElement { true };
    bool inFunctionalPseudoClass { false };
    bool pseudoElementEffective { true };
    bool hasScrollbarPseudo { false };
    bool hasSelectionPseudo { false };
    bool mustMatchHostPseudoClass { false };
};

static inline void addStyleRelation(SelectorChecker::CheckingContext& checkingContext, const Element& element, Style::Relation::Type type, unsigned value = 1)
{
    ASSERT(value == 1 || type == Style::Relation::NthChildIndex || type == Style::Relation::AffectedByEmpty);
    if (checkingContext.resolvingMode != SelectorChecker::Mode::ResolvingStyle)
        return;
    if (type == Style::Relation::AffectsNextSibling && !checkingContext.styleRelations.isEmpty()) {
        auto& last = checkingContext.styleRelations.last();
        if (last.type == Style::Relation::AffectsNextSibling && last.element == element.nextElementSibling()) {
            ++last.value;
            last.element = &element;
            return;
        }
    }
    checkingContext.styleRelations.append({ element, type, value });
}

static inline bool isFirstChildElement(const Element& element)
{
    return !ElementTraversal::previousSibling(element);
}

static inline bool isLastChildElement(const Element& element)
{
    return !ElementTraversal::nextSibling(element);
}

static inline bool isFirstOfType(const Element& element, const QualifiedName& type)
{
    for (const Element* sibling = ElementTraversal::previousSibling(element); sibling; sibling = ElementTraversal::previousSibling(*sibling)) {
        if (sibling->hasTagName(type))
            return false;
    }
    return true;
}

static inline bool isLastOfType(const Element& element, const QualifiedName& type)
{
    for (const Element* sibling = ElementTraversal::nextSibling(element); sibling; sibling = ElementTraversal::nextSibling(*sibling)) {
        if (sibling->hasTagName(type))
            return false;
    }
    return true;
}

static inline int countElementsBefore(const Element& element)
{
    int count = 0;
    for (const Element* sibling = ElementTraversal::previousSibling(element); sibling; sibling = ElementTraversal::previousSibling(*sibling)) {
        unsigned index = sibling->childIndex();
        if (index) {
            count += index;
            break;
        }
        count++;
    }
    return count;
}

static inline int countElementsOfTypeBefore(const Element& element, const QualifiedName& type)
{
    int count = 0;
    for (const Element* sibling = ElementTraversal::previousSibling(element); sibling; sibling = ElementTraversal::previousSibling(*sibling)) {
        if (sibling->hasTagName(type))
            ++count;
    }
    return count;
}

static inline int countElementsAfter(const Element& element)
{
    int count = 0;
    for (const Element* sibling = ElementTraversal::nextSibling(element); sibling; sibling = ElementTraversal::nextSibling(*sibling))
        ++count;
    return count;
}

static inline int countElementsOfTypeAfter(const Element& element, const QualifiedName& type)
{
    int count = 0;
    for (const Element* sibling = ElementTraversal::nextSibling(element); sibling; sibling = ElementTraversal::nextSibling(*sibling)) {
        if (sibling->hasTagName(type))
            ++count;
    }
    return count;
}

SelectorChecker::SelectorChecker(Document& document)
    : m_strictParsing(!document.inQuirksMode())
    , m_documentIsHTML(document.isHTMLDocument())
{
}

bool SelectorChecker::match(const CSSSelector& selector, const Element& element, CheckingContext& checkingContext) const
{
    LocalContext context(selector, element, checkingContext.resolvingMode == SelectorChecker::Mode::QueryingRules ? VisitedMatchType::Disabled : VisitedMatchType::Enabled, checkingContext.pseudoId);

    if (checkingContext.styleScopeOrdinal == Style::ScopeOrdinal::Shadow) {
        ASSERT(element.shadowRoot());
        // Rules coming from the element's shadow tree must match :host pseudo class.
        context.mustMatchHostPseudoClass = true;
    }

    PseudoIdSet pseudoIdSet;
    MatchResult result = matchRecursively(checkingContext, context, pseudoIdSet);
    if (result.match != Match::SelectorMatches)
        return false;
    if (checkingContext.pseudoId != PseudoId::None && !pseudoIdSet.has(checkingContext.pseudoId))
        return false;

    if (checkingContext.pseudoId == PseudoId::None && pseudoIdSet) {
        PseudoIdSet publicPseudoIdSet = pseudoIdSet & PseudoIdSet::fromMask(static_cast<unsigned>(PseudoId::PublicPseudoIdMask));
        if (checkingContext.resolvingMode == Mode::ResolvingStyle && publicPseudoIdSet)
            checkingContext.pseudoIDSet = publicPseudoIdSet;

        // When ignoring virtual pseudo elements, the context's pseudo should also be PseudoId::None but that does
        // not cause a failure.
        return checkingContext.resolvingMode == Mode::CollectingRulesIgnoringVirtualPseudoElements || result.matchType == MatchType::Element;
    }
    return true;
}

bool SelectorChecker::matchHostPseudoClass(const CSSSelector& selector, const Element& element, CheckingContext& checkingContext) const
{
    if (!element.shadowRoot())
        return false;

    if (auto* selectorList = selector.selectorList()) {
        LocalContext context(*selectorList->first(), element, VisitedMatchType::Enabled, PseudoId::None);
        context.inFunctionalPseudoClass = true;
        context.pseudoElementEffective = false;
        PseudoIdSet ignoreDynamicPseudo;
        if (matchRecursively(checkingContext, context, ignoreDynamicPseudo).match != Match::SelectorMatches)
            return false;
    }
    return true;
}

inline static bool hasScrollbarPseudoElement(const PseudoIdSet& dynamicPseudoIdSet)
{
    PseudoIdSet scrollbarIdSet = { PseudoId::Scrollbar, PseudoId::ScrollbarThumb, PseudoId::ScrollbarButton, PseudoId::ScrollbarTrack, PseudoId::ScrollbarTrackPiece, PseudoId::ScrollbarCorner };
    if (dynamicPseudoIdSet & scrollbarIdSet)
        return true;

    // PseudoId::Resizer does not always have a scrollbar but it is a scrollbar-like pseudo element
    // because it can have more than one pseudo element.
    return dynamicPseudoIdSet.has(PseudoId::Resizer);
}

static SelectorChecker::LocalContext localContextForParent(const SelectorChecker::LocalContext& context)
{
    SelectorChecker::LocalContext updatedContext(context);
    // Disable :visited matching when we see the first link.
    if (context.element->isLink())
        updatedContext.visitedMatchType = VisitedMatchType::Disabled;

    updatedContext.isMatchElement = false;
    updatedContext.isSubjectOrAdjacentElement = false;

    if (updatedContext.mustMatchHostPseudoClass) {
        updatedContext.element = nullptr;
        return updatedContext;
    }

    // Move to the shadow host if matching :host and the parent is the shadow root.
    if (context.selector->match() == CSSSelector::PseudoClass && context.selector->pseudoClassType() == CSSSelector::PseudoClassHost && is<ShadowRoot>(context.element->parentNode())) {
        updatedContext.element = downcast<ShadowRoot>(*context.element->parentNode()).host();
        updatedContext.mustMatchHostPseudoClass = true;
        return updatedContext;
    }

    updatedContext.element = context.element->parentElement();
    return updatedContext;
}

// Recursive check of selectors and combinators
// It can return 4 different values:
// * SelectorMatches          - the selector matches the element e
// * SelectorFailsLocally     - the selector fails for the element e
// * SelectorFailsAllSiblings - the selector fails for e and any sibling of e
// * SelectorFailsCompletely  - the selector fails for e and any sibling or ancestor of e
SelectorChecker::MatchResult SelectorChecker::matchRecursively(CheckingContext& checkingContext, const LocalContext& context, PseudoIdSet& dynamicPseudoIdSet) const
{
    MatchType matchType = MatchType::Element;

    // The first selector has to match.
    if (!checkOne(checkingContext, context, matchType))
        return MatchResult::fails(Match::SelectorFailsLocally);

    if (context.selector->match() == CSSSelector::PseudoElement) {
        if (context.selector->isCustomPseudoElement()) {
            // In functional pseudo class, custom pseudo elements are always disabled.
            // FIXME: We should accept custom pseudo elements inside :is()/:matches().
            if (context.inFunctionalPseudoClass)
                return MatchResult::fails(Match::SelectorFailsCompletely);
            if (ShadowRoot* root = context.element->containingShadowRoot()) {
                if (context.element->shadowPseudoId() != context.selector->value())
                    return MatchResult::fails(Match::SelectorFailsLocally);

                if (context.selector->isWebKitCustomPseudoElement() && root->mode() != ShadowRootMode::UserAgent)
                    return MatchResult::fails(Match::SelectorFailsLocally);
            } else
                return MatchResult::fails(Match::SelectorFailsLocally);
        } else {
            if (!context.pseudoElementEffective)
                return MatchResult::fails(Match::SelectorFailsCompletely);

            if (checkingContext.resolvingMode == Mode::QueryingRules)
                return MatchResult::fails(Match::SelectorFailsCompletely);

            PseudoId pseudoId = CSSSelector::pseudoId(context.selector->pseudoElementType());
            if (pseudoId != PseudoId::None)
                dynamicPseudoIdSet.add(pseudoId);
            matchType = MatchType::VirtualPseudoElementOnly;
        }
    }

    // The rest of the selectors has to match
    auto relation = context.selector->relation();

    // Prepare next selector
    const CSSSelector* leftSelector = context.selector->tagHistory();
    if (!leftSelector)
        return MatchResult::matches(matchType);

    LocalContext nextContext(context);
    nextContext.selector = leftSelector;

    if (relation != CSSSelector::Subselector) {
        // Bail-out if this selector is irrelevant for the pseudoId
        if (context.pseudoId != PseudoId::None && !dynamicPseudoIdSet.has(context.pseudoId))
            return MatchResult::fails(Match::SelectorFailsCompletely);

        // Disable :visited matching when we try to match anything else than an ancestors.
        if (!context.selector->hasDescendantOrChildRelation())
            nextContext.visitedMatchType = VisitedMatchType::Disabled;

        nextContext.pseudoId = PseudoId::None;

        bool allowMultiplePseudoElements = relation == CSSSelector::ShadowDescendant;
        // Virtual pseudo element is only effective in the rightmost fragment.
        if (!allowMultiplePseudoElements)
            nextContext.pseudoElementEffective = false;

        nextContext.isMatchElement = false;
    }

    switch (relation) {
    case CSSSelector::DescendantSpace:
        nextContext = localContextForParent(nextContext);
        nextContext.firstSelectorOfTheFragment = nextContext.selector;
        for (; nextContext.element; nextContext = localContextForParent(nextContext)) {
            PseudoIdSet ignoreDynamicPseudo;
            MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo);
            ASSERT(!nextContext.pseudoElementEffective && !ignoreDynamicPseudo);

            if (result.match == Match::SelectorMatches || result.match == Match::SelectorFailsCompletely)
                return MatchResult::updateWithMatchType(result, matchType);
        }
        return MatchResult::fails(Match::SelectorFailsCompletely);

    case CSSSelector::Child:
        {
            nextContext = localContextForParent(nextContext);
            if (!nextContext.element)
                return MatchResult::fails(Match::SelectorFailsCompletely);
            nextContext.firstSelectorOfTheFragment = nextContext.selector;
            PseudoIdSet ignoreDynamicPseudo;
            MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo);
            ASSERT(!nextContext.pseudoElementEffective && !ignoreDynamicPseudo);

            if (result.match == Match::SelectorMatches || result.match == Match::SelectorFailsCompletely)
                return MatchResult::updateWithMatchType(result, matchType);
            return MatchResult::fails(Match::SelectorFailsAllSiblings);
        }

    case CSSSelector::DirectAdjacent:
        {
            auto relation = context.isMatchElement ? Style::Relation::AffectedByPreviousSibling : Style::Relation::DescendantsAffectedByPreviousSibling;
            addStyleRelation(checkingContext, *context.element, relation);

            Element* previousElement = context.element->previousElementSibling();
            if (!previousElement)
                return MatchResult::fails(Match::SelectorFailsAllSiblings);

            addStyleRelation(checkingContext, *previousElement, Style::Relation::AffectsNextSibling);

            nextContext.element = previousElement;
            nextContext.firstSelectorOfTheFragment = nextContext.selector;
            PseudoIdSet ignoreDynamicPseudo;
            MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo);
            ASSERT(!nextContext.pseudoElementEffective && !ignoreDynamicPseudo);

            return MatchResult::updateWithMatchType(result, matchType);
        }
    case CSSSelector::IndirectAdjacent: {
        auto relation = context.isMatchElement ? Style::Relation::AffectedByPreviousSibling : Style::Relation::DescendantsAffectedByPreviousSibling;
        addStyleRelation(checkingContext, *context.element, relation);

        nextContext.element = context.element->previousElementSibling();
        nextContext.firstSelectorOfTheFragment = nextContext.selector;
        for (; nextContext.element; nextContext.element = nextContext.element->previousElementSibling()) {
            addStyleRelation(checkingContext, *nextContext.element, Style::Relation::AffectsNextSibling);

            PseudoIdSet ignoreDynamicPseudo;
            MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo);
            ASSERT(!nextContext.pseudoElementEffective && !ignoreDynamicPseudo);

            if (result.match == Match::SelectorMatches || result.match == Match::SelectorFailsAllSiblings || result.match == Match::SelectorFailsCompletely)
                return MatchResult::updateWithMatchType(result, matchType);
        };
        return MatchResult::fails(Match::SelectorFailsAllSiblings);
    }
    case CSSSelector::Subselector:
        {
            // a selector is invalid if something follows a pseudo-element
            // We make an exception for scrollbar pseudo elements and allow a set of pseudo classes (but nothing else)
            // to follow the pseudo elements.
            nextContext.hasScrollbarPseudo = hasScrollbarPseudoElement(dynamicPseudoIdSet);
            nextContext.hasSelectionPseudo = dynamicPseudoIdSet.has(PseudoId::Selection);
            if ((context.isMatchElement || checkingContext.resolvingMode == Mode::CollectingRules) && dynamicPseudoIdSet
                && !nextContext.hasSelectionPseudo
                && !(nextContext.hasScrollbarPseudo && nextContext.selector->match() == CSSSelector::PseudoClass))
                return MatchResult::fails(Match::SelectorFailsCompletely);

            MatchResult result = matchRecursively(checkingContext, nextContext, dynamicPseudoIdSet);

            return MatchResult::updateWithMatchType(result, matchType);
        }
    case CSSSelector::ShadowDescendant:  {
        auto* host = context.element->shadowHost();
        if (!host)
            return MatchResult::fails(Match::SelectorFailsCompletely);

        nextContext.element = host;
        nextContext.firstSelectorOfTheFragment = nextContext.selector;
        nextContext.isSubjectOrAdjacentElement = false;
        PseudoIdSet ignoreDynamicPseudo;
        MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo);

        return MatchResult::updateWithMatchType(result, matchType);
    }
    case CSSSelector::ShadowPartDescendant: {
        // Continue matching in the scope where this rule came from.
        auto* host = checkingContext.styleScopeOrdinal == Style::ScopeOrdinal::Element
            ? context.element->shadowHost()
            : Style::hostForScopeOrdinal(*context.element, checkingContext.styleScopeOrdinal);
        if (!host)
            return MatchResult::fails(Match::SelectorFailsCompletely);

        nextContext.element = host;
        nextContext.firstSelectorOfTheFragment = nextContext.selector;
        nextContext.isSubjectOrAdjacentElement = false;
        // ::part rules from the element's own scope can only match if they apply to :host.
        nextContext.mustMatchHostPseudoClass = checkingContext.styleScopeOrdinal == Style::ScopeOrdinal::Element;
        PseudoIdSet ignoreDynamicPseudo;
        MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo);

        return MatchResult::updateWithMatchType(result, matchType);
    }
    case CSSSelector::ShadowSlotted: {
        // We continue matching in the scope where this rule came from.
        auto slot = Style::assignedSlotForScopeOrdinal(*context.element, checkingContext.styleScopeOrdinal);
        if (!slot)
            return MatchResult::fails(Match::SelectorFailsCompletely);

        nextContext.element = slot;
        nextContext.firstSelectorOfTheFragment = nextContext.selector;
        nextContext.isSubjectOrAdjacentElement = false;
        PseudoIdSet ignoreDynamicPseudo;
        MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo);

        return MatchResult::updateWithMatchType(result, matchType);
    }
    }

    ASSERT_NOT_REACHED();
    return MatchResult::fails(Match::SelectorFailsCompletely);
}

static bool attributeValueMatches(const Attribute& attribute, CSSSelector::Match match, const AtomString& selectorValue, bool caseSensitive)
{
    const AtomString& value = attribute.value();
    ASSERT(!value.isNull());

    switch (match) {
    case CSSSelector::Set:
        break;
    case CSSSelector::Exact:
        if (caseSensitive ? selectorValue != value : !equalIgnoringASCIICase(selectorValue, value))
            return false;
        break;
    case CSSSelector::List:
        {
            // Ignore empty selectors or selectors containing spaces.
            if (selectorValue.isEmpty() || selectorValue.find(isHTMLSpace<UChar>) != notFound)
                return false;

            unsigned startSearchAt = 0;
            while (true) {
                size_t foundPos;
                if (caseSensitive)
                    foundPos = value.find(selectorValue, startSearchAt);
                else
                    foundPos = value.findIgnoringASCIICase(selectorValue, startSearchAt);
                if (foundPos == notFound)
                    return false;
                if (!foundPos || isHTMLSpace(value[foundPos - 1])) {
                    unsigned endStr = foundPos + selectorValue.length();
                    if (endStr == value.length() || isHTMLSpace(value[endStr]))
                        break; // We found a match.
                }

                // No match. Keep looking.
                startSearchAt = foundPos + 1;
            }
            break;
        }
    case CSSSelector::Contain: {
        bool valueContainsSelectorValue;
        if (caseSensitive)
            valueContainsSelectorValue = value.contains(selectorValue);
        else
            valueContainsSelectorValue = value.containsIgnoringASCIICase(selectorValue);

        if (!valueContainsSelectorValue || selectorValue.isEmpty())
            return false;

        break;
    }
    case CSSSelector::Begin:
        if (selectorValue.isEmpty())
            return false;
        if (caseSensitive) {
            if (!value.startsWith(selectorValue))
                return false;
        } else {
            if (!value.startsWithIgnoringASCIICase(selectorValue))
                return false;
        }
        break;
    case CSSSelector::End:
        if (selectorValue.isEmpty())
            return false;
        if (caseSensitive) {
            if (!value.endsWith(selectorValue))
                return false;
        } else {
            if (!value.endsWithIgnoringASCIICase(selectorValue))
                return false;
        }
        break;
    case CSSSelector::Hyphen:
        if (value.length() < selectorValue.length())
            return false;
        if (caseSensitive) {
            if (!value.startsWith(selectorValue))
                return false;
        } else {
            if (!value.startsWithIgnoringASCIICase(selectorValue))
                return false;
        }
        // It they start the same, check for exact match or following '-':
        if (value.length() != selectorValue.length() && value[selectorValue.length()] != '-')
            return false;
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

static bool anyAttributeMatches(const Element& element, const CSSSelector& selector, const QualifiedName& selectorAttr, bool caseSensitive)
{
    ASSERT(element.hasAttributesWithoutUpdate());
    for (const Attribute& attribute : element.attributesIterator()) {
        if (!attribute.matches(selectorAttr.prefix(), element.isHTMLElement() ? selector.attributeCanonicalLocalName() : selectorAttr.localName(), selectorAttr.namespaceURI()))
            continue;

        if (attributeValueMatches(attribute, selector.match(), selector.value(), caseSensitive))
            return true;
    }

    return false;
}

bool SelectorChecker::attributeSelectorMatches(const Element& element, const QualifiedName& attributeName, const AtomString& attributeValue, const CSSSelector& selector)
{
    ASSERT(selector.isAttributeSelector());
    auto& selectorAttribute = selector.attribute();
    auto& selectorName = element.isHTMLElement() ? selector.attributeCanonicalLocalName() : selectorAttribute.localName();
    if (!Attribute::nameMatchesFilter(attributeName, selectorAttribute.prefix(), selectorName, selectorAttribute.namespaceURI()))
        return false;
    bool caseSensitive = true;
    if (selector.attributeValueMatchingIsCaseInsensitive())
        caseSensitive = false;
    else if (element.document().isHTMLDocument() && element.isHTMLElement() && !HTMLDocument::isCaseSensitiveAttribute(selector.attribute()))
        caseSensitive = false;
    return attributeValueMatches(Attribute(attributeName, attributeValue), selector.match(), selector.value(), caseSensitive);
}

static bool canMatchHoverOrActiveInQuirksMode(const SelectorChecker::LocalContext& context)
{
    // For quirks mode, follow this: http://quirks.spec.whatwg.org/#the-:active-and-:hover-quirk
    // In quirks mode, a compound selector 'selector' that matches the following conditions must not match elements that would not also match the ':any-link' selector.
    //
    //    selector uses the ':active' or ':hover' pseudo-classes.
    //    selector does not use a type selector.
    //    selector does not use an attribute selector.
    //    selector does not use an ID selector.
    //    selector does not use a class selector.
    //    selector does not use a pseudo-class selector other than ':active' and ':hover'.
    //    selector does not use a pseudo-element selector.
    //    selector is not part of an argument to a functional pseudo-class or pseudo-element.
    if (context.inFunctionalPseudoClass)
        return true;

    for (const CSSSelector* selector = context.firstSelectorOfTheFragment; selector; selector = selector->tagHistory()) {
        switch (selector->match()) {
        case CSSSelector::Tag:
            if (selector->tagQName() != anyQName())
                return true;
            break;
        case CSSSelector::PseudoClass: {
            CSSSelector::PseudoClassType pseudoClassType = selector->pseudoClassType();
            if (pseudoClassType != CSSSelector::PseudoClassHover && pseudoClassType != CSSSelector::PseudoClassActive)
                return true;
            break;
        }
        case CSSSelector::Id:
        case CSSSelector::Class:
        case CSSSelector::Exact:
        case CSSSelector::Set:
        case CSSSelector::List:
        case CSSSelector::Hyphen:
        case CSSSelector::Contain:
        case CSSSelector::Begin:
        case CSSSelector::End:
        case CSSSelector::PagePseudoClass:
        case CSSSelector::PseudoElement:
            return true;
        case CSSSelector::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }

        auto relation = selector->relation();
        if (relation == CSSSelector::ShadowDescendant || relation == CSSSelector::ShadowPartDescendant)
            return true;

        if (relation != CSSSelector::Subselector)
            return false;
    }
    return false;
}

static inline bool tagMatches(const Element& element, const CSSSelector& simpleSelector)
{
    const QualifiedName& tagQName = simpleSelector.tagQName();

    if (tagQName == anyQName())
        return true;

    const AtomString& localName = (element.isHTMLElement() && element.document().isHTMLDocument()) ? simpleSelector.tagLowercaseLocalName() : tagQName.localName();

    if (localName != starAtom() && localName != element.localName())
        return false;
    const AtomString& namespaceURI = tagQName.namespaceURI();
    return namespaceURI == starAtom() || namespaceURI == element.namespaceURI();
}

bool SelectorChecker::checkOne(CheckingContext& checkingContext, const LocalContext& context, MatchType& matchType) const
{
    const Element& element = *context.element;
    const CSSSelector& selector = *context.selector;

    if (context.mustMatchHostPseudoClass) {
        // :host doesn't combine with anything except pseudo elements.
        bool isHostPseudoClass = selector.match() == CSSSelector::PseudoClass && selector.pseudoClassType() == CSSSelector::PseudoClassHost;
        bool isPseudoElement = selector.match() == CSSSelector::PseudoElement;
        if (!isHostPseudoClass && !isPseudoElement)
            return false;
    }

    if (selector.match() == CSSSelector::Tag)
        return tagMatches(element, selector);

    if (selector.match() == CSSSelector::Class)
        return element.hasClass() && element.classNames().contains(selector.value());

    if (selector.match() == CSSSelector::Id) {
        ASSERT(!selector.value().isNull());
        return element.idForStyleResolution() == selector.value();
    }

    if (selector.isAttributeSelector()) {
        if (!element.hasAttributes())
            return false;

        const QualifiedName& attr = selector.attribute();
        bool caseSensitive = true;
        if (selector.attributeValueMatchingIsCaseInsensitive())
            caseSensitive = false;
        else if (m_documentIsHTML && element.isHTMLElement() && !HTMLDocument::isCaseSensitiveAttribute(attr))
            caseSensitive = false;

        return anyAttributeMatches(element, selector, attr, caseSensitive);
    }

    if (selector.match() == CSSSelector::PseudoClass) {
        // Handle :not up front.
        if (selector.pseudoClassType() == CSSSelector::PseudoClassNot) {
            const CSSSelectorList* selectorList = selector.selectorList();

            for (const CSSSelector* subselector = selectorList->first(); subselector; subselector = CSSSelectorList::next(subselector)) {
                LocalContext subcontext(context);
                subcontext.inFunctionalPseudoClass = true;
                subcontext.pseudoElementEffective = false;
                subcontext.selector = subselector;
                subcontext.firstSelectorOfTheFragment = selectorList->first();
                PseudoIdSet ignoreDynamicPseudo;

                if (matchRecursively(checkingContext, subcontext, ignoreDynamicPseudo).match == Match::SelectorMatches) {
                    ASSERT(!ignoreDynamicPseudo);
                    return false;
                }
            }
            return true;
        }
        if (context.hasScrollbarPseudo) {
            // CSS scrollbars match a specific subset of pseudo classes, and they have specialized rules for each
            // (since there are no elements involved except with window-inactive).
            return checkScrollbarPseudoClass(checkingContext, element, selector);
        }

        // Normal element pseudo class checking.
        switch (selector.pseudoClassType()) {
            // Pseudo classes:
        case CSSSelector::PseudoClassNot:
            break; // Already handled up above.
        case CSSSelector::PseudoClassEmpty:
            {
                bool result = true;
                for (Node* node = element.firstChild(); node; node = node->nextSibling()) {
                    if (is<Element>(*node)) {
                        result = false;
                        break;
                    }
                    if (is<Text>(*node)) {
                        Text& textNode = downcast<Text>(*node);
                        if (!textNode.data().isEmpty()) {
                            result = false;
                            break;
                        }
                    }
                }
                addStyleRelation(checkingContext, *context.element, Style::Relation::AffectedByEmpty, result);

                return result;
            }
        case CSSSelector::PseudoClassFirstChild: {
            // first-child matches the first child that is an element
            bool isFirstChild = isFirstChildElement(element);
            auto* parent = element.parentNode();
            if (is<Element>(parent))
                addStyleRelation(checkingContext, downcast<Element>(*parent), Style::Relation::ChildrenAffectedByFirstChildRules);
            else if (!is<ShadowRoot>(parent))
                break; // FIXME: Add the support for specifying relations on ShadowRoot.
            if (!isFirstChild)
                break;
            addStyleRelation(checkingContext, element, Style::Relation::FirstChild);
            return true;
        }
        case CSSSelector::PseudoClassFirstOfType: {
            // first-of-type matches the first element of its type
            auto* parent = element.parentNode();
            if (is<Element>(parent)) {
                auto relation = context.isSubjectOrAdjacentElement ? Style::Relation::ChildrenAffectedByForwardPositionalRules : Style::Relation::DescendantsAffectedByForwardPositionalRules;
                addStyleRelation(checkingContext, downcast<Element>(*parent), relation);
            } else if (!is<ShadowRoot>(parent))
                break; // FIXME: Add the support for specifying relations on ShadowRoot.
            return isFirstOfType(element, element.tagQName());
        }
        case CSSSelector::PseudoClassLastChild: {
            // last-child matches the last child that is an element
            auto* parent = element.parentNode();
            bool isLastChild = isLastChildElement(element);
            if (is<Element>(parent)) {
                auto& parentElement = downcast<Element>(*parent);
                if (!parentElement.isFinishedParsingChildren())
                    isLastChild = false;
                addStyleRelation(checkingContext, parentElement, Style::Relation::ChildrenAffectedByLastChildRules);
            } else if (!is<ShadowRoot>(parent))
                break; // FIXME: Add the support for specifying relations on ShadowRoot.
            if (!isLastChild)
                break;
            addStyleRelation(checkingContext, element, Style::Relation::LastChild);
            return true;
        }
        case CSSSelector::PseudoClassLastOfType: {
            // last-of-type matches the last element of its type
            auto* parent = element.parentNode();
            if (is<Element>(parent)) {
                auto& parentElement = downcast<Element>(*parent);
                auto relation = context.isSubjectOrAdjacentElement ? Style::Relation::ChildrenAffectedByBackwardPositionalRules : Style::Relation::DescendantsAffectedByBackwardPositionalRules;
                addStyleRelation(checkingContext, parentElement, relation);
                if (!parentElement.isFinishedParsingChildren())
                    return false;
            } else if (!is<ShadowRoot>(parent))
                break; // FIXME: Add the support for specifying relations on ShadowRoot.
            return isLastOfType(element, element.tagQName());
        }
        case CSSSelector::PseudoClassOnlyChild: {
            auto* parent = element.parentNode();
            bool firstChild = isFirstChildElement(element);
            bool onlyChild = firstChild && isLastChildElement(element);
            if (is<Element>(parent)) {
                auto& parentElement = downcast<Element>(*parent);
                addStyleRelation(checkingContext, parentElement, Style::Relation::ChildrenAffectedByFirstChildRules);
                addStyleRelation(checkingContext, parentElement, Style::Relation::ChildrenAffectedByLastChildRules);
                if (!parentElement.isFinishedParsingChildren())
                    onlyChild = false;
            } else if (!is<ShadowRoot>(parent))
                break; // FIXME: Add the support for specifying relations on ShadowRoot.
            if (firstChild)
                addStyleRelation(checkingContext, element, Style::Relation::FirstChild);
            if (onlyChild)
                addStyleRelation(checkingContext, element, Style::Relation::LastChild);
            return onlyChild;
        }
        case CSSSelector::PseudoClassOnlyOfType: {
            // FIXME: This selector is very slow.
            auto* parent = element.parentNode();
            if (is<Element>(parent)) {
                auto& parentElement = downcast<Element>(*parent);
                auto forwardRelation = context.isSubjectOrAdjacentElement ? Style::Relation::ChildrenAffectedByForwardPositionalRules : Style::Relation::DescendantsAffectedByForwardPositionalRules;
                addStyleRelation(checkingContext, parentElement, forwardRelation);
                auto backwardRelation = context.isSubjectOrAdjacentElement ? Style::Relation::ChildrenAffectedByBackwardPositionalRules : Style::Relation::DescendantsAffectedByBackwardPositionalRules;
                addStyleRelation(checkingContext, parentElement, backwardRelation);

                if (!parentElement.isFinishedParsingChildren())
                    return false;
            } else if (!is<ShadowRoot>(parent))
                break; // FIXME: Add the support for specifying relations on ShadowRoot.
            return isFirstOfType(element, element.tagQName()) && isLastOfType(element, element.tagQName());
        }
        case CSSSelector::PseudoClassIs:
        case CSSSelector::PseudoClassWhere:
        case CSSSelector::PseudoClassMatches:
        case CSSSelector::PseudoClassAny:
            {
                bool hasMatchedAnything = false;

                MatchType localMatchType = MatchType::VirtualPseudoElementOnly;
                for (const CSSSelector* subselector = selector.selectorList()->first(); subselector; subselector = CSSSelectorList::next(subselector)) {
                    LocalContext subcontext(context);
                    subcontext.inFunctionalPseudoClass = true;
                    subcontext.pseudoElementEffective = context.pseudoElementEffective;
                    subcontext.selector = subselector;
                    subcontext.firstSelectorOfTheFragment = subselector;
                    subcontext.pseudoId = PseudoId::None;
                    PseudoIdSet localDynamicPseudoIdSet;
                    MatchResult result = matchRecursively(checkingContext, subcontext, localDynamicPseudoIdSet);

                    // Pseudo elements are not valid inside :is()/:matches()
                    if (localDynamicPseudoIdSet)
                        continue;

                    if (result.match == Match::SelectorMatches) {
                        if (result.matchType == MatchType::Element)
                            localMatchType = MatchType::Element;

                        hasMatchedAnything = true;
                    }
                }
                if (hasMatchedAnything)
                    matchType = localMatchType;
                return hasMatchedAnything;
            }
        case CSSSelector::PseudoClassHas: {
            for (auto* hasSelector = selector.selectorList()->first(); hasSelector; hasSelector = CSSSelectorList::next(hasSelector)) {
                if (matchHasPseudoClass(checkingContext, element, *hasSelector))
                    return true;
            }
            return false;
        }
        case CSSSelector::PseudoClassPlaceholderShown:
            if (is<HTMLTextFormControlElement>(element)) {
                addStyleRelation(checkingContext, element, Style::Relation::Unique);
                return downcast<HTMLTextFormControlElement>(element).isPlaceholderVisible();
            }
            return false;
        case CSSSelector::PseudoClassNthChild: {
            auto* parent = element.parentNode();
            if (is<Element>(parent)) {
                auto relation = context.isSubjectOrAdjacentElement ? Style::Relation::ChildrenAffectedByForwardPositionalRules : Style::Relation::DescendantsAffectedByForwardPositionalRules;
                addStyleRelation(checkingContext, downcast<Element>(*parent), relation);
            } else if (!is<ShadowRoot>(parent))
                break; // FIXME: Add the support for specifying relations on ShadowRoot.

            if (const CSSSelectorList* selectorList = selector.selectorList()) {
                if (!matchSelectorList(checkingContext, context, element, *selectorList))
                    return false;
            }

            int count = 1;
            if (const CSSSelectorList* selectorList = selector.selectorList()) {
                for (Element* sibling = ElementTraversal::previousSibling(element); sibling; sibling = ElementTraversal::previousSibling(*sibling)) {
                    if (matchSelectorList(checkingContext, context, *sibling, *selectorList))
                        ++count;
                }
            } else {
                count += countElementsBefore(element);
                addStyleRelation(checkingContext, element, Style::Relation::NthChildIndex, count);
            }

            if (selector.matchNth(count))
                return true;
            break;
        }
        case CSSSelector::PseudoClassNthOfType: {
            auto* parent = element.parentNode();
            if (is<Element>(parent)) {
                auto relation = context.isSubjectOrAdjacentElement ? Style::Relation::ChildrenAffectedByForwardPositionalRules : Style::Relation::DescendantsAffectedByForwardPositionalRules;
                addStyleRelation(checkingContext, downcast<Element>(*parent), relation);
            } else if (!is<ShadowRoot>(parent))
                break; // FIXME: Add the support for specifying relations on ShadowRoot.

            int count = 1 + countElementsOfTypeBefore(element, element.tagQName());
            if (selector.matchNth(count))
                return true;
            break;
        }
        case CSSSelector::PseudoClassNthLastChild: {
            auto* parent = element.parentNode();
            if (is<Element>(parent)) {
                auto& parentElement = downcast<Element>(*parent);
                if (const CSSSelectorList* selectorList = selector.selectorList()) {
                    if (!matchSelectorList(checkingContext, context, element, *selectorList))
                        return false;

                    addStyleRelation(checkingContext, parentElement, Style::Relation::ChildrenAffectedByPropertyBasedBackwardPositionalRules);
                } else {
                    auto relation = context.isSubjectOrAdjacentElement ? Style::Relation::ChildrenAffectedByBackwardPositionalRules : Style::Relation::DescendantsAffectedByBackwardPositionalRules;
                    addStyleRelation(checkingContext, parentElement, relation);
                }
                if (!parentElement.isFinishedParsingChildren())
                    return false;
            } else if (!is<ShadowRoot>(parent))
                break; // FIXME: Add the support for specifying relations on ShadowRoot.

            int count = 1;
            if (const CSSSelectorList* selectorList = selector.selectorList()) {
                for (Element* sibling = ElementTraversal::nextSibling(element); sibling; sibling = ElementTraversal::nextSibling(*sibling)) {
                    if (matchSelectorList(checkingContext, context, *sibling, *selectorList))
                        ++count;
                }
            } else
                count += countElementsAfter(element);

            return selector.matchNth(count);
        }
        case CSSSelector::PseudoClassNthLastOfType: {
            auto* parent = element.parentNode();
            if (is<Element>(parent)) {
                auto& parentElement = downcast<Element>(*parent);
                auto relation = context.isSubjectOrAdjacentElement ? Style::Relation::ChildrenAffectedByBackwardPositionalRules : Style::Relation::DescendantsAffectedByBackwardPositionalRules;
                addStyleRelation(checkingContext, parentElement, relation);

                if (!parentElement.isFinishedParsingChildren())
                    return false;
            } else if (!is<ShadowRoot>(parent))
                break; // FIXME: Add the support for specifying relations on ShadowRoot.
            int count = 1 + countElementsOfTypeAfter(element, element.tagQName());
            return selector.matchNth(count);
        }
        case CSSSelector::PseudoClassTarget:
            if (&element == element.document().cssTarget() || InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassTarget))
                return true;
            break;
        case CSSSelector::PseudoClassAutofill:
            return isAutofilled(element);
        case CSSSelector::PseudoClassAutofillAndObscured:
            return isAutofilledAndObscured(element);
        case CSSSelector::PseudoClassAutofillStrongPassword:
            return isAutofilledStrongPassword(element);
        case CSSSelector::PseudoClassAutofillStrongPasswordViewable:
            return isAutofilledStrongPasswordViewable(element);
        case CSSSelector::PseudoClassAnyLink:
        case CSSSelector::PseudoClassAnyLinkDeprecated:
        case CSSSelector::PseudoClassLink:
            // :visited and :link matches are separated later when applying the style. Here both classes match all links...
            return element.isLink();
        case CSSSelector::PseudoClassVisited:
            // ...except if :visited matching is disabled for ancestor/sibling matching.
            // Inside functional pseudo class except for :not, :visited never matches.
            if (context.inFunctionalPseudoClass)
                return false;
            return element.isLink() && context.visitedMatchType == VisitedMatchType::Enabled;
        case CSSSelector::PseudoClassDrag:
            return element.isBeingDragged();
        case CSSSelector::PseudoClassFocus:
            return matchesFocusPseudoClass(element);
        case CSSSelector::PseudoClassFocusVisible:
            return matchesFocusVisiblePseudoClass(element);
        case CSSSelector::PseudoClassFocusWithin:
            return matchesFocusWithinPseudoClass(element);
        case CSSSelector::PseudoClassHover:
            if (m_strictParsing || element.isLink() || canMatchHoverOrActiveInQuirksMode(context)) {
                // See the comment in generateElementIsHovered() in SelectorCompiler.
                if (checkingContext.resolvingMode == SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements && !context.isMatchElement)
                    return true;

                if (element.hovered() || InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassHover))
                    return true;
            }
            break;
        case CSSSelector::PseudoClassActive:
            if (m_strictParsing || element.isLink() || canMatchHoverOrActiveInQuirksMode(context)) {
                if (element.active() || InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassActive))
                    return true;
            }
            break;
        case CSSSelector::PseudoClassEnabled:
            return matchesEnabledPseudoClass(element);
        case CSSSelector::PseudoClassFullPageMedia:
            return isMediaDocument(element);
        case CSSSelector::PseudoClassDefault:
            return matchesDefaultPseudoClass(element);
        case CSSSelector::PseudoClassDisabled:
            return matchesDisabledPseudoClass(element);
        case CSSSelector::PseudoClassReadOnly:
            return matchesReadOnlyPseudoClass(element);
        case CSSSelector::PseudoClassReadWrite:
            return matchesReadWritePseudoClass(element);
        case CSSSelector::PseudoClassOptional:
            return isOptionalFormControl(element);
        case CSSSelector::PseudoClassRequired:
            return isRequiredFormControl(element);
        case CSSSelector::PseudoClassValid:
            return isValid(element);
        case CSSSelector::PseudoClassInvalid:
            return isInvalid(element);
        case CSSSelector::PseudoClassChecked:
            return isChecked(element);
        case CSSSelector::PseudoClassIndeterminate:
            return matchesIndeterminatePseudoClass(element);
        case CSSSelector::PseudoClassRoot:
            if (&element == element.document().documentElement())
                return true;
            break;
        case CSSSelector::PseudoClassLang:
            ASSERT(selector.argumentList() && !selector.argumentList()->isEmpty());
            return matchesLangPseudoClass(element, *selector.argumentList());
#if ENABLE(FULLSCREEN_API)
        case CSSSelector::PseudoClassFullScreen:
            return matchesFullScreenPseudoClass(element);
        case CSSSelector::PseudoClassAnimatingFullScreenTransition:
            return matchesFullScreenAnimatingFullScreenTransitionPseudoClass(element);
        case CSSSelector::PseudoClassFullScreenAncestor:
            return matchesFullScreenAncestorPseudoClass(element);
        case CSSSelector::PseudoClassFullScreenParent:
            return matchesFullScreenParentPseudoClass(element);
        case CSSSelector::PseudoClassFullScreenDocument:
            return matchesFullScreenDocumentPseudoClass(element);
        case CSSSelector::PseudoClassFullScreenControlsHidden:
            return matchesFullScreenControlsHiddenPseudoClass(element);
#endif
#if ENABLE(PICTURE_IN_PICTURE_API)
        case CSSSelector::PseudoClassPictureInPicture:
            return matchesPictureInPicturePseudoClass(element);
#endif
        case CSSSelector::PseudoClassInRange:
            return isInRange(element);
        case CSSSelector::PseudoClassOutOfRange:
            return isOutOfRange(element);
#if ENABLE(VIDEO)
        case CSSSelector::PseudoClassFuture:
            return matchesFutureCuePseudoClass(element);
        case CSSSelector::PseudoClassPast:
            return matchesPastCuePseudoClass(element);
        case CSSSelector::PseudoClassPlaying:
            return matchesPlayingPseudoClass(element);
        case CSSSelector::PseudoClassPaused:
            return matchesPausedPseudoClass(element);
        case CSSSelector::PseudoClassSeeking:
            return matchesSeekingPseudoClass(element);
        case CSSSelector::PseudoClassBuffering:
            return matchesBufferingPseudoClass(element);
        case CSSSelector::PseudoClassStalled:
            return matchesStalledPseudoClass(element);
        case CSSSelector::PseudoClassMuted:
            return matchesMutedPseudoClass(element);
        case CSSSelector::PseudoClassVolumeLocked:
            return matchesVolumeLockedPseudoClass(element);
#endif

        case CSSSelector::PseudoClassScope:
        case CSSSelector::PseudoClassRelativeScope: {
            const Node* contextualReferenceNode = !checkingContext.scope ? element.document().documentElement() : checkingContext.scope;

            bool matches = &element == contextualReferenceNode || checkingContext.matchesAllScopes;

            if (!matches && checkingContext.scope) {
                if (element.isDescendantOf(*checkingContext.scope))
                    checkingContext.matchedInsideScope = true;
            }

            return matches;
        }
        case CSSSelector::PseudoClassHost: {
            if (!context.mustMatchHostPseudoClass)
                return false;
            return matchHostPseudoClass(selector, element, checkingContext);
        }
        case CSSSelector::PseudoClassDefined:
            return isDefinedElement(element);
        case CSSSelector::PseudoClassWindowInactive:
            return isWindowInactive(element);

        case CSSSelector::PseudoClassHorizontal:
        case CSSSelector::PseudoClassVertical:
        case CSSSelector::PseudoClassDecrement:
        case CSSSelector::PseudoClassIncrement:
        case CSSSelector::PseudoClassStart:
        case CSSSelector::PseudoClassEnd:
        case CSSSelector::PseudoClassDoubleButton:
        case CSSSelector::PseudoClassSingleButton:
        case CSSSelector::PseudoClassNoButton:
        case CSSSelector::PseudoClassCornerPresent:
            return false;

        case CSSSelector::PseudoClassDir:
            return matchesDirPseudoClass(element, selector.argument());

#if ENABLE(ATTACHMENT_ELEMENT)
        case CSSSelector::PseudoClassHasAttachment:
            return hasAttachment(element);
#endif

        case CSSSelector::PseudoClassModal:
            return matchesModalPseudoClass(element);

        case CSSSelector::PseudoClassUnknown:
            ASSERT_NOT_REACHED();
            break;
        }
        return false;
    }

    if (selector.match() == CSSSelector::PseudoElement) {
        switch (selector.pseudoElementType()) {
#if ENABLE(VIDEO)
        case CSSSelector::PseudoElementCue: {
            LocalContext subcontext(context);

            const CSSSelector* const & selector = context.selector;
            for (subcontext.selector = selector->selectorList()->first(); subcontext.selector; subcontext.selector = CSSSelectorList::next(subcontext.selector)) {
                subcontext.firstSelectorOfTheFragment = subcontext.selector;
                subcontext.inFunctionalPseudoClass = true;
                subcontext.pseudoElementEffective = false;
                PseudoIdSet ignoredDynamicPseudo;
                if (matchRecursively(checkingContext, subcontext, ignoredDynamicPseudo).match == Match::SelectorMatches)
                    return true;
            }
            return false;
        }
#endif
        case CSSSelector::PseudoElementSlotted: {
            if (!context.element->assignedSlot())
                return false;
            // ::slotted matches after flattening so it can't match an active <slot>.
            if (is<HTMLSlotElement>(*context.element) && context.element->containingShadowRoot())
                return false;
            auto* subselector = context.selector->selectorList()->first();
            LocalContext subcontext(context);
            subcontext.selector = subselector;
            subcontext.firstSelectorOfTheFragment = subselector;
            subcontext.pseudoElementEffective = false;
            subcontext.inFunctionalPseudoClass = true;
            PseudoIdSet ignoredDynamicPseudo;
            return matchRecursively(checkingContext, subcontext, ignoredDynamicPseudo).match == Match::SelectorMatches;
        }
        case CSSSelector::PseudoElementPart: {
            auto translatePartNameToRuleScope = [&](AtomString partName) {
                Vector<AtomString, 1> mappedNames { partName };

                if (checkingContext.styleScopeOrdinal == Style::ScopeOrdinal::Element)
                    return mappedNames;

                auto* ruleScopeHost = Style::hostForScopeOrdinal(*context.element, checkingContext.styleScopeOrdinal);

                for (auto* shadowRoot = element.containingShadowRoot(); shadowRoot; shadowRoot = shadowRoot->host()->containingShadowRoot()) {
                    // Apply mappings up to the scope the rules are coming from.
                    if (shadowRoot->host() == ruleScopeHost)
                        break;

                    Vector<AtomString, 1> newMappedNames;
                    for (auto& name : mappedNames)
                        newMappedNames.appendVector(shadowRoot->partMappings().get(name));
                    mappedNames = newMappedNames;

                    if (mappedNames.isEmpty())
                        break;
                }
                return mappedNames;
            };

            Vector<AtomString, 4> translatedPartNames;
            for (unsigned i = 0; i < element.partNames().size(); ++i)
                translatedPartNames.appendVector(translatePartNameToRuleScope(element.partNames()[i]));

            for (auto& part : *selector.argumentList()) {
                if (!translatedPartNames.contains(part))
                    return false;
            }
            return true;
        }

        case CSSSelector::PseudoElementHighlight:
            // Always matches when not specifically requested so it gets added to the pseudoIdSet.
            if (checkingContext.pseudoId == PseudoId::None)
                return true;
            if (checkingContext.pseudoId != PseudoId::Highlight || !selector.argumentList())
                return false;
            return selector.argumentList()->first() == checkingContext.nameForHightlightPseudoElement;

        default:
            return true;
        }
    }
    return true;
}

bool SelectorChecker::matchSelectorList(CheckingContext& checkingContext, const LocalContext& context, const Element& element, const CSSSelectorList& selectorList) const
{
    bool hasMatchedAnything = false;

    for (const CSSSelector* subselector = selectorList.first(); subselector; subselector = CSSSelectorList::next(subselector)) {
        LocalContext subcontext(context);
        subcontext.element = &element;
        subcontext.selector = subselector;
        subcontext.inFunctionalPseudoClass = true;
        subcontext.pseudoElementEffective = false;
        subcontext.firstSelectorOfTheFragment = subselector;
        PseudoIdSet ignoreDynamicPseudo;
        if (matchRecursively(checkingContext, subcontext, ignoreDynamicPseudo).match == Match::SelectorMatches) {
            ASSERT(!ignoreDynamicPseudo);

            hasMatchedAnything = true;
        }
    }
    return hasMatchedAnything;
}

bool SelectorChecker::matchHasPseudoClass(CheckingContext& checkingContext, const Element& element, const CSSSelector& hasSelector) const
{
    auto matchElement = Style::computeHasPseudoClassMatchElement(hasSelector);

    auto canMatch = [&] {
        switch (matchElement) {
        case Style::MatchElement::HasChild:
        case Style::MatchElement::HasDescendant:
            return !!element.firstElementChild();
        case Style::MatchElement::HasSibling:
        case Style::MatchElement::HasSiblingDescendant:
            return !!element.nextElementSibling();
        default:
            return true;
        };
    };

    // See if there are any elements that this :has() selector could match.
    if (!canMatch())
        return false;

    auto* cache = checkingContext.selectorMatchingState ? &checkingContext.selectorMatchingState->hasPseudoClassMatchCache : nullptr;

    auto checkForCachedMatch = [&]() -> std::optional<bool> {
        if (!cache)
            return { };
        switch (cache->get(Style::makeHasPseudoClassCacheKey(element, hasSelector))) {
        case Style::HasPseudoClassMatch::Matches:
            return true;
        case Style::HasPseudoClassMatch::Fails:
        case Style::HasPseudoClassMatch::FailsSubtree:
            return false;
        case Style::HasPseudoClassMatch::None:
            break;
        }
        return { };
    };

    // See if we know the result already.
    if (auto match = checkForCachedMatch())
        return *match;

    auto filterForElement = [&]() -> Style::HasSelectorFilter* {
        if (!checkingContext.selectorMatchingState)
            return nullptr;
        auto type = Style::HasSelectorFilter::typeForMatchElement(matchElement);
        if (!type)
            return nullptr;
        auto& filtersMap = checkingContext.selectorMatchingState->hasPseudoClassSelectorFilters;
        auto addResult = filtersMap.add(Style::makeHasPseudoClassFilterKey(element, *type), std::unique_ptr<Style::HasSelectorFilter>());
        // Only build a filter if the same element gets checked second time with a different selector (misses the match cache).
        if (addResult.isNewEntry)
            return nullptr;

        if (!addResult.iterator->value)
            addResult.iterator->value = makeUnique<Style::HasSelectorFilter>(element, *type);
        return addResult.iterator->value.get();
    };

    // Check if the bloom filter rejects this selector
    if (auto* filter = filterForElement()) {
        if (filter->reject(hasSelector))
            return false;
    }

    CheckingContext hasCheckingContext(SelectorChecker::Mode::ResolvingStyle);
    hasCheckingContext.scope = &element;

    bool matchedInsideScope = false;

    auto checkRelative = [&](auto& elementToCheck) {
        LocalContext hasContext(hasSelector, elementToCheck, VisitedMatchType::Disabled, PseudoId::None);
        hasContext.inFunctionalPseudoClass = true;
        hasContext.pseudoElementEffective = false;

        PseudoIdSet ignorePseudoElements;
        auto result = matchRecursively(hasCheckingContext, hasContext, ignorePseudoElements).match == Match::SelectorMatches;

        if (hasCheckingContext.matchedInsideScope)
            matchedInsideScope = true;

        return result;
    };

    auto checkDescendants = [&](const Element& descendantRoot) {
        for (auto it = descendantsOfType<Element>(descendantRoot).begin(); it;) {
            auto& descendant = *it;
            if (cache && descendant.firstElementChild()) {
                auto key = Style::makeHasPseudoClassCacheKey(descendant, hasSelector);
                if (cache->get(key) == Style::HasPseudoClassMatch::FailsSubtree) {
                    it.traverseNextSkippingChildren();
                    continue;
                }
            }
            if (checkRelative(descendant))
                return true;

            it.traverseNext();
        }

        return false;
    };

    auto match = [&] {
        switch (matchElement) {
        // :has(> .child)
        case Style::MatchElement::HasChild:
            for (auto& child : childrenOfType<Element>(element)) {
                if (checkRelative(child))
                    return true;
            }
            break;
        // :has(.descendant)
        case Style::MatchElement::HasDescendant: {
            if (cache) {
                // See if we already know this descendant selector doesn't match in this subtree.
                for (auto* ancestor = element.parentElement(); ancestor; ancestor = ancestor->parentElement()) {
                    auto key = Style::makeHasPseudoClassCacheKey(*ancestor, hasSelector);
                    if (cache->get(key) == Style::HasPseudoClassMatch::FailsSubtree)
                        return false;
                }
            }
            if (checkDescendants(element))
                return true;

            break;
        }
        // FIXME: Add a separate case for adjacent combinator.
        // :has(+ .sibling)
        // :has(~ .sibling)
        case Style::MatchElement::HasSibling:
            for (auto* sibling = element.nextElementSibling(); sibling; sibling = sibling->nextElementSibling()) {
                if (checkRelative(*sibling))
                    return true;
            }
            break;
        // FIXME: Add a separate case for adjacent combinator.
        // :has(+ .sibling .descendant)
        // :has(~ .sibling .descendant)
        case Style::MatchElement::HasSiblingDescendant:
            for (auto* sibling = element.nextElementSibling(); sibling; sibling = sibling->nextElementSibling()) {
                if (checkDescendants(*sibling))
                    return true;
            }
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
        return false;
    };

    auto result = match();

    auto matchTypeForCache = [&] {
        if (result)
            return Style::HasPseudoClassMatch::Matches;
        if (matchedInsideScope)
            return Style::HasPseudoClassMatch::Fails;
        return Style::HasPseudoClassMatch::FailsSubtree;
    };

    if (cache)
        cache->add(Style::makeHasPseudoClassCacheKey(element, hasSelector), matchTypeForCache());

    return result;
}

bool SelectorChecker::checkScrollbarPseudoClass(const CheckingContext& checkingContext, const Element& element, const CSSSelector& selector) const
{
    ASSERT(selector.match() == CSSSelector::PseudoClass);

    switch (selector.pseudoClassType()) {
    case CSSSelector::PseudoClassWindowInactive:
        return isWindowInactive(element);
    case CSSSelector::PseudoClassEnabled:
        return scrollbarMatchesEnabledPseudoClass(checkingContext);
    case CSSSelector::PseudoClassDisabled:
        return scrollbarMatchesDisabledPseudoClass(checkingContext);
    case CSSSelector::PseudoClassHover:
        return scrollbarMatchesHoverPseudoClass(checkingContext);
    case CSSSelector::PseudoClassActive:
        return scrollbarMatchesActivePseudoClass(checkingContext);
    case CSSSelector::PseudoClassHorizontal:
        return scrollbarMatchesHorizontalPseudoClass(checkingContext);
    case CSSSelector::PseudoClassVertical:
        return scrollbarMatchesVerticalPseudoClass(checkingContext);
    case CSSSelector::PseudoClassDecrement:
        return scrollbarMatchesDecrementPseudoClass(checkingContext);
    case CSSSelector::PseudoClassIncrement:
        return scrollbarMatchesIncrementPseudoClass(checkingContext);
    case CSSSelector::PseudoClassStart:
        return scrollbarMatchesStartPseudoClass(checkingContext);
    case CSSSelector::PseudoClassEnd:
        return scrollbarMatchesEndPseudoClass(checkingContext);
    case CSSSelector::PseudoClassDoubleButton:
        return scrollbarMatchesDoubleButtonPseudoClass(checkingContext);
    case CSSSelector::PseudoClassSingleButton:
        return scrollbarMatchesSingleButtonPseudoClass(checkingContext);
    case CSSSelector::PseudoClassNoButton:
        return scrollbarMatchesNoButtonPseudoClass(checkingContext);
    case CSSSelector::PseudoClassCornerPresent:
        return scrollbarMatchesCornerPresentPseudoClass(checkingContext);
    default:
        return false;
    }
}

unsigned SelectorChecker::determineLinkMatchType(const CSSSelector* selector)
{
    unsigned linkMatchType = MatchAll;

    // Statically determine if this selector will match a link in visited, unvisited or any state, or never.
    // :visited never matches other elements than the innermost link element.
    for (; selector; selector = selector->tagHistory()) {
        if (selector->match() == CSSSelector::PseudoClass) {
            switch (selector->pseudoClassType()) {
            case CSSSelector::PseudoClassLink:
                linkMatchType &= ~SelectorChecker::MatchVisited;
                break;
            case CSSSelector::PseudoClassVisited:
                linkMatchType &= ~SelectorChecker::MatchLink;
                break;
            default:
                break;
            }
        }
        auto relation = selector->relation();
        if (relation == CSSSelector::Subselector)
            continue;
        if (!selector->hasDescendantOrChildRelation())
            return linkMatchType;
        if (linkMatchType != MatchAll)
            return linkMatchType;
    }
    return linkMatchType;
}

}
