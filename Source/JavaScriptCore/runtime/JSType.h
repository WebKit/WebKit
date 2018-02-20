/*
 *  Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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

#pragma once

namespace JSC {

enum JSType : uint8_t {
    // The CellType value must come before any JSType that is a JSCell.
    CellType,
    StringType,
    SymbolType,
    BigIntType,

    CustomGetterSetterType,
    APIValueWrapperType,

    ProgramExecutableType,
    ModuleProgramExecutableType,
    EvalExecutableType,
    FunctionExecutableType,

    UnlinkedFunctionExecutableType,

    UnlinkedProgramCodeBlockType,
    UnlinkedModuleProgramCodeBlockType,
    UnlinkedEvalCodeBlockType,
    UnlinkedFunctionCodeBlockType,
        
    CodeBlockType,

    JSFixedArrayType,
    JSSourceCodeType,
    JSScriptFetcherType,
    JSScriptFetchParametersType,

    // The ObjectType value must come before any JSType that is a subclass of JSObject.
    ObjectType,
    FinalObjectType,
    JSCalleeType,
    JSFunctionType,
    InternalFunctionType,
    NumberObjectType,
    ErrorInstanceType,
    PureForwardingProxyType,
    ImpureProxyType,
    WithScopeType,
    DirectArgumentsType,
    ScopedArgumentsType,
    ClonedArgumentsType,

    // Start JSArray types.
    ArrayType,
    DerivedArrayType,
    // End JSArray types.

    // Start JSArrayBufferView types.
    Int8ArrayType,
    Uint8ArrayType,
    Uint8ClampedArrayType,
    Int16ArrayType,
    Uint16ArrayType,
    Int32ArrayType,
    Uint32ArrayType,
    Float32ArrayType,
    Float64ArrayType,
    DataViewType,
    // End JSArrayBufferView types.

    GetterSetterType,

    // Start environment record types.
    GlobalObjectType,
    LexicalEnvironmentType,
    GlobalLexicalEnvironmentType,
    ModuleEnvironmentType,
    StrictEvalActivationType,
    // End environment record types.

    RegExpObjectType,
    ProxyObjectType,
    JSMapType,
    JSSetType,
    JSWeakMapType,
    JSWeakSetType,

    WebAssemblyFunctionType,
    WebAssemblyToJSCalleeType,

    LastJSCObjectType = WebAssemblyToJSCalleeType,
    MaxJSType = 0b11111111,
};

static const uint32_t FirstTypedArrayType = Int8ArrayType;
static const uint32_t LastTypedArrayType = DataViewType;
static constexpr uint32_t NumberOfTypedArrayTypes = LastTypedArrayType - FirstTypedArrayType + 1;
static constexpr uint32_t NumberOfTypedArrayTypesExcludingDataView = NumberOfTypedArrayTypes - 1;

static_assert(sizeof(JSType) == sizeof(uint8_t), "sizeof(JSType) is one byte.");
static_assert(LastJSCObjectType < 128, "The highest bit is reserved for embedder's extension.");

} // namespace JSC
