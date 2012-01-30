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

#include "CSSParser.h"
#include "CSSSelectorList.h"
#include "HTMLContentElement.h"

namespace WebCore {

ContentSelectorQuery::ContentSelectorQuery(const HTMLContentElement* element)
    : m_contentElement(element)
    , m_selectorChecker(element->document(), !element->document()->inQuirksMode())
{
    m_selectorChecker.setCollectingRulesOnly(true);

    if (element->select().isNull() || element->select().isEmpty()) {
        m_isValidSelector = true;
        return;
    }

    CSSParser parser(true);
    parser.parseSelector(element->select(), element->document(), m_selectorList);

    m_isValidSelector = ContentSelectorQuery::validateSelectorList();
    if (m_isValidSelector)
        m_selectors.initialize(m_selectorList);
}

bool ContentSelectorQuery::isValidSelector() const
{
    return m_isValidSelector;
}

bool ContentSelectorQuery::matches(Node* node) const
{
    ASSERT(node);
    if (!node)
        return false;

    ASSERT(node->parentNode() == m_contentElement->shadowTreeRootNode()->shadowHost());

    if (m_contentElement->select().isNull() || m_contentElement->select().isEmpty())
        return true;

    if (!m_isValidSelector)
        return false;

    if (!node->isElementNode())
        return false;

    return m_selectors.matches(m_selectorChecker, toElement(node));
}

static bool validateSubSelector(CSSSelector* selector)
{
    switch (selector->m_match) {
    case CSSSelector::None:
    case CSSSelector::Id:
    case CSSSelector::Class:
    case CSSSelector::Exact:
    case CSSSelector::Set:
    case CSSSelector::List:
    case CSSSelector::Hyphen:
    case CSSSelector::Contain:
    case CSSSelector::Begin:
    case CSSSelector::End:
        return true;
    case CSSSelector::PseudoElement:
        return false;
    case CSSSelector::PagePseudoClass:
    case CSSSelector::PseudoClass:
        break;
    }

    switch (selector->pseudoType()) {
    case CSSSelector::PseudoEmpty:
    case CSSSelector::PseudoLink:
    case CSSSelector::PseudoVisited:
    case CSSSelector::PseudoTarget:
    case CSSSelector::PseudoEnabled:
    case CSSSelector::PseudoDisabled:
    case CSSSelector::PseudoChecked:
    case CSSSelector::PseudoIndeterminate:
    case CSSSelector::PseudoNthChild:
    case CSSSelector::PseudoNthLastChild:
    case CSSSelector::PseudoNthOfType:
    case CSSSelector::PseudoNthLastOfType:
    case CSSSelector::PseudoFirstChild:
    case CSSSelector::PseudoLastChild:
    case CSSSelector::PseudoFirstOfType:
    case CSSSelector::PseudoLastOfType:
    case CSSSelector::PseudoOnlyOfType:
        return true;
    default:
        return false;
    }
}

static bool validateSelector(CSSSelector* selector)
{
    ASSERT(selector);

    if (!validateSubSelector(selector))
        return false;

    CSSSelector* prevSubSelector = selector;
    CSSSelector* subSelector = selector->tagHistory();

    while (subSelector) {
        if (prevSubSelector->relation() != CSSSelector::SubSelector)
            return false;
        if (!validateSubSelector(subSelector))
            return false;

        prevSubSelector = subSelector;
        subSelector = subSelector->tagHistory();
    }

    return true;
}

bool ContentSelectorQuery::validateSelectorList()
{
    if (!m_selectorList.first())
        return false;

    for (CSSSelector* selector = m_selectorList.first(); selector; selector = m_selectorList.next(selector)) {
        if (!validateSelector(selector))
            return false;
    }

    return true;
}

}
