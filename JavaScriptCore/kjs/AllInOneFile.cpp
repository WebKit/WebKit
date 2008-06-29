/*
 *  Copyright (C) 2006 Apple Inc. All rights reserved.
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

#include "JSFunction.cpp"
#include "debugger.cpp"
#include "JSArray.cpp"
#include "ArrayConstructor.cpp"
#include "ArrayPrototype.cpp"
#include "BooleanConstructor.cpp"
#include "BooleanObject.cpp"
#include "BooleanPrototype.cpp"
#include "collector.cpp"
#if PLATFORM(DARWIN)
#include "CollectorHeapIntrospector.cpp"
#endif
#include "CommonIdentifiers.cpp"
#include "DateConstructor.cpp"
#include "DateMath.cpp"
#include "DatePrototype.cpp"
#include "date_object.cpp"
#include "dtoa.cpp"
#include "ErrorInstance.cpp"
#include "ErrorPrototype.cpp"
#include "ErrorConstructor.cpp"
#include "FunctionConstructor.cpp"
#include "FunctionPrototype.cpp"
#include "grammar.cpp"
#include "identifier.cpp"
#include "internal.cpp"
#include "interpreter.cpp"
#include "JSImmediate.cpp"
#include "JSLock.cpp"
#include "JSWrapperObject.cpp"
#include "lexer.cpp"
#include "list.cpp"
#include "lookup.cpp"
#include "MathObject.cpp"
#include "NativeErrorConstructor.cpp"
#include "NativeErrorPrototype.cpp"
#include "NumberConstructor.cpp"
#include "NumberObject.cpp"
#include "NumberPrototype.cpp"
#include "nodes.cpp"
#include "nodes2string.cpp"
#include "JSObject.cpp"
#include "JSGlobalObject.cpp"
#include "ObjectConstructor.cpp"
#include "ObjectPrototype.cpp"
#include "operations.cpp"
#include "Parser.cpp"
#include "PropertyMap.cpp"
#include "PropertySlot.cpp"
#include "PropertyNameArray.cpp"
#include "regexp.cpp"
#include "RegExpConstructor.cpp"
#include "RegExpObject.cpp"
#include "RegExpPrototype.cpp"
#include "ScopeChain.cpp"
#include "StringConstructor.cpp"
#include "StringObject.cpp"
#include "StringPrototype.cpp"
#include "ustring.cpp"
#include "JSValue.cpp"
#include "JSVariableObject.cpp"
#include "wtf/FastMalloc.cpp"
#include "wtf/TCSystemAlloc.cpp"
#include "VM/CodeGenerator.cpp"
#include "VM/RegisterFile.cpp"
