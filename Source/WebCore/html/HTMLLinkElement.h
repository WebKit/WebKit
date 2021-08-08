/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSStyleSheet.h"
#include "CachedStyleSheetClient.h"
#include "CachedResourceHandle.h"
#include "DOMTokenList.h"
#include "HTMLElement.h"
#include "LinkLoader.h"
#include "LinkLoaderClient.h"
#include "LinkRelAttribute.h"

namespace WebCore {

class DOMTokenList;
class HTMLLinkElement;
class Page;
struct MediaQueryParserContext;

template<typename T> class EventSender;
using LinkEventSender = EventSender<HTMLLinkElement>;

class HTMLLinkElement final : public HTMLElement, public CachedStyleSheetClient, public LinkLoaderClient {
    WTF_MAKE_ISO_ALLOCATED(HTMLLinkElement);
public:
    static Ref<HTMLLinkElement> create(const QualifiedName&, Document&, bool createdByParser);
    virtual ~HTMLLinkElement();

    URL href() const;
    WEBCORE_EXPORT const AtomString& rel() const;

    String target() const final;

    const AtomString& type() const;

    std::optional<LinkIconType> iconType() const;

    CSSStyleSheet* sheet() const { return m_sheet.get(); }

    bool styleSheetIsLoading() const;

    bool isDisabled() const { return m_disabledState == Disabled; }
    bool isEnabledViaScript() const { return m_disabledState == EnabledViaScript; }
    DOMTokenList& sizes();

    WEBCORE_EXPORT void setCrossOrigin(const AtomString&);
    WEBCORE_EXPORT String crossOrigin() const;
    WEBCORE_EXPORT void setAs(const AtomString&);
    WEBCORE_EXPORT String as() const;

    void dispatchPendingEvent(LinkEventSender*);
    static void dispatchPendingLoadEvents(Page*);

    WEBCORE_EXPORT DOMTokenList& relList();

#if ENABLE(APPLICATION_MANIFEST)
    bool isApplicationManifest() const { return m_relAttribute.isApplicationManifest; }
#endif

    void allowPrefetchLoadAndErrorForTesting() { m_allowPrefetchLoadAndErrorForTesting = true; }

    void setReferrerPolicyForBindings(const AtomString&);
    String referrerPolicyForBindings() const;
    ReferrerPolicy referrerPolicy() const;

private:
    void parseAttribute(const QualifiedName&, const AtomString&) final;

    bool shouldLoadLink() final;
    void process();
    static void processCallback(Node*);
    void clearSheet();

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void didFinishInsertingNode() final;
    void removedFromAncestor(RemovalType, ContainerNode&) final;

    void initializeStyleSheet(Ref<StyleSheetContents>&&, const CachedCSSStyleSheet&, MediaQueryParserContext);

    // from CachedResourceClient
    void setCSSStyleSheet(const String& href, const URL& baseURL, const String& charset, const CachedCSSStyleSheet*) final;
    bool sheetLoaded() final;
    void notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred) final;
    void startLoadingDynamicSheet() final;

    void linkLoaded() final;
    void linkLoadingErrored() final;

    bool isAlternate() const { return m_disabledState == Unset && m_relAttribute.isAlternate; }
    
    void setDisabledState(bool);

    bool isURLAttribute(const Attribute&) const final;

    HTMLLinkElement(const QualifiedName&, Document&, bool createdByParser);

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const final;

    void finishParsingChildren() final;

    String debugDescription() const final;

    enum PendingSheetType : uint8_t { Unknown, ActiveSheet, InactiveSheet };
    void addPendingSheet(PendingSheetType);

    void removePendingSheet();

    LinkLoader m_linkLoader;
    Style::Scope* m_styleScope { nullptr };
    CachedResourceHandle<CachedCSSStyleSheet> m_cachedSheet;
    RefPtr<CSSStyleSheet> m_sheet;
    enum DisabledState : uint8_t {
        Unset,
        EnabledViaScript,
        Disabled
    };

    String m_type;
    String m_media;
    String m_integrityMetadataForPendingSheetRequest;
    std::unique_ptr<DOMTokenList> m_sizes;
    std::unique_ptr<DOMTokenList> m_relList;
    DisabledState m_disabledState;
    LinkRelAttribute m_relAttribute;
    bool m_loading : 1;
    bool m_createdByParser : 1;
    bool m_firedLoad : 1;
    bool m_loadedResource : 1;
    bool m_isHandlingBeforeLoad : 1;
    bool m_allowPrefetchLoadAndErrorForTesting : 1;
    PendingSheetType m_pendingSheetType;
};

}
