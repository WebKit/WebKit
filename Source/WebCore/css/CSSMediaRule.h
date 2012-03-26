/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSMediaRule_h
#define CSSMediaRule_h

#include "CSSRule.h"
#include "MediaList.h"
#include "PlatformString.h" // needed so bindings will compile

namespace WebCore {

class CSSRuleList;

class CSSMediaRule : public CSSRule {
public:
    static PassRefPtr<CSSMediaRule> create(CSSStyleSheet* parent, PassRefPtr<MediaQuerySet> media, Vector<RefPtr<CSSRule> >& adoptRules)
    {
        return adoptRef(new CSSMediaRule(parent, media, adoptRules));
    }
    ~CSSMediaRule();

    MediaList* media() const { return m_mediaQueries ? m_mediaQueries->ensureMediaList(parentStyleSheet()) : 0; }
    CSSRuleList* cssRules();

    unsigned insertRule(const String& rule, unsigned index, ExceptionCode&);
    void deleteRule(unsigned index, ExceptionCode&);

    String cssText() const;

    // Not part of the CSSOM
    unsigned append(CSSRule*);
    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }

    unsigned ruleCount() const { return m_childRules.size(); }
    CSSRule* ruleAt(unsigned index) const { return m_childRules[index].get(); }

private:
    CSSMediaRule(CSSStyleSheet* parent, PassRefPtr<MediaQuerySet>, Vector<RefPtr<CSSRule> >& adoptRules);

    RefPtr<MediaQuerySet> m_mediaQueries;
    Vector<RefPtr<CSSRule> > m_childRules;
    
    OwnPtr<CSSRuleList> m_ruleListCSSOMWrapper;
};

} // namespace WebCore

#endif // CSSMediaRule_h
