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
#include "kdom.h"
#include "CSSRuleImpl.h"
#include "CSSRuleListImpl.h"

using namespace KDOM;

CSSRuleListImpl::CSSRuleListImpl() : Shared<CSSRuleListImpl>()
{
}

CSSRuleListImpl::~CSSRuleListImpl()
{
    CSSRuleImpl *rule;
    while(!m_lstCSSRules.isEmpty() && (rule = m_lstCSSRules.take(0)))
        rule->deref();
}

unsigned long CSSRuleListImpl::length() const
{
    return m_lstCSSRules.count();
}

CSSRuleImpl *CSSRuleListImpl::item(unsigned long index)
{
    return m_lstCSSRules.at(index);
}

void CSSRuleListImpl::deleteRule(unsigned long index)
{
    CSSRuleImpl *rule = m_lstCSSRules.take(index);
    if(rule)
        rule->deref();
    else
        throw new DOMExceptionImpl(INDEX_SIZE_ERR);
}

unsigned long CSSRuleListImpl::insertRule(CSSRuleImpl *rule, unsigned long index)
{
    if(rule && m_lstCSSRules.insert(index, rule))
    {
        rule->ref();
        return index;
    }

    throw new DOMExceptionImpl(INDEX_SIZE_ERR);
    return 0;
}

void CSSRuleListImpl::append(CSSRuleImpl *rule)
{
    m_lstCSSRules.append(rule);
}

// vim:ts=4:noet
