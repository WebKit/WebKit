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
#include "RegExpPrototype.h"

#include "ArrayPrototype.h"
#include "Error.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSObject.h"
#include "JSString.h"
#include "JSStringBuilder.h"
#include "JSValue.h"
#include "ObjectPrototype.h"
#include "PrototypeFunction.h"
#include "RegExpObject.h"
#include "RegExp.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(RegExpPrototype);

static JSValue JSC_HOST_CALL regExpProtoFuncTest(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL regExpProtoFuncExec(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL regExpProtoFuncCompile(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL regExpProtoFuncToString(ExecState*, JSObject*, JSValue, const ArgList&);

// ECMA 15.10.5

const ClassInfo RegExpPrototype::info = { "RegExpPrototype", 0, 0, 0 };

RegExpPrototype::RegExpPrototype(ExecState* exec, NonNullPassRefPtr<Structure> structure, Structure* prototypeFunctionStructure)
    : JSObject(structure)
{
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 0, exec->propertyNames().compile, regExpProtoFuncCompile), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 0, exec->propertyNames().exec, regExpProtoFuncExec), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 0, exec->propertyNames().test, regExpProtoFuncTest), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 0, exec->propertyNames().toString, regExpProtoFuncToString), DontEnum);
}

// ------------------------------ Functions ---------------------------
    
JSValue JSC_HOST_CALL regExpProtoFuncTest(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    if (!thisValue.inherits(&RegExpObject::info))
        return throwError(exec, TypeError);
    return asRegExpObject(thisValue)->test(exec, args);
}

JSValue JSC_HOST_CALL regExpProtoFuncExec(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    if (!thisValue.inherits(&RegExpObject::info))
        return throwError(exec, TypeError);
    return asRegExpObject(thisValue)->exec(exec, args);
}

JSValue JSC_HOST_CALL regExpProtoFuncCompile(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    if (!thisValue.inherits(&RegExpObject::info))
        return throwError(exec, TypeError);

    RefPtr<RegExp> regExp;
    JSValue arg0 = args.at(0);
    JSValue arg1 = args.at(1);
    
    if (arg0.inherits(&RegExpObject::info)) {
        if (!arg1.isUndefined())
            return throwError(exec, TypeError, "Cannot supply flags when constructing one RegExp from another.");
        regExp = asRegExpObject(arg0)->regExp();
    } else {
        UString pattern = args.isEmpty() ? UString("") : arg0.toString(exec);
        UString flags = arg1.isUndefined() ? UString("") : arg1.toString(exec);
        regExp = RegExp::create(&exec->globalData(), pattern, flags);
    }

    if (!regExp->isValid())
        return throwError(exec, SyntaxError, makeString("Invalid regular expression: ", regExp->errorMessage()));

    asRegExpObject(thisValue)->setRegExp(regExp.release());
    asRegExpObject(thisValue)->setLastIndex(0);
    return jsUndefined();
}

JSValue JSC_HOST_CALL regExpProtoFuncToString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&RegExpObject::info)) {
        if (thisValue.inherits(&RegExpPrototype::info))
            return jsNontrivialString(exec, "//");
        return throwError(exec, TypeError);
    }

    char postfix[5] = { '/', 0, 0, 0, 0 };
    int index = 1;
    if (asRegExpObject(thisValue)->get(exec, exec->propertyNames().global).toBoolean(exec))
        postfix[index++] = 'g';
    if (asRegExpObject(thisValue)->get(exec, exec->propertyNames().ignoreCase).toBoolean(exec))
        postfix[index++] = 'i';
    if (asRegExpObject(thisValue)->get(exec, exec->propertyNames().multiline).toBoolean(exec))
        postfix[index] = 'm';
    UString source = asRegExpObject(thisValue)->get(exec, exec->propertyNames().source).toString(exec);
    // If source is empty, use "/(?:)/" to avoid colliding with comment syntax
    return jsMakeNontrivialString(exec, "/", source.size() ? source : UString("(?:)"), postfix);
}

} // namespace JSC
