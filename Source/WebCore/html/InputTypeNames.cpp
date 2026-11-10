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

const RefPtr<AtomStringImpl>& button()
{
    return HTMLNames::buttonTag->localName();
}

const RefPtr<AtomStringImpl>& checkbox()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("checkbox"_s);
    return name;
}

const RefPtr<AtomStringImpl>& color()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("color"_s);
    return name;
}

const RefPtr<AtomStringImpl>& date()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("date"_s);
    return name;
}

const RefPtr<AtomStringImpl>& datetimelocal()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("datetime-local"_s);
    return name;
}

const RefPtr<AtomStringImpl>& email()
{
    return emailAtom();
}

const RefPtr<AtomStringImpl>& file()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("file"_s);
    return name;
}

const RefPtr<AtomStringImpl>& hidden()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("hidden"_s);
    return name;
}

const RefPtr<AtomStringImpl>& image()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("image"_s);
    return name;
}

const RefPtr<AtomStringImpl>& month()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("month"_s);
    return name;
}

const RefPtr<AtomStringImpl>& number()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("number"_s);
    return name;
}

const RefPtr<AtomStringImpl>& password()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("password"_s);
    return name;
}

const RefPtr<AtomStringImpl>& radio()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("radio"_s);
    return name;
}

const RefPtr<AtomStringImpl>& range()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("range"_s);
    return name;
}

const RefPtr<AtomStringImpl>& reset()
{
    return resetAtom();
}

const RefPtr<AtomStringImpl>& search()
{
    return searchAtom();
}

const RefPtr<AtomStringImpl>& submit()
{
    return submitAtom();
}

const RefPtr<AtomStringImpl>& telephone()
{
    return telAtom();
}

const RefPtr<AtomStringImpl>& text()
{
    return textAtom();
}

const RefPtr<AtomStringImpl>& time()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("time"_s);
    return name;
}

const RefPtr<AtomStringImpl>& url()
{
    return urlAtom();
}

const RefPtr<AtomStringImpl>& week()
{
    static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> name("week"_s);
    return name;
}

} // namespace WebCore::InputTypeNames

} // namespace WebCore
