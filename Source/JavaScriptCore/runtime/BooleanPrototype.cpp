/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "BooleanPrototype.h"

#include "IntegrityInlines.h"
#include "JSCInlines.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL booleanProtoFuncToString(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL booleanProtoFuncValueOf(JSGlobalObject*, CallFrame*);

}

#include "BooleanPrototype.lut.h"

namespace JSC {

const ClassInfo BooleanPrototype::s_info = { "Boolean", &BooleanObject::s_info, &booleanPrototypeTable, nullptr, CREATE_METHOD_TABLE(BooleanPrototype) };

/* Source for BooleanPrototype.lut.h
@begin booleanPrototypeTable
  toString  booleanProtoFuncToString    DontEnum|Function 0
  valueOf   booleanProtoFuncValueOf     DontEnum|Function 0
@end
*/

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(BooleanPrototype);

BooleanPrototype::BooleanPrototype(VM& vm, Structure* structure)
    : BooleanObject(vm, structure)
{
}

void BooleanPrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    setInternalValue(vm, jsBoolean(false));

    ASSERT(inherits(vm, info()));
}

// ------------------------------ Functions ---------------------------

EncodedJSValue JSC_HOST_CALL booleanProtoFuncToString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (thisValue == jsBoolean(false))
        return JSValue::encode(vm.smallStrings.falseString());

    if (thisValue == jsBoolean(true))
        return JSValue::encode(vm.smallStrings.trueString());

    auto* thisObject = jsDynamicCast<BooleanObject*>(vm, thisValue);
    if (UNLIKELY(!thisObject))
        return throwVMTypeError(globalObject, scope);

    Integrity::auditStructureID(vm, thisObject->structureID());
    if (thisObject->internalValue() == jsBoolean(false))
        return JSValue::encode(vm.smallStrings.falseString());

    ASSERT(thisObject->internalValue() == jsBoolean(true));
    return JSValue::encode(vm.smallStrings.trueString());
}

EncodedJSValue JSC_HOST_CALL booleanProtoFuncValueOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (thisValue.isBoolean())
        return JSValue::encode(thisValue);

    auto* thisObject = jsDynamicCast<BooleanObject*>(vm, thisValue);
    if (UNLIKELY(!thisObject))
        return throwVMTypeError(globalObject, scope);

    Integrity::auditStructureID(vm, thisObject->structureID());
    return JSValue::encode(thisObject->internalValue());
}

} // namespace JSC
