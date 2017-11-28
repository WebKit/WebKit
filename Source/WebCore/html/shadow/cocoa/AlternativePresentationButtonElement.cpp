/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AlternativePresentationButtonElement.h"

#if ENABLE(ALTERNATIVE_PRESENTATION_BUTTON_ELEMENT)

#include "Chrome.h"
#include "ChromeClient.h"
#include "DocumentFragment.h"
#include "Editor.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLSpanElement.h"
#include "HTMLStyleElement.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "Text.h"
#include "UserAgentStyleSheets.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>

namespace WebCore {

Ref<AlternativePresentationButtonElement> AlternativePresentationButtonElement::create(Document& document)
{
    return adoptRef(*new AlternativePresentationButtonElement { document });
}

AlternativePresentationButtonElement::AlternativePresentationButtonElement(Document& document)
    : HTMLDivElement { HTMLNames::divTag, document }
{
    setPseudo(AtomicString { "-webkit-alternative-presentation-button", AtomicString::ConstructFromLiteral });
    setAttributeWithoutSynchronization(HTMLNames::aria_labelAttr, AXAlternativePresentationButtonLabel());
    setAttributeWithoutSynchronization(HTMLNames::roleAttr, AtomicString { "button", AtomicString::ConstructFromLiteral });
}

auto AlternativePresentationButtonElement::insertedIntoAncestor(InsertionType type, ContainerNode& parentOfInsertedTree) -> InsertedIntoAncestorResult
{
    HTMLElement::insertedIntoAncestor(type, parentOfInsertedTree);
    if (type.connectedToDocument) {
        if (auto* frame = document().frame())
            frame->editor().didInsertAlternativePresentationButtonElement(*this);
        return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
    }
    return InsertedIntoAncestorResult::Done;
}

void AlternativePresentationButtonElement::removedFromAncestor(RemovalType type, ContainerNode& ancestor)
{
    HTMLDivElement::removedFromAncestor(type, ancestor);
    if (type.disconnectedFromDocument) {
        if (auto* frame = document().frame())
            frame->editor().didRemoveAlternativePresentationButtonElement(*this);
    }
}

void AlternativePresentationButtonElement::didFinishInsertingNode()
{
    auto fragment = document().createDocumentFragment();

#if __has_include(<WebKitAdditions/alternativePresentationButtonElementShadow.css>)
    static NeverDestroyed<String> styleSheet { alternativePresentationButtonElementShadowUserAgentStyleSheet, String::ConstructFromLiteral };
    auto style = HTMLStyleElement::create(document());
    style->setTextContent(styleSheet);
    fragment->appendChild(style);
#endif

    auto iconContainer = HTMLDivElement::create(document());
    iconContainer->setPseudo(AtomicString { "-webkit-alternative-presentation-button-icon-container" , AtomicString::ConstructFromLiteral });
    fragment->appendChild(iconContainer);

    auto icon = HTMLDivElement::create(document());
    icon->setPseudo(AtomicString { "-webkit-alternative-presentation-button-icon", AtomicString::ConstructFromLiteral });
    icon->setAttributeWithoutSynchronization(HTMLNames::classAttr, AtomicString { "center-img", AtomicString::ConstructFromLiteral });
    iconContainer->appendChild(icon);

    auto textContainer = HTMLDivElement::create(document());
    textContainer->setPseudo(AtomicString { "-webkit-alternative-presentation-button-text-container", AtomicString::ConstructFromLiteral });
    textContainer->setAttributeWithoutSynchronization(HTMLNames::classAttr, AtomicString { "center", AtomicString::ConstructFromLiteral });
    fragment->appendChild(textContainer);

    auto text = HTMLSpanElement::create(document());
    text->setPseudo(AtomicString { "-webkit-alternative-presentation-button-title", AtomicString::ConstructFromLiteral });
    text->appendChild(document().createTextNode(alternativePresentationButtonTitle()));
    textContainer->appendChild(text);

    auto subtext = HTMLDivElement::create(document());
    subtext->setPseudo(AtomicString { "-webkit-alternative-presentation-button-subtitle", AtomicString::ConstructFromLiteral });
    subtext->appendChild(document().createTextNode(alternativePresentationButtonSubtitle()));
    textContainer->appendChild(subtext);

    appendChild(fragment);
}

void AlternativePresentationButtonElement::defaultEventHandler(Event& event)
{
    if (!is<MouseEvent>(event)) {
        if (!event.defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    MouseEvent& mouseEvent = downcast<MouseEvent>(event);

    if (mouseEvent.type() == eventNames().clickEvent) {
        document().page()->chrome().client().handleAlternativePresentationButtonClick(*this);
        event.setDefaultHandled();
    }

    if (!event.defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

} // namespace WebCore

#endif // ENABLE(ALTERNATIVE_PRESENTATION_BUTTON_ELEMENT)
