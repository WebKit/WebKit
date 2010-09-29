/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "InputType.h"

#include "ButtonInputType.h"
#include "CheckboxInputType.h"
#include "ColorInputType.h"
#include "DateInputType.h"
#include "DateTimeInputType.h"
#include "DateTimeLocalInputType.h"
#include "EmailInputType.h"
#include "FileInputType.h"
#include "HTMLInputElement.h"
#include "HiddenInputType.h"
#include "ImageInputType.h"
#include "IsIndexInputType.h"
#include "MonthInputType.h"
#include "NumberInputType.h"
#include "PasswordInputType.h"
#include "RadioInputType.h"
#include "RangeInputType.h"
#include "RegularExpression.h"
#include "ResetInputType.h"
#include "SearchInputType.h"
#include "SubmitInputType.h"
#include "TelephoneInputType.h"
#include "TextInputType.h"
#include "TimeInputType.h"
#include "URLInputType.h"
#include "WeekInputType.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

typedef HashMap<String, PassOwnPtr<InputType> (*)(HTMLInputElement*), CaseFoldingHash> InputTypeFactoryMap;
static PassOwnPtr<InputTypeFactoryMap> createInputTypeFactoryMap()
{
    OwnPtr<InputTypeFactoryMap> map = adoptPtr(new InputTypeFactoryMap);
    map->add(InputTypeNames::button(), ButtonInputType::create);
    map->add(InputTypeNames::checkbox(), CheckboxInputType::create);
    map->add(InputTypeNames::color(), ColorInputType::create);
    map->add(InputTypeNames::date(), DateInputType::create);
    map->add(InputTypeNames::datetime(), DateTimeInputType::create);
    map->add(InputTypeNames::datetimelocal(), DateTimeLocalInputType::create);
    map->add(InputTypeNames::email(), EmailInputType::create);
    map->add(InputTypeNames::file(), FileInputType::create);
    map->add(InputTypeNames::hidden(), HiddenInputType::create);
    map->add(InputTypeNames::image(), ImageInputType::create);
    map->add(InputTypeNames::isindex(), IsIndexInputType::create);
    map->add(InputTypeNames::month(), MonthInputType::create);
    map->add(InputTypeNames::number(), NumberInputType::create);
    map->add(InputTypeNames::password(), PasswordInputType::create);
    map->add(InputTypeNames::radio(), RadioInputType::create);
    map->add(InputTypeNames::range(), RangeInputType::create);
    map->add(InputTypeNames::reset(), ResetInputType::create);
    map->add(InputTypeNames::search(), SearchInputType::create);
    map->add(InputTypeNames::submit(), SubmitInputType::create);
    map->add(InputTypeNames::telephone(), TelephoneInputType::create);
    map->add(InputTypeNames::time(), TimeInputType::create);
    map->add(InputTypeNames::url(), URLInputType::create);
    map->add(InputTypeNames::week(), WeekInputType::create);
    // No need to register "text" because it is the default type.
    return map.release();
}

PassOwnPtr<InputType> InputType::create(HTMLInputElement* element, const AtomicString& typeName)
{
    static const InputTypeFactoryMap* factoryMap = createInputTypeFactoryMap().leakPtr();
    PassOwnPtr<InputType> (*factory)(HTMLInputElement*) = typeName.isEmpty() ? 0 : factoryMap->get(typeName);
    if (!factory)
        factory = TextInputType::create;
    return factory(element);
}

PassOwnPtr<InputType> InputType::createText(HTMLInputElement* element)
{
    return TextInputType::create(element);
}

InputType::~InputType()
{
}

bool InputType::isTextField() const
{
    return false;
}

bool InputType::isTextType() const
{
    return false;
}

bool InputType::patternMismatch(const String&) const
{
    return false;
}

namespace InputTypeNames {

// The type names must be lowercased because they will be the return values of
// input.type and input.type must be lowercase according to DOM Level 2.

const AtomicString& button()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("button"));
    return name;
}

const AtomicString& checkbox()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("checkbox"));
    return name;
}

const AtomicString& color()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("color"));
    return name;
}

const AtomicString& date()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("date"));
    return name;
}

const AtomicString& datetime()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("datetime"));
    return name;
}

const AtomicString& datetimelocal()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("datetime-local"));
    return name;
}

const AtomicString& email()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("email"));
    return name;
}

const AtomicString& file()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("file"));
    return name;
}

const AtomicString& hidden()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("hidden"));
    return name;
}

const AtomicString& image()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("image"));
    return name;
}

const AtomicString& isindex()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("khtml_isindex"));
    return name;
}

const AtomicString& month()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("month"));
    return name;
}

const AtomicString& number()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("number"));
    return name;
}

const AtomicString& password()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("password"));
    return name;
}

const AtomicString& radio()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("radio"));
    return name;
}

const AtomicString& range()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("range"));
    return name;
}

const AtomicString& reset()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("reset"));
    return name;
}

const AtomicString& search()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("search"));
    return name;
}

const AtomicString& submit()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("submit"));
    return name;
}

const AtomicString& telephone()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("tel"));
    return name;
}

const AtomicString& text()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("text"));
    return name;
}

const AtomicString& time()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("time"));
    return name;
}

const AtomicString& url()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("url"));
    return name;
}

const AtomicString& week()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("week"));
    return name;
}

} // namespace WebCore::InpuTypeNames

} // namespace WebCore

