/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#include "CSSParser.h"
#include "ElementDescendantIterator.h"
#include "HTMLNames.h"
#include "SelectorChecker.h"
#include "StaticNodeList.h"
#include "StyledElement.h"

namespace WebCore {

#if !ASSERT_DISABLED
static bool isSingleTagNameSelector(const CSSSelector& selector)
{
    return selector.isLastInTagHistory() && selector.match() == CSSSelector::Tag;
}

static bool isSingleClassNameSelector(const CSSSelector& selector)
{
    return selector.isLastInTagHistory() && selector.match() == CSSSelector::Class;
}
#endif

enum class IdMatchingType : uint8_t {
    None,
    Rightmost,
    Filter
};

static bool canBeUsedForIdFastPath(const CSSSelector& selector)
{
    return selector.match() == CSSSelector::Id
        || (selector.match() == CSSSelector::Exact && selector.attribute() == HTMLNames::idAttr && !selector.attributeValueMatchingIsCaseInsensitive());
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
        if (selector->relation() != CSSSelector::Subselector)
            inRightmost = false;
    }
    return IdMatchingType::None;
}

SelectorDataList::SelectorDataList(const CSSSelectorList& selectorList)
{
    unsigned selectorCount = 0;
    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector))
        selectorCount++;

    m_selectors.reserveInitialCapacity(selectorCount);
    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector))
        m_selectors.uncheckedAppend(SelectorData(selector));

    if (selectorCount == 1) {
        const CSSSelector& selector = *m_selectors.first().selector;
        if (selector.isLastInTagHistory()) {
            switch (selector.match()) {
            case CSSSelector::Tag:
                m_matchType = TagNameMatch;
                break;
            case CSSSelector::Class:
                m_matchType = ClassNameMatch;
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
    unsigned ignoredSpecificity;
    return selectorChecker.match(*selectorData.selector, element, selectorCheckingContext, ignoredSpecificity);
}

inline Element* SelectorDataList::selectorClosest(const SelectorData& selectorData, Element& element, const ContainerNode& rootNode) const
{
    SelectorChecker selectorChecker(element.document());
    SelectorChecker::CheckingContext selectorCheckingContext(SelectorChecker::Mode::QueryingRules);
    selectorCheckingContext.scope = rootNode.isDocumentNode() ? nullptr : &rootNode;
    unsigned ignoredSpecificity;
    if (!selectorChecker.match(*selectorData.selector, element, selectorCheckingContext, ignoredSpecificity))
        return nullptr;
    return &element;
}

bool SelectorDataList::matches(Element& targetElement) const
{
    for (auto& selctor : m_selectors) {
        if (selectorMatches(selctor, targetElement, targetElement))
            return true;
    }
    return false;
}

Element* SelectorDataList::closest(Element& targetElement) const
{
    Element* currentNode = &targetElement;
    do {
        for (auto& selector : m_selectors) {
            Element* candidateElement = selectorClosest(selector, *currentNode, targetElement);
            if (candidateElement)
                return candidateElement;
        }
        currentNode = currentNode->parentElement();
    } while (currentNode);
    return nullptr;
}

struct AllElementExtractorSelectorQueryTrait {
    typedef Vector<Ref<Element>> OutputType;
    static const bool shouldOnlyMatchFirstElement = false;
    ALWAYS_INLINE static void appendOutputForElement(OutputType& output, Element* element) { ASSERT(element); output.append(*element); }
};

Ref<NodeList> SelectorDataList::queryAll(ContainerNode& rootNode) const
{
    Vector<Ref<Element>> result;
    execute<AllElementExtractorSelectorQueryTrait>(rootNode, result);
    return StaticElementList::create(WTFMove(result));
}

struct SingleElementExtractorSelectorQueryTrait {
    typedef Element* OutputType;
    static const bool shouldOnlyMatchFirstElement = true;
    ALWAYS_INLINE static void appendOutputForElement(OutputType& output, Element* element)
    {
        ASSERT(element);
        ASSERT(!output);
        output = element;
    }
};

Element* SelectorDataList::queryFirst(ContainerNode& rootNode) const
{
    Element* result = nullptr;
    execute<SingleElementExtractorSelectorQueryTrait>(rootNode, result);
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
        if (selector->relation() != CSSSelector::Subselector)
            break;
    }

    return nullptr;
}

static inline bool isTreeScopeRoot(const ContainerNode& node)
{
    return node.isDocumentNode() || node.isShadowRoot();
}

template <typename SelectorQueryTrait>
ALWAYS_INLINE void SelectorDataList::executeFastPathForIdSelector(const ContainerNode& rootNode, const SelectorData& selectorData, const CSSSelector* idSelector, typename SelectorQueryTrait::OutputType& output) const
{
    ASSERT(m_selectors.size() == 1);
    ASSERT(idSelector);

    const AtomString& idToMatch = idSelector->value();
    if (UNLIKELY(rootNode.treeScope().containsMultipleElementsWithId(idToMatch))) {
        const Vector<Element*>* elements = rootNode.treeScope().getAllElementsById(idToMatch);
        ASSERT(elements);
        bool rootNodeIsTreeScopeRoot = isTreeScopeRoot(rootNode);
        for (auto& element : *elements) {
            if ((rootNodeIsTreeScopeRoot || element->isDescendantOf(rootNode)) && selectorMatches(selectorData, *element, rootNode)) {
                SelectorQueryTrait::appendOutputForElement(output, element);
                if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                    return;
            }
        }
        return;
    }

    Element* element = rootNode.treeScope().getElementById(idToMatch);
    if (!element || !(isTreeScopeRoot(rootNode) || element->isDescendantOf(rootNode)))
        return;
    if (selectorMatches(selectorData, *element, rootNode))
        SelectorQueryTrait::appendOutputForElement(output, element);
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
        if (selector->relation() != CSSSelector::Subselector)
            break;
        selector = selector->tagHistory();
    } while (selector);

    bool inAdjacentChain = false;
    for (; selector; selector = selector->tagHistory()) {
        if (canBeUsedForIdFastPath(*selector)) {
            const AtomString& idToMatch = selector->value();
            if (ContainerNode* searchRoot = rootNode.treeScope().getElementById(idToMatch)) {
                if (LIKELY(!rootNode.treeScope().containsMultipleElementsWithId(idToMatch))) {
                    if (inAdjacentChain)
                        searchRoot = searchRoot->parentNode();
                    if (searchRoot && (isTreeScopeRoot(rootNode) || searchRoot == &rootNode || searchRoot->isDescendantOf(rootNode)))
                        return *searchRoot;
                }
            }
        }
        if (selector->relation() == CSSSelector::Subselector)
            continue;
        if (selector->relation() == CSSSelector::DirectAdjacent || selector->relation() == CSSSelector::IndirectAdjacent)
            inAdjacentChain = true;
        else
            inAdjacentChain = false;
    }
    return rootNode;
}

static ALWAYS_INLINE bool localNameMatches(const Element& element, const AtomString& localName, const AtomString& lowercaseLocalName)
{
    if (element.isHTMLElement() && element.document().isHTMLDocument())
        return element.localName() == lowercaseLocalName;
    return element.localName() == localName;

}

template <typename SelectorQueryTrait>
static inline void elementsForLocalName(const ContainerNode& rootNode, const AtomString& localName, const AtomString& lowercaseLocalName, typename SelectorQueryTrait::OutputType& output)
{
    if (localName == lowercaseLocalName) {
        for (auto& element : elementDescendants(const_cast<ContainerNode&>(rootNode))) {
            if (element.tagQName().localName() == localName) {
                SelectorQueryTrait::appendOutputForElement(output, &element);
                if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                return;
            }
        }
    } else {
        for (auto& element : elementDescendants(const_cast<ContainerNode&>(rootNode))) {
            if (localNameMatches(element, localName, lowercaseLocalName)) {
                SelectorQueryTrait::appendOutputForElement(output, &element);
                if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                return;
            }
        }
    }
}

template <typename SelectorQueryTrait>
static inline void anyElement(const ContainerNode& rootNode, typename SelectorQueryTrait::OutputType& output)
{
    for (auto& element : elementDescendants(const_cast<ContainerNode&>(rootNode))) {
        SelectorQueryTrait::appendOutputForElement(output, &element);
        if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
            return;
    }
}


template <typename SelectorQueryTrait>
ALWAYS_INLINE void SelectorDataList::executeSingleTagNameSelectorData(const ContainerNode& rootNode, const SelectorData& selectorData, typename SelectorQueryTrait::OutputType& output) const
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
            elementsForLocalName<SelectorQueryTrait>(rootNode, selectorLocalName, selectorLowercaseLocalName, output);
        } else {
            // Other fairly common case: both are wildcards.
            anyElement<SelectorQueryTrait>(rootNode, output);
        }
    } else {
        // Fallback: NamespaceURI is set, selectorLocalName may be starAtom().
        for (auto& element : elementDescendants(const_cast<ContainerNode&>(rootNode))) {
            if (element.namespaceURI() == selectorNamespaceURI && localNameMatches(element, selectorLocalName, selectorLowercaseLocalName)) {
                SelectorQueryTrait::appendOutputForElement(output, &element);
                if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                    return;
            }
        }
    }
}

template <typename SelectorQueryTrait>
ALWAYS_INLINE void SelectorDataList::executeSingleClassNameSelectorData(const ContainerNode& rootNode, const SelectorData& selectorData, typename SelectorQueryTrait::OutputType& output) const
{
    ASSERT(m_selectors.size() == 1);
    ASSERT(isSingleClassNameSelector(*selectorData.selector));

    const AtomString& className = selectorData.selector->value();
    for (auto& element : elementDescendants(const_cast<ContainerNode&>(rootNode))) {
        if (element.hasClass() && element.classNames().contains(className)) {
            SelectorQueryTrait::appendOutputForElement(output, &element);
            if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                return;
        }
    }
}

template <typename SelectorQueryTrait>
ALWAYS_INLINE void SelectorDataList::executeSingleSelectorData(const ContainerNode& rootNode, const ContainerNode& searchRootNode, const SelectorData& selectorData, typename SelectorQueryTrait::OutputType& output) const
{
    ASSERT(m_selectors.size() == 1);

    for (auto& element : elementDescendants(const_cast<ContainerNode&>(searchRootNode))) {
        if (selectorMatches(selectorData, element, rootNode)) {
            SelectorQueryTrait::appendOutputForElement(output, &element);
            if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                return;
        }
    }
}

template <typename SelectorQueryTrait>
ALWAYS_INLINE void SelectorDataList::executeSingleMultiSelectorData(const ContainerNode& rootNode, typename SelectorQueryTrait::OutputType& output) const
{
    for (auto& element : elementDescendants(const_cast<ContainerNode&>(rootNode))) {
        for (auto& selector : m_selectors) {
            if (selectorMatches(selector, element, rootNode)) {
                SelectorQueryTrait::appendOutputForElement(output, &element);
                if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                    return;
                break;
            }
        }
    }
}

#if ENABLE(CSS_SELECTOR_JIT)
template <typename SelectorQueryTrait>
ALWAYS_INLINE void SelectorDataList::executeCompiledSimpleSelectorChecker(const ContainerNode& searchRootNode, SelectorCompiler::QuerySelectorSimpleSelectorChecker selectorChecker, typename SelectorQueryTrait::OutputType& output, const SelectorData& selectorData) const
{
    for (auto& element : elementDescendants(const_cast<ContainerNode&>(searchRootNode))) {
#if CSS_SELECTOR_JIT_PROFILING
        selectorData.compiledSelectorUsed();
#else
        UNUSED_PARAM(selectorData);
#endif
        if (selectorChecker(&element)) {
            SelectorQueryTrait::appendOutputForElement(output, &element);
            if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                return;
        }
    }
}

template <typename SelectorQueryTrait>
ALWAYS_INLINE void SelectorDataList::executeCompiledSelectorCheckerWithCheckingContext(const ContainerNode& rootNode, const ContainerNode& searchRootNode, SelectorCompiler::QuerySelectorSelectorCheckerWithCheckingContext selectorChecker, typename SelectorQueryTrait::OutputType& output, const SelectorData& selectorData) const
{
    SelectorChecker::CheckingContext checkingContext(SelectorChecker::Mode::QueryingRules);
    checkingContext.scope = rootNode.isDocumentNode() ? nullptr : &rootNode;

    for (auto& element : elementDescendants(const_cast<ContainerNode&>(searchRootNode))) {
#if CSS_SELECTOR_JIT_PROFILING
        selectorData.compiledSelectorUsed();
#else
        UNUSED_PARAM(selectorData);
#endif
        if (selectorChecker(&element, &checkingContext)) {
            SelectorQueryTrait::appendOutputForElement(output, &element);
            if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                return;
        }
    }
}

template <typename SelectorQueryTrait>
ALWAYS_INLINE void SelectorDataList::executeCompiledSingleMultiSelectorData(const ContainerNode& rootNode, typename SelectorQueryTrait::OutputType& output) const
{
    SelectorChecker::CheckingContext checkingContext(SelectorChecker::Mode::QueryingRules);
    checkingContext.scope = rootNode.isDocumentNode() ? nullptr : &rootNode;
    for (auto& element : elementDescendants(const_cast<ContainerNode&>(rootNode))) {
        for (auto& selector : m_selectors) {
#if CSS_SELECTOR_JIT_PROFILING
            selector.compiledSelectorUsed();
#endif
            bool matched = false;
            void* compiledSelectorChecker = selector.compiledSelectorCodeRef.code().executableAddress();
            if (selector.compilationStatus == SelectorCompilationStatus::SimpleSelectorChecker) {
                auto selectorChecker = SelectorCompiler::querySelectorSimpleSelectorCheckerFunction(compiledSelectorChecker, selector.compilationStatus);
                matched = selectorChecker(&element);
            } else {
                ASSERT(selector.compilationStatus == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);
                auto selectorChecker = SelectorCompiler::querySelectorSelectorCheckerFunctionWithCheckingContext(compiledSelectorChecker, selector.compilationStatus);
                matched = selectorChecker(&element, &checkingContext);
            }
            if (matched) {
                SelectorQueryTrait::appendOutputForElement(output, &element);
                if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                    return;
                break;
            }
        }
    }
}

static bool isCompiledSelector(SelectorCompilationStatus compilationStatus)
{
    return compilationStatus == SelectorCompilationStatus::SimpleSelectorChecker || compilationStatus == SelectorCompilationStatus::SelectorCheckerWithCheckingContext;
}

bool SelectorDataList::compileSelector(const SelectorData& selectorData)
{
    if (selectorData.compilationStatus != SelectorCompilationStatus::NotCompiled)
        return isCompiledSelector(selectorData.compilationStatus);

    selectorData.compilationStatus = SelectorCompiler::compileSelector(selectorData.selector, SelectorCompiler::SelectorContext::QuerySelector, selectorData.compiledSelectorCodeRef);
    return isCompiledSelector(selectorData.compilationStatus);
}


#endif // ENABLE(CSS_SELECTOR_JIT)

template <typename SelectorQueryTrait>
ALWAYS_INLINE void SelectorDataList::execute(ContainerNode& rootNode, typename SelectorQueryTrait::OutputType& output) const
{
    ContainerNode* searchRootNode = &rootNode;
    switch (m_matchType) {
    case RightMostWithIdMatch:
        {
        const SelectorData& selectorData = m_selectors.first();
        if (const CSSSelector* idSelector = selectorForIdLookup(*searchRootNode, *selectorData.selector)) {
            executeFastPathForIdSelector<SelectorQueryTrait>(*searchRootNode, m_selectors.first(), idSelector, output);
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
        ASSERT(selectorData.compilationStatus == SelectorCompilationStatus::NotCompiled);
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
        void* compiledSelectorChecker = selectorData.compiledSelectorCodeRef.code().executableAddress();
        if (selectorData.compilationStatus == SelectorCompilationStatus::SimpleSelectorChecker) {
            SelectorCompiler::QuerySelectorSimpleSelectorChecker selectorChecker = SelectorCompiler::querySelectorSimpleSelectorCheckerFunction(compiledSelectorChecker, selectorData.compilationStatus);
            executeCompiledSimpleSelectorChecker<SelectorQueryTrait>(*searchRootNode, selectorChecker, output, selectorData);
        } else {
            ASSERT(selectorData.compilationStatus == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);
            SelectorCompiler::QuerySelectorSelectorCheckerWithCheckingContext selectorChecker = SelectorCompiler::querySelectorSelectorCheckerFunctionWithCheckingContext(compiledSelectorChecker, selectorData.compilationStatus);
            executeCompiledSelectorCheckerWithCheckingContext<SelectorQueryTrait>(rootNode, *searchRootNode, selectorChecker, output, selectorData);
        }
        break;
        }
#else
    case CompiledSingleWithRootFilter:
    case CompiledSingle:
        ASSERT_NOT_REACHED();
#if ASSERT_DISABLED
        FALLTHROUGH;
#endif
#endif // ENABLE(CSS_SELECTOR_JIT)

    case SingleSelectorWithRootFilter:
        SingleSelectorWithRootFilterCase:
        searchRootNode = &filterRootById(*searchRootNode, *m_selectors.first().selector);
        FALLTHROUGH;
    case SingleSelector:
        SingleSelectorCase:
        executeSingleSelectorData<SelectorQueryTrait>(rootNode, *searchRootNode, m_selectors.first(), output);
        break;

    case TagNameMatch:
        executeSingleTagNameSelectorData<SelectorQueryTrait>(*searchRootNode, m_selectors.first(), output);
        break;
    case ClassNameMatch:
        executeSingleClassNameSelectorData<SelectorQueryTrait>(*searchRootNode, m_selectors.first(), output);
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
        executeCompiledSingleMultiSelectorData<SelectorQueryTrait>(*searchRootNode, output);
        break;
#else
        FALLTHROUGH;
#endif // ENABLE(CSS_SELECTOR_JIT)
    case MultipleSelectorMatch:
#if ENABLE(CSS_SELECTOR_JIT)
        MultipleSelectorMatch:
#endif
        executeSingleMultiSelectorData<SelectorQueryTrait>(*searchRootNode, output);
        break;
    }
}

SelectorQuery::SelectorQuery(CSSSelectorList&& selectorList)
    : m_selectorList(WTFMove(selectorList))
    , m_selectors(m_selectorList)
{
}

ExceptionOr<SelectorQuery&> SelectorQueryCache::add(const String& selectors, Document& document)
{
    if (auto* entry = m_entries.get(selectors))
        return *entry;

    CSSParser parser(document);
    CSSSelectorList selectorList;
    parser.parseSelector(selectors, selectorList);

    if (!selectorList.first() || selectorList.hasInvalidSelector())
        return Exception { SyntaxError };

    if (selectorList.selectorsNeedNamespaceResolution())
        return Exception { SyntaxError };

    const int maximumSelectorQueryCacheSize = 256;
    if (m_entries.size() == maximumSelectorQueryCacheSize)
        m_entries.remove(m_entries.random());

    return *m_entries.add(selectors, makeUnique<SelectorQuery>(WTFMove(selectorList))).iterator->value;
}

}
