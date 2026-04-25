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

#include "CommonAtomStrings.h"
#include "HTMLNames.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

namespace InputTypeNames {

// The type names must be lowercased because they will be the return values of
// input.type and input.type must be lowercase according to DOM Level 2.

const AtomString& button()
{
    return HTMLNames::buttonTag->localName();
}

const AtomString& checkbox()
{
    static MainThreadNeverDestroyed<const AtomString> name("checkbox"_s);
    return name;
}

const AtomString& color()
{
    static MainThreadNeverDestroyed<const AtomString> name("color"_s);
    return name;
}

const AtomString& date()
{
    static MainThreadNeverDestroyed<const AtomString> name("date"_s);
    return name;
}

const AtomString& datetimelocal()
{
    static MainThreadNeverDestroyed<const AtomString> name("datetime-local"_s);
    return name;
}

const AtomString& email()
{
    return emailAtom();
}

const AtomString& file()
{
    static MainThreadNeverDestroyed<const AtomString> name("file"_s);
    return name;
}

const AtomString& hidden()
{
    static MainThreadNeverDestroyed<const AtomString> name("hidden"_s);
    return name;
}

const AtomString& image()
{
    static MainThreadNeverDestroyed<const AtomString> name("image"_s);
    return name;
}

const AtomString& month()
{
    static MainThreadNeverDestroyed<const AtomString> name("month"_s);
    return name;
}

const AtomString& number()
{
    static MainThreadNeverDestroyed<const AtomString> name("number"_s);
    return name;
}

const AtomString& password()
{
    static MainThreadNeverDestroyed<const AtomString> name("password"_s);
    return name;
}

const AtomString& radio()
{
    static MainThreadNeverDestroyed<const AtomString> name("radio"_s);
    return name;
}

const AtomString& range()
{
    static MainThreadNeverDestroyed<const AtomString> name("range"_s);
    return name;
}

const AtomString& reset()
{
    return resetAtom();
}

const AtomString& search()
{
    return searchAtom();
}

const AtomString& submit()
{
    return submitAtom();
}

const AtomString& telephone()
{
    return telAtom();
}

const AtomString& text()
{
    return textAtom();
}

const AtomString& time()
{
    static MainThreadNeverDestroyed<const AtomString> name("time"_s);
    return name;
}

const AtomString& url()
{
    return urlAtom();
}

const AtomString& week()
{
    static MainThreadNeverDestroyed<const AtomString> name("week"_s);
    return name;
}

} // namespace WebCore::InputTypeNames

} // namespace WebCore
