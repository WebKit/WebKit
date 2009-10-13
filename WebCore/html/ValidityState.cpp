/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2009 Michelangelo De Simone <micdesim@gmail.com>
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
#include "KURL.h"
#include "RegularExpression.h"
#include <wtf/StdLibExtras.h>

#define EMAIL_LOCALPART "[a-z0-9!#$%&'*+/=?^_`{|}~.-]+"
#define EMAIL_DOMAINPART "[a-z0-9-]+(\\.[a-z0-9-]+)+"
#define EMAIL_PATTERN EMAIL_LOCALPART "@" EMAIL_DOMAINPART

namespace WebCore {

using namespace HTMLNames;

ValidityState::ValidityState(HTMLFormControlElement* parent)
    : m_control(parent)
{
    ASSERT(parent);
}

bool ValidityState::typeMismatch()
{
    if (!control()->hasTagName(inputTag))
        return false;

    HTMLInputElement* input = static_cast<HTMLInputElement*>(control());
    String value = input->value();

    if (value.isEmpty())
        return false;

    switch (input->inputType()) {
    case HTMLInputElement::COLOR:
        return !isValidColorString(value);
    case HTMLInputElement::NUMBER:
        return !HTMLInputElement::formStringToDouble(value, 0);
    case HTMLInputElement::URL:
        return !KURL(KURL(), value).isValid();
    case HTMLInputElement::EMAIL:
    {
        if (!input->multiple())
            return !isValidEmailAddress(value);
            
        Vector<String> email_list;
        value.split(',', email_list);
        for (unsigned i = 0; i < email_list.size(); ++i)
            if (!isValidEmailAddress(email_list[i]))
                return true;

        return false;
    }
    default:
        return false;
    }
}

bool ValidityState::rangeUnderflow()
{
    if (!control()->hasTagName(inputTag))
        return false;
    return static_cast<HTMLInputElement*>(control())->rangeUnderflow();
}

bool ValidityState::rangeOverflow()
{
    if (!control()->hasTagName(inputTag))
        return false;
    return static_cast<HTMLInputElement*>(control())->rangeOverflow();
}

bool ValidityState::valid()
{
    bool someError = typeMismatch() || stepMismatch() || rangeUnderflow() || rangeOverflow() ||
                       tooLong() || patternMismatch() || valueMissing() || customError();

    return !someError;
}

bool ValidityState::isValidColorString(const String& value)
{
    if (value.isEmpty())
        return false;
    if (value[0] == '#') {
        // We don't accept #rgb and #aarrggbb formats.
        if (value.length() != 7)
            return false;
    }
    Color color(value);  // This accepts named colors such as "white".
    return color.isValid() && !color.hasAlpha();
}

bool ValidityState::isValidEmailAddress(const String& email)
{
    if (email.isEmpty())
        return false;

    DEFINE_STATIC_LOCAL(AtomicString, emailPattern, (EMAIL_PATTERN));
    DEFINE_STATIC_LOCAL(RegularExpression, regExp, (emailPattern, TextCaseInsensitive));

    int matchLength = 0;
    int emailLength = email.length();
    int matchOffset = regExp.match(email, 0, &matchLength);

    return matchOffset == 0 && matchLength == emailLength;
}

} // namespace
