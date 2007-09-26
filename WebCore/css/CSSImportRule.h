/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006 Apple Computer, Inc.
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
#include "CachedResourceClient.h"
#include "PlatformString.h"

namespace WebCore {

class CachedCSSStyleSheet;
class MediaList;

class CSSImportRule : public CSSRule, public CachedResourceClient {
public:
    CSSImportRule(StyleBase* parent, const String& href, MediaList*);
    virtual ~CSSImportRule();

    virtual bool isImportRule() { return true; }

    String href() const { return m_strHref; }
    MediaList* media() const { return m_lstMedia.get(); }
    CSSStyleSheet* styleSheet() const { return m_styleSheet.get(); }

    // Inherited from CSSRule
    virtual unsigned short type() const { return IMPORT_RULE; }

    virtual String cssText() const;

    // Not part of the CSSOM
    bool isLoading() const;

    // from CachedResourceClient
    virtual void setCSSStyleSheet(const String& url, const String& charset, const String& sheet);

    virtual void insertedIntoParent();

protected:
    String m_strHref;
    RefPtr<MediaList> m_lstMedia;
    RefPtr<CSSStyleSheet> m_styleSheet;
    CachedCSSStyleSheet* m_cachedSheet;
    bool m_loading;
};

} // namespace WebCore

#endif // CSSImportRule_h
