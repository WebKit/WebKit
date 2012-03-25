/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
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
#include "CSSMediaRule.h"

#include "CSSParser.h"
#include "CSSRuleList.h"
#include "ExceptionCode.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSMediaRule::CSSMediaRule(CSSStyleSheet* parent, PassRefPtr<MediaList> media, Vector<RefPtr<CSSRule> >& adoptRules)
    : CSSRule(parent, CSSRule::MEDIA_RULE)
    , m_lstMedia(media)
{
    m_childRules.swap(adoptRules);

    m_lstMedia->setParentStyleSheet(parent);
    unsigned size = m_childRules.size();
    for (unsigned i = 0; i < size; i++)
        m_childRules[i]->setParentRule(this);
}

CSSMediaRule::~CSSMediaRule()
{
    if (m_lstMedia)
        m_lstMedia->setParentStyleSheet(0);

    unsigned size = m_childRules.size();
    for (unsigned i = 0; i < size; i++)
        m_childRules[i]->setParentRule(0);
}

unsigned CSSMediaRule::append(CSSRule* rule)
{
    if (!rule)
        return 0;

    rule->setParentRule(this);
    m_childRules.append(rule);
    return m_childRules.size() - 1;
}

unsigned CSSMediaRule::insertRule(const String& rule, unsigned index, ExceptionCode& ec)
{
    if (index > m_childRules.size()) {
        // INDEX_SIZE_ERR: Raised if the specified index is not a valid insertion point.
        ec = INDEX_SIZE_ERR;
        return 0;
    }

    CSSParser p(useStrictParsing());
    RefPtr<CSSRule> newRule = p.parseRule(parentStyleSheet(), rule);
    if (!newRule) {
        // SYNTAX_ERR: Raised if the specified rule has a syntax error and is unparsable.
        ec = SYNTAX_ERR;
        return 0;
    }

    if (newRule->isImportRule()) {
        // FIXME: an HIERARCHY_REQUEST_ERR should also be thrown for a @charset or a nested
        // @media rule.  They are currently not getting parsed, resulting in a SYNTAX_ERR
        // to get raised above.

        // HIERARCHY_REQUEST_ERR: Raised if the rule cannot be inserted at the specified
        // index, e.g., if an @import rule is inserted after a standard rule set or other
        // at-rule.
        ec = HIERARCHY_REQUEST_ERR;
        return 0;
    }

    newRule->setParentRule(this);
    m_childRules.insert(index, newRule.get());

    if (CSSStyleSheet* styleSheet = parentStyleSheet())
        styleSheet->styleSheetChanged();

    return index;
}

void CSSMediaRule::deleteRule(unsigned index, ExceptionCode& ec)
{
    if (index >= m_childRules.size()) {
        // INDEX_SIZE_ERR: Raised if the specified index does not correspond to a
        // rule in the media rule list.
        ec = INDEX_SIZE_ERR;
        return;
    }

    m_childRules[index]->setParentRule(0);
    m_childRules.remove(index);

    if (CSSStyleSheet* styleSheet = parentStyleSheet())
        styleSheet->styleSheetChanged();
}

String CSSMediaRule::cssText() const
{
    StringBuilder result;
    result.append("@media ");
    if (m_lstMedia) {
        result.append(m_lstMedia->mediaText());
        result.append(" ");
    }
    result.append("{ \n");
    
    for (unsigned i = 0; i < m_childRules.size(); ++i) {
        result.append("  ");
        result.append(m_childRules[i]->cssText());
        result.append("\n");
    }

    result.append("}");
    return result.toString();
}

CSSRuleList* CSSMediaRule::cssRules()
{
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = adoptPtr(new LiveCSSRuleList<CSSMediaRule>(this));
    return m_ruleListCSSOMWrapper.get();
}

} // namespace WebCore
