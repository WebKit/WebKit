/*
 *  Copyright (C) 1999-2000,2003 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "NumberConstructor.h"
#include "NumberConstructor.lut.h"

#include "NumberObject.h"
#include "NumberPrototype.h"

namespace KJS {


const ClassInfo NumberConstructor::info = { "Function", &InternalFunction::info, 0, ExecState::numberTable };

/* Source for NumberObject.lut.h
@begin numberTable
  NaN                   NumberConstructor::NaNValue       DontEnum|DontDelete|ReadOnly
  NEGATIVE_INFINITY     NumberConstructor::NegInfinity    DontEnum|DontDelete|ReadOnly
  POSITIVE_INFINITY     NumberConstructor::PosInfinity    DontEnum|DontDelete|ReadOnly
  MAX_VALUE             NumberConstructor::MaxValue       DontEnum|DontDelete|ReadOnly
  MIN_VALUE             NumberConstructor::MinValue       DontEnum|DontDelete|ReadOnly
@end
*/
NumberConstructor::NumberConstructor(ExecState* exec, FunctionPrototype* funcProto, NumberPrototype* numberProto)
    : InternalFunction(funcProto, Identifier(exec, numberProto->info.className))
{
    // Number.Prototype
    putDirect(exec->propertyNames().prototype, numberProto, DontEnum|DontDelete|ReadOnly);

    // no. of arguments for constructor
    putDirect(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontEnum | DontDelete);
}

bool NumberConstructor::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<NumberConstructor, InternalFunction>(exec, ExecState::numberTable(exec), this, propertyName, slot);
}

JSValue* NumberConstructor::getValueProperty(ExecState* exec, int token) const
{
    // ECMA 15.7.3
    switch (token) {
        case NaNValue:
            return jsNaN(exec);
        case NegInfinity:
            return jsNumberCell(exec, -Inf);
        case PosInfinity:
            return jsNumberCell(exec, Inf);
        case MaxValue:
            return jsNumberCell(exec, 1.7976931348623157E+308);
        case MinValue:
            return jsNumberCell(exec, 5E-324);
    }
    ASSERT_NOT_REACHED();
    return jsNull();
}

// ECMA 15.7.1
static JSObject* constructWithNumberConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    NumberObject* obj = new (exec) NumberObject(exec->lexicalGlobalObject()->numberPrototype());
    double n = args.isEmpty() ? 0 : args[0]->toNumber(exec);
    obj->setInternalValue(jsNumber(exec, n));
    return obj;
}

ConstructType NumberConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithNumberConstructor;
    return ConstructTypeNative;
}

// ECMA 15.7.2
static JSValue* callNumberConstructor(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, args.isEmpty() ? 0 : args[0]->toNumber(exec));
}

CallType NumberConstructor::getCallData(CallData& callData)
{
    callData.native.function = callNumberConstructor;
    return CallTypeNative;
}

NumberObject* constructNumber(ExecState* exec, JSNumberCell* number)
{
    NumberObject* obj = new (exec) NumberObject(exec->lexicalGlobalObject()->numberPrototype());
    obj->setInternalValue(number);
    return obj;
}

NumberObject* constructNumberFromImmediateNumber(ExecState* exec, JSValue* value)
{
    NumberObject* obj = new (exec) NumberObject(exec->lexicalGlobalObject()->numberPrototype());
    obj->setInternalValue(value);
    return obj;
}

} // namespace KJS
