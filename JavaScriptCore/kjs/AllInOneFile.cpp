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
#include "date_object.cpp"
#include "DateMath.cpp"
#include "dtoa.cpp"
#include "error_object.cpp"
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
#include "nodes.cpp"
#include "nodes2string.cpp"
#include "NumberObject.cpp"
#include "JSObject.cpp"
#include "JSGlobalObject.cpp"
#include "object_object.cpp"
#include "operations.cpp"
#include "Parser.cpp"
#include "PropertyMap.cpp"
#include "PropertySlot.cpp"
#include "PropertyNameArray.cpp"
#include "regexp.cpp"
#include "RegExpObject.cpp"
#include "ScopeChain.cpp"
#include "string_object.cpp"
#include "ustring.cpp"
#include "JSValue.cpp"
#include "wtf/FastMalloc.cpp"
#include "wtf/TCSystemAlloc.cpp"
#include "VM/CodeGenerator.cpp"
#include "VM/RegisterFile.cpp"
