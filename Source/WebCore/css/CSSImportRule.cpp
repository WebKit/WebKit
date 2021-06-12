/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2009, 2010, 2012, 2013 Apple Inc. All rights reserved.
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
#include "CSSImportRule.h"

#include "CSSStyleSheet.h"
#include "MediaList.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSImportRule::CSSImportRule(StyleRuleImport& importRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_importRule(importRule)
{
}

CSSImportRule::~CSSImportRule()
{
    if (m_styleSheetCSSOMWrapper)
        m_styleSheetCSSOMWrapper->clearOwnerRule();
    if (m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper->clearParentRule();
}

String CSSImportRule::href() const
{
    return m_importRule.get().href();
}

MediaList& CSSImportRule::media() const
{
    if (!m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper = MediaList::create(m_importRule.get().mediaQueries(), const_cast<CSSImportRule*>(this));
    return *m_mediaCSSOMWrapper;
}

String CSSImportRule::cssText() const
{
    if (auto queries = m_importRule.get().mediaQueries()) {
        if (auto mediaText = queries->mediaText(); !mediaText.isEmpty())
            return makeString("@import url(\"", m_importRule.get().href(), "\") ", mediaText, ';');
    }
    return makeString("@import url(\"", m_importRule.get().href(), "\");");
}

CSSStyleSheet* CSSImportRule::styleSheet() const
{ 
    if (!m_importRule.get().styleSheet())
        return nullptr;
    if (!m_styleSheetCSSOMWrapper)
        m_styleSheetCSSOMWrapper = CSSStyleSheet::create(*m_importRule.get().styleSheet(), const_cast<CSSImportRule*>(this));
    return m_styleSheetCSSOMWrapper.get(); 
}

void CSSImportRule::reattach(StyleRuleBase&)
{
    // FIXME: Implement when enabling caching for stylesheets with import rules.
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
