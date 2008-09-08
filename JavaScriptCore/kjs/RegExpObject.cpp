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
#include "RegExpObject.lut.h"

#include "JSArray.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "RegExpConstructor.h"
#include "RegExpPrototype.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(RegExpObject);

const ClassInfo RegExpObject::info = { "RegExp", 0, 0, ExecState::regExpTable };

/* Source for RegExpObject.lut.h
@begin regExpTable
    global        RegExpObject::Global       DontDelete|ReadOnly|DontEnum
    ignoreCase    RegExpObject::IgnoreCase   DontDelete|ReadOnly|DontEnum
    multiline     RegExpObject::Multiline    DontDelete|ReadOnly|DontEnum
    source        RegExpObject::Source       DontDelete|ReadOnly|DontEnum
    lastIndex     RegExpObject::LastIndex    DontDelete|DontEnum
@end
*/

RegExpObject::RegExpObject(RegExpPrototype* regExpPrototype, PassRefPtr<RegExp> regExp)
    : JSObject(regExpPrototype)
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

JSValue* RegExpObject::getValueProperty(ExecState* exec, int token) const
{
    switch (token) {
        case Global:
            return jsBoolean(d->regExp->global());
        case IgnoreCase:
            return jsBoolean(d->regExp->ignoreCase());
        case Multiline:
            return jsBoolean(d->regExp->multiline());
        case Source:
            return jsString(exec, d->regExp->pattern());
        case LastIndex:
            return jsNumber(exec, d->lastIndex);
    }
    
    ASSERT_NOT_REACHED();
    return 0;
}

void RegExpObject::put(ExecState* exec, const Identifier& propertyName, JSValue* value, PutPropertySlot& slot)
{
    lookupPut<RegExpObject, JSObject>(exec, propertyName, value, ExecState::regExpTable(exec), this, slot);
}

void RegExpObject::putValueProperty(ExecState* exec, int token, JSValue* value)
{
    UNUSED_PARAM(token);
    ASSERT(token == LastIndex);
    d->lastIndex = value->toInteger(exec);
}

bool RegExpObject::match(ExecState* exec, const ArgList& args)
{
    RegExpConstructor* regExpObj = exec->lexicalGlobalObject()->regExpConstructor();

    UString input;
    if (!args.isEmpty())
        input = args.at(exec, 0)->toString(exec);
    else {
        input = regExpObj->input();
        if (input.isNull()) {
            throwError(exec, GeneralError, "No input.");
            return false;
        }
    }

    bool global = get(exec, exec->propertyNames().global)->toBoolean(exec);
    int lastIndex = 0;
    if (global) {
        if (d->lastIndex < 0 || d->lastIndex > input.size()) {
            d->lastIndex = 0;
            return false;
        }
        lastIndex = static_cast<int>(d->lastIndex);
    }

    int foundIndex;
    int foundLength;
    regExpObj->performMatch(d->regExp.get(), input, lastIndex, foundIndex, foundLength);

    if (global) {
        lastIndex = foundIndex < 0 ? 0 : foundIndex + foundLength;
        d->lastIndex = lastIndex;
    }

    return foundIndex >= 0;
}

JSValue* RegExpObject::test(ExecState* exec, const ArgList& args)
{
    return jsBoolean(match(exec, args));
}

JSValue* RegExpObject::exec(ExecState* exec, const ArgList& args)
{
    if (match(exec, args))
        return exec->lexicalGlobalObject()->regExpConstructor()->arrayOfMatches(exec);
    return jsNull();
}

static JSValue* callRegExpObject(ExecState* exec, JSObject* function, JSValue*, const ArgList& args)
{
    return static_cast<RegExpObject*>(function)->exec(exec, args);
}

CallType RegExpObject::getCallData(CallData& callData)
{
    callData.native.function = callRegExpObject;
    return CallTypeHost;
}

} // namespace JSC
