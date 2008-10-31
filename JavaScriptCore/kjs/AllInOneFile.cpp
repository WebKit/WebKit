/*
 *  Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

// This file exists to help compile the essential code of
// JavaScriptCore all as one file, for compilers and build systems
// that see a significant speed gain from this.

#define KDE_USE_FINAL 1
#define JAVASCRIPTCORE_BUILDING_ALL_IN_ONE_FILE 1
#include "config.h"

// these headers are included here to avoid confusion between ::JSType and JSC::JSType
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"

#include "JSStaticScopeObject.cpp"
#include "runtime/JSFunction.cpp"
#include "Arguments.cpp"
#include "runtime/JSGlobalObjectFunctions.cpp"
#include "runtime/PrototypeFunction.cpp"
#include "runtime/GlobalEvalFunction.cpp"
#include "debugger/Debugger.cpp"
#include "runtime/JSArray.cpp"
#include "runtime/ArrayConstructor.cpp"
#include "runtime/ArrayPrototype.cpp"
#include "runtime/BooleanConstructor.cpp"
#include "runtime/BooleanObject.cpp"
#include "runtime/BooleanPrototype.cpp"
#include "collector.cpp"
#include "CommonIdentifiers.cpp"
#include "runtime/DateConstructor.cpp"
#include "runtime/DateMath.cpp"
#include "runtime/DatePrototype.cpp"
#include "runtime/DateInstance.cpp"
#include "dtoa.cpp"
#include "runtime/ErrorInstance.cpp"
#include "runtime/ErrorPrototype.cpp"
#include "runtime/ErrorConstructor.cpp"
#include "runtime/FunctionConstructor.cpp"
#include "runtime/FunctionPrototype.cpp"
#include "grammar.cpp"
#include "identifier.cpp"
#include "runtime/JSString.cpp"
#include "runtime/JSNumberCell.cpp"
#include "GetterSetter.cpp"
#include "runtime/InternalFunction.cpp"
#include "interpreter.cpp"
#include "runtime/JSImmediate.cpp"
#include "JSLock.cpp"
#include "runtime/JSWrapperObject.cpp"
#include "lexer.cpp"
#include "ArgList.cpp"
#include "lookup.cpp"
#include "runtime/MathObject.cpp"
#include "runtime/NativeErrorConstructor.cpp"
#include "runtime/NativeErrorPrototype.cpp"
#include "runtime/NumberConstructor.cpp"
#include "runtime/NumberObject.cpp"
#include "runtime/NumberPrototype.cpp"
#include "nodes.cpp"
#include "nodes2string.cpp"
#include "runtime/JSObject.cpp"
#include "runtime/Error.cpp"
#include "runtime/JSGlobalObject.cpp"
#include "runtime/ObjectConstructor.cpp"
#include "runtime/ObjectPrototype.cpp"
#include "operations.cpp"
#include "Parser.cpp"
#include "runtime/PropertySlot.cpp"
#include "PropertyNameArray.cpp"
#include "regexp.cpp"
#include "runtime/RegExpConstructor.cpp"
#include "runtime/RegExpObject.cpp"
#include "runtime/RegExpPrototype.cpp"
#include "ScopeChain.cpp"
#include "runtime/StringConstructor.cpp"
#include "runtime/StringObject.cpp"
#include "runtime/StringPrototype.cpp"
#include "ustring.cpp"
#include "runtime/JSValue.cpp"
#include "runtime/CallData.cpp"
#include "runtime/ConstructData.cpp"
#include "runtime/JSCell.cpp"
#include "runtime/JSVariableObject.cpp"
#include "wtf/FastMalloc.cpp"
#include "wtf/TCSystemAlloc.cpp"
#include "VM/CodeGenerator.cpp"
#include "VM/RegisterFile.cpp"
