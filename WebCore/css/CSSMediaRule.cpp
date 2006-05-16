/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include "CSSMediaRule.h"

#include "cssparser.h"
#include "CSSRuleList.h"
#include "MediaList.h"

namespace WebCore {

CSSMediaRule::CSSMediaRule(StyleBase* parent, MediaList* mediaList, CSSRuleList* ruleList)
    : CSSRule(parent)
    , m_lstMedia(mediaList)
    , m_lstCSSRules(ruleList)
{
    m_type = MEDIA_RULE;
}

CSSMediaRule::CSSMediaRule(StyleBase* parent)
    : CSSRule(parent)
    , m_lstMedia(0)
    , m_lstCSSRules(new CSSRuleList())

{
    m_type = MEDIA_RULE;
}

CSSMediaRule::CSSMediaRule(StyleBase* parent, const String &media)
    : CSSRule(parent)
    , m_lstMedia(new MediaList(this, media))
    , m_lstCSSRules(new CSSRuleList())
{
    m_type = MEDIA_RULE;
}

CSSMediaRule::~CSSMediaRule()
{
    if (m_lstMedia)
        m_lstMedia->setParent(0);

    int length = m_lstCSSRules->length();
    for (int i = 0; i < length; i++)
        m_lstCSSRules->item(i)->setParent(0);
}

unsigned CSSMediaRule::append(CSSRule* rule)
{
    if (!rule)
        return 0;

    rule->setParent(this);
    return m_lstCSSRules->insertRule(rule, m_lstCSSRules->length());
}

unsigned CSSMediaRule::insertRule(const String& rule, unsigned index)
{
    CSSParser p(useStrictParsing());
    RefPtr<CSSRule> newRule = p.parseRule(parentStyleSheet(), rule);
    if (!newRule)
        return 0;
    newRule->setParent(this);
    return m_lstCSSRules->insertRule(newRule.get(), index);
}

void CSSMediaRule::deleteRule(unsigned index)
{
    m_lstCSSRules->deleteRule(index);
}

String CSSMediaRule::cssText() const
{
    String result = "@media ";
    if (m_lstMedia) {
        result += m_lstMedia->mediaText();
        result += " ";
    }
    result += "{ \n";
    
    if (m_lstCSSRules) {
        unsigned len = m_lstCSSRules->length();
        for (unsigned i = 0; i < len; i++) {
            result += "  ";
            result += m_lstCSSRules->item(i)->cssText();
            result += "\n";
        }
    }
    
    result += "}";
    return result;
}
}
