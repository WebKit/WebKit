/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DateTimeFieldElement.h"

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "CSSPropertyNames.h"
#include "DateComponents.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "PlatformLocale.h"
#include "RenderStyle.h"
#include "RenderTheme.h"
#include "StyleResolver.h"
#include "Text.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeFieldElement);

DateTimeFieldElement::FieldOwner::~FieldOwner() = default;

DateTimeFieldElement::DateTimeFieldElement(Document& document, FieldOwner& fieldOwner)
    : HTMLDivElement(divTag, document)
    , m_fieldOwner(fieldOwner)
{
    setHasCustomStyleResolveCallbacks();
}

void DateTimeFieldElement::initialize(const AtomString& pseudo)
{
    setPseudo(pseudo);
}

std::optional<Style::ElementStyle> DateTimeFieldElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle* shadowHostStyle)
{
    auto elementStyle = resolveStyle(resolutionContext);
    if (!elementStyle.renderStyle)
        return std::nullopt;

    auto& style = *elementStyle.renderStyle;
    adjustMinInlineSize(style);

    if (!hasValue() && shadowHostStyle) {
        auto textColor = shadowHostStyle->visitedDependentColorWithColorFilter(CSSPropertyColor);
        auto backgroundColor = shadowHostStyle->visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
        style.setColor(RenderTheme::singleton().datePlaceholderTextColor(textColor, backgroundColor));
    }

    return elementStyle;
}

void DateTimeFieldElement::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().blurEvent)
        handleBlurEvent(event);

    if (is<KeyboardEvent>(event)) {
        auto& keyboardEvent = downcast<KeyboardEvent>(event);
        if (!isFieldOwnerDisabled() && !isFieldOwnerReadOnly()) {
            handleKeyboardEvent(keyboardEvent);
            if (keyboardEvent.defaultHandled())
                return;
        }

        defaultKeyboardEventHandler(keyboardEvent);
        if (keyboardEvent.defaultHandled())
            return;
    }

    HTMLDivElement::defaultEventHandler(event);
}

void DateTimeFieldElement::defaultKeyboardEventHandler(KeyboardEvent& keyboardEvent)
{
    if (isFieldOwnerDisabled())
        return;

    if (keyboardEvent.type() != eventNames().keydownEvent)
        return;

    auto key = keyboardEvent.keyIdentifier();
    auto code = keyboardEvent.code();

    if (key == "Left"_s && m_fieldOwner && m_fieldOwner->focusOnPreviousField(*this)) {
        keyboardEvent.setDefaultHandled();
        return;
    }

    if ((key == "Right"_s || code == "Comma"_s || code == "Minus"_s || code == "Period"_s || code == "Slash"_s || code == "Semicolon"_s)
        && m_fieldOwner && m_fieldOwner->focusOnNextField(*this)) {
        keyboardEvent.setDefaultHandled();
        return;
    }

    if (isFieldOwnerReadOnly())
        return;

    if (key == "Up"_s) {
        stepUp();
        keyboardEvent.setDefaultHandled();
        return;
    }

    if (key == "Down"_s) {
        stepDown();
        keyboardEvent.setDefaultHandled();
        return;
    }

    // Clear value when pressing backspace or delete.
    if (key == "U+0008"_s || key == "U+007F"_s) {
        setEmptyValue(DispatchInputAndChangeEvents);
        keyboardEvent.setDefaultHandled();
        return;
    }
}

bool DateTimeFieldElement::isFieldOwnerDisabled() const
{
    return m_fieldOwner && m_fieldOwner->isFieldOwnerDisabled();
}

bool DateTimeFieldElement::isFieldOwnerReadOnly() const
{
    return m_fieldOwner && m_fieldOwner->isFieldOwnerReadOnly();
}

bool DateTimeFieldElement::isFocusable() const
{
    if (isFieldOwnerDisabled())
        return false;
    return HTMLElement::isFocusable();
}

void DateTimeFieldElement::handleBlurEvent(Event& event)
{
    if (m_fieldOwner)
        m_fieldOwner->didBlurFromField(event);
}

Locale& DateTimeFieldElement::localeForOwner() const
{
    return document().getCachedLocale(localeIdentifier());
}

AtomString DateTimeFieldElement::localeIdentifier() const
{
    return m_fieldOwner ? m_fieldOwner->localeIdentifier() : nullAtom();
}

String DateTimeFieldElement::visibleValue() const
{
    return hasValue() ? value() : placeholderValue();
}

void DateTimeFieldElement::updateVisibleValue(EventBehavior eventBehavior)
{
    if (!firstChild())
        appendChild(Text::create(document(), String { emptyString() }));

    Ref textNode = downcast<Text>(*firstChild());
    String newVisibleValue = visibleValue();
    if (textNode->wholeText() != newVisibleValue)
        textNode->replaceWholeText(newVisibleValue);

    if (eventBehavior == DispatchInputAndChangeEvents && m_fieldOwner)
        m_fieldOwner->fieldValueChanged();
}

bool DateTimeFieldElement::supportsFocus() const
{
    return true;
}

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
