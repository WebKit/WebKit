/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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

#include "JSCInlines.h"
#include "JSObject.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "PropertySlot.h"
#include "StructureInlines.h"
#include "StructureRareDataInlines.h"

namespace JSC {

inline Structure* structureForPrimitiveValue(JSGlobalObject* globalObject, JSValue value)
{
    if (value.isCell()) {
        if (value.isString())
            return globalObject->stringObjectStructure();
        if (value.isBigInt())
            return globalObject->bigIntObjectStructure();
        ASSERT(value.isSymbol());
        return globalObject->symbolObjectStructure();
    }

    if (value.isNumber())
        return globalObject->numberObjectStructure();
    if (value.isBoolean())
        return globalObject->booleanObjectStructure();

    ASSERT(value.isUndefinedOrNull());
    return nullptr;
}

ALWAYS_INLINE JSString* objectToString(ExecState* exec, JSValue thisValue)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = nullptr;
    if (thisValue.isObject()) {
        thisObject = jsCast<JSObject*>(thisValue);
        if (auto* result = thisObject->structure(vm)->objectToStringValue())
            return result;
    } else {
        if (thisValue.isUndefinedOrNull())
            return thisValue.isUndefined() ? vm.smallStrings.undefinedObjectString() : vm.smallStrings.nullObjectString();

        auto* structure = structureForPrimitiveValue(exec->lexicalGlobalObject(), thisValue);
        ASSERT(structure);
        if (auto* result = structure->objectToStringValue())
            return result;
        thisObject = thisValue.toObject(exec);
        EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
        if (!thisObject)
            return nullptr;
    }

    RELEASE_AND_RETURN(scope, thisObject->getPropertySlot(exec, vm.propertyNames->toStringTagSymbol, [&] (bool found, PropertySlot& toStringTagSlot) -> JSString* {
        auto scope = DECLARE_THROW_SCOPE(vm);
        if (found) {
            JSValue stringTag = toStringTagSlot.getValue(exec, vm.propertyNames->toStringTagSymbol);
            RETURN_IF_EXCEPTION(scope, { });
            if (stringTag.isString()) {
                JSRopeString::RopeBuilder<RecordOverflow> ropeBuilder(vm);
                ropeBuilder.append(vm.smallStrings.objectStringStart());
                ropeBuilder.append(asString(stringTag));
                ropeBuilder.append(vm.smallStrings.singleCharacterString(']'));
                if (ropeBuilder.hasOverflowed()) {
                    throwOutOfMemoryError(exec, scope);
                    return nullptr;
                }

                JSString* result = ropeBuilder.release();
                thisObject->structure(vm)->setObjectToStringValue(exec, vm, result, toStringTagSlot);
                return result;
            }
        }

        String tag = thisObject->methodTable(vm)->toStringName(thisObject, exec);
        RETURN_IF_EXCEPTION(scope, { });
        String newString = tryMakeString("[object ", WTFMove(tag), "]");
        if (!newString) {
            throwOutOfMemoryError(exec, scope);
            return nullptr;
        }

        auto result = jsNontrivialString(&vm, WTFMove(newString));
        thisObject->structure(vm)->setObjectToStringValue(exec, vm, result, toStringTagSlot);
        return result;
    }));
}

} // namespace JSC
