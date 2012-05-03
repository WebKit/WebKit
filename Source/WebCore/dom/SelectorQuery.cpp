/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "CSSSelectorList.h"
#include "Document.h"
#include "StaticNodeList.h"
#include "StyledElement.h"

namespace WebCore {

SelectorDataList::SelectorDataList()
{
}

SelectorDataList::SelectorDataList(const CSSSelectorList& selectorList)
{
    initialize(selectorList);
}

void SelectorDataList::initialize(const CSSSelectorList& selectorList)
{
    ASSERT(m_selectors.isEmpty());

    for (CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector))
        m_selectors.append(SelectorData(selector, SelectorChecker::isFastCheckableSelector(selector)));
}

bool SelectorDataList::matches(const SelectorChecker& selectorChecker, Element* targetElement) const
{
    ASSERT(targetElement);

    unsigned selectorCount = m_selectors.size();
    for (unsigned i = 0; i < selectorCount; ++i) {
        if (selectorChecker.checkSelector(m_selectors[i].selector, targetElement, m_selectors[i].isFastCheckable))
            return true;
    }

    return false;
}

PassRefPtr<NodeList> SelectorDataList::queryAll(const SelectorChecker& selectorChecker, Node* rootNode) const
{
    Vector<RefPtr<Node> > result;
    execute<false>(selectorChecker, rootNode, result);
    return StaticNodeList::adopt(result);
}

PassRefPtr<Element> SelectorDataList::queryFirst(const SelectorChecker& selectorChecker, Node* rootNode) const
{
    Vector<RefPtr<Node> > result;
    execute<true>(selectorChecker, rootNode, result);
    if (result.isEmpty())
        return 0;
    ASSERT(result.size() == 1);
    ASSERT(result.first()->isElementNode());
    return static_cast<Element*>(result.first().get());
}

bool SelectorDataList::canUseIdLookup(Node* rootNode) const
{
    // We need to return the matches in document order. To use id lookup while there is possiblity of multiple matches
    // we would need to sort the results. For now, just traverse the document in that case.
    if (m_selectors.size() != 1)
        return false;
    if (m_selectors[0].selector->m_match != CSSSelector::Id)
        return false;
    if (!rootNode->inDocument())
        return false;
    if (rootNode->document()->inQuirksMode())
        return false;
    if (rootNode->document()->containsMultipleElementsWithId(m_selectors[0].selector->value()))
        return false;
    return true;
}

static inline bool isTreeScopeRoot(Node* node)
{
    ASSERT(node);
    return node->isDocumentNode() || node->isShadowRoot();
}

template <bool firstMatchOnly>
void SelectorDataList::execute(const SelectorChecker& selectorChecker, Node* rootNode, Vector<RefPtr<Node> >& matchedElements) const
{
    if (canUseIdLookup(rootNode)) {
        ASSERT(m_selectors.size() == 1);
        CSSSelector* selector = m_selectors[0].selector;
        Element* element = rootNode->treeScope()->getElementById(selector->value());
        if (!element || !(isTreeScopeRoot(rootNode) || element->isDescendantOf(rootNode)))
            return;
        if (selectorChecker.checkSelector(m_selectors[0].selector, element, m_selectors[0].isFastCheckable))
            matchedElements.append(element);
        return;
    }

    unsigned selectorCount = m_selectors.size();

    Node* n = rootNode->firstChild();
    while (n) {
        if (n->isElementNode()) {
            Element* element = static_cast<Element*>(n);
            for (unsigned i = 0; i < selectorCount; ++i) {
                if (selectorChecker.checkSelector(m_selectors[i].selector, element, m_selectors[i].isFastCheckable)) {
                    matchedElements.append(element);
                    if (firstMatchOnly)
                        return;
                    break;
                }
            }
            if (element->firstChild()) {
                n = element->firstChild();
                continue;
            }
        }
        while (!n->nextSibling()) {
            n = n->parentNode();
            if (n == rootNode)
                return;
        }
        n = n->nextSibling();
    }
}

SelectorQuery::SelectorQuery(Node* rootNode, const CSSSelectorList& selectorList)
    : m_rootNode(rootNode)
    , m_selectorChecker(rootNode->document(), !rootNode->document()->inQuirksMode())
    , m_selectors(selectorList)
{
    m_selectorChecker.setMode(SelectorChecker::QueryingRules);
}

PassRefPtr<NodeList> SelectorQuery::queryAll() const
{
    return m_selectors.queryAll(m_selectorChecker, m_rootNode);
}

PassRefPtr<Element> SelectorQuery::queryFirst() const
{
    return m_selectors.queryFirst(m_selectorChecker, m_rootNode);
}

}
