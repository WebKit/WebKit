/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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
#include "BooleanConstructor.h"

#include "BooleanPrototype.h"
#include "JSGlobalObject.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(BooleanConstructor);

BooleanConstructor::BooleanConstructor(ExecState* exec, NonNullPassRefPtr<Structure> structure, BooleanPrototype* booleanPrototype)
    : InternalFunction(&exec->globalData(), structure, Identifier(exec, booleanPrototype->classInfo()->className))
{
    putDirectWithoutTransition(exec->propertyNames().prototype, booleanPrototype, DontEnum | DontDelete | ReadOnly);

    // no. of arguments for constructor
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontDelete | DontEnum);
}

// ECMA 15.6.2
JSObject* constructBoolean(ExecState* exec, const ArgList& args)
{
    BooleanObject* obj = new (exec) BooleanObject(exec->lexicalGlobalObject()->booleanObjectStructure());
    obj->setInternalValue(jsBoolean(args.at(0).toBoolean(exec)));
    return obj;
}

static JSObject* constructWithBooleanConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    return constructBoolean(exec, args);
}

ConstructType BooleanConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithBooleanConstructor;
    return ConstructTypeHost;
}

// ECMA 15.6.1
static JSValue JSC_HOST_CALL callBooleanConstructor(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    return jsBoolean(args.at(0).toBoolean(exec));
}

CallType BooleanConstructor::getCallData(CallData& callData)
{
    callData.native.function = callBooleanConstructor;
    return CallTypeHost;
}

JSObject* constructBooleanFromImmediateBoolean(ExecState* exec, JSValue immediateBooleanValue)
{
    BooleanObject* obj = new (exec) BooleanObject(exec->lexicalGlobalObject()->booleanObjectStructure());
    obj->setInternalValue(immediateBooleanValue);
    return obj;
}

} // namespace JSC
