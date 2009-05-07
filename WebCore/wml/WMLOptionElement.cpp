/**
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#if ENABLE(WML)
#include "WMLOptionElement.h"

#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "NodeRenderStyle.h"
#include "RenderStyle.h"
#include "WMLNames.h"

namespace WebCore {

using namespace WMLNames;

WMLOptionElement::WMLOptionElement(const QualifiedName& tagName, Document* doc)
    : WMLFormControlElement(tagName, doc)
    , m_data(this)
{
}

WMLOptionElement::~WMLOptionElement()
{
}

const AtomicString& WMLOptionElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, option, ("option"));
    return option;
}

// FIXME: Activate once WMLSelectElement is available 
#if 0
static inline WMLElement* ownerSelectElement()
{
    Node* select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();

    if (!select)
        return 0;

    return static_cast<WMLSelectElement*>(select);
}
#endif

void WMLOptionElement::accessKeyAction(bool)
{
    // FIXME: Activate once WMLSelectElement is available 
#if 0
    if (WMLSelectElement* select = ownerSelectElement())
        select->accessKeySetSelectedIndex(index());
#endif
}

void WMLOptionElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    // FIXME: Activate once WMLSelectElement is available 
#if 0
    if (WMLSelectElement* select = ownerSelectElement())
        select->childrenChanged(changedByParser);
#endif

    WMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

void WMLOptionElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::valueAttr)
        m_data.setValue(parseValueSubstitutingVariableReferences(attr->value()));
    else if (attr->name() == HTMLNames::titleAttr)
        m_data.setLabel(parseValueSubstitutingVariableReferences(attr->value()));
    else if (attr->name() == onpickAttr) {
        // Register intrinsic event in card
        RefPtr<WMLIntrinsicEvent> event = WMLIntrinsicEvent::create(document(), attr->value());

        createEventHandlerIfNeeded();
        eventHandler()->registerIntrinsicEvent(WMLIntrinsicEventOnPick, event);
    } else
        WMLElement::parseMappedAttribute(attr);
}

void WMLOptionElement::attach()
{
    if (parentNode()->renderStyle())
        setRenderStyle(styleForRenderer());
    WMLElement::attach();
}

void WMLOptionElement::detach()
{
    m_style.clear();
    WMLElement::detach();
}

void WMLOptionElement::setRenderStyle(PassRefPtr<RenderStyle> style)
{
    m_style = style;
}

void WMLOptionElement::insertedIntoDocument()
{
    // FIXME: Activate once WMLSelectElement is available 
#if 0
    WMLSelectElement* select;
    if (selected() && (select = ownerSelectElement()))
        select->scrollToSelection();
#endif

    WMLElement::insertedIntoDocument();
}

bool WMLOptionElement::selected() const
{
    return m_data.selected();
}

void WMLOptionElement::setSelectedState(bool selected)
{
    OptionElement::setSelectedState(m_data, selected);
}

String WMLOptionElement::value() const
{
    return OptionElement::collectOptionValue(m_data, document());
}

String WMLOptionElement::textIndentedToRespectGroupLabel() const
{
    return OptionElement::collectOptionTextRespectingGroupLabel(m_data, document());
}

RenderStyle* WMLOptionElement::nonRendererRenderStyle() const
{
    return m_style.get();
}

void WMLOptionElement::handleIntrinsicEventIfNeeded()
{
    WMLIntrinsicEventHandler* eventHandler = this->eventHandler();
    if (!eventHandler)
        return;

    if (eventHandler->hasIntrinsicEvent(WMLIntrinsicEventOnPick))
        eventHandler->triggerIntrinsicEvent(WMLIntrinsicEventOnPick);
}

}

#endif
