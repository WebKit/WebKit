/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2022 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include "JSGlobalObject.h"
#include "RegExpPrototype.h"

namespace JSC {

ALWAYS_INLINE bool regExpExecWatchpointIsValid(VM& vm, JSObject* thisObject)
{
    JSGlobalObject* globalObject = thisObject->globalObject();
    RegExpPrototype* regExpPrototype = globalObject->regExpPrototype();

    ASSERT(globalObject->regExpPrimordialPropertiesWatchpointSet().state() != ClearWatchpoint);
    if (regExpPrototype != thisObject->getPrototypeDirect())
        return false;

    if (globalObject->regExpPrimordialPropertiesWatchpointSet().state() != IsWatched)
        return false;

    if (!thisObject->hasCustomProperties())
        return true;

    return thisObject->getDirectOffset(vm, vm.propertyNames->exec) == invalidOffset;
}

inline Structure* RegExpPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

} // namespace JSC
