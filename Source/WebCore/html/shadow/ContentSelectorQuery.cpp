/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContentSelectorQuery.h"

#include "CSSSelectorList.h"
#include "InsertionPoint.h"
#include "SelectorChecker.h"
#include "ShadowRoot.h"
#include "SiblingTraversalStrategies.h"

namespace WebCore {

ContentSelectorChecker::ContentSelectorChecker(Document* document)
    : m_selectorChecker(document)
{
    m_selectorChecker.setMode(SelectorChecker::CollectingRules);
}

bool ContentSelectorChecker::checkContentSelector(CSSSelector* selector, const Vector<RefPtr<Node> >& siblings, int nth) const
{
    SelectorChecker::SelectorCheckingContext context(selector, toElement(siblings[nth].get()), SelectorChecker::VisitedMatchEnabled);
    ShadowDOMSiblingTraversalStrategy strategy(siblings, nth);
    return m_selectorChecker.checkOne(context, strategy);
}

void ContentSelectorDataList::initialize(const CSSSelectorList& selectors)
{
    for (CSSSelector* selector = selectors.first(); selector; selector = CSSSelectorList::next(selector))
        m_selectors.append(selector);
}

bool ContentSelectorDataList::matches(const ContentSelectorChecker& selectorChecker, const Vector<RefPtr<Node> >& siblings, int nth) const
{
    unsigned selectorCount = m_selectors.size();
    for (unsigned i = 0; i < selectorCount; ++i) {
        if (selectorChecker.checkContentSelector(m_selectors[i], siblings, nth))
            return true;
    }
    return false;
}

ContentSelectorQuery::ContentSelectorQuery(InsertionPoint* insertionPoint)
    : m_insertionPoint(insertionPoint)
    , m_selectorChecker(insertionPoint->document())
{
    m_selectors.initialize(insertionPoint->selectorList());
}

bool ContentSelectorQuery::matches(const Vector<RefPtr<Node> >& siblings, int nth) const
{
    Node* node = siblings[nth].get();
    ASSERT(node);

    switch (m_insertionPoint->matchTypeFor(node)) {
    case InsertionPoint::AlwaysMatches:
        return true;
    case InsertionPoint::NeverMatches:
        return false;
    case InsertionPoint::HasToMatchSelector:
        return node->isElementNode() && m_selectors.matches(m_selectorChecker, siblings, nth);
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

}
