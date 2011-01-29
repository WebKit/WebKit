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
#include "NativeErrorConstructor.h"

#include "ErrorInstance.h"
#include "JSFunction.h"
#include "JSString.h"
#include "NativeErrorPrototype.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(NativeErrorConstructor);

const ClassInfo NativeErrorConstructor::info = { "Function", &InternalFunction::info, 0, 0 };

NativeErrorConstructor::NativeErrorConstructor(ExecState* exec, JSGlobalObject* globalObject, NonNullPassRefPtr<Structure> structure, NonNullPassRefPtr<Structure> prototypeStructure, const UString& nameAndMessage)
    : InternalFunction(&exec->globalData(), globalObject, structure, Identifier(exec, nameAndMessage))
{
    NativeErrorPrototype* prototype = new (exec) NativeErrorPrototype(exec, globalObject, prototypeStructure, nameAndMessage, this);

    putDirect(exec->propertyNames().length, jsNumber(1), DontDelete | ReadOnly | DontEnum); // ECMA 15.11.7.5
    putDirect(exec->propertyNames().prototype, prototype, DontDelete | ReadOnly | DontEnum);
    m_errorStructure = ErrorInstance::createStructure(prototype);
}

static EncodedJSValue JSC_HOST_CALL constructWithNativeErrorConstructor(ExecState* exec)
{
    JSValue message = exec->argumentCount() ? exec->argument(0) : jsUndefined();
    Structure* errorStructure = static_cast<NativeErrorConstructor*>(exec->callee())->errorStructure();
    return JSValue::encode(ErrorInstance::create(exec, errorStructure, message));
}

ConstructType NativeErrorConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithNativeErrorConstructor;
    return ConstructTypeHost;
}
    
static EncodedJSValue JSC_HOST_CALL callNativeErrorConstructor(ExecState* exec)
{
    JSValue message = exec->argumentCount() ? exec->argument(0) : jsUndefined();
    Structure* errorStructure = static_cast<NativeErrorConstructor*>(exec->callee())->errorStructure();
    return JSValue::encode(ErrorInstance::create(exec, errorStructure, message));
}

CallType NativeErrorConstructor::getCallData(CallData& callData)
{
    callData.native.function = callNativeErrorConstructor;
    return CallTypeHost;
}

} // namespace JSC
