/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#include "KDOMCSSParser.h"
#include "MediaListImpl.h"
#include "CSSRuleListImpl.h"
#include "CSSMediaRuleImpl.h"

using namespace KDOM;

CSSMediaRuleImpl::CSSMediaRuleImpl(StyleBaseImpl *parent) : CSSRuleImpl(parent)
{
    m_type = MEDIA_RULE;

    m_lstMedia = 0;

    m_lstCSSRules = new CSSRuleListImpl();
    m_lstCSSRules->ref();
}

CSSMediaRuleImpl::CSSMediaRuleImpl(StyleBaseImpl *parent, DOMStringImpl *media) : CSSRuleImpl(parent)
{
    m_type = MEDIA_RULE;

    m_lstMedia = new MediaListImpl(this, media);
    m_lstMedia->ref();
    
    m_lstCSSRules = new CSSRuleListImpl();
    m_lstCSSRules->ref();
}

CSSMediaRuleImpl::CSSMediaRuleImpl(StyleBaseImpl *parent, MediaListImpl *mediaList, CSSRuleListImpl *ruleList) : CSSRuleImpl(parent)
{
    m_type = MEDIA_RULE;

    m_lstMedia = mediaList;
    m_lstMedia->ref();
    
    m_lstCSSRules = ruleList;
    m_lstCSSRules->ref();
}

CSSMediaRuleImpl::~CSSMediaRuleImpl()
{
    if(m_lstMedia)
    {
        m_lstMedia->setParent(0);
        m_lstMedia->deref();
    }
    
    for(unsigned int i = 0; i < m_lstCSSRules->length(); ++i)
        m_lstCSSRules->item(i)->setParent(0);
        
    m_lstCSSRules->deref();
}

MediaListImpl *CSSMediaRuleImpl::media() const
{
    return m_lstMedia;
}

CSSRuleListImpl *CSSMediaRuleImpl::cssRules() const
{
    // FIXME!
    return m_lstCSSRules;
}

unsigned long CSSMediaRuleImpl::insertRule(DOMStringImpl *rule, unsigned long index)
{
    CSSParser *p = createCSSParser(m_strictParsing);
    if(!p)
        return 0;
        
    CSSRuleImpl *newRule = p->parseRule(parentStyleSheet(), rule);
    delete p;

    return newRule ? m_lstCSSRules->insertRule(newRule, index) : 0;
}

void CSSMediaRuleImpl::deleteRule(unsigned long index)
{
    m_lstCSSRules->deleteRule(index);
}

unsigned long CSSMediaRuleImpl::append(CSSRuleImpl *rule)
{
    return rule ? m_lstCSSRules->insertRule(rule, m_lstCSSRules->length()) : 0;
}

// vim:ts=4:noet
