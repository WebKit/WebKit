/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Yusuke Suzuki <utatane.tea@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SelectorQuery.h"

#include "CSSSelectorParser.h"
#include "CSSTokenizer.h"
#include "CommonAtomStrings.h"
#include "ElementAncestorIteratorInlines.h"
#include "HTMLBaseElement.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "SVGElement.h"
#include "SelectorChecker.h"
#include "StaticNodeList.h"
#include "StyledElement.h"
#include "TreeScopeInlines.h"
#include "TypedElementDescendantIteratorInlines.h"

namespace WebCore {

#if ASSERT_ENABLED
static bool isSingleTagNameSelector(const CSSSelector& selector)
{
    return selector.isLastInTagHistory() && selector.match() == CSSSelector::Match::Tag;
}

static bool isSingleClassNameSelector(const CSSSelector& selector)
{
    return selector.isLastInTagHistory() && selector.match() == CSSSelector::Match::Class;
}

static bool isSingleAttributeExactSelector(const CSSSelector& selector)
{
    return selector.isLastInTagHistory() && selector.match() == CSSSelector::Match::Exact;
}

#endif // ASSERT_ENABLED

enum class IdMatchingType : uint8_t {
    None,
    Rightmost,
    Filter
};

template<typename Output> static ALWAYS_INLINE void appendOutputForElement(Output& output, Element& element)
{
    if constexpr (std::is_same_v<Output, Element*>) {
        ASSERT(!output);
        output = &element;
    } else
        output.append(element);
}

static bool canBeUsedForIdFastPath(const CSSSelector& selector)
{
    return selector.match() == CSSSelector::Match::Id
        || (selector.match() == CSSSelector::Match::Exact && selector.attribute() == HTMLNames::idAttr && !selector.attributeValueMatchingIsCaseInsensitive());
}

static IdMatchingType findIdMatchingType(const CSSSelector& firstSelector)
{
    bool inRightmost = true;
    for (const CSSSelector* selector = &firstSelector; selector; selector = selector->tagHistory()) {
        if (canBeUsedForIdFastPath(*selector)) {
            if (inRightmost)
                return IdMatchingType::Rightmost;
            return IdMatchingType::Filter;
        }
        if (selector->relation() != CSSSelector::Relation::Subselector)
            inRightmost = false;
    }
    return IdMatchingType::None;
}

static bool canOptimizeSingleAttributeExactMatch(const CSSSelector& selector)
{
    // Bailout if attribute name needs to be definitely case-insensitive.
    if (selector.attributeValueMatchingIsCaseInsensitive())
        return false;

    const auto& attribute = selector.attribute();

    if (!HTMLDocument::isCaseSensitiveAttribute(attribute))
        return false;

    // Bailout if we need to synchronize attributes.
    if (Attribute::nameMatchesFilter(HTMLNames::styleAttr, attribute.prefix(), attribute.localNameLowercase(), attribute.namespaceURI()))
        return false;

    if (Attribute::nameMatchesFilter(SVGElement::animatableAttributeForName(attribute.localName()), attribute.prefix(), attribute.localName(), attribute.namespaceURI()))
        return false;

    return true;
}

SelectorDataList::SelectorDataList(const CSSSelectorList& selectorList)
{
    unsigned selectorCount = 0;
    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector))
        selectorCount++;

    m_selectors.reserveInitialCapacity(selectorCount);
    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector))
        m_selectors.append({ selector });

    if (selectorCount == 1) {
        const CSSSelector& selector = *m_selectors.first().selector;
        if (selector.isLastInTagHistory()) {
            switch (selector.match()) {
            case CSSSelector::Match::Tag:
                m_matchType = TagNameMatch;
                break;
            case CSSSelector::Match::Class:
                m_matchType = ClassNameMatch;
                break;
            case CSSSelector::Match::Exact:
                if (canBeUsedForIdFastPath(selector))
                    m_matchType = RightMostWithIdMatch; // [id="name"] pattern goes here.
                else if (canOptimizeSingleAttributeExactMatch(selector))
                    m_matchType = AttributeExactMatch;
                else
                    m_matchType = CompilableSingle;
                break;
            default:
                if (canBeUsedForIdFastPath(selector))
                    m_matchType = RightMostWithIdMatch;
                else
                    m_matchType = CompilableSingle;
                break;
            }
        } else {
            switch (findIdMatchingType(selector)) {
            case IdMatchingType::None:
                m_matchType = CompilableSingle;
                break;
            case IdMatchingType::Rightmost:
                m_matchType = RightMostWithIdMatch;
                break;
            case IdMatchingType::Filter:
                m_matchType = CompilableSingleWithRootFilter;
                break;
            }
        }
    } else
        m_matchType = CompilableMultipleSelectorMatch;
}

inline bool SelectorDataList::selectorMatches(const SelectorData& selectorData, Element& element, const ContainerNode& rootNode) const
{
    SelectorChecker selectorChecker(element.document());
    SelectorChecker::CheckingContext selectorCheckingContext(SelectorChecker::Mode::QueryingRules);
    selectorCheckingContext.scope = rootNode.isDocumentNode() ? nullptr : &rootNode;
    return selectorChecker.match(*selectorData.selector, element, selectorCheckingContext);
}

inline Element* SelectorDataList::selectorClosest(const SelectorData& selectorData, Element& element, const ContainerNode& rootNode) const
{
    SelectorChecker selectorChecker(element.document());
    SelectorChecker::CheckingContext selectorCheckingContext(SelectorChecker::Mode::QueryingRules);
    selectorCheckingContext.scope = rootNode.isDocumentNode() ? nullptr : &rootNode;
    if (!selectorChecker.match(*selectorData.selector, element, selectorCheckingContext))
        return nullptr;
    return &element;
}

bool SelectorDataList::matches(Element& targetElement) const
{
    for (auto& selector : m_selectors) {
        if (selectorMatches(selector, targetElement, targetElement))
            return true;
    }
    return false;
}

Element* SelectorDataList::closest(Element& targetElement) const
{
    for (auto& currentElement : lineageOfType<Element>(targetElement)) {
        for (auto& selector : m_selectors) {
            if (auto* candidateElement = selectorClosest(selector, currentElement, targetElement))
                return candidateElement;
        }
    }
    return nullptr;
}

Ref<NodeList> SelectorDataList::queryAll(ContainerNode& rootNode) const
{
    Vector<Ref<Element>> result;
    execute(rootNode, result);
    return StaticElementList::create(WTFMove(result));
}

Element* SelectorDataList::queryFirst(ContainerNode& rootNode) const
{
    Element* result = nullptr;
    execute(rootNode, result);
    return result;
}

static const CSSSelector* selectorForIdLookup(const ContainerNode& rootNode, const CSSSelector& firstSelector)
{
    if (!rootNode.isConnected())
        return nullptr;
    if (rootNode.document().inQuirksMode())
        return nullptr;

    for (const CSSSelector* selector = &firstSelector; selector; selector = selector->tagHistory()) {
        if (canBeUsedForIdFastPath(*selector))
            return selector;
        if (selector->relation() != CSSSelector::Relation::Subselector)
            break;
    }

    return nullptr;
}

template<typename OutputType>
ALWAYS_INLINE void SelectorDataList::executeFastPathForIdSelector(const ContainerNode& rootNode, const SelectorData& selectorData, const CSSSelector* idSelector, OutputType& output) const
{
    ASSERT(m_selectors.size() == 1);
    ASSERT(idSelector);

    const AtomString& idToMatch = idSelector->value();
    if (UNLIKELY(rootNode.treeScope().containsMultipleElementsWithId(idToMatch))) {
        auto* elements = rootNode.treeScope().getAllElementsById(idToMatch);
        ASSERT(elements);
        bool rootNodeIsTreeScopeRoot = rootNode.isTreeScope();
        for (auto& element : *elements) {
            if ((rootNodeIsTreeScopeRoot || element->isDescendantOf(rootNode)) && selectorMatches(selectorData, element, rootNode)) {
                appendOutputForElement(output, element);
                if constexpr (std::is_same_v<OutputType, Element*>)
                    return;
            }
        }
        return;
    }

    RefPtr element = rootNode.treeScope().getElementById(idToMatch);
    if (!element || !(rootNode.isTreeScope() || element->isDescendantOf(rootNode)))
        return;
    if (selectorMatches(selectorData, *element, rootNode))
        appendOutputForElement(output, *element);
}

static ContainerNode& filterRootById(ContainerNode& rootNode, const CSSSelector& firstSelector)
{
    if (!rootNode.isConnected())
        return rootNode;
    if (rootNode.document().inQuirksMode())
        return rootNode;

    // If there was an Id match in the rightmost Simple Selector, we should be in a RightMostWithIdMatch, not in filter.
    // Thus we can skip the rightmost match.
    const CSSSelector* selector = &firstSelector;
    do {
        ASSERT(!canBeUsedForIdFastPath(*selector));
        if (selector->relation() != CSSSelector::Relation::Subselector)
            break;
        selector = selector->tagHistory();
    } while (selector);

    bool inAdjacentChain = false;
    for (; selector; selector = selector->tagHistory()) {
        if (canBeUsedForIdFastPath(*selector)) {
            const AtomString& idToMatch = selector->value();
            if (RefPtr<ContainerNode> searchRoot = rootNode.treeScope().getElementById(idToMatch)) {
                if (LIKELY(!rootNode.treeScope().containsMultipleElementsWithId(idToMatch))) {
                    if (inAdjacentChain)
                        searchRoot = searchRoot->parentNode();
                    if (searchRoot && (rootNode.isTreeScope() || searchRoot == &rootNode || searchRoot->isDescendantOf(rootNode)))
                        return *searchRoot;
                }
            }
        }
        if (selector->relation() == CSSSelector::Relation::Subselector)
            continue;
        inAdjacentChain = selector->relation() == CSSSelector::Relation::DirectAdjacent || selector->relation() == CSSSelector::Relation::IndirectAdjacent;
    }
    return rootNode;
}

static ALWAYS_INLINE bool localNameMatches(const Element& element, const AtomString& localName, const AtomString& lowercaseLocalName)
{
    if (element.isHTMLElement() && element.document().isHTMLDocument())
        return element.localName() == lowercaseLocalName;
    return element.localName() == localName;

}

template<typename OutputType>
static inline void elementsForLocalName(const ContainerNode& rootNode, const AtomString& localName, const AtomString& lowercaseLocalName, OutputType& output)
{
    if (auto* rootDocument = dynamicDowncast<Document>(rootNode); rootDocument && lowercaseLocalName == HTMLNames::baseTag->localName()) {
        RefPtr firstBaseElement = rootDocument->firstBaseElement();
        if (!firstBaseElement)
            return;
        if constexpr (std::is_same_v<OutputType, Element*>) {
            appendOutputForElement(output, *firstBaseElement);
            return;
        }
    }

    if (localName == lowercaseLocalName) {
        for (auto& element : descendantsOfType<Element>(const_cast<ContainerNode&>(rootNode))) {
            if (element.tagQName().localName() == localName) {
                appendOutputForElement(output, element);
                if constexpr (std::is_same_v<OutputType, Element*>)
                return;
            }
        }
    } else {
        for (auto& element : descendantsOfType<Element>(const_cast<ContainerNode&>(rootNode))) {
            if (localNameMatches(element, localName, lowercaseLocalName)) {
                appendOutputForElement(output, element);
                if constexpr (std::is_same_v<OutputType, Element*>)
                return;
            }
        }
    }
}

template<typename OutputType>
static inline void anyElement(const ContainerNode& rootNode, OutputType& output)
{
    for (auto& element : descendantsOfType<Element>(const_cast<ContainerNode&>(rootNode))) {
        appendOutputForElement(output, element);
        if constexpr (std::is_same_v<OutputType, Element*>)
            return;
    }
}


template<typename OutputType>
ALWAYS_INLINE void SelectorDataList::executeSingleTagNameSelectorData(const ContainerNode& rootNode, const SelectorData& selectorData, OutputType& output) const
{
    ASSERT(m_selectors.size() == 1);
    ASSERT(isSingleTagNameSelector(*selectorData.selector));

    const QualifiedName& tagQualifiedName = selectorData.selector->tagQName();
    const AtomString& selectorLocalName = tagQualifiedName.localName();
    const AtomString& selectorLowercaseLocalName = selectorData.selector->tagLowercaseLocalName();
    const AtomString& selectorNamespaceURI = tagQualifiedName.namespaceURI();

    if (selectorNamespaceURI == starAtom()) {
        if (selectorLocalName != starAtom()) {
            // Common case: name defined, selectorNamespaceURI is a wildcard.
            elementsForLocalName(rootNode, selectorLocalName, selectorLowercaseLocalName, output);
        } else {
            // Other fairly common case: both are wildcards.
            anyElement(rootNode, output);
        }
    } else {
        // Fallback: NamespaceURI is set, selectorLocalName may be starAtom().
        for (auto& element : descendantsOfType<Element>(const_cast<ContainerNode&>(rootNode))) {
            if (element.namespaceURI() == selectorNamespaceURI && localNameMatches(element, selectorLocalName, selectorLowercaseLocalName)) {
                appendOutputForElement(output, element);
                if constexpr (std::is_same_v<OutputType, Element*>)
                    return;
            }
        }
    }
}

template<typename OutputType>
ALWAYS_INLINE void SelectorDataList::executeSingleClassNameSelectorData(const ContainerNode& rootNode, const SelectorData& selectorData, OutputType& output) const
{
    ASSERT(m_selectors.size() == 1);
    ASSERT(isSingleClassNameSelector(*selectorData.selector));

    const AtomString& className = selectorData.selector->value();
    for (auto& element : descendantsOfType<Element>(const_cast<ContainerNode&>(rootNode))) {
        if (element.hasClassName(className)) {
            appendOutputForElement(output, element);
            if constexpr (std::is_same_v<OutputType, Element*>)
                return;
        }
    }
}

AtomString SelectorDataList::classNameToMatch() const
{
    if (m_matchType != MatchType::ClassNameMatch)
        return nullAtom();
    ASSERT(m_selectors.size() == 1);
    ASSERT(isSingleClassNameSelector(*m_selectors.first().selector));
    return m_selectors.first().selector->value();
}

template<typename OutputType>
ALWAYS_INLINE void SelectorDataList::executeSingleAttributeExactSelectorData(const ContainerNode& rootNode, const SelectorData& selectorData, OutputType& output) const
{
    ASSERT(m_selectors.size() == 1);
    ASSERT(isSingleAttributeExactSelector(*selectorData.selector));
    ASSERT(canOptimizeSingleAttributeExactMatch(*selectorData.selector));

    const auto& selectorAttribute = selectorData.selector->attribute();
    const auto& selectorValue = selectorData.selector->value();
    const auto& localNameLowercase = selectorAttribute.localNameLowercase();
    const auto& localName = selectorAttribute.localName();
    const auto& prefix = selectorAttribute.prefix();
    const auto& namespaceURI = selectorAttribute.namespaceURI();

    bool documentIsHTML = rootNode.document().isHTMLDocument();
    for (auto& element : descendantsOfType<Element>(const_cast<ContainerNode&>(rootNode))) {
        if (!element.hasAttributesWithoutUpdate())
            continue;

        bool isHTML = documentIsHTML && element.isHTMLElement();
        const auto& localNameToMatch = isHTML ? localNameLowercase : localName;
        for (const Attribute& attribute : element.attributesIterator()) {
            if (!attribute.matches(prefix, localNameToMatch, namespaceURI))
                continue;

            if (selectorValue == attribute.value()) {
                appendOutputForElement(output, element);
                if constexpr (std::is_same_v<OutputType, Element*>)
                    return;
                break;
            }
        }
    }
}

template<typename OutputType>
ALWAYS_INLINE void SelectorDataList::executeSingleSelectorData(const ContainerNode& rootNode, const ContainerNode& searchRootNode, const SelectorData& selectorData, OutputType& output) const
{
    ASSERT(m_selectors.size() == 1);

    for (auto& element : descendantsOfType<Element>(const_cast<ContainerNode&>(searchRootNode))) {
        if (selectorMatches(selectorData, element, rootNode)) {
            appendOutputForElement(output, element);
            if constexpr (std::is_same_v<OutputType, Element*>)
                return;
        }
    }
}

template<typename OutputType>
ALWAYS_INLINE void SelectorDataList::executeSingleMultiSelectorData(const ContainerNode& rootNode, OutputType& output) const
{
    for (auto& element : descendantsOfType<Element>(const_cast<ContainerNode&>(rootNode))) {
        for (auto& selector : m_selectors) {
            if (selectorMatches(selector, element, rootNode)) {
                appendOutputForElement(output, element);
                if constexpr (std::is_same_v<OutputType, Element*>)
                    return;
                break;
            }
        }
    }
}

#if ENABLE(CSS_SELECTOR_JIT)
template<typename Checker, typename OutputType>
ALWAYS_INLINE void SelectorDataList::executeCompiledSimpleSelectorChecker(const ContainerNode& searchRootNode, Checker selectorChecker, OutputType& output, const SelectorData& selectorData) const
{
    for (auto& element : descendantsOfType<Element>(const_cast<ContainerNode&>(searchRootNode))) {
        selectorData.compiledSelector.wasUsed();

        if (selectorChecker(&element)) {
            appendOutputForElement(output, element);
            if constexpr (std::is_same_v<OutputType, Element*>)
                return;
        }
    }
}

template<typename Checker, typename OutputType>
ALWAYS_INLINE void SelectorDataList::executeCompiledSelectorCheckerWithCheckingContext(const ContainerNode& rootNode, const ContainerNode& searchRootNode, Checker selectorChecker, OutputType& output, const SelectorData& selectorData) const
{
    SelectorChecker::CheckingContext checkingContext(SelectorChecker::Mode::QueryingRules);
    checkingContext.scope = rootNode.isDocumentNode() ? nullptr : &rootNode;

    for (auto& element : descendantsOfType<Element>(const_cast<ContainerNode&>(searchRootNode))) {
        selectorData.compiledSelector.wasUsed();

        if (selectorChecker(&element, &checkingContext)) {
            appendOutputForElement(output, element);
            if constexpr (std::is_same_v<OutputType, Element*>)
                return;
        }
    }
}

template<typename OutputType>
ALWAYS_INLINE void SelectorDataList::executeCompiledSingleMultiSelectorData(const ContainerNode& rootNode, OutputType& output) const
{
    SelectorChecker::CheckingContext checkingContext(SelectorChecker::Mode::QueryingRules);
    checkingContext.scope = rootNode.isDocumentNode() ? nullptr : &rootNode;
    for (auto& element : descendantsOfType<Element>(const_cast<ContainerNode&>(rootNode))) {
        for (auto& selector : m_selectors) {
            selector.compiledSelector.wasUsed();

            bool matched = false;
            if (selector.compiledSelector.status == SelectorCompilationStatus::SimpleSelectorChecker)
                matched = SelectorCompiler::querySelectorSimpleSelectorChecker(selector.compiledSelector, &element);
            else {
                ASSERT(selector.compiledSelector.status == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);
                matched = SelectorCompiler::querySelectorSelectorCheckerWithCheckingContext(selector.compiledSelector, &element, &checkingContext);
            }
            if (matched) {
                appendOutputForElement(output, element);
                if constexpr (std::is_same_v<OutputType, Element*>)
                    return;
                break;
            }
        }
    }
}

bool SelectorDataList::compileSelector(const SelectorData& selectorData)
{
    auto& compiledSelector = selectorData.compiledSelector;
    
    if (compiledSelector.status == SelectorCompilationStatus::NotCompiled)
        SelectorCompiler::compileSelector(compiledSelector, selectorData.selector, SelectorCompiler::SelectorContext::QuerySelector);

    return compiledSelector.status != SelectorCompilationStatus::CannotCompile;
}

#endif // ENABLE(CSS_SELECTOR_JIT)

template<typename OutputType>
ALWAYS_INLINE void SelectorDataList::execute(ContainerNode& rootNode, OutputType& output) const
{
    ContainerNode* searchRootNode = &rootNode;
    switch (m_matchType) {
    case RightMostWithIdMatch:
        {
        const SelectorData& selectorData = m_selectors.first();
        if (const CSSSelector* idSelector = selectorForIdLookup(*searchRootNode, *selectorData.selector)) {
            executeFastPathForIdSelector(*searchRootNode, m_selectors.first(), idSelector, output);
            break;
        }
#if ENABLE(CSS_SELECTOR_JIT)
        if (compileSelector(selectorData))
            goto CompiledSingleCase;
#endif // ENABLE(CSS_SELECTOR_JIT)
        goto SingleSelectorCase;
        ASSERT_NOT_REACHED();
        }

    case CompilableSingleWithRootFilter:
    case CompilableSingle:
        {
#if ENABLE(CSS_SELECTOR_JIT)
        const SelectorData& selectorData = m_selectors.first();
        ASSERT(selectorData.compiledSelector.status == SelectorCompilationStatus::NotCompiled);
        ASSERT(m_matchType == CompilableSingle || m_matchType == CompilableSingleWithRootFilter);
        if (compileSelector(selectorData)) {
            if (m_matchType == CompilableSingle) {
                m_matchType = CompiledSingle;
                goto CompiledSingleCase;
            }
            ASSERT(m_matchType == CompilableSingleWithRootFilter);
            m_matchType = CompiledSingleWithRootFilter;
            goto CompiledSingleWithRootFilterCase;
        }
#endif // ENABLE(CSS_SELECTOR_JIT)
        if (m_matchType == CompilableSingle) {
            m_matchType = SingleSelector;
            goto SingleSelectorCase;
        }
        ASSERT(m_matchType == CompilableSingleWithRootFilter);
        m_matchType = SingleSelectorWithRootFilter;
        goto SingleSelectorWithRootFilterCase;
        ASSERT_NOT_REACHED();
        }

#if ENABLE(CSS_SELECTOR_JIT)
    case CompiledSingleWithRootFilter:
        CompiledSingleWithRootFilterCase:
        searchRootNode = &filterRootById(*searchRootNode, *m_selectors.first().selector);
        FALLTHROUGH;
    case CompiledSingle:
        {
        CompiledSingleCase:
        const SelectorData& selectorData = m_selectors.first();
        if (selectorData.compiledSelector.status == SelectorCompilationStatus::SimpleSelectorChecker) {
            executeCompiledSimpleSelectorChecker(*searchRootNode, [&] (const Element* element) {
                return SelectorCompiler::querySelectorSimpleSelectorChecker(selectorData.compiledSelector, element);
            }, output, selectorData);
        } else {
            ASSERT(selectorData.compiledSelector.status == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);
            executeCompiledSelectorCheckerWithCheckingContext(rootNode, *searchRootNode, [&] (const Element* element, const SelectorChecker::CheckingContext* context) {
                return SelectorCompiler::querySelectorSelectorCheckerWithCheckingContext(selectorData.compiledSelector, element, context);
            }, output, selectorData);
        }
        break;
        }
#else
    case CompiledSingleWithRootFilter:
    case CompiledSingle:
        ASSERT_NOT_REACHED();
#if !ASSERT_ENABLED
        FALLTHROUGH;
#endif
#endif // ENABLE(CSS_SELECTOR_JIT)

    case SingleSelectorWithRootFilter:
        SingleSelectorWithRootFilterCase:
        searchRootNode = &filterRootById(*searchRootNode, *m_selectors.first().selector);
        FALLTHROUGH;
    case SingleSelector:
        SingleSelectorCase:
        executeSingleSelectorData(rootNode, *searchRootNode, m_selectors.first(), output);
        break;

    case TagNameMatch:
        executeSingleTagNameSelectorData(*searchRootNode, m_selectors.first(), output);
        break;
    case ClassNameMatch:
        executeSingleClassNameSelectorData(*searchRootNode, m_selectors.first(), output);
        break;
    case AttributeExactMatch:
        executeSingleAttributeExactSelectorData(*searchRootNode, m_selectors.first(), output);
        break;
    case CompilableMultipleSelectorMatch:
#if ENABLE(CSS_SELECTOR_JIT)
        {
        for (auto& selector : m_selectors) {
            if (!compileSelector(selector)) {
                m_matchType = MultipleSelectorMatch;
                goto MultipleSelectorMatch;
            }
        }
        m_matchType = CompiledMultipleSelectorMatch;
        goto CompiledMultipleSelectorMatch;
        }
#else
        FALLTHROUGH;
#endif // ENABLE(CSS_SELECTOR_JIT)
    case CompiledMultipleSelectorMatch:
#if ENABLE(CSS_SELECTOR_JIT)
        CompiledMultipleSelectorMatch:
        executeCompiledSingleMultiSelectorData(*searchRootNode, output);
        break;
#else
        FALLTHROUGH;
#endif // ENABLE(CSS_SELECTOR_JIT)
    case MultipleSelectorMatch:
#if ENABLE(CSS_SELECTOR_JIT)
        MultipleSelectorMatch:
#endif
        executeSingleMultiSelectorData(*searchRootNode, output);
        break;
    }
}

SelectorQuery::SelectorQuery(CSSSelectorList&& selectorList)
    : m_selectorList(WTFMove(selectorList))
    , m_selectors(m_selectorList)
{
}

SelectorQueryCache& SelectorQueryCache::singleton()
{
    static NeverDestroyed<SelectorQueryCache> cache;
    return cache.get();
}

SelectorQuery* SelectorQueryCache::add(const String& selectors, const Document& document)
{
    ASSERT(!selectors.isEmpty());

    constexpr auto maximumSelectorQueryCacheSize = 512;
    if (m_entries.size() == maximumSelectorQueryCacheSize)
        m_entries.remove(m_entries.random());

    auto context = CSSSelectorParserContext { document };
    auto key = Key { selectors, context, document.securityOrigin().data() };

    return m_entries.ensure(key, [&]() -> std::unique_ptr<SelectorQuery> {
        auto tokenizer = CSSTokenizer { selectors };
        auto selectorList = parseCSSSelectorList(tokenizer.tokenRange(), context);

        if (!selectorList)
            return nullptr;

        if (selectorList->hasExplicitNestingParent())
            selectorList = CSSSelectorParser::resolveNestingParent(WTFMove(*selectorList), nullptr);

        return makeUnique<SelectorQuery>(WTFMove(*selectorList));
    }).iterator->value.get();
}

void SelectorQueryCache::clear()
{
    m_entries.clear();
}

}
