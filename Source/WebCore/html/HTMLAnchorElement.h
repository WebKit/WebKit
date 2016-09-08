/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "LinkHash.h"
#include "URLUtils.h"

namespace WebCore {

class DOMTokenList;

// Link relation bitmask values.
// FIXME: Uncomment as the various link relations are implemented.
enum {
//     RelationAlternate   = 0x00000001,
//     RelationArchives    = 0x00000002,
//     RelationAuthor      = 0x00000004,
//     RelationBoomark     = 0x00000008,
//     RelationExternal    = 0x00000010,
//     RelationFirst       = 0x00000020,
//     RelationHelp        = 0x00000040,
//     RelationIndex       = 0x00000080,
//     RelationLast        = 0x00000100,
//     RelationLicense     = 0x00000200,
//     RelationNext        = 0x00000400,
//     RelationNoFolow    = 0x00000800,
    RelationNoReferrer     = 0x00001000,
//     RelationPrev        = 0x00002000,
//     RelationSearch      = 0x00004000,
//     RelationSidebar     = 0x00008000,
//     RelationTag         = 0x00010000,
//     RelationUp          = 0x00020000,
};

class HTMLAnchorElement : public HTMLElement, public URLUtils<HTMLAnchorElement> {
public:
    static Ref<HTMLAnchorElement> create(Document&);
    static Ref<HTMLAnchorElement> create(const QualifiedName&, Document&);

    virtual ~HTMLAnchorElement();

    WEBCORE_EXPORT URL href() const;
    void setHref(const AtomicString&);

    const AtomicString& name() const;

    WEBCORE_EXPORT String origin() const;

    WEBCORE_EXPORT String text();
    void setText(const String&);

    bool isLiveLink() const;

    bool willRespondToMouseClickEvents() final;

    bool hasRel(uint32_t relation) const;
    
    LinkHash visitedLinkHash() const;
    void invalidateCachedVisitedLinkHash() { m_cachedVisitedLinkHash = 0; }

    WEBCORE_EXPORT DOMTokenList& relList();

protected:
    HTMLAnchorElement(const QualifiedName&, Document&);

    void parseAttribute(const QualifiedName&, const AtomicString&) override;

private:
    bool supportsFocus() const override;
    bool isMouseFocusable() const override;
    bool isKeyboardFocusable(KeyboardEvent&) const override;
    void defaultEventHandler(Event&) final;
    void setActive(bool active = true, bool pause = false) final;
    void accessKeyAction(bool sendMouseEvents) final;
    bool isURLAttribute(const Attribute&) const final;
    bool canStartSelection() const final;
    String target() const override;
    int tabIndex() const final;
    bool draggable() const final;

    void sendPings(const URL& destinationURL);

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

    bool m_hasRootEditableElementForSelectionOnMouseDown : 1;
    bool m_wasShiftKeyDownOnMouseDown : 1;
    uint32_t m_linkRelations : 30;
    mutable LinkHash m_cachedVisitedLinkHash;

    std::unique_ptr<DOMTokenList> m_relList;
};

inline LinkHash HTMLAnchorElement::visitedLinkHash() const
{
    if (!m_cachedVisitedLinkHash)
        m_cachedVisitedLinkHash = WebCore::visitedLinkHash(document().baseURL(), attributeWithoutSynchronization(HTMLNames::hrefAttr));
    return m_cachedVisitedLinkHash; 
}

// Functions shared with the other anchor elements (i.e., SVG).

bool isEnterKeyKeydownEvent(Event&);
bool shouldProhibitLinks(Element*);

} // namespace WebCore
