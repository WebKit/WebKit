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
#include "ShadowContentSelectorQuery.h"

#include "CSSParser.h"
#include "CSSSelectorList.h"
#include "ShadowContentElement.h"

namespace WebCore {

ShadowContentSelectorQuery::ShadowContentSelectorQuery(ShadowContentElement* element)
    : m_contentElement(element)
    , m_selectorChecker(element->document(), !element->document()->inQuirksMode())
{
    m_selectorChecker.setCollectingRulesOnly(true);

    if (element->select().isEmpty())
        return;

    CSSParser parser(true);
    parser.parseSelector(element->select(), element->document(), m_selectorList);

    m_selectors.initialize(m_selectorList);
}

bool ShadowContentSelectorQuery::matches(Node* node) const
{
    ASSERT(node);
    if (!node)
        return false;

    ASSERT(node->parentNode() == m_contentElement->shadowTreeRootNode()->shadowHost());

    if (m_contentElement->select().isEmpty())
        return true;

    if (!node->isElementNode())
        return false;

    return m_selectors.matches(m_selectorChecker, toElement(node));
}

}
