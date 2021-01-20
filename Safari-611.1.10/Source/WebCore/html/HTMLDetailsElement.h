/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

namespace WebCore {

class HTMLSlotElement;

template<typename T> class EventSender;
typedef EventSender<HTMLDetailsElement> DetailEventSender;

class HTMLDetailsElement final : public HTMLElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLDetailsElement);
public:
    static Ref<HTMLDetailsElement> create(const QualifiedName& tagName, Document&);
    ~HTMLDetailsElement();

    void toggleOpen();

    bool isOpen() const { return m_isOpen; }
    bool isActiveSummary(const HTMLSummaryElement&) const;

    void dispatchPendingEvent(DetailEventSender*);
    
private:
    HTMLDetailsElement(const QualifiedName&, Document&);

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    void parseAttribute(const QualifiedName&, const AtomString&) final;

    void didAddUserAgentShadowRoot(ShadowRoot&) final;
    bool hasCustomFocusLogic() const final { return true; }
    bool isInteractiveContent() const final { return true; }

    bool m_isOpen { false };
    HTMLSlotElement* m_summarySlot { nullptr };
    HTMLSummaryElement* m_defaultSummary { nullptr };
    RefPtr<HTMLSlotElement> m_defaultSlot;
};

} // namespace WebCore
