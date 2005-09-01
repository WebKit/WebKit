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

#ifndef KDOM_CSSMediaRuleImpl_H
#define KDOM_CSSMediaRuleImpl_H

#include <kdom/css/CSSRuleImpl.h>

namespace KDOM
{
    class MediaListImpl;
    class CSSRuleListImpl;
    class CSSMediaRuleImpl : public CSSRuleImpl
    {
    public:
        CSSMediaRuleImpl(StyleBaseImpl *parent);
        CSSMediaRuleImpl(StyleBaseImpl *parent, DOMStringImpl *media);
        CSSMediaRuleImpl(StyleBaseImpl *parent, MediaListImpl *mediaList, CSSRuleListImpl *ruleList);
        virtual ~CSSMediaRuleImpl();

        // 'CSSMediaRuleImpl' functions
        MediaListImpl *media() const;
        CSSRuleListImpl *cssRules() const;

        unsigned long insertRule(DOMStringImpl *rule, unsigned long index);
        void deleteRule(unsigned long index);

        // 'CSSRule' functions
        virtual bool isMediaRule() const { return true; }

        // Helper
        unsigned long append(CSSRuleImpl *rule);

    protected:
        MediaListImpl *m_lstMedia;
        CSSRuleListImpl *m_lstCSSRules;
    };
};

#endif

// vim:ts=4:noet
