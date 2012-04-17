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
#include "CachedStyleSheetClient.h"
#include "CachedResourceHandle.h"
#include "DOMSettableTokenList.h"
#include "HTMLElement.h"
#include "IconURL.h"
#include "LinkLoader.h"
#include "LinkLoaderClient.h"
#include "LinkRelAttribute.h"
#include "Timer.h"

namespace WebCore {

class HTMLLinkElement;
class KURL;

template<typename T> class EventSender;
typedef EventSender<HTMLLinkElement> LinkEventSender;

class HTMLLinkElement : public HTMLElement, public CachedStyleSheetClient, public LinkLoaderClient {
public:
    static PassRefPtr<HTMLLinkElement> create(const QualifiedName&, Document*, bool createdByParser);
    virtual ~HTMLLinkElement();

    KURL href() const;
    String rel() const;

    virtual String target() const;

    String type() const;

    CSSStyleSheet* sheet() const { return m_sheet.get(); }

    bool styleSheetIsLoading() const;

    bool isDisabled() const { return m_disabledState == Disabled; }
    bool isEnabledViaScript() const { return m_disabledState == EnabledViaScript; }
    void setSizes(const String&);
    DOMSettableTokenList* sizes() const;

    void dispatchPendingEvent(LinkEventSender*);
    static void dispatchPendingLoadEvents();

private:
    virtual void parseAttribute(Attribute*) OVERRIDE;

    virtual bool shouldLoadLink();
    void process();
    static void processCallback(Node*);
    void clearSheet();

    virtual InsertionNotificationRequest insertedInto(Node*) OVERRIDE;
    virtual void removedFrom(Node*) OVERRIDE;

    // from CachedResourceClient
    virtual void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet* sheet);
    virtual bool sheetLoaded();
    virtual void notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred);
    virtual void startLoadingDynamicSheet();

    virtual void linkLoaded();
    virtual void linkLoadingErrored();

    bool isAlternate() const { return m_disabledState == Unset && m_relAttribute.m_isAlternate; }
    
    void setDisabledState(bool);

    virtual bool isURLAttribute(Attribute*) const;

private:
    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

    virtual void finishParsingChildren();
    
    enum PendingSheetType { None, NonBlocking, Blocking };
    void addPendingSheet(PendingSheetType);
    void removePendingSheet();

#if ENABLE(MICRODATA)
    virtual String itemValueText() const OVERRIDE;
    virtual void setItemValueText(const String&, ExceptionCode&) OVERRIDE;
#endif

private:
    HTMLLinkElement(const QualifiedName&, Document*, bool createdByParser);

    LinkLoader m_linkLoader;
    CachedResourceHandle<CachedCSSStyleSheet> m_cachedSheet;
    RefPtr<CSSStyleSheet> m_sheet;
    enum DisabledState {
        Unset,
        EnabledViaScript,
        Disabled
    };

    KURL m_url;
    String m_type;
    String m_media;
    RefPtr<DOMSettableTokenList> m_sizes;
    DisabledState m_disabledState;
    LinkRelAttribute m_relAttribute;
    bool m_loading;
    bool m_createdByParser;
    bool m_isInShadowTree;
    bool m_firedLoad;
    bool m_loadedSheet;

    PendingSheetType m_pendingSheetType;
};

} //namespace

#endif
