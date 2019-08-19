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
#include "ElementIterator.h"
#include "EventNames.h"
#include "EventSender.h"
#include "HTMLSlotElement.h"
#include "HTMLSummaryElement.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "RenderBlockFlow.h"
#include "ShadowRoot.h"
#include "SlotAssignment.h"
#include "Text.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLDetailsElement);

using namespace HTMLNames;

static DetailEventSender& detailToggleEventSender()
{
    static NeverDestroyed<DetailEventSender> sharedToggleEventSender(eventNames().toggleEvent);
    return sharedToggleEventSender;
}

static const AtomString& summarySlotName()
{
    static NeverDestroyed<AtomString> summarySlot("summarySlot");
    return summarySlot;
}

class DetailsSlotAssignment final : public SlotAssignment {
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
        didChangeSlot(SlotAssignment::defaultSlotName(), shadowRoot);
}

const AtomString& DetailsSlotAssignment::slotNameForHostChild(const Node& child) const
{
    auto& parent = *child.parentNode();
    ASSERT(is<HTMLDetailsElement>(parent));
    auto& details = downcast<HTMLDetailsElement>(parent);

    // The first summary child gets assigned to the summary slot.
    if (is<HTMLSummaryElement>(child)) {
        if (&child == childrenOfType<HTMLSummaryElement>(details).first())
            return summarySlotName();
    }
    return SlotAssignment::defaultSlotName();
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

HTMLDetailsElement::~HTMLDetailsElement()
{
    detailToggleEventSender().cancelEvent(*this);
}

RenderPtr<RenderElement> HTMLDetailsElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderBlockFlow>(*this, WTFMove(style));
}

void HTMLDetailsElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    auto summarySlot = HTMLSlotElement::create(slotTag, document());
    summarySlot->setAttributeWithoutSynchronization(nameAttr, summarySlotName());
    m_summarySlot = summarySlot.ptr();

    auto defaultSummary = HTMLSummaryElement::create(summaryTag, document());
    defaultSummary->appendChild(Text::create(document(), defaultDetailsSummaryText()));
    m_defaultSummary = defaultSummary.ptr();

    summarySlot->appendChild(defaultSummary);
    root.appendChild(summarySlot);

    m_defaultSlot = HTMLSlotElement::create(slotTag, document());
    ASSERT(!m_isOpen);
}

bool HTMLDetailsElement::isActiveSummary(const HTMLSummaryElement& summary) const
{
    if (!m_summarySlot->assignedNodes())
        return &summary == m_defaultSummary;

    if (summary.parentNode() != this)
        return false;

    auto slot = makeRefPtr(shadowRoot()->findAssignedSlot(summary));
    if (!slot)
        return false;
    return slot == m_summarySlot;
}

void HTMLDetailsElement::dispatchPendingEvent(DetailEventSender* eventSender)
{
    ASSERT_UNUSED(eventSender, eventSender == &detailToggleEventSender());
    dispatchEvent(Event::create(eventNames().toggleEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void HTMLDetailsElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == openAttr) {
        bool oldValue = m_isOpen;
        m_isOpen = !value.isNull();
        if (oldValue != m_isOpen) {
            auto root = makeRefPtr(shadowRoot());
            ASSERT(root);
            if (m_isOpen)
                root->appendChild(*m_defaultSlot);
            else
                root->removeChild(*m_defaultSlot);

            // https://html.spec.whatwg.org/#details-notification-task-steps.
            detailToggleEventSender().cancelEvent(*this);
            detailToggleEventSender().dispatchEventSoon(*this);
        }
    } else
        HTMLElement::parseAttribute(name, value);
}


void HTMLDetailsElement::toggleOpen()
{
    setAttributeWithoutSynchronization(openAttr, m_isOpen ? nullAtom() : emptyAtom());

    // We need to post to the document because toggling this element will delete it.
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->postNotification(nullptr, &document(), AXObjectCache::AXExpandedChanged);
}

}
