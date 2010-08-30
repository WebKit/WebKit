/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2009 Michelangelo De Simone <micdesim@gmail.com>
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "ValidityState.h"

#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTreeBuilder.h"
#include "LocalizedStrings.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

String ValidityState::validationMessage() const
{
    if (!m_control->willValidate())
        return String();

    if (customError())
        return m_customErrorMessage;
    if (valueMissing())
        return validationMessageValueMissingText();
    if (typeMismatch())
        return validationMessageTypeMismatchText();
    if (patternMismatch())
        return validationMessagePatternMismatchText();
    if (tooLong())
        return validationMessageTooLongText();
    if (rangeUnderflow())
        return validationMessageRangeUnderflowText();
    if (rangeOverflow())
        return validationMessageRangeOverflowText();
    if (stepMismatch())
        return validationMessageStepMismatchText();

    return String();
}

void ValidityState::setCustomErrorMessage(const String& message)
{
    m_customErrorMessage = message;
    m_control->setNeedsValidityCheck();
}

bool ValidityState::valueMissing() const
{
    if (m_control->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_control);
        return input->valueMissing(input->value());
    }
    if (m_control->hasTagName(textareaTag)) {
        HTMLTextAreaElement* textArea = static_cast<HTMLTextAreaElement*>(m_control);
        return textArea->valueMissing(textArea->value());
    }
    return false;
}

bool ValidityState::typeMismatch() const
{
    if (!m_control->hasTagName(inputTag))
        return false;

    HTMLInputElement* input = static_cast<HTMLInputElement*>(m_control);
    String value = input->value();
    if (value.isEmpty())
        return false;
    return input->typeMismatch(value);
}

bool ValidityState::patternMismatch() const
{
    if (!m_control->hasTagName(inputTag))
        return false;
    HTMLInputElement* input = static_cast<HTMLInputElement*>(m_control);
    return input->patternMismatch(input->value());
}

bool ValidityState::tooLong() const
{
    if (m_control->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_control);
        return input->tooLong(input->value(), HTMLTextFormControlElement::CheckDirtyFlag);
    }
    if (m_control->hasTagName(textareaTag)) {
        HTMLTextAreaElement* textArea = static_cast<HTMLTextAreaElement*>(m_control);
        return textArea->tooLong(textArea->value(), HTMLTextFormControlElement::CheckDirtyFlag);
    }
    return false;
}

bool ValidityState::rangeUnderflow() const
{
    if (!m_control->hasTagName(inputTag))
        return false;
    HTMLInputElement* input = static_cast<HTMLInputElement*>(m_control);
    return input->rangeUnderflow(input->value());
}

bool ValidityState::rangeOverflow() const
{
    if (!m_control->hasTagName(inputTag))
        return false;
    HTMLInputElement* input = static_cast<HTMLInputElement*>(m_control);
    return input->rangeOverflow(input->value());
}

bool ValidityState::stepMismatch() const
{
    if (!m_control->hasTagName(inputTag))
        return false;
    HTMLInputElement* input = static_cast<HTMLInputElement*>(m_control);
    return input->stepMismatch(input->value());
}

bool ValidityState::valid() const
{
    bool someError = typeMismatch() || stepMismatch() || rangeUnderflow() || rangeOverflow()
        || tooLong() || patternMismatch() || valueMissing() || customError();
    return !someError;
}

} // namespace
