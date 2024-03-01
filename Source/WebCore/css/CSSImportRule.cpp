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

#include "CSSLayerBlockRule.h"
#include "CSSMarkup.h"
#include "CSSStyleSheet.h"
#include "MediaList.h"
#include "MediaQueryParser.h"
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
        m_mediaCSSOMWrapper->detachFromParent();
}

String CSSImportRule::href() const
{
    return m_importRule.get().href();
}

MediaList& CSSImportRule::media() const
{
    if (!m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper = MediaList::create(const_cast<CSSImportRule*>(this));
    return *m_mediaCSSOMWrapper;
}

String CSSImportRule::layerName() const
{
    auto name = m_importRule->cascadeLayerName();
    if (!name)
        return { };

    return stringFromCascadeLayerName(*name);
}

String CSSImportRule::supportsText() const
{
    return m_importRule->supportsText();
}

String CSSImportRule::cssTextInternal(const String& urlString) const
{
    StringBuilder builder;
    builder.append("@import ", serializeURL(urlString));

    if (auto layerName = this->layerName(); !layerName.isNull()) {
        if (layerName.isEmpty())
            builder.append(" layer");
        else
            builder.append(" layer(", layerName, ')');
    }

    auto supports = supportsText();
    if (!supports.isNull())
        builder.append(" supports(", WTFMove(supports), ')');

    if (!mediaQueries().isEmpty()) {
        builder.append(' ');
        MQ::serialize(builder, mediaQueries());
    }

    builder.append(';');
    return builder.toString();
}

String CSSImportRule::cssText() const
{
    return cssTextInternal(m_importRule->href());
}

String CSSImportRule::cssTextWithReplacementURLs(const HashMap<String, String>& replacementURLStrings, const HashMap<RefPtr<CSSStyleSheet>, String>& replacementURLStringsForCSSStyleSheet) const
{
    if (RefPtr sheet = styleSheet()) {
        auto urlString = replacementURLStringsForCSSStyleSheet.get(sheet);
        if (!urlString.isEmpty())
            return cssTextInternal(urlString);
    }

    auto urlString = m_importRule->href();
    auto replacementURLString = replacementURLStrings.get(urlString);
    return replacementURLString.isEmpty() ? cssTextInternal(urlString) : cssTextInternal(replacementURLString);
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

const MQ::MediaQueryList& CSSImportRule::mediaQueries() const
{
    return m_importRule->mediaQueries();
}

void CSSImportRule::setMediaQueries(MQ::MediaQueryList&& queries)
{
    m_importRule->setMediaQueries(WTFMove(queries));
}

void CSSImportRule::getChildStyleSheets(HashSet<RefPtr<CSSStyleSheet>>& childStyleSheets)
{
    RefPtr sheet = styleSheet();
    if (!sheet)
        return;

    if (childStyleSheets.add(sheet).isNewEntry)
        sheet->getChildStyleSheets(childStyleSheets);
}

} // namespace WebCore
