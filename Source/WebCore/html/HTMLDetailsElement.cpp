/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#if ENABLE(DETAILS_ELEMENT)
#include "AXObjectCache.h"
#include "ElementIterator.h"
#include "HTMLSlotElement.h"
#include "HTMLSummaryElement.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "RenderBlockFlow.h"
#include "ShadowRoot.h"
#include "SlotAssignment.h"
#include "Text.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using namespace HTMLNames;

static const AtomicString& summarySlotName()
{
    static NeverDestroyed<AtomicString> summarySlot("summarySlot");
    return summarySlot;
}

static AtomicString slotNameFunction(const Node& child)
{
    auto& parent = *child.parentNode();
    ASSERT(is<HTMLDetailsElement>(parent));
    auto& details = downcast<HTMLDetailsElement>(parent);

    // The first summary child gets assigned to the summary slot.
    if (is<HTMLSummaryElement>(child)) {
        if (&child == childrenOfType<HTMLSummaryElement>(details).first())
            return summarySlotName();
    }
    // Everything else is assigned to the default slot if details is open.
    if (details.isOpen())
        return SlotAssignment::defaultSlotName();

    // Otherwise don't render the content.
    return nullAtom;
};

Ref<HTMLDetailsElement> HTMLDetailsElement::create(const QualifiedName& tagName, Document& document)
{
    auto details = adoptRef(*new HTMLDetailsElement(tagName, document));
    details->addShadowRoot(ShadowRoot::create(document, std::make_unique<SlotAssignment>(slotNameFunction)));
    return details;
}

HTMLDetailsElement::HTMLDetailsElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(detailsTag));
}

RenderPtr<RenderElement> HTMLDetailsElement::createElementRenderer(Ref<RenderStyle>&& style, const RenderTreePosition&)
{
    return createRenderer<RenderBlockFlow>(*this, WTFMove(style));
}

void HTMLDetailsElement::didAddUserAgentShadowRoot(ShadowRoot* root)
{
    auto summarySlot = HTMLSlotElement::create(slotTag, document());
    summarySlot->setAttribute(nameAttr, summarySlotName());
    m_summarySlot = summarySlot.ptr();

    auto defaultSummary = HTMLSummaryElement::create(summaryTag, document());
    defaultSummary->appendChild(Text::create(document(), defaultDetailsSummaryText()), ASSERT_NO_EXCEPTION);
    m_defaultSummary = defaultSummary.ptr();

    summarySlot->appendChild(WTFMove(defaultSummary));
    root->appendChild(WTFMove(summarySlot));

    auto defaultSlot = HTMLSlotElement::create(slotTag, document());
    root->appendChild(WTFMove(defaultSlot));
}

bool HTMLDetailsElement::isActiveSummary(const HTMLSummaryElement& summary) const
{
    if (!m_summarySlot->assignedNodes())
        return &summary == m_defaultSummary;

    if (summary.parentNode() != this)
        return false;

    auto* slot = shadowRoot()->findAssignedSlot(summary);
    if (!slot)
        return false;
    return slot == m_summarySlot;
}

void HTMLDetailsElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == openAttr) {
        bool oldValue = m_isOpen;
        m_isOpen = !value.isNull();
        if (oldValue != m_isOpen)
            shadowRoot()->invalidateSlotAssignments();
    } else
        HTMLElement::parseAttribute(name, value);
}


void HTMLDetailsElement::toggleOpen()
{
    setAttribute(openAttr, m_isOpen ? nullAtom : emptyAtom);

    // We need to post to the document because toggling this element will delete it.
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->postNotification(nullptr, &document(), AXObjectCache::AXExpandedChanged);
}

}

#endif
