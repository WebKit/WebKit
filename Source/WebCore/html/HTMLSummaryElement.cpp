/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "HTMLSummaryElement.h"

#if ENABLE(DETAILS_ELEMENT)
#include "DetailsMarkerControl.h"
#include "EventNames.h"
#include "HTMLDetailsElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLSlotElement.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "PlatformMouseEvent.h"
#include "RenderBlockFlow.h"
#include "ShadowRoot.h"
#include "SlotAssignment.h"

namespace WebCore {

using namespace HTMLNames;

class SummarySlotElement final : public SlotAssignment {
private:
    void hostChildElementDidChange(const Element&, ShadowRoot& shadowRoot) override
    {
        didChangeSlot(SlotAssignment::defaultSlotName(), SlotAssignment::ChangeType::DirectChild, shadowRoot);
    }

    const AtomicString& slotNameForHostChild(const Node&) const override { return SlotAssignment::defaultSlotName(); }
};

Ref<HTMLSummaryElement> HTMLSummaryElement::create(const QualifiedName& tagName, Document& document)
{
    Ref<HTMLSummaryElement> summary = adoptRef(*new HTMLSummaryElement(tagName, document));
    summary->addShadowRoot(ShadowRoot::create(document, std::make_unique<SummarySlotElement>()));
    return summary;
}

HTMLSummaryElement::HTMLSummaryElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(summaryTag));
}

RenderPtr<RenderElement> HTMLSummaryElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderBlockFlow>(*this, WTFMove(style));
}

void HTMLSummaryElement::didAddUserAgentShadowRoot(ShadowRoot* root)
{
    root->appendChild(DetailsMarkerControl::create(document()), ASSERT_NO_EXCEPTION);
    root->appendChild(HTMLSlotElement::create(slotTag, document()));
}

HTMLDetailsElement* HTMLSummaryElement::detailsElement() const
{
    auto* parent = parentElement();
    if (parent && is<HTMLDetailsElement>(*parent))
        return downcast<HTMLDetailsElement>(parent);
    // Fallback summary element is in the shadow tree.
    auto* host = shadowHost();
    if (host && is<HTMLDetailsElement>(*host))
        return downcast<HTMLDetailsElement>(host);
    return nullptr;
}

bool HTMLSummaryElement::isActiveSummary() const
{
    HTMLDetailsElement* details = detailsElement();
    if (!details)
        return false;
    return details->isActiveSummary(*this);
}

static bool isClickableControl(Node* node)
{
    ASSERT(node);
    if (!is<Element>(*node))
        return false;
    Element& element = downcast<Element>(*node);
    if (is<HTMLFormControlElement>(element))
        return true;
    Element* host = element.shadowHost();
    return host && is<HTMLFormControlElement>(host);
}

bool HTMLSummaryElement::supportsFocus() const
{
    return isActiveSummary();
}

void HTMLSummaryElement::defaultEventHandler(Event& event)
{
    if (isActiveSummary() && renderer()) {
        if (event.type() == eventNames().DOMActivateEvent && !isClickableControl(event.target()->toNode())) {
            if (HTMLDetailsElement* details = detailsElement())
                details->toggleOpen();
            event.setDefaultHandled();
            return;
        }

        if (is<KeyboardEvent>(event)) {
            KeyboardEvent& keyboardEvent = downcast<KeyboardEvent>(event);
            if (keyboardEvent.type() == eventNames().keydownEvent && keyboardEvent.keyIdentifier() == "U+0020") {
                setActive(true, true);
                // No setDefaultHandled() - IE dispatches a keypress in this case.
                return;
            }
            if (keyboardEvent.type() == eventNames().keypressEvent) {
                switch (keyboardEvent.charCode()) {
                case '\r':
                    dispatchSimulatedClick(&event);
                    keyboardEvent.setDefaultHandled();
                    return;
                case ' ':
                    // Prevent scrolling down the page.
                    keyboardEvent.setDefaultHandled();
                    return;
                }
            }
            if (keyboardEvent.type() == eventNames().keyupEvent && keyboardEvent.keyIdentifier() == "U+0020") {
                if (active())
                    dispatchSimulatedClick(&event);
                keyboardEvent.setDefaultHandled();
                return;
            }
        }
    }

    HTMLElement::defaultEventHandler(event);
}

bool HTMLSummaryElement::willRespondToMouseClickEvents()
{
    if (isActiveSummary() && renderer())
        return true;

    return HTMLElement::willRespondToMouseClickEvents();
}

}

#endif
