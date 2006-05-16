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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef CSSImportRule_H
#define CSSImportRule_H

#include "CSSRule.h"
#include "CachedObjectClient.h"

namespace WebCore {

class CachedCSSStyleSheet;

class CSSImportRule : public CSSRule, public CachedObjectClient
{
public:
    CSSImportRule(StyleBase* parent, const String& href, MediaList*);
    virtual ~CSSImportRule();

    String href() const { return m_strHref; }
    MediaList* media() const { return m_lstMedia.get(); }
    CSSStyleSheet* styleSheet() const { return m_styleSheet.get(); }

    virtual bool isImportRule() { return true; }
    virtual String cssText() const;
  
    bool isLoading() const;

    // from CachedObjectClient
    virtual void setStyleSheet(const String& url, const String& sheet);

    virtual void insertedIntoParent();

protected:
    String m_strHref;
    RefPtr<MediaList> m_lstMedia;
    RefPtr<CSSStyleSheet> m_styleSheet;
    CachedCSSStyleSheet* m_cachedSheet;
    bool m_loading;
};

} // namespace

#endif
