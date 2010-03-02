/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All Rights Reserved.
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
#include "RegExpObject.h"

#include "Error.h"
#include "JSArray.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "RegExpConstructor.h"
#include "RegExpPrototype.h"

namespace JSC {

static JSValue regExpObjectGlobal(ExecState*, JSValue, const Identifier&);
static JSValue regExpObjectIgnoreCase(ExecState*, JSValue, const Identifier&);
static JSValue regExpObjectMultiline(ExecState*, JSValue, const Identifier&);
static JSValue regExpObjectSource(ExecState*, JSValue, const Identifier&);
static JSValue regExpObjectLastIndex(ExecState*, JSValue, const Identifier&);
static void setRegExpObjectLastIndex(ExecState*, JSObject*, JSValue);

} // namespace JSC

#include "RegExpObject.lut.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(RegExpObject);

const ClassInfo RegExpObject::info = { "RegExp", 0, 0, ExecState::regExpTable };

/* Source for RegExpObject.lut.h
@begin regExpTable
    global        regExpObjectGlobal       DontDelete|ReadOnly|DontEnum
    ignoreCase    regExpObjectIgnoreCase   DontDelete|ReadOnly|DontEnum
    multiline     regExpObjectMultiline    DontDelete|ReadOnly|DontEnum
    source        regExpObjectSource       DontDelete|ReadOnly|DontEnum
    lastIndex     regExpObjectLastIndex    DontDelete|DontEnum
@end
*/

RegExpObject::RegExpObject(NonNullPassRefPtr<Structure> structure, NonNullPassRefPtr<RegExp> regExp)
    : JSObject(structure)
    , d(new RegExpObjectData(regExp, 0))
{
}

RegExpObject::~RegExpObject()
{
}

bool RegExpObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<RegExpObject, JSObject>(exec, ExecState::regExpTable(exec), this, propertyName, slot);
}

bool RegExpObject::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    return getStaticValueDescriptor<RegExpObject, JSObject>(exec, ExecState::regExpTable(exec), this, propertyName, descriptor);
}

JSValue regExpObjectGlobal(ExecState*, JSValue slotBase, const Identifier&)
{
    return jsBoolean(asRegExpObject(slotBase)->regExp()->global());
}

JSValue regExpObjectIgnoreCase(ExecState*, JSValue slotBase, const Identifier&)
{
    return jsBoolean(asRegExpObject(slotBase)->regExp()->ignoreCase());
}
 
JSValue regExpObjectMultiline(ExecState*, JSValue slotBase, const Identifier&)
{            
    return jsBoolean(asRegExpObject(slotBase)->regExp()->multiline());
}

JSValue regExpObjectSource(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return jsString(exec, asRegExpObject(slotBase)->regExp()->pattern());
}

JSValue regExpObjectLastIndex(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return jsNumber(exec, asRegExpObject(slotBase)->lastIndex());
}

void RegExpObject::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    lookupPut<RegExpObject, JSObject>(exec, propertyName, value, ExecState::regExpTable(exec), this, slot);
}

void setRegExpObjectLastIndex(ExecState* exec, JSObject* baseObject, JSValue value)
{
    asRegExpObject(baseObject)->setLastIndex(value.toInteger(exec));
}

JSValue RegExpObject::test(ExecState* exec, const ArgList& args)
{
    return jsBoolean(match(exec, args));
}

JSValue RegExpObject::exec(ExecState* exec, const ArgList& args)
{
    if (match(exec, args))
        return exec->lexicalGlobalObject()->regExpConstructor()->arrayOfMatches(exec);
    return jsNull();
}

static JSValue JSC_HOST_CALL callRegExpObject(ExecState* exec, JSObject* function, JSValue, const ArgList& args)
{
    return asRegExpObject(function)->exec(exec, args);
}

CallType RegExpObject::getCallData(CallData& callData)
{
    callData.native.function = callRegExpObject;
    return CallTypeHost;
}

// Shared implementation used by test and exec.
bool RegExpObject::match(ExecState* exec, const ArgList& args)
{
    RegExpConstructor* regExpConstructor = exec->lexicalGlobalObject()->regExpConstructor();

    UString input = args.isEmpty() ? regExpConstructor->input() : args.at(0).toString(exec);
    if (input.isNull()) {
        throwError(exec, GeneralError, makeString("No input to ", toString(exec), "."));
        return false;
    }

    if (!regExp()->global()) {
        int position;
        int length;
        regExpConstructor->performMatch(d->regExp.get(), input, 0, position, length);
        return position >= 0;
    }

    if (d->lastIndex < 0 || d->lastIndex > input.size()) {
        d->lastIndex = 0;
        return false;
    }

    int position;
    int length = 0;
    regExpConstructor->performMatch(d->regExp.get(), input, static_cast<int>(d->lastIndex), position, length);
    if (position < 0) {
        d->lastIndex = 0;
        return false;
    }

    d->lastIndex = position + length;
    return true;
}

} // namespace JSC
