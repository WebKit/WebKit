/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef StyleRuleImport_h
#define StyleRuleImport_h

#include "CSSParserMode.h"
#include "CachedResourceHandle.h"
#include "CachedStyleSheetClient.h"
#include "StyleRule.h"

namespace WebCore {

class CachedCSSStyleSheet;
class MediaQuerySet;
class StyleSheetContents;

class StyleRuleImport : public StyleRuleBase, public CachedStyleSheetClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<StyleRuleImport> create(const String& href, PassRefPtr<MediaQuerySet>);

    virtual ~StyleRuleImport();

    String href() const { return m_strHref; }
    StyleSheetContents* styleSheet() const { return m_styleSheet.get(); }

    bool isLoading() const;
    bool hadLoadError() const;
    MediaQuerySet* mediaQueries() { return m_mediaQueries.get(); }

    void requestStyleSheet(CSSStyleSheet* rootSheet, const CSSParserContext&);

    void reportDescendantMemoryUsage(MemoryObjectInfo*) const;

    void reattachStyleSheetContents(StyleSheetContents*);

private:
    StyleRuleImport(const String& href, PassRefPtr<MediaQuerySet>);

    virtual void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet*);

    String m_strHref;
    RefPtr<MediaQuerySet> m_mediaQueries;
    RefPtr<StyleSheetContents> m_styleSheet;

    CachedResourceHandle<CachedCSSStyleSheet> m_cachedSheet;
    struct LoadContext {
        LoadContext(CSSStyleSheet* rootStyleSheet, const CSSParserContext& parentParserContext);
        RefPtr<CSSStyleSheet> rootStyleSheet;
        CSSParserContext parentParserContext;
    };
    OwnPtr<LoadContext> m_loadContext;
};

} // namespace WebCore

#endif
