/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 *
 */
#ifndef HTMLLinkElement_H
#define HTMLLinkElement_H

#include "HTMLElement.h"
#include "CachedObjectClient.h"
#include "CSSStyleSheet.h"

namespace WebCore {

class CachedCSSStyleSheet;

class HTMLLinkElement : public HTMLElement, public CachedObjectClient
{
public:
    HTMLLinkElement(Document*);
    ~HTMLLinkElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    bool disabled() const;
    void setDisabled(bool);

    String charset() const;
    void setCharset(const String&);

    String href() const;
    void setHref(const String&);

    String hreflang() const;
    void setHreflang(const String&);

    String media() const;
    void setMedia(const String&);

    String rel() const;
    void setRel(const String&);

    String rev() const;
    void setRev(const String&);

    String target() const;
    void setTarget(const String&);

    String type() const;
    void setType(const String&);

    StyleSheet* sheet() const;

    // overload from HTMLElement
    virtual void parseMappedAttribute(MappedAttribute*);

    void process();

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    // from CachedObjectClient
    virtual void setStyleSheet(const String &url, const String &sheet);
    bool isLoading() const;
    void sheetLoaded();

    bool isAlternate() const { return m_disabledState == 0 && m_alternate; }
    bool isDisabled() const { return m_disabledState == 2; }
    bool isEnabledViaScript() const { return m_disabledState == 1; }

    int disabledState() { return m_disabledState; }
    void setDisabledState(bool _disabled);

    virtual bool isURLAttribute(Attribute*) const;
    
    void tokenizeRelAttribute(const AtomicString& rel);

protected:
    CachedCSSStyleSheet* m_cachedSheet;
    RefPtr<CSSStyleSheet> m_sheet;
    String m_url;
    String m_type;
    String m_media;
    int m_disabledState; // 0=unset(default), 1=enabled via script, 2=disabled
    bool m_loading : 1;
    bool m_alternate : 1;
    bool m_isStyleSheet : 1;
    bool m_isIcon : 1;
};

} //namespace

#endif
