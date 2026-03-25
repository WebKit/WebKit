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
#include "ContainerNodeInlines.h"
#include "DocumentView.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "HTMLSlotElement.h"
#include "HTMLStyleElement.h"
#include "HTMLSummaryElement.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "PseudoClassChangeInvalidation.h"
#include "ShadowRoot.h"
#include "ShouldNotFireMutationEventsScope.h"
#include "SlotAssignment.h"
#include "Text.h"
#include "ToggleEvent.h"
#include "ToggleEventTask.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UserAgentParts.h"
#include "UserAgentStyle.h"
#include "UserAgentStyleSheets.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLDetailsElement);

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

        if (RefPtr associatedDetails = dynamicDowncast<HTMLDetailsElement>(shadowRoot.host())) {
            if (CheckedPtr cache = associatedDetails->protectedDocument()->existingAXObjectCache())
                cache->onDetailsSummarySlotChange(*associatedDetails);
        }
    } else
        didChangeSlot(NamedSlotAssignment::defaultSlotName(), shadowRoot);
}

const AtomString& DetailsSlotAssignment::slotNameForHostChild(const Node& child) const
{
    Ref details = downcast<HTMLDetailsElement>(*child.parentNode());

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

HTMLDetailsElement::~HTMLDetailsElement() = default;

void HTMLDetailsElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    Ref document = this->document();
    Ref summarySlot = HTMLSlotElement::create(slotTag, document);
    summarySlot->setAttributeWithoutSynchronization(nameAttr, summarySlotName());

    Ref defaultSummary = HTMLSummaryElement::create(summaryTag, document);
    defaultSummary->appendChild(Text::create(document, defaultDetailsSummaryText()));
    m_defaultSummary = defaultSummary.get();

    summarySlot->appendChild(defaultSummary);
    root.appendChild(summarySlot);
    m_summarySlot = WTF::move(summarySlot);

    Ref defaultSlot = HTMLSlotElement::create(slotTag, document);
    defaultSlot->setUserAgentPart(UserAgentParts::detailsContent());
    ASSERT(!hasAttributeWithoutSynchronization(openAttr));
    defaultSlot->setInlineStyleProperty(CSSPropertyContentVisibility, CSSValueHidden);
    defaultSlot->setInlineStyleProperty(CSSPropertyDisplay, CSSValueBlock);
    root.appendChild(defaultSlot);
    m_defaultSlot = WTF::move(defaultSlot);

    static MainThreadNeverDestroyed<const String> stylesheet(StringImpl::createWithoutCopying(detailsElementShadowUserAgentStyleSheet));
    Ref style = HTMLStyleElement::create(HTMLNames::styleTag, document, false);
    style->setTextContent(String { stylesheet });
    root.appendChild(WTF::move(style));
}

bool HTMLDetailsElement::isActiveSummary(const HTMLSummaryElement& summary) const
{
    RefPtr summarySlot = m_summarySlot.get();
    if (!summarySlot->assignedNodes())
        return &summary == m_defaultSummary;

    if (summary.parentNode() != this)
        return false;

    RefPtr slot = protectedShadowRoot()->findAssignedSlot(summary);
    return slot && slot == summarySlot.get();
}

void HTMLDetailsElement::queueDetailsToggleEventTask(ToggleState oldState, ToggleState newState)
{
    if (!m_toggleEventTask)
        m_toggleEventTask = ToggleEventTask::create(*this);

    RefPtr { m_toggleEventTask }->queue(oldState, newState, nullptr);
}

void HTMLDetailsElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
    if (name == openAttr) {
        if (oldValue != newValue) {
            RefPtr root = shadowRoot();
            RefPtr defaultSlot = m_defaultSlot;
            ASSERT(root);
            auto isOpen = !newValue.isNull();
            Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Open, isOpen);
            m_isOpen = isOpen;
            if (!newValue.isNull()) {
                defaultSlot->removeInlineStyleProperty(CSSPropertyContentVisibility);
                queueDetailsToggleEventTask(ToggleState::Closed, ToggleState::Open);
                if (!attributeWithoutSynchronization(nameAttr).isEmpty()) {
                    ShouldNotFireMutationEventsScope scope(document());
                    for (auto& otherDetailsElement : otherElementsInNameGroup())
                        otherDetailsElement->removeAttribute(openAttr);
                }
            } else {
                defaultSlot->setInlineStyleProperty(CSSPropertyContentVisibility, CSSValueHidden);
                queueDetailsToggleEventTask(ToggleState::Open, ToggleState::Closed);
            }
        }
    } else
        ensureDetailsExclusivityAfterMutation();
}

Node::InsertedIntoAncestorResult HTMLDetailsElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLDetailsElement::didFinishInsertingNode()
{
    ensureDetailsExclusivityAfterMutation();
}

Vector<Ref<HTMLDetailsElement>> HTMLDetailsElement::otherElementsInNameGroup()
{
    Vector<Ref<HTMLDetailsElement>> otherElementsInNameGroup;
    const auto& detailElementName = attributeWithoutSynchronization(nameAttr);
    for (Ref element : descendantsOfType<HTMLDetailsElement>(rootNode())) {
        if (element.ptr() != this && element->attributeWithoutSynchronization(nameAttr) == detailElementName)
            otherElementsInNameGroup.append(WTF::move(element));
    }
    return otherElementsInNameGroup;
}

void HTMLDetailsElement::ensureDetailsExclusivityAfterMutation()
{
    if (hasAttributeWithoutSynchronization(openAttr) && !attributeWithoutSynchronization(nameAttr).isEmpty()) {
        ShouldNotFireMutationEventsScope scope(document());
        for (auto& otherDetailsElement : otherElementsInNameGroup()) {
            if (otherDetailsElement->hasAttributeWithoutSynchronization(openAttr)) {
                toggleOpen();
                break;
            }
        }
    }
}

void HTMLDetailsElement::toggleOpen()
{
    setBooleanAttribute(HTMLNames::openAttr, !hasAttributeWithoutSynchronization(HTMLNames::openAttr));
}

bool HTMLDetailsElement::isOpen() const
{
    return m_isOpen;
}

} // namespace WebCore
