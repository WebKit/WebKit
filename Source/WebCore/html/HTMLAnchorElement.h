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

#include "HTMLElement.h"
#include "HTMLNames.h"
#include "SharedStringHash.h"
#include "URLUtils.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class AdClickAttribution;
class DOMTokenList;

// Link relation bitmask values.
enum class Relation : uint8_t {
    NoReferrer = 1 << 0,
    NoOpener = 1 << 1,
    Opener = 1 << 2,
};

class HTMLAnchorElement : public HTMLElement, public URLUtils<HTMLAnchorElement> {
    WTF_MAKE_ISO_ALLOCATED(HTMLAnchorElement);
public:
    static Ref<HTMLAnchorElement> create(Document&);
    static Ref<HTMLAnchorElement> create(const QualifiedName&, Document&);

    virtual ~HTMLAnchorElement();

    WEBCORE_EXPORT URL href() const;
    void setHref(const AtomString&);

    const AtomString& name() const;

    WEBCORE_EXPORT String origin() const;

    WEBCORE_EXPORT String text();
    void setText(const String&);

    bool isLiveLink() const;

    bool willRespondToMouseClickEvents() final;

    bool hasRel(Relation) const;
    
    SharedStringHash visitedLinkHash() const;

    WEBCORE_EXPORT DOMTokenList& relList() const;

#if USE(SYSTEM_PREVIEW)
    WEBCORE_EXPORT bool isSystemPreviewLink() const;
#endif

protected:
    HTMLAnchorElement(const QualifiedName&, Document&);

    void parseAttribute(const QualifiedName&, const AtomString&) override;

private:
    bool supportsFocus() const override;
    bool isMouseFocusable() const override;
    bool isKeyboardFocusable(KeyboardEvent*) const override;
    void defaultEventHandler(Event&) final;
    void setActive(bool active = true, bool pause = false) final;
    void accessKeyAction(bool sendMouseEvents) final;
    bool isURLAttribute(const Attribute&) const final;
    bool canStartSelection() const final;
    String target() const override;
    int defaultTabIndex() const final;
    bool draggable() const final;
    bool isInteractiveContent() const final;

    String effectiveTarget() const;

    void sendPings(const URL& destinationURL);

    Optional<AdClickAttribution> parseAdClickAttribution() const;

    void handleClick(Event&);

    enum EventType {
        MouseEventWithoutShiftKey,
        MouseEventWithShiftKey,
        NonMouseEvent,
    };
    static EventType eventType(Event&);
    bool treatLinkAsLiveForEventType(EventType) const;

    Element* rootEditableElementForSelectionOnMouseDown() const;
    void setRootEditableElementForSelectionOnMouseDown(Element*);
    void clearRootEditableElementForSelectionOnMouseDown();

    bool m_hasRootEditableElementForSelectionOnMouseDown;
    bool m_wasShiftKeyDownOnMouseDown;
    OptionSet<Relation> m_linkRelations;

    // This is computed only once and must not be affected by subsequent URL changes.
    mutable Markable<SharedStringHash, SharedStringHashMarkableTraits> m_storedVisitedLinkHash;

    mutable std::unique_ptr<DOMTokenList> m_relList;
};

inline SharedStringHash HTMLAnchorElement::visitedLinkHash() const
{
    ASSERT(isLink());
    if (!m_storedVisitedLinkHash)
        m_storedVisitedLinkHash = computeVisitedLinkHash(document().baseURL(), attributeWithoutSynchronization(HTMLNames::hrefAttr));
    return *m_storedVisitedLinkHash;
}

// Functions shared with the other anchor elements (i.e., SVG).

bool isEnterKeyKeydownEvent(Event&);
bool shouldProhibitLinks(Element*);

} // namespace WebCore
