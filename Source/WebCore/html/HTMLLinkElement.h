/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *
 */

#ifndef HTMLLinkElement_h
#define HTMLLinkElement_h

#include "CSSStyleSheet.h"
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "DOMSettableTokenList.h"
#include "HTMLElement.h"
#include "IconURL.h"
#include "LinkLoader.h"
#include "LinkLoaderClient.h"
#include "LinkRelAttribute.h"
#include "Timer.h"

namespace WebCore {

class CachedCSSStyleSheet;
class CachedResource;
class KURL;

class HTMLLinkElement : public HTMLElement, public CachedResourceClient, public LinkLoaderClient {
public:
    static PassRefPtr<HTMLLinkElement> create(const QualifiedName&, Document*, bool createdByParser);
    virtual ~HTMLLinkElement();

    KURL href() const;
    String rel() const;

    virtual String target() const;

    String type() const;

    StyleSheet* sheet() const;

    // FIXME: This should be renamed isStyleSheetLoading as this is only used for stylesheets.
    bool isLoading() const;
    bool isEnabledViaScript() const { return m_isEnabledViaScript; }
    bool disabled() const;
    void setDisabled(bool);
    void setSizes(const String&);
    DOMSettableTokenList* sizes() const;

private:
    virtual void parseMappedAttribute(Attribute*);

    virtual bool shouldLoadLink();
    void process();
    static void processCallback(Node*);

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    // from CachedResourceClient
    virtual void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet* sheet);
    virtual bool sheetLoaded();
    virtual void startLoadingDynamicSheet();

    virtual void linkLoaded();
    virtual void linkLoadingErrored();

    bool isAlternate() const { return m_relAttribute.m_isAlternate; }
    
    virtual bool isURLAttribute(Attribute*) const;

private:
    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

    virtual void finishParsingChildren();
    
    enum PendingSheetType { None, NonBlocking, Blocking };
    void addPendingSheet(PendingSheetType);
    void removePendingSheet();

private:
    HTMLLinkElement(const QualifiedName&, Document*, bool createdByParser);

    LinkLoader m_linkLoader;
    CachedResourceHandle<CachedCSSStyleSheet> m_cachedSheet;
    RefPtr<CSSStyleSheet> m_sheet;
    KURL m_url;
    String m_type;
    String m_media;
    RefPtr<DOMSettableTokenList> m_sizes;
    LinkRelAttribute m_relAttribute;
    bool m_loading;
    bool m_isEnabledViaScript;
    bool m_createdByParser;
    bool m_isInShadowTree;
    
    PendingSheetType m_pendingSheetType;
};

} //namespace

#endif
