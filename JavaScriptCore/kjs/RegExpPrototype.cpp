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
#include "FunctionPrototype.h"
#include "JSArray.h"
#include "JSObject.h"
#include "JSString.h"
#include "JSValue.h"
#include "ObjectPrototype.h"
#include "RegExpObject.h"
#include "regexp.h"

namespace KJS {

static JSValue* regExpProtoFuncTest(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* regExpProtoFuncExec(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* regExpProtoFuncCompile(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* regExpProtoFuncToString(ExecState*, JSObject*, JSValue*, const ArgList&);

// ECMA 15.10.5

const ClassInfo RegExpPrototype::info = { "RegExpPrototype", 0, 0, 0 };

RegExpPrototype::RegExpPrototype(ExecState* exec, ObjectPrototype* objectPrototype, FunctionPrototype* functionPrototype)
    : JSObject(objectPrototype)
{
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().compile, regExpProtoFuncCompile), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().exec, regExpProtoFuncExec), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().test, regExpProtoFuncTest), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toString, regExpProtoFuncToString), DontEnum);
}

// ------------------------------ Functions ---------------------------
    
JSValue* regExpProtoFuncTest(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&RegExpObject::info))
        return throwError(exec, TypeError);
    return static_cast<RegExpObject*>(thisValue)->test(exec, args);
}

JSValue* regExpProtoFuncExec(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&RegExpObject::info))
        return throwError(exec, TypeError);
    return static_cast<RegExpObject*>(thisValue)->exec(exec, args);
}

JSValue* regExpProtoFuncCompile(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&RegExpObject::info))
        return throwError(exec, TypeError);

    RefPtr<RegExp> regExp;
    JSValue* arg0 = args[0];
    JSValue* arg1 = args[1];
    
    if (arg0->isObject(&RegExpObject::info)) {
        if (!arg1->isUndefined())
            return throwError(exec, TypeError, "Cannot supply flags when constructing one RegExp from another.");
        regExp = static_cast<RegExpObject*>(arg0)->regExp();
    } else {
        UString pattern = args.isEmpty() ? UString("") : arg0->toString(exec);
        UString flags = arg1->isUndefined() ? UString("") : arg1->toString(exec);
        regExp = RegExp::create(pattern, flags);
    }

    if (!regExp->isValid())
        return throwError(exec, SyntaxError, UString("Invalid regular expression: ").append(regExp->errorMessage()));

    static_cast<RegExpObject*>(thisValue)->setRegExp(regExp.release());
    static_cast<RegExpObject*>(thisValue)->setLastIndex(0);
    return jsUndefined();
}

JSValue* regExpProtoFuncToString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&RegExpObject::info)) {
        if (thisValue->isObject(&RegExpPrototype::info))
            return jsString(exec, "//");
        return throwError(exec, TypeError);
    }

    UString result = "/" + static_cast<RegExpObject*>(thisValue)->get(exec, exec->propertyNames().source)->toString(exec) + "/";
    if (static_cast<RegExpObject*>(thisValue)->get(exec, exec->propertyNames().global)->toBoolean(exec))
        result += "g";
    if (static_cast<RegExpObject*>(thisValue)->get(exec, exec->propertyNames().ignoreCase)->toBoolean(exec))
        result += "i";
    if (static_cast<RegExpObject*>(thisValue)->get(exec, exec->propertyNames().multiline)->toBoolean(exec))
        result += "m";
    return jsString(exec, result);
}

} // namespace KJS
