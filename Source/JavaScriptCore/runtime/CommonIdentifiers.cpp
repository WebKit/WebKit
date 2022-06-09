/*
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "CommonIdentifiers.h"

#include "BuiltinNames.h"
#include "IdentifierInlines.h"

namespace JSC {

#define INITIALIZE_PROPERTY_NAME(name) , name(Identifier::fromString(vm, #name))
#define INITIALIZE_KEYWORD(name) , name##Keyword(Identifier::fromString(vm, #name))
#define INITIALIZE_PRIVATE_NAME(name) , name##PrivateName(m_builtinNames->name##PrivateName())
#define INITIALIZE_SYMBOL(name) , name##Symbol(m_builtinNames->name##Symbol())
#define INITIALIZE_PRIVATE_FIELD_NAME(name) , name##PrivateField(Identifier::fromString(vm, "#" #name))

CommonIdentifiers::CommonIdentifiers(VM& vm)
    : nullIdentifier()
    , emptyIdentifier(Identifier::EmptyIdentifier)
    , underscoreProto(Identifier::fromString(vm, "__proto__"))
    , useStrictIdentifier(Identifier::fromString(vm, "use strict"))
    , timesIdentifier(Identifier::fromString(vm, "*"))
    , m_builtinNames(makeUnique<BuiltinNames>(vm, this))
    JSC_PARSER_PRIVATE_NAMES(INITIALIZE_PRIVATE_NAME)
    JSC_COMMON_IDENTIFIERS_EACH_KEYWORD(INITIALIZE_KEYWORD)
    JSC_COMMON_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_PROPERTY_NAME)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(INITIALIZE_SYMBOL)
    JSC_COMMON_IDENTIFIERS_EACH_PRIVATE_FIELD(INITIALIZE_PRIVATE_FIELD_NAME)
{
}

CommonIdentifiers::~CommonIdentifiers()
{
}

void CommonIdentifiers::appendExternalName(const Identifier& publicName, const Identifier& privateName)
{
    m_builtinNames->appendExternalName(publicName, privateName);
}

} // namespace JSC
