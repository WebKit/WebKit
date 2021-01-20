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

const AtomString& button()
{
    static MainThreadNeverDestroyed<const AtomString> name("button", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& checkbox()
{
    static MainThreadNeverDestroyed<const AtomString> name("checkbox", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& color()
{
    static MainThreadNeverDestroyed<const AtomString> name("color", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& date()
{
    static MainThreadNeverDestroyed<const AtomString> name("date", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& datetimelocal()
{
    static MainThreadNeverDestroyed<const AtomString> name("datetime-local", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& email()
{
    static MainThreadNeverDestroyed<const AtomString> name("email", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& file()
{
    static MainThreadNeverDestroyed<const AtomString> name("file", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& hidden()
{
    static MainThreadNeverDestroyed<const AtomString> name("hidden", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& image()
{
    static MainThreadNeverDestroyed<const AtomString> name("image", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& month()
{
    static MainThreadNeverDestroyed<const AtomString> name("month", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& number()
{
    static MainThreadNeverDestroyed<const AtomString> name("number", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& password()
{
    static MainThreadNeverDestroyed<const AtomString> name("password", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& radio()
{
    static MainThreadNeverDestroyed<const AtomString> name("radio", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& range()
{
    static MainThreadNeverDestroyed<const AtomString> name("range", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& reset()
{
    static MainThreadNeverDestroyed<const AtomString> name("reset", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& search()
{
    static MainThreadNeverDestroyed<const AtomString> name("search", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& submit()
{
    static MainThreadNeverDestroyed<const AtomString> name("submit", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& telephone()
{
    static MainThreadNeverDestroyed<const AtomString> name("tel", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& text()
{
    static MainThreadNeverDestroyed<const AtomString> name("text", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& time()
{
    static MainThreadNeverDestroyed<const AtomString> name("time", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& url()
{
    static MainThreadNeverDestroyed<const AtomString> name("url", AtomString::ConstructFromLiteral);
    return name;
}

const AtomString& week()
{
    static MainThreadNeverDestroyed<const AtomString> name("week", AtomString::ConstructFromLiteral);
    return name;
}

} // namespace WebCore::InputTypeNames

} // namespace WebCore
