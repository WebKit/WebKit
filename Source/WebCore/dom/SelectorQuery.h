/*
 * Copyright (C) 2011, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef SelectorQuery_h
#define SelectorQuery_h

#include "CSSSelectorList.h"
#include "NodeList.h"
#include "SelectorCompiler.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

typedef int ExceptionCode;
    
class CSSSelector;
class ContainerNode;
class Document;
class Element;
class Node;
class NodeList;

class SelectorDataList {
public:
    explicit SelectorDataList(const CSSSelectorList&);
    bool matches(Element&) const;
    RefPtr<NodeList> queryAll(ContainerNode& rootNode) const;
    Element* queryFirst(ContainerNode& rootNode) const;

private:
    struct SelectorData {
        SelectorData(const CSSSelector* selector, bool isFastCheckable)
            : selector(selector)
            , isFastCheckable(isFastCheckable)
        {
        }

        const CSSSelector* selector;
#if ENABLE(CSS_SELECTOR_JIT)
        mutable JSC::MacroAssemblerCodeRef compiledSelectorCodeRef;
        mutable SelectorCompilationStatus compilationStatus;
#endif // ENABLE(CSS_SELECTOR_JIT)
        bool isFastCheckable;
    };

    bool selectorMatches(const SelectorData&, Element&, const ContainerNode& rootNode) const;

    template <typename SelectorQueryTrait> void execute(ContainerNode& rootNode, typename SelectorQueryTrait::OutputType&) const;
    template <typename SelectorQueryTrait> void executeFastPathForIdSelector(const ContainerNode& rootNode, const SelectorData&, const CSSSelector* idSelector, typename SelectorQueryTrait::OutputType&) const;
    template <typename SelectorQueryTrait> void executeSingleTagNameSelectorData(const ContainerNode& rootNode, const SelectorData&, typename SelectorQueryTrait::OutputType&) const;
    template <typename SelectorQueryTrait> void executeSingleClassNameSelectorData(const ContainerNode& rootNode, const SelectorData&, typename SelectorQueryTrait::OutputType&) const;
    template <typename SelectorQueryTrait> void executeSingleSelectorData(const ContainerNode& rootNode, const SelectorData&, typename SelectorQueryTrait::OutputType&) const;
    template <typename SelectorQueryTrait> void executeSingleMultiSelectorData(const ContainerNode& rootNode, typename SelectorQueryTrait::OutputType&) const;
#if ENABLE(CSS_SELECTOR_JIT)
    template <typename SelectorQueryTrait> void executeCompiledSimpleSelectorChecker(const ContainerNode& rootNode, SelectorCompiler::SimpleSelectorChecker, typename SelectorQueryTrait::OutputType&) const;
#endif // ENABLE(CSS_SELECTOR_JIT)

    Vector<SelectorData> m_selectors;
    mutable enum MatchType {
        CompilableSingle,
        CompilableSingleWithRootFilter,
        CompiledSingle,
        CompiledSingleWithRootFilter,
        SingleSelector,
        RightMostWithIdMatch,
        TagNameMatch,
        ClassNameMatch,
        MultipleSelectorMatch,
    } m_matchType;
};

class SelectorQuery {
    WTF_MAKE_NONCOPYABLE(SelectorQuery);
    WTF_MAKE_FAST_ALLOCATED;

public:
    explicit SelectorQuery(CSSSelectorList&&);
    bool matches(Element&) const;
    RefPtr<NodeList> queryAll(ContainerNode& rootNode) const;
    Element* queryFirst(ContainerNode& rootNode) const;

private:
    CSSSelectorList m_selectorList;
    SelectorDataList m_selectors;
};

class SelectorQueryCache {
    WTF_MAKE_FAST_ALLOCATED;

public:
    SelectorQuery* add(const String&, Document&, ExceptionCode&);
    void invalidate();

private:
    HashMap<String, std::unique_ptr<SelectorQuery>> m_entries;
};

inline bool SelectorQuery::matches(Element& element) const
{
    return m_selectors.matches(element);
}

inline RefPtr<NodeList> SelectorQuery::queryAll(ContainerNode& rootNode) const
{
    return m_selectors.queryAll(rootNode);
}

inline Element* SelectorQuery::queryFirst(ContainerNode& rootNode) const
{
    return m_selectors.queryFirst(rootNode);
}

}

#endif
