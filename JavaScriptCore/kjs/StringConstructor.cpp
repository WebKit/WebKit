/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "StringConstructor.h"

#include "FunctionPrototype.h"
#include "JSGlobalObject.h"
#include "StringPrototype.h"

namespace KJS {

static JSValue* stringFromCharCode(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    UString s;
    if (args.size()) {
        UChar* buf = static_cast<UChar*>(fastMalloc(args.size() * sizeof(UChar)));
        UChar* p = buf;
        ArgList::const_iterator end = args.end();
        for (ArgList::const_iterator it = args.begin(); it != end; ++it)
          *p++ = static_cast<UChar>((*it)->toUInt32(exec));
        s = UString(buf, args.size(), false);
    } else
        s = "";

    return jsString(exec, s);
}

StringConstructor::StringConstructor(ExecState* exec, FunctionPrototype* funcProto, StringPrototype* stringProto)
  : InternalFunction(funcProto, Identifier(exec, stringProto->classInfo()->className))
{
  // ECMA 15.5.3.1 String.prototype
  putDirect(exec->propertyNames().prototype, stringProto, ReadOnly | DontEnum | DontDelete);

  // ECMA 15.5.3.2 fromCharCode()
  putDirectFunction(new (exec) PrototypeFunction(exec, funcProto, 1, exec->propertyNames().fromCharCode, stringFromCharCode), DontEnum);

  // no. of arguments for constructor
  putDirect(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontEnum | DontDelete);
}

// ECMA 15.5.2
static JSObject* constructWithStringConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    JSObject* prototype = exec->lexicalGlobalObject()->stringPrototype();
    if (args.isEmpty())
        return new (exec) StringObject(exec, prototype);
    return new (exec) StringObject(exec, prototype, args[0]->toString(exec));
}

ConstructType StringConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithStringConstructor;
    return ConstructTypeNative;
}

// ECMA 15.5.1
static JSValue* callStringConstructor(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    if (args.isEmpty())
        return jsString(exec, "");
    return jsString(exec, args[0]->toString(exec));
}

CallType StringConstructor::getCallData(CallData& callData)
{
    callData.native.function = callStringConstructor;
    return CallTypeNative;
}

} // namespace KJS
