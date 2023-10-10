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

enum class DetailsState : bool {
    Open,
    Closed,
};

struct DetailsToggleEventData {
    DetailsState oldState;
    DetailsState newState;
};

class HTMLDetailsElement final : public HTMLElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLDetailsElement);
public:
    static Ref<HTMLDetailsElement> create(const QualifiedName& tagName, Document&);

    void toggleOpen();

    bool isActiveSummary(const HTMLSummaryElement&) const;

    void queueDetailsToggleEventTask(DetailsState oldState, DetailsState newState);

    std::optional<DetailsToggleEventData> queuedToggleEventData() const { return m_queuedToggleEventData; }
    void setQueuedToggleEventData(DetailsToggleEventData data) { m_queuedToggleEventData = data; }
    void clearQueuedToggleEventData() { m_queuedToggleEventData = std::nullopt; }

private:
    HTMLDetailsElement(const QualifiedName&, Document&);

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    Vector<RefPtr<HTMLDetailsElement>> otherElementsInNameGroup();
    void ensureDetailsExclusivityAfterMutation();
    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;

    void didAddUserAgentShadowRoot(ShadowRoot&) final;
    bool isInteractiveContent() const final { return true; }

    WeakPtr<HTMLSlotElement, WeakPtrImplWithEventTargetData> m_summarySlot;
    WeakPtr<HTMLSummaryElement, WeakPtrImplWithEventTargetData> m_defaultSummary;
    RefPtr<HTMLSlotElement> m_defaultSlot;

    std::optional<DetailsToggleEventData> m_queuedToggleEventData;
};

} // namespace WebCore
