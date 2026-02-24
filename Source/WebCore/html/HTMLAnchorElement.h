/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2007-2016 Apple Inc. All rights reserved.
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

#include <WebCore/Document.h>
#include <WebCore/HTMLElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/SharedStringHash.h>
#include <WebCore/SpeculationRules.h>
#include <WebCore/URLDecomposition.h>
#include <wtf/OptionSet.h>

namespace WebCore {

class DOMTokenList;

enum class ReferrerPolicy : uint8_t;

// Link relation bitmask values.
enum class Relation : uint8_t {
    NoReferrer = 1 << 0,
    NoOpener = 1 << 1,
    Opener = 1 << 2,
};

class HTMLAnchorElement : public HTMLElement, public URLDecomposition {
    WTF_MAKE_TZONE_ALLOCATED(HTMLAnchorElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(HTMLAnchorElement);
public:
    static Ref<HTMLAnchorElement> create(Document&);
    static Ref<HTMLAnchorElement> create(const QualifiedName&, Document&);

    virtual ~HTMLAnchorElement();

    WEBCORE_EXPORT URL href() const;

    const AtomString& name() const;

    WEBCORE_EXPORT String origin() const;

    WEBCORE_EXPORT void setProtocol(StringView value);

    WEBCORE_EXPORT String text();
    void setText(String&&);

    bool isLiveLink() const;

    bool willRespondToMouseClickEventsWithEditability(Editability) const final;

    bool NODELETE hasRel(Relation) const;
    
    inline SharedStringHash visitedLinkHash() const;

    WEBCORE_EXPORT DOMTokenList& relList();

#if USE(SYSTEM_PREVIEW)
    WEBCORE_EXPORT bool isSystemPreviewLink();
#endif

    String referrerPolicyForBindings() const;
    ReferrerPolicy referrerPolicy() const;

    Node::InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode& parentOfInsertedTree) override;
    void didFinishInsertingNode() override;

    AtomString target() const override;

    void setShouldBePrefetched(SpeculationRules::Eagerness, Vector<String>&& tags, std::optional<ReferrerPolicy>&&);

protected:
    HTMLAnchorElement(const QualifiedName&, Document&);

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) override;

private:
    bool supportsFocus() const override;
    bool isMouseFocusable() const override;
    bool isKeyboardFocusable(const FocusEventData&) const override;
    void defaultEventHandler(Event&) final;
    void setActive(bool active, Style::InvalidationScope) final;
    bool NODELETE isURLAttribute(const Attribute&) const final;
    bool canStartSelection() const final;
    int defaultTabIndex() const final;
    bool draggable() const final;
    bool isInteractiveContent() const final;

    AtomString effectiveTarget() const;

    void sendPings(const URL& destinationURL);

    std::optional<URL> attributionDestinationURLForPCM() const;
    std::optional<RegistrableDomain> mainDocumentRegistrableDomainForPCM() const;
    std::optional<PCM::EphemeralNonce> attributionSourceNonceForPCM() const;
    std::optional<PrivateClickMeasurement> parsePrivateClickMeasurementForSKAdNetwork(const URL&) const;
    std::optional<PrivateClickMeasurement> parsePrivateClickMeasurement(const URL&) const;

    void handleClick(Event&);

    enum EventType {
        MouseEventWithoutShiftKey,
        MouseEventWithShiftKey,
        NonMouseEvent,
    };
    static EventType NODELETE eventType(Event&);
    bool treatLinkAsLiveForEventType(EventType) const;

    Element* rootEditableElementForSelectionOnMouseDown() const;
    void setRootEditableElementForSelectionOnMouseDown(Element*);
    void clearRootEditableElementForSelectionOnMouseDown();

    URL fullURL() const final { return href(); }
    void setFullURL(const URL&) final;

    void checkForSpeculationRules();

    enum class PrefetchEagerness : uint8_t {
        None,
        Conservative,
        Immediate,
    };

    PrefetchEagerness m_prefetchEagerness { PrefetchEagerness::None };
    Vector<String> m_speculationRulesTags;
    std::optional<ReferrerPolicy> m_prefetchReferrerPolicy;

    bool m_hasRootEditableElementForSelectionOnMouseDown { false };
    bool m_wasShiftKeyDownOnMouseDown { false };
    OptionSet<Relation> m_linkRelations;

    // This is computed only once and must not be affected by subsequent URL changes.
    mutable Markable<SharedStringHash> m_storedVisitedLinkHash;

    const std::unique_ptr<DOMTokenList> m_relList;
};

// Functions shared with the other anchor elements (i.e., SVG).

bool isEnterKeyKeydownEvent(Event&);
bool shouldProhibitLinks(Element*);

} // namespace WebCore
