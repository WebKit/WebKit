/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#if ENABLE(INPUT_TYPE_COLOR)

#include "ColorInputType.h"

#include "CSSPropertyNames.h"
#include "Chrome.h"
#include "Color.h"
#include "ElementChildIterator.h"
#include "Event.h"
#include "HTMLDataListElement.h"
#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "HTMLOptionElement.h"
#include "InputTypeNames.h"
#include "RenderView.h"
#include "ScopedEventQueue.h"
#include "ShadowRoot.h"
#include "UserGestureIndicator.h"

namespace WebCore {

using namespace HTMLNames;

// https://html.spec.whatwg.org/multipage/infrastructure.html#valid-simple-colour
static bool isValidSimpleColor(StringView string)
{
    if (string.length() != 7)
        return false;
    if (string[0] != '#')
        return false;
    for (unsigned i = 1; i < 7; ++i) {
        if (!isASCIIHexDigit(string[i]))
            return false;
    }
    return true;
}

// https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#rules-for-parsing-simple-colour-values
static std::optional<RGBA32> parseSimpleColorValue(StringView string)
{
    if (!isValidSimpleColor(string))
        return std::nullopt;
    return makeRGB(toASCIIHexValue(string[1], string[2]), toASCIIHexValue(string[3], string[4]), toASCIIHexValue(string[5], string[6]));
}

ColorInputType::~ColorInputType()
{
    endColorChooser();
}

bool ColorInputType::isMouseFocusable() const
{
    ASSERT(element());
    return element()->isTextFormControlFocusable();
}

bool ColorInputType::isKeyboardFocusable(KeyboardEvent*) const
{
    ASSERT(element());
#if PLATFORM(IOS_FAMILY)
    if (element()->isReadOnly())
        return false;

    return element()->isTextFormControlFocusable();
#endif
    return false;
}

bool ColorInputType::isColorControl() const
{
    return true;
}

bool ColorInputType::isPresentingAttachedView() const
{
    return !!m_chooser;
}

const AtomicString& ColorInputType::formControlType() const
{
    return InputTypeNames::color();
}

bool ColorInputType::supportsRequired() const
{
    return false;
}

String ColorInputType::fallbackValue() const
{
    return "#000000"_s;
}

String ColorInputType::sanitizeValue(const String& proposedValue) const
{
    if (!isValidSimpleColor(proposedValue))
        return fallbackValue();

    return proposedValue.convertToASCIILowercase();
}

Color ColorInputType::valueAsColor() const
{
    ASSERT(element());
    return parseSimpleColorValue(element()->value()).value();
}

void ColorInputType::createShadowSubtree()
{
    ASSERT(element());
    ASSERT(element()->shadowRoot());

    Document& document = element()->document();
    auto wrapperElement = HTMLDivElement::create(document);
    wrapperElement->setPseudo(AtomicString("-webkit-color-swatch-wrapper", AtomicString::ConstructFromLiteral));
    auto colorSwatch = HTMLDivElement::create(document);
    colorSwatch->setPseudo(AtomicString("-webkit-color-swatch", AtomicString::ConstructFromLiteral));
    wrapperElement->appendChild(colorSwatch);
    element()->userAgentShadowRoot()->appendChild(wrapperElement);

    updateColorSwatch();
}

void ColorInputType::setValue(const String& value, bool valueChanged, TextFieldEventBehavior eventBehavior)
{
    InputType::setValue(value, valueChanged, eventBehavior);

    if (!valueChanged)
        return;

    updateColorSwatch();
    if (m_chooser)
        m_chooser->setSelectedColor(valueAsColor());
}

void ColorInputType::handleDOMActivateEvent(Event& event)
{
    ASSERT(element());
    if (element()->isDisabledFormControl() || !element()->renderer())
        return;

    if (!UserGestureIndicator::processingUserGesture())
        return;

    if (Chrome* chrome = this->chrome()) {
        if (!m_chooser)
            m_chooser = chrome->createColorChooser(*this, valueAsColor());
        else
            m_chooser->reattachColorChooser(valueAsColor());
    }

    event.setDefaultHandled();
}

void ColorInputType::detach()
{
    endColorChooser();
}

void ColorInputType::elementDidBlur()
{
    endColorChooser();
}

bool ColorInputType::shouldRespectListAttribute()
{
    return true;
}

bool ColorInputType::typeMismatchFor(const String& value) const
{
    return !isValidSimpleColor(value);
}

bool ColorInputType::shouldResetOnDocumentActivation()
{
    return true;
}

void ColorInputType::didChooseColor(const Color& color)
{
    ASSERT(element());
    if (element()->isDisabledFormControl() || color == valueAsColor())
        return;
    EventQueueScope scope;
    element()->setValueFromRenderer(color.serialized());
    updateColorSwatch();
    element()->dispatchFormControlChangeEvent();
}

void ColorInputType::didEndChooser()
{
    m_chooser = nullptr;
    if (element()->renderer())
        element()->renderer()->repaint();
}

void ColorInputType::endColorChooser()
{
    if (m_chooser)
        m_chooser->endChooser();
}

void ColorInputType::updateColorSwatch()
{
    RefPtr<HTMLElement> colorSwatch = shadowColorSwatch();
    if (!colorSwatch)
        return;

    ASSERT(element());
    colorSwatch->setInlineStyleProperty(CSSPropertyBackgroundColor, element()->value(), false);
}

HTMLElement* ColorInputType::shadowColorSwatch() const
{
    ASSERT(element());
    RefPtr<ShadowRoot> shadow = element()->userAgentShadowRoot();
    if (!shadow)
        return nullptr;

    auto wrapper = childrenOfType<HTMLDivElement>(*shadow).first();
    if (!wrapper)
        return nullptr;

    return childrenOfType<HTMLDivElement>(*wrapper).first();
}

IntRect ColorInputType::elementRectRelativeToRootView() const
{
    ASSERT(element());
    if (!element()->renderer())
        return IntRect();
    return element()->document().view()->contentsToRootView(element()->renderer()->absoluteBoundingBoxRect());
}

Color ColorInputType::currentColor()
{
    return valueAsColor();
}

bool ColorInputType::shouldShowSuggestions() const
{
#if ENABLE(DATALIST_ELEMENT)
    ASSERT(element());
    return element()->hasAttributeWithoutSynchronization(listAttr);
#else
    return false;
#endif
}

Vector<Color> ColorInputType::suggestedColors() const
{
    Vector<Color> suggestions;
#if ENABLE(DATALIST_ELEMENT)
    ASSERT(element());
    if (auto dataList = element()->dataList()) {
        Ref<HTMLCollection> options = dataList->options();
        unsigned length = options->length();
        suggestions.reserveInitialCapacity(length);
        for (unsigned i = 0; i != length; ++i) {
            auto value = downcast<HTMLOptionElement>(*options->item(i)).value();
            if (isValidSimpleColor(value))
                suggestions.uncheckedAppend(Color(value));
        }
    }
#endif
    return suggestions;
}

void ColorInputType::selectColor(StringView string)
{
    if (auto color = parseSimpleColorValue(string))
        didChooseColor(color.value());
}

} // namespace WebCore

#endif // ENABLE(INPUT_TYPE_COLOR)
