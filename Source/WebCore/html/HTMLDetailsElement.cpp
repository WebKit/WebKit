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

#include "config.h"
#include "HTMLDetailsElement.h"

#include "AXObjectCache.h"
#include "DocumentInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "HTMLSlotElement.h"
#include "HTMLSummaryElement.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "RenderBlockFlow.h"
#include "ShadowRoot.h"
#include "ShouldNotFireMutationEventsScope.h"
#include "SlotAssignment.h"
#include "Text.h"
#include "ToggleEvent.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLDetailsElement);

using namespace HTMLNames;

static const AtomString& summarySlotName()
{
    static MainThreadNeverDestroyed<const AtomString> summarySlot("summarySlot"_s);
    return summarySlot;
}

class DetailsSlotAssignment final : public NamedSlotAssignment {
private:
    void hostChildElementDidChange(const Element&, ShadowRoot&) override;
    const AtomString& slotNameForHostChild(const Node&) const override;
};

void DetailsSlotAssignment::hostChildElementDidChange(const Element& childElement, ShadowRoot& shadowRoot)
{
    if (is<HTMLSummaryElement>(childElement)) {
        // Don't check whether this is the first summary element
        // since we don't know the answer when this function is called inside Element::removedFrom.
        didChangeSlot(summarySlotName(), shadowRoot);
    } else
        didChangeSlot(NamedSlotAssignment::defaultSlotName(), shadowRoot);
}

const AtomString& DetailsSlotAssignment::slotNameForHostChild(const Node& child) const
{
    auto& details = downcast<HTMLDetailsElement>(*child.parentNode());

    // The first summary child gets assigned to the summary slot.
    if (is<HTMLSummaryElement>(child)) {
        if (&child == childrenOfType<HTMLSummaryElement>(details).first())
            return summarySlotName();
    }
    return NamedSlotAssignment::defaultSlotName();
}

Ref<HTMLDetailsElement> HTMLDetailsElement::create(const QualifiedName& tagName, Document& document)
{
    auto details = adoptRef(*new HTMLDetailsElement(tagName, document));
    details->addShadowRoot(ShadowRoot::create(document, makeUnique<DetailsSlotAssignment>()));
    return details;
}

HTMLDetailsElement::HTMLDetailsElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(detailsTag));
}

RenderPtr<RenderElement> HTMLDetailsElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, *this, WTFMove(style));
}

void HTMLDetailsElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    auto summarySlot = HTMLSlotElement::create(slotTag, document());
    summarySlot->setAttributeWithoutSynchronization(nameAttr, summarySlotName());
    m_summarySlot = summarySlot.get();

    auto defaultSummary = HTMLSummaryElement::create(summaryTag, document());
    defaultSummary->appendChild(Text::create(document(), defaultDetailsSummaryText()));
    m_defaultSummary = defaultSummary.get();

    summarySlot->appendChild(defaultSummary);
    root.appendChild(summarySlot);

    m_defaultSlot = HTMLSlotElement::create(slotTag, document());
    ASSERT(!hasAttribute(openAttr));
}

bool HTMLDetailsElement::isActiveSummary(const HTMLSummaryElement& summary) const
{
    if (!m_summarySlot->assignedNodes())
        return &summary == m_defaultSummary;

    if (summary.parentNode() != this)
        return false;

    RefPtr slot = shadowRoot()->findAssignedSlot(summary);
    if (!slot)
        return false;
    return slot == m_summarySlot.get();
}

void HTMLDetailsElement::queueDetailsToggleEventTask(DetailsState oldState, DetailsState newState)
{
    if (auto queuedEventData = queuedToggleEventData())
        oldState = queuedEventData->oldState;
    setQueuedToggleEventData({ oldState, newState });
    queueTaskKeepingThisNodeAlive(TaskSource::DOMManipulation, [this, newState] {
        auto queuedEventData = queuedToggleEventData();
        if (!queuedEventData || queuedEventData->newState != newState)
            return;
        clearQueuedToggleEventData();
        auto stringForState = [](DetailsState state) {
            return state == DetailsState::Closed ? "closed"_s : "open"_s;
        };
        dispatchEvent(ToggleEvent::create(eventNames().toggleEvent, { EventInit { }, stringForState(queuedEventData->oldState), stringForState(queuedEventData->newState) }, Event::IsCancelable::No));
    });
}

void HTMLDetailsElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == openAttr) {
        if (oldValue != newValue) {
            RefPtr root = shadowRoot();
            ASSERT(root);
            if (!newValue.isNull()) {
                root->appendChild(*m_defaultSlot);
                queueDetailsToggleEventTask(DetailsState::Closed, DetailsState::Open);
                if (document().settings().detailsNameAttributeEnabled() && !attributeWithoutSynchronization(nameAttr).isEmpty()) {
                    ShouldNotFireMutationEventsScope scope(document());
                    for (auto& otherDetailsElement : otherElementsInNameGroup())
                        otherDetailsElement->removeAttribute(openAttr);
                }
            } else {
                root->removeChild(*m_defaultSlot);
                queueDetailsToggleEventTask(DetailsState::Open, DetailsState::Closed);
            }
        }
    } else {
        ensureDetailsExclusivityAfterMutation();
        HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
    }
}

Node::InsertedIntoAncestorResult HTMLDetailsElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    ensureDetailsExclusivityAfterMutation();
    return HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
}

Vector<RefPtr<HTMLDetailsElement>> HTMLDetailsElement::otherElementsInNameGroup()
{
    Vector<RefPtr<HTMLDetailsElement>> otherElementsInNameGroup;
    for (auto& element : descendantsOfType<HTMLDetailsElement>(rootNode())) {
        if (&element != this && element.attributeWithoutSynchronization(nameAttr) == attributeWithoutSynchronization(nameAttr))
            otherElementsInNameGroup.append(&element);
    }
    return otherElementsInNameGroup;
}

void HTMLDetailsElement::ensureDetailsExclusivityAfterMutation()
{
    if (document().settings().detailsNameAttributeEnabled() && hasAttribute(openAttr) && !attributeWithoutSynchronization(nameAttr).isEmpty()) {
        ShouldNotFireMutationEventsScope scope(document());
        for (auto& otherDetailsElement : otherElementsInNameGroup()) {
            if (otherDetailsElement->hasAttribute(openAttr)) {
                toggleOpen();
                break;
            }
        }
    }
}

void HTMLDetailsElement::toggleOpen()
{
    setBooleanAttribute(openAttr, !hasAttribute(openAttr));

    // We need to post to the document because toggling this element will delete it.
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->postNotification(nullptr, &document(), AXObjectCache::AXExpandedChanged);
}

}
