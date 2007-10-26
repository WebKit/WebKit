// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2006 Apple Computer, Inc
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
#include "config.h"

#include "function.cpp"
#include "debugger.cpp"
#include "array_instance.cpp"
#include "array_object.cpp"
#include "bool_object.cpp"
#include "collector.cpp"
#if PLATFORM(DARWIN)
#include "CollectorHeapIntrospector.cpp"
#endif
#include "CommonIdentifiers.cpp"
#include "date_object.cpp"
#include "DateMath.cpp"
#include "dtoa.cpp"
#include "error_object.cpp"
#include "ExecState.cpp"
#include "function_object.cpp"
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
#include "math_object.cpp"
#include "nodes.cpp"
#include "nodes2string.cpp"
#include "number_object.cpp"
#include "object.cpp"
#include "object_object.cpp"
#include "operations.cpp"
#include "Parser.cpp"
#include "property_map.cpp"
#include "property_slot.cpp"
#include "PropertyNameArray.cpp"
#include "regexp.cpp"
#include "regexp_object.cpp"
#include "scope_chain.cpp"
#include "string_object.cpp"
#include "ustring.cpp"
#include "value.cpp"
#include "wtf/FastMalloc.cpp"
#include "wtf/TCSystemAlloc.cpp"
