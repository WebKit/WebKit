/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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
#include "InputTypeNames.h"

#include <wtf/NeverDestroyed.h>

namespace WebCore {

namespace InputTypeNames {

// The type names must be lowercased because they will be the return values of
// input.type and input.type must be lowercase according to DOM Level 2.

const AtomicString& button()
{
    static NeverDestroyed<AtomicString> name("button", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& checkbox()
{
    static NeverDestroyed<AtomicString> name("checkbox", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& color()
{
    static NeverDestroyed<AtomicString> name("color", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& date()
{
    static NeverDestroyed<AtomicString> name("date", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& datetime()
{
    static NeverDestroyed<AtomicString> name("datetime", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& datetimelocal()
{
    static NeverDestroyed<AtomicString> name("datetime-local", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& email()
{
    static NeverDestroyed<AtomicString> name("email", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& file()
{
    static NeverDestroyed<AtomicString> name("file", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& hidden()
{
    static NeverDestroyed<AtomicString> name("hidden", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& image()
{
    static NeverDestroyed<AtomicString> name("image", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& month()
{
    static NeverDestroyed<AtomicString> name("month", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& number()
{
    static NeverDestroyed<AtomicString> name("number", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& password()
{
    static NeverDestroyed<AtomicString> name("password", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& radio()
{
    static NeverDestroyed<AtomicString> name("radio", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& range()
{
    static NeverDestroyed<AtomicString> name("range", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& reset()
{
    static NeverDestroyed<AtomicString> name("reset", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& search()
{
    static NeverDestroyed<AtomicString> name("search", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& submit()
{
    static NeverDestroyed<AtomicString> name("submit", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& telephone()
{
    static NeverDestroyed<AtomicString> name("tel", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& text()
{
    static NeverDestroyed<AtomicString> name("text", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& time()
{
    static NeverDestroyed<AtomicString> name("time", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& url()
{
    static NeverDestroyed<AtomicString> name("url", AtomicString::ConstructFromLiteral);
    return name;
}

const AtomicString& week()
{
    static NeverDestroyed<AtomicString> name("week", AtomicString::ConstructFromLiteral);
    return name;
}

} // namespace WebCore::InputTypeNames

} // namespace WebCore
