/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSSelectorList.h"
#include "CSSSelectorParser.h"
#include "ExceptionOr.h"
#include "NodeList.h"
#include "SecurityOriginData.h"
#include "SelectorCompiler.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class CSSSelector;
class ContainerNode;
class Document;
class Element;

class SelectorDataList {
public:
    explicit SelectorDataList(const CSSSelectorList&);
    bool matches(Element&) const;
    Element* closest(Element&) const;
    Ref<NodeList> queryAll(ContainerNode& rootNode) const;
    Element* queryFirst(ContainerNode& rootNode) const;

    bool shouldStoreInDocument() const { return m_matchType == MatchType::TagNameMatch || m_matchType == MatchType::ClassNameMatch; }
    AtomString classNameToMatch() const;

private:
    struct SelectorData {
        const CSSSelector* selector;
#if ENABLE(CSS_SELECTOR_JIT)
        mutable CompiledSelector compiledSelector { };
#endif
    };

    bool selectorMatches(const SelectorData&, Element&, const ContainerNode& rootNode) const;
    Element* selectorClosest(const SelectorData&, Element&, const ContainerNode& rootNode) const;

    template <typename OutputType> void execute(ContainerNode& rootNode, OutputType&) const;
    template <typename OutputType> void executeFastPathForIdSelector(const ContainerNode& rootNode, const SelectorData&, const CSSSelector* idSelector, OutputType&) const;
    template <typename OutputType> void executeSingleTagNameSelectorData(const ContainerNode& rootNode, const SelectorData&, OutputType&) const;
    template <typename OutputType> void executeSingleClassNameSelectorData(const ContainerNode& rootNode, const SelectorData&, OutputType&) const;
    template <typename OutputType> void executeSingleAttributeExactSelectorData(const ContainerNode& rootNode, const SelectorData&, OutputType&) const;
    template <typename OutputType> void executeSingleSelectorData(const ContainerNode& rootNode, const ContainerNode& searchRootNode, const SelectorData&, OutputType&) const;
    template <typename OutputType> void executeSingleMultiSelectorData(const ContainerNode& rootNode, OutputType&) const;
#if ENABLE(CSS_SELECTOR_JIT)
    template <typename Checker, typename OutputType> void executeCompiledSimpleSelectorChecker(const ContainerNode& searchRootNode, Checker, OutputType&, const SelectorData&) const;
    template <typename Checker, typename OutputType> void executeCompiledSelectorCheckerWithCheckingContext(const ContainerNode& rootNode, const ContainerNode& searchRootNode, Checker, OutputType&, const SelectorData&) const;
    template <typename OutputType> void executeCompiledSingleMultiSelectorData(const ContainerNode& rootNode, OutputType&) const;
    static bool compileSelector(const SelectorData&);
#endif // ENABLE(CSS_SELECTOR_JIT)

    Vector<SelectorData> m_selectors;
    mutable enum MatchType {
        CompilableSingle,
        CompilableSingleWithRootFilter,
        CompilableMultipleSelectorMatch,
        CompiledSingle,
        CompiledSingleWithRootFilter,
        CompiledMultipleSelectorMatch,
        SingleSelector,
        SingleSelectorWithRootFilter,
        RightMostWithIdMatch,
        TagNameMatch,
        ClassNameMatch,
        AttributeExactMatch,
        MultipleSelectorMatch,
    } m_matchType;
};

class SelectorQuery {
    WTF_MAKE_NONCOPYABLE(SelectorQuery);
    WTF_MAKE_FAST_ALLOCATED;

public:
    explicit SelectorQuery(CSSSelectorList&&);
    bool matches(Element&) const;
    Element* closest(Element&) const;
    Ref<NodeList> queryAll(ContainerNode& rootNode) const;
    Element* queryFirst(ContainerNode& rootNode) const;

    bool shouldStoreInDocument() const { return m_selectors.shouldStoreInDocument(); }
    AtomString classNameToMatch() const { return m_selectors.classNameToMatch(); }

private:
    CSSSelectorList m_selectorList;
    SelectorDataList m_selectors;
};

class SelectorQueryCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static SelectorQueryCache& singleton();

    SelectorQuery* add(const String&, const Document&);
    void clear();

private:
    using Key = std::tuple<String, CSSSelectorParserContext, SecurityOriginData>;
    HashMap<Key, std::unique_ptr<SelectorQuery>> m_entries;
};

inline bool SelectorQuery::matches(Element& element) const
{
    return m_selectors.matches(element);
}

inline Element* SelectorQuery::closest(Element& element) const
{
    return m_selectors.closest(element);
}

inline Ref<NodeList> SelectorQuery::queryAll(ContainerNode& rootNode) const
{
    return m_selectors.queryAll(rootNode);
}

inline Element* SelectorQuery::queryFirst(ContainerNode& rootNode) const
{
    return m_selectors.queryFirst(rootNode);
}

} // namespace WebCore
