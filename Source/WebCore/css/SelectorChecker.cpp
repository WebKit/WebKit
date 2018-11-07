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
#include "Document.h"
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
#include "SelectorCheckerTestFunctions.h"
#include "ShadowRoot.h"
#include "Text.h"

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
    bool mayMatchHostPseudoClass { false };

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

bool SelectorChecker::match(const CSSSelector& selector, const Element& element, CheckingContext& checkingContext, unsigned& specificity) const
{
    specificity = 0;

    LocalContext context(selector, element, checkingContext.resolvingMode == SelectorChecker::Mode::QueryingRules ? VisitedMatchType::Disabled : VisitedMatchType::Enabled, checkingContext.pseudoId);

    if (checkingContext.isMatchingHostPseudoClass) {
        ASSERT(element.shadowRoot());
        context.mayMatchHostPseudoClass = true;
    }

    PseudoIdSet pseudoIdSet;
    MatchResult result = matchRecursively(checkingContext, context, pseudoIdSet, specificity);
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

bool SelectorChecker::matchHostPseudoClass(const CSSSelector& selector, const Element& element, CheckingContext& checkingContext, unsigned& specificity) const
{
    ASSERT(element.shadowRoot());
    ASSERT(selector.match() == CSSSelector::PseudoClass && selector.pseudoClassType() == CSSSelector::PseudoClassHost);

    specificity = selector.simpleSelectorSpecificity();

    if (auto* selectorList = selector.selectorList()) {
        LocalContext context(*selectorList->first(), element, VisitedMatchType::Enabled, PseudoId::None);
        context.inFunctionalPseudoClass = true;
        context.pseudoElementEffective = false;
        PseudoIdSet ignoreDynamicPseudo;
        unsigned subselectorSpecificity = 0;
        if (matchRecursively(checkingContext, context, ignoreDynamicPseudo, subselectorSpecificity).match != Match::SelectorMatches)
            return false;
        specificity = CSSSelector::addSpecificities(specificity, subselectorSpecificity);
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

    if (updatedContext.mayMatchHostPseudoClass) {
        updatedContext.element = nullptr;
        return updatedContext;
    }

    // Move to the shadow host if matching :host and the parent is the shadow root.
    if (context.selector->match() == CSSSelector::PseudoClass && context.selector->pseudoClassType() == CSSSelector::PseudoClassHost && is<ShadowRoot>(context.element->parentNode())) {
        updatedContext.element = downcast<ShadowRoot>(*context.element->parentNode()).host();
        updatedContext.mayMatchHostPseudoClass = true;
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
SelectorChecker::MatchResult SelectorChecker::matchRecursively(CheckingContext& checkingContext, const LocalContext& context, PseudoIdSet& dynamicPseudoIdSet, unsigned& specificity) const
{
    MatchType matchType = MatchType::Element;

    // The first selector has to match.
    if (!checkOne(checkingContext, context, dynamicPseudoIdSet, matchType, specificity))
        return MatchResult::fails(Match::SelectorFailsLocally);

    if (context.selector->match() == CSSSelector::PseudoElement) {
        if (context.selector->isCustomPseudoElement()) {
            // In functional pseudo class, custom pseudo elements are always disabled.
            // FIXME: We should accept custom pseudo elements inside :matches().
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
        // Virtual pseudo element is only effective in the rightmost fragment.
        nextContext.pseudoElementEffective = false;
        nextContext.isMatchElement = false;
    }

    switch (relation) {
    case CSSSelector::DescendantSpace:
        nextContext = localContextForParent(nextContext);
        nextContext.firstSelectorOfTheFragment = nextContext.selector;
        for (; nextContext.element; nextContext = localContextForParent(nextContext)) {
            PseudoIdSet ignoreDynamicPseudo;
            unsigned descendantsSpecificity = 0;
            MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo, descendantsSpecificity);
            ASSERT(!nextContext.pseudoElementEffective && !ignoreDynamicPseudo);

            if (result.match == Match::SelectorMatches)
                specificity = CSSSelector::addSpecificities(specificity, descendantsSpecificity);

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
            unsigned childSpecificity = 0;
            MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo, childSpecificity);
            ASSERT(!nextContext.pseudoElementEffective && !ignoreDynamicPseudo);

            if (result.match == Match::SelectorMatches)
                specificity = CSSSelector::addSpecificities(specificity, childSpecificity);

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
            unsigned adjacentSpecificity = 0;
            MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo, adjacentSpecificity);
            ASSERT(!nextContext.pseudoElementEffective && !ignoreDynamicPseudo);

            if (result.match == Match::SelectorMatches)
                specificity = CSSSelector::addSpecificities(specificity, adjacentSpecificity);

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
            unsigned indirectAdjacentSpecificity = 0;
            MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo, indirectAdjacentSpecificity);
            ASSERT(!nextContext.pseudoElementEffective && !ignoreDynamicPseudo);

            if (result.match == Match::SelectorMatches)
                specificity = CSSSelector::addSpecificities(specificity, indirectAdjacentSpecificity);

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

            unsigned subselectorSpecificity = 0;
            MatchResult result = matchRecursively(checkingContext, nextContext, dynamicPseudoIdSet, subselectorSpecificity);

            if (result.match == Match::SelectorMatches)
                specificity = CSSSelector::addSpecificities(specificity, subselectorSpecificity);

            return MatchResult::updateWithMatchType(result, matchType);
        }
    case CSSSelector::ShadowDescendant:
        {
            Element* shadowHostNode = context.element->shadowHost();
            if (!shadowHostNode)
                return MatchResult::fails(Match::SelectorFailsCompletely);
            nextContext.element = shadowHostNode;
            nextContext.firstSelectorOfTheFragment = nextContext.selector;
            nextContext.isSubjectOrAdjacentElement = false;
            PseudoIdSet ignoreDynamicPseudo;
            unsigned shadowDescendantSpecificity = 0;
            MatchResult result = matchRecursively(checkingContext, nextContext, ignoreDynamicPseudo, shadowDescendantSpecificity);

            if (result.match == Match::SelectorMatches)
                specificity = CSSSelector::addSpecificities(specificity, shadowDescendantSpecificity);

            return MatchResult::updateWithMatchType(result, matchType);
        }
    }


    ASSERT_NOT_REACHED();
    return MatchResult::fails(Match::SelectorFailsCompletely);
}

static bool attributeValueMatches(const Attribute& attribute, CSSSelector::Match match, const AtomicString& selectorValue, bool caseSensitive)
{
    const AtomicString& value = attribute.value();
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

bool SelectorChecker::attributeSelectorMatches(const Element& element, const QualifiedName& attributeName, const AtomicString& attributeValue, const CSSSelector& selector)
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
        if (relation == CSSSelector::ShadowDescendant)
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

    const AtomicString& localName = (element.isHTMLElement() && element.document().isHTMLDocument()) ? simpleSelector.tagLowercaseLocalName() : tagQName.localName();

    if (localName != starAtom() && localName != element.localName())
        return false;
    const AtomicString& namespaceURI = tagQName.namespaceURI();
    return namespaceURI == starAtom() || namespaceURI == element.namespaceURI();
}

bool SelectorChecker::checkOne(CheckingContext& checkingContext, const LocalContext& context, PseudoIdSet& dynamicPseudoIdSet, MatchType& matchType, unsigned& specificity) const
{
    const Element& element = *context.element;
    const CSSSelector& selector = *context.selector;

    specificity = CSSSelector::addSpecificities(specificity, selector.simpleSelectorSpecificity());

    if (context.mayMatchHostPseudoClass) {
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

                unsigned ignoredSpecificity;
                if (matchRecursively(checkingContext, subcontext, ignoreDynamicPseudo, ignoredSpecificity).match == Match::SelectorMatches) {
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
        case CSSSelector::PseudoClassMatches:
            {
                bool hasMatchedAnything = false;
                unsigned maxSpecificity = 0;

                MatchType localMatchType = MatchType::VirtualPseudoElementOnly;
                for (const CSSSelector* subselector = selector.selectorList()->first(); subselector; subselector = CSSSelectorList::next(subselector)) {
                    LocalContext subcontext(context);
                    subcontext.inFunctionalPseudoClass = true;
                    subcontext.pseudoElementEffective = context.pseudoElementEffective;
                    subcontext.selector = subselector;
                    subcontext.firstSelectorOfTheFragment = subselector;
                    PseudoIdSet localDynamicPseudoIdSet;
                    unsigned localSpecificity = 0;
                    MatchResult result = matchRecursively(checkingContext, subcontext, localDynamicPseudoIdSet, localSpecificity);
                    if (result.match == Match::SelectorMatches) {
                        maxSpecificity = std::max(maxSpecificity, localSpecificity);

                        if (result.matchType == MatchType::Element)
                            localMatchType = MatchType::Element;

                        dynamicPseudoIdSet.merge(localDynamicPseudoIdSet);
                        hasMatchedAnything = true;
                    }
                }
                if (hasMatchedAnything) {
                    matchType = localMatchType;
                    specificity = CSSSelector::addSpecificities(specificity, maxSpecificity);
                }
                return hasMatchedAnything;
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
                unsigned selectorListSpecificity;
                if (!matchSelectorList(checkingContext, context, element, *selectorList, selectorListSpecificity))
                    return false;
                specificity = CSSSelector::addSpecificities(specificity, selectorListSpecificity);
            }

            int count = 1;
            if (const CSSSelectorList* selectorList = selector.selectorList()) {
                for (Element* sibling = ElementTraversal::previousSibling(element); sibling; sibling = ElementTraversal::previousSibling(*sibling)) {
                    unsigned ignoredSpecificity;
                    if (matchSelectorList(checkingContext, context, *sibling, *selectorList, ignoredSpecificity))
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
                    unsigned selectorListSpecificity;
                    if (!matchSelectorList(checkingContext, context, element, *selectorList, selectorListSpecificity))
                        return false;
                    specificity = CSSSelector::addSpecificities(specificity, selectorListSpecificity);

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
                    unsigned ignoredSpecificity;
                    if (matchSelectorList(checkingContext, context, *sibling, *selectorList, ignoredSpecificity))
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
            if (&element == element.document().cssTarget())
                return true;
            break;
        case CSSSelector::PseudoClassAny:
            {
                LocalContext subcontext(context);
                subcontext.inFunctionalPseudoClass = true;
                subcontext.pseudoElementEffective = false;
                for (subcontext.selector = selector.selectorList()->first(); subcontext.selector; subcontext.selector = CSSSelectorList::next(subcontext.selector)) {
                    subcontext.firstSelectorOfTheFragment = subcontext.selector;
                    PseudoIdSet ignoreDynamicPseudo;
                    unsigned ingoredSpecificity = 0;
                    if (matchRecursively(checkingContext, subcontext, ignoreDynamicPseudo, ingoredSpecificity).match == Match::SelectorMatches)
                        return true;
                }
            }
            break;
        case CSSSelector::PseudoClassAutofill:
            return isAutofilled(element);
        case CSSSelector::PseudoClassAutofillStrongPassword:
            return isAutofilledStrongPassword(element);
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
            addStyleRelation(checkingContext, element, Style::Relation::AffectedByDrag);

            if (element.renderer() && element.renderer()->isDragging())
                return true;
            break;
        case CSSSelector::PseudoClassFocus:
            return matchesFocusPseudoClass(element);
        case CSSSelector::PseudoClassFocusWithin:
            addStyleRelation(checkingContext, element, Style::Relation::AffectedByFocusWithin);
            return element.hasFocusWithin();
        case CSSSelector::PseudoClassHover:
            if (m_strictParsing || element.isLink() || canMatchHoverOrActiveInQuirksMode(context)) {
                addStyleRelation(checkingContext, element, Style::Relation::AffectedByHover);

                // See the comment in generateElementIsHovered() in SelectorCompiler.
                if (checkingContext.resolvingMode == SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements && !context.isMatchElement)
                    return true;

                if (element.hovered() || InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassHover))
                    return true;
            }
            break;
        case CSSSelector::PseudoClassActive:
            if (m_strictParsing || element.isLink() || canMatchHoverOrActiveInQuirksMode(context)) {
                addStyleRelation(checkingContext, element, Style::Relation::AffectedByActive);

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
            {
                ASSERT(selector.langArgumentList() && !selector.langArgumentList()->isEmpty());
                return matchesLangPseudoClass(element, *selector.langArgumentList());
            }
#if ENABLE(FULLSCREEN_API)
        case CSSSelector::PseudoClassFullScreen:
            return matchesFullScreenPseudoClass(element);
        case CSSSelector::PseudoClassAnimatingFullScreenTransition:
            return matchesFullScreenAnimatingFullScreenTransitionPseudoClass(element);
        case CSSSelector::PseudoClassFullScreenAncestor:
            return matchesFullScreenAncestorPseudoClass(element);
        case CSSSelector::PseudoClassFullScreenDocument:
            return matchesFullScreenDocumentPseudoClass(element);
        case CSSSelector::PseudoClassFullScreenControlsHidden:
            return matchesFullScreenControlsHiddenPseudoClass(element);
#endif
        case CSSSelector::PseudoClassInRange:
            return isInRange(element);
        case CSSSelector::PseudoClassOutOfRange:
            return isOutOfRange(element);
#if ENABLE(VIDEO_TRACK)
        case CSSSelector::PseudoClassFuture:
            return matchesFutureCuePseudoClass(element);
        case CSSSelector::PseudoClassPast:
            return matchesPastCuePseudoClass(element);
#endif

        case CSSSelector::PseudoClassScope: {
            const Node* contextualReferenceNode = !checkingContext.scope ? element.document().documentElement() : checkingContext.scope;
            if (&element == contextualReferenceNode)
                return true;
            break;
        }
        case CSSSelector::PseudoClassHost: {
            if (!context.mayMatchHostPseudoClass)
                return false;
            unsigned hostSpecificity;
            if (!matchHostPseudoClass(selector, element, checkingContext, hostSpecificity))
                return false;
            specificity = CSSSelector::addSpecificities(specificity, hostSpecificity);
            return true;
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

#if ENABLE(CSS_SELECTORS_LEVEL4)
        // FIXME: Implement :dir() selector.
        case CSSSelector::PseudoClassDir:
            return false;

        // FIXME: Implement :role() selector.
        case CSSSelector::PseudoClassRole:
            return false;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
        case CSSSelector::PseudoClassHasAttachment:
            return hasAttachment(element);
#endif

        case CSSSelector::PseudoClassUnknown:
            ASSERT_NOT_REACHED();
            break;
        }
        return false;
    }
#if ENABLE(VIDEO_TRACK)
    if (selector.match() == CSSSelector::PseudoElement && selector.pseudoElementType() == CSSSelector::PseudoElementCue) {
        LocalContext subcontext(context);

        const CSSSelector* const & selector = context.selector;
        for (subcontext.selector = selector->selectorList()->first(); subcontext.selector; subcontext.selector = CSSSelectorList::next(subcontext.selector)) {
            subcontext.firstSelectorOfTheFragment = subcontext.selector;
            subcontext.inFunctionalPseudoClass = true;
            subcontext.pseudoElementEffective = false;
            PseudoIdSet ignoredDynamicPseudo;
            unsigned ignoredSpecificity = 0;
            if (matchRecursively(checkingContext, subcontext, ignoredDynamicPseudo, ignoredSpecificity).match == Match::SelectorMatches)
                return true;
        }
        return false;
    }
#endif
    if (selector.match() == CSSSelector::PseudoElement && selector.pseudoElementType() == CSSSelector::PseudoElementSlotted) {
        // We see ::slotted() pseudo elements when collecting slotted rules from the slot shadow tree only.
        ASSERT(checkingContext.resolvingMode == Mode::CollectingRules);
        return is<HTMLSlotElement>(element);
    }
    return true;
}

bool SelectorChecker::matchSelectorList(CheckingContext& checkingContext, const LocalContext& context, const Element& element, const CSSSelectorList& selectorList, unsigned& specificity) const
{
    specificity = 0;
    bool hasMatchedAnything = false;

    for (const CSSSelector* subselector = selectorList.first(); subselector; subselector = CSSSelectorList::next(subselector)) {
        LocalContext subcontext(context);
        subcontext.element = &element;
        subcontext.selector = subselector;
        subcontext.inFunctionalPseudoClass = true;
        subcontext.pseudoElementEffective = false;
        subcontext.firstSelectorOfTheFragment = subselector;
        PseudoIdSet ignoreDynamicPseudo;
        unsigned localSpecificity = 0;
        if (matchRecursively(checkingContext, subcontext, ignoreDynamicPseudo, localSpecificity).match == Match::SelectorMatches) {
            ASSERT(!ignoreDynamicPseudo);

            hasMatchedAnything = true;
            specificity = std::max(specificity, localSpecificity);
        }
    }
    return hasMatchedAnything;
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

static bool isFrameFocused(const Element& element)
{
    return element.document().frame() && element.document().frame()->selection().isFocusedAndActive();
}

bool SelectorChecker::matchesFocusPseudoClass(const Element& element)
{
    if (InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassFocus))
        return true;
    return element.focused() && isFrameFocused(element);
}

}
