/*
 *  Copyright (C) 2003, 2007, 2009, 2012 Apple Inc. All rights reserved.
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
#include "JSCBuiltins.h"
#include "PrivateName.h"

namespace JSC {

#define INITIALIZE_PROPERTY_NAME(name) , name(vm, #name)
#define INITIALIZE_KEYWORD(name) , name##Keyword(vm, #name)
#define INITIALIZE_PRIVATE_NAME(name) , name##PrivateName(m_builtinNames->name##PrivateName())

CommonIdentifiers::CommonIdentifiers(VM* vm)
    : nullIdentifier()
    , emptyIdentifier(Identifier::EmptyIdentifier)
    , underscoreProto(vm, "__proto__")
    , thisIdentifier(vm, "this")
    , useStrictIdentifier(vm, "use strict")
    , m_builtinNames(new BuiltinNames(vm, this))
    JSC_COMMON_IDENTIFIERS_EACH_KEYWORD(INITIALIZE_KEYWORD)
    JSC_COMMON_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_PROPERTY_NAME)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_PRIVATE_NAME)
{
}

CommonIdentifiers::~CommonIdentifiers()
{
}

const Identifier* CommonIdentifiers::getPrivateName(const Identifier& ident) const
{
    return m_builtinNames->getPrivateName(ident);
}
    
Identifier CommonIdentifiers::getPublicName(const Identifier& ident) const
{
    return m_builtinNames->getPublicName(ident);
}


} // namespace JSC
