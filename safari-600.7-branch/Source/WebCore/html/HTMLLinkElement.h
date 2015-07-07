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

namespace WebCore {

class HTMLLinkElement;
class URL;

template<typename T> class EventSender;
typedef EventSender<HTMLLinkElement> LinkEventSender;

class HTMLLinkElement final : public HTMLElement, public CachedStyleSheetClient, public LinkLoaderClient {
public:
    static PassRefPtr<HTMLLinkElement> create(const QualifiedName&, Document&, bool createdByParser);
    virtual ~HTMLLinkElement();

    URL href() const;
    String rel() const;

    virtual String target() const override;

    String type() const;

    IconType iconType() const;

    // the icon size string as parsed from the HTML attribute
    String iconSizes() const;

    CSSStyleSheet* sheet() const { return m_sheet.get(); }

    bool styleSheetIsLoading() const;

    bool isDisabled() const { return m_disabledState == Disabled; }
    bool isEnabledViaScript() const { return m_disabledState == EnabledViaScript; }
    void setSizes(const String&);
    DOMSettableTokenList* sizes() const;

    void dispatchPendingEvent(LinkEventSender*);
    static void dispatchPendingLoadEvents();

private:
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;

    virtual bool shouldLoadLink() override;
    void process();
    static void processCallback(Node*);
    void clearSheet();

    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;

    // from CachedResourceClient
    virtual void setCSSStyleSheet(const String& href, const URL& baseURL, const String& charset, const CachedCSSStyleSheet* sheet) override;
    virtual bool sheetLoaded() override;
    virtual void notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred) override;
    virtual void startLoadingDynamicSheet() override;

    virtual void linkLoaded() override;
    virtual void linkLoadingErrored() override;

    bool isAlternate() const { return m_disabledState == Unset && m_relAttribute.m_isAlternate; }
    
    void setDisabledState(bool);

    virtual bool isURLAttribute(const Attribute&) const override;

private:
    HTMLLinkElement(const QualifiedName&, Document&, bool createdByParser);

    virtual void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    virtual void finishParsingChildren() override;

    enum PendingSheetType { Unknown, ActiveSheet, InactiveSheet };
    void addPendingSheet(PendingSheetType);

    enum RemovePendingSheetNotificationType {
        RemovePendingSheetNotifyImmediately,
        RemovePendingSheetNotifyLater
    };

    void removePendingSheet(RemovePendingSheetNotificationType = RemovePendingSheetNotifyImmediately);

    LinkLoader m_linkLoader;
    CachedResourceHandle<CachedCSSStyleSheet> m_cachedSheet;
    RefPtr<CSSStyleSheet> m_sheet;
    enum DisabledState {
        Unset,
        EnabledViaScript,
        Disabled
    };

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

NODE_TYPE_CASTS(HTMLLinkElement)

} //namespace

#endif
