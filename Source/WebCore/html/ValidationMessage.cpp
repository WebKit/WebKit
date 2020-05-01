/*
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ValidationMessage.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLBRElement.h"
#include "HTMLDivElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLNames.h"
#include "Page.h"
#include "RenderBlock.h"
#include "RenderObject.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "Text.h"
#include "ValidationMessageClient.h"

namespace WebCore {

using namespace HTMLNames;

ValidationMessage::ValidationMessage(HTMLFormControlElement* element)
    : m_element(element)
{
    ASSERT(m_element);
}

ValidationMessage::~ValidationMessage()
{
    if (ValidationMessageClient* client = validationMessageClient()) {
        client->hideValidationMessage(*m_element);
        return;
    }

    deleteBubbleTree();
}

ValidationMessageClient* ValidationMessage::validationMessageClient() const
{
    if (Page* page = m_element->document().page())
        return page->validationMessageClient();
    return 0;
}

void ValidationMessage::updateValidationMessage(const String& message)
{
    // We want to hide the validation message as soon as the user starts
    // typing, even if a constraint is still violated. Thefore, we hide the message instead
    // of updating it if it is already visible.
    if (isVisible()) {
        requestToHideMessage();
        return;
    }

    String updatedMessage = message;
    if (!validationMessageClient()) {
        // HTML5 specification doesn't ask UA to show the title attribute value
        // with the validationMessage. However, this behavior is same as Opera
        // and the specification describes such behavior as an example.
        if (!updatedMessage.isEmpty()) {
            const AtomString& title = m_element->attributeWithoutSynchronization(titleAttr);
            if (!title.isEmpty())
                updatedMessage = updatedMessage + '\n' + title;
        }
    }

    if (updatedMessage.isEmpty()) {
        requestToHideMessage();
        return;
    }
    setMessage(updatedMessage);
}

void ValidationMessage::setMessage(const String& message)
{
    if (ValidationMessageClient* client = validationMessageClient()) {
        client->showValidationMessage(*m_element, message);
        return;
    }

    // Don't modify the DOM tree in this context.
    // If so, an assertion in Element::isFocusable() fails.
    ASSERT(!message.isEmpty());
    m_message = message;
    if (!m_bubble)
        m_timer = makeUnique<Timer>(*this, &ValidationMessage::buildBubbleTree);
    else
        m_timer = makeUnique<Timer>(*this, &ValidationMessage::setMessageDOMAndStartTimer);
    m_timer->startOneShot(0_s);
}

void ValidationMessage::setMessageDOMAndStartTimer()
{
    ASSERT(!validationMessageClient());
    ASSERT(m_messageHeading);
    ASSERT(m_messageBody);
    m_messageHeading->removeChildren();
    m_messageBody->removeChildren();
    Vector<String> lines = m_message.split('\n');
    Document& document = m_messageHeading->document();
    for (unsigned i = 0; i < lines.size(); ++i) {
        if (i) {
            m_messageBody->appendChild(Text::create(document, lines[i]));
            if (i < lines.size() - 1)
                m_messageBody->appendChild(HTMLBRElement::create(document));
        } else
            m_messageHeading->setInnerText(lines[i]);
    }

    int magnification = document.page() ? document.page()->settings().validationMessageTimerMagnification() : -1;
    if (magnification <= 0)
        m_timer = nullptr;
    else {
        m_timer = makeUnique<Timer>(*this, &ValidationMessage::deleteBubbleTree);
        m_timer->startOneShot(std::max(5_s, 1_ms * static_cast<double>(m_message.length()) * magnification));
    }
}

static void adjustBubblePosition(const LayoutRect& hostRect, HTMLElement* bubble)
{
    ASSERT(bubble);
    if (hostRect.isEmpty())
        return;
    double hostX = hostRect.x();
    double hostY = hostRect.y();
    if (RenderObject* renderer = bubble->renderer()) {
        if (RenderBox* container = renderer->containingBlock()) {
            FloatPoint containerLocation = container->localToAbsolute();
            hostX -= containerLocation.x() + container->borderLeft();
            hostY -= containerLocation.y() + container->borderTop();
        }
    }

    bubble->setInlineStyleProperty(CSSPropertyTop, hostY + hostRect.height(), CSSUnitType::CSS_PX);
    // The 'left' value of ::-webkit-validation-bubble-arrow.
    const int bubbleArrowTopOffset = 32;
    double bubbleX = hostX;
    if (hostRect.width() / 2 < bubbleArrowTopOffset)
        bubbleX = std::max(hostX + hostRect.width() / 2 - bubbleArrowTopOffset, 0.0);
    bubble->setInlineStyleProperty(CSSPropertyLeft, bubbleX, CSSUnitType::CSS_PX);
}

void ValidationMessage::buildBubbleTree()
{
    ASSERT(!validationMessageClient());

    if (!m_element->renderer())
        return;

    ShadowRoot& shadowRoot = m_element->ensureUserAgentShadowRoot();

    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleName("-webkit-validation-bubble", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleArrowClipperName("-webkit-validation-bubble-arrow-clipper", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleArrowName("-webkit-validation-bubble-arrow", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleMessageName("-webkit-validation-bubble-message", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleIconName("-webkit-validation-bubble-icon", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleTextBlockName("-webkit-validation-bubble-text-block", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleHeadingName("-webkit-validation-bubble-heading", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleBodyName("-webkit-validation-bubble-body", AtomString::ConstructFromLiteral);

    Document& document = m_element->document();
    m_bubble = HTMLDivElement::create(document);
    m_bubble->setPseudo(webkitValidationBubbleName);
    // Need to force position:absolute because RenderMenuList doesn't assume it
    // contains non-absolute or non-fixed renderers as children.
    m_bubble->setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);
    shadowRoot.appendChild(*m_bubble);

    auto weakElement = makeWeakPtr(*m_element);

    document.updateLayout();

    if (!weakElement || !m_element->renderer())
        return;

    adjustBubblePosition(m_element->renderer()->absoluteBoundingBoxRect(), m_bubble.get());

    auto clipper = HTMLDivElement::create(document);
    clipper->setPseudo(webkitValidationBubbleArrowClipperName);
    auto bubbleArrow = HTMLDivElement::create(document);
    bubbleArrow->setPseudo(webkitValidationBubbleArrowName);
    clipper->appendChild(bubbleArrow);
    m_bubble->appendChild(clipper);

    auto message = HTMLDivElement::create(document);
    message->setPseudo(webkitValidationBubbleMessageName);
    auto icon = HTMLDivElement::create(document);
    icon->setPseudo(webkitValidationBubbleIconName);
    message->appendChild(icon);
    auto textBlock = HTMLDivElement::create(document);
    textBlock->setPseudo(webkitValidationBubbleTextBlockName);
    m_messageHeading = HTMLDivElement::create(document);
    m_messageHeading->setPseudo(webkitValidationBubbleHeadingName);
    textBlock->appendChild(*m_messageHeading);
    m_messageBody = HTMLDivElement::create(document);
    m_messageBody->setPseudo(webkitValidationBubbleBodyName);
    textBlock->appendChild(*m_messageBody);
    message->appendChild(textBlock);
    m_bubble->appendChild(message);

    setMessageDOMAndStartTimer();

    // FIXME: Use transition to show the bubble.
}

void ValidationMessage::requestToHideMessage()
{
    if (ValidationMessageClient* client = validationMessageClient()) {
        client->hideValidationMessage(*m_element);
        return;
    }

    // We must not modify the DOM tree in this context by the same reason as setMessage().
    m_timer = makeUnique<Timer>(*this, &ValidationMessage::deleteBubbleTree);
    m_timer->startOneShot(0_s);
}

bool ValidationMessage::shadowTreeContains(const Node& node) const
{
    if (validationMessageClient() || !m_bubble)
        return false;
    return &m_bubble->treeScope() == &node.treeScope();
}

void ValidationMessage::deleteBubbleTree()
{
    ASSERT(!validationMessageClient());
    if (m_bubble) {
        m_messageHeading = nullptr;
        m_messageBody = nullptr;
        m_element->userAgentShadowRoot()->removeChild(*m_bubble);
        m_bubble = nullptr;
    }
    m_message = String();
}

bool ValidationMessage::isVisible() const
{
    if (ValidationMessageClient* client = validationMessageClient())
        return client->isValidationMessageVisible(*m_element);
    return !m_message.isEmpty();
}

} // namespace WebCore
