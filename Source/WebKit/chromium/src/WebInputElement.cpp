/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebInputElement.h"

#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "RenderObject.h"
#include "RenderTextControlSingleLine.h"
#include "TextControlInnerElements.h"
#include "platform/WebString.h"
#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

bool WebInputElement::isTextField() const
{
    return constUnwrap<HTMLInputElement>()->isTextField();
}

bool WebInputElement::isText() const
{
    return constUnwrap<HTMLInputElement>()->isText();
}

bool WebInputElement::isPasswordField() const
{
    return constUnwrap<HTMLInputElement>()->isPasswordField();
}

bool WebInputElement::isImageButton() const
{
    return constUnwrap<HTMLInputElement>()->isImageButton();
}

bool WebInputElement::autoComplete() const
{
    return constUnwrap<HTMLInputElement>()->shouldAutocomplete();
}

int WebInputElement::maxLength() const
{
    return constUnwrap<HTMLInputElement>()->maxLength();
}

bool WebInputElement::isActivatedSubmit() const
{
    return constUnwrap<HTMLInputElement>()->isActivatedSubmit();
}

void WebInputElement::setActivatedSubmit(bool activated)
{
    unwrap<HTMLInputElement>()->setActivatedSubmit(activated);
}

int WebInputElement::size() const
{
    return constUnwrap<HTMLInputElement>()->size();
}

void WebInputElement::setValue(const WebString& value, bool sendChangeEvent)
{
    unwrap<HTMLInputElement>()->setValue(value, sendChangeEvent);
}

WebString WebInputElement::value() const
{
    return constUnwrap<HTMLInputElement>()->value();
}

void WebInputElement::setSuggestedValue(const WebString& value)
{
    unwrap<HTMLInputElement>()->setSuggestedValue(value);
}

WebString WebInputElement::suggestedValue() const
{
    return constUnwrap<HTMLInputElement>()->suggestedValue();
}

void WebInputElement::setPlaceholder(const WebString& value)
{
    unwrap<HTMLInputElement>()->setPlaceholder(value);
}

WebString WebInputElement::placeholder() const
{
    return constUnwrap<HTMLInputElement>()->placeholder();
}

bool WebInputElement::isAutofilled() const
{
    return constUnwrap<HTMLInputElement>()->isAutofilled();
}

void WebInputElement::setAutofilled(bool autofilled)
{
    unwrap<HTMLInputElement>()->setAutofilled(autofilled);
}

void WebInputElement::setSelectionRange(int start, int end)
{
    unwrap<HTMLInputElement>()->setSelectionRange(start, end);
}

int WebInputElement::selectionStart() const
{
    return constUnwrap<HTMLInputElement>()->selectionStart();
}

int WebInputElement::selectionEnd() const
{
    return constUnwrap<HTMLInputElement>()->selectionEnd();
}

bool WebInputElement::isValidValue(const WebString& value) const
{
    return constUnwrap<HTMLInputElement>()->isValidValue(value);
}

bool WebInputElement::isChecked() const
{
    return constUnwrap<HTMLInputElement>()->checked();
}

bool WebInputElement::isSpeechInputEnabled() const
{
#if ENABLE(INPUT_SPEECH)
    return constUnwrap<HTMLInputElement>()->isSpeechEnabled();
#else
    return false;
#endif
}

WebInputElement::SpeechInputState WebInputElement::getSpeechInputState() const
{
#if ENABLE(INPUT_SPEECH)
    RenderObject* renderer = constUnwrap<HTMLInputElement>()->renderer();
    if (!renderer)
        return Idle;

    RenderTextControlSingleLine* control = toRenderTextControlSingleLine(renderer);
    InputFieldSpeechButtonElement* speechButton = toInputFieldSpeechButtonElement(control->speechButtonElement());
    if (speechButton)
        return static_cast<WebInputElement::SpeechInputState>(speechButton->state());
#endif

    return Idle;
}

void WebInputElement::startSpeechInput()
{
#if ENABLE(INPUT_SPEECH)
    RenderObject* renderer = constUnwrap<HTMLInputElement>()->renderer();
    if (!renderer)
        return;

    RenderTextControlSingleLine* control = toRenderTextControlSingleLine(renderer);
    InputFieldSpeechButtonElement* speechButton = toInputFieldSpeechButtonElement(control->speechButtonElement());
    if (speechButton)
        speechButton->startSpeechInput();
#endif
}

void WebInputElement::stopSpeechInput()
{
#if ENABLE(INPUT_SPEECH)
    RenderObject* renderer = constUnwrap<HTMLInputElement>()->renderer();
    if (!renderer)
        return;

    RenderTextControlSingleLine* control = toRenderTextControlSingleLine(renderer);
    InputFieldSpeechButtonElement* speechButton = toInputFieldSpeechButtonElement(control->speechButtonElement());
    if (speechButton)
        speechButton->stopSpeechInput();
#endif
}

int WebInputElement::defaultMaxLength()
{
    return HTMLInputElement::maximumLength;
}

WebInputElement::WebInputElement(const PassRefPtr<HTMLInputElement>& elem)
    : WebFormControlElement(elem)
{
}

WebInputElement& WebInputElement::operator=(const PassRefPtr<HTMLInputElement>& elem)
{
    m_private = elem;
    return *this;
}

WebInputElement::operator PassRefPtr<HTMLInputElement>() const
{
    return static_cast<HTMLInputElement*>(m_private.get());
}

WebInputElement* toWebInputElement(WebElement* webElement)
{
    HTMLInputElement* inputElement = webElement->unwrap<Element>()->toInputElement();
    if (!inputElement)
        return 0;

    return static_cast<WebInputElement*>(webElement);
}

} // namespace WebKit
