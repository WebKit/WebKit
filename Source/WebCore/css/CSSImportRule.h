/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSImportRule_h
#define CSSImportRule_h

#include "CSSRule.h"
#include "CachedResourceHandle.h"
#include "CachedStyleSheetClient.h"
#include "PlatformString.h"
#include "StyleRule.h"

namespace WebCore {

class CachedCSSStyleSheet;
class MediaList;
class MediaQuerySet;
class StyleSheetInternal;

class StyleRuleImport : public StyleRuleBase {
public:
    static PassRefPtr<StyleRuleImport> create(const String& href, PassRefPtr<MediaQuerySet>);

    ~StyleRuleImport();
    
    StyleSheetInternal* parentStyleSheet() const { return m_parentStyleSheet; }
    void setParentStyleSheet(StyleSheetInternal* sheet) { ASSERT(sheet); m_parentStyleSheet = sheet; }
    void clearParentStyleSheet() { m_parentStyleSheet = 0; }

    String href() const { return m_strHref; }
    StyleSheetInternal* styleSheet() const { return m_styleSheet.get(); }

    bool isLoading() const;
    MediaQuerySet* mediaQueries() { return m_mediaQueries.get(); }

    void requestStyleSheet();

private:
    // NOTE: We put the CachedStyleSheetClient in a member instead of inheriting from it
    // to avoid adding a vptr to StyleRuleImport.
    class ImportedStyleSheetClient : public CachedStyleSheetClient {
    public:
        ImportedStyleSheetClient(StyleRuleImport* ownerRule) : m_ownerRule(ownerRule) { }
        virtual ~ImportedStyleSheetClient() { }
        virtual void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet* sheet)
        {
            m_ownerRule->setCSSStyleSheet(href, baseURL, charset, sheet);
        }
    private:
        StyleRuleImport* m_ownerRule;
    };

    void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet*);
    friend class ImportedStyleSheetClient;

    StyleRuleImport(const String& href, PassRefPtr<MediaQuerySet>);

    StyleSheetInternal* m_parentStyleSheet;

    ImportedStyleSheetClient m_styleSheetClient;
    String m_strHref;
    RefPtr<MediaQuerySet> m_mediaQueries;
    RefPtr<StyleSheetInternal> m_styleSheet;
    CachedResourceHandle<CachedCSSStyleSheet> m_cachedSheet;
    bool m_loading;
};

class CSSImportRule : public CSSRule {
public:
    static PassRefPtr<CSSImportRule> create(StyleRuleImport* rule, CSSStyleSheet* sheet) { return adoptRef(new CSSImportRule(rule, sheet)); }
    
    ~CSSImportRule();

    String href() const { return m_importRule->href(); }
    MediaList* media() const;
    CSSStyleSheet* styleSheet() const;
    
    String cssText() const;

private:
    CSSImportRule(StyleRuleImport*, CSSStyleSheet*);

    RefPtr<StyleRuleImport> m_importRule;

    mutable RefPtr<MediaList> m_mediaCSSOMWrapper;
    mutable RefPtr<CSSStyleSheet> m_styleSheetCSSOMWrapper;
};

} // namespace WebCore

#endif // CSSImportRule_h
