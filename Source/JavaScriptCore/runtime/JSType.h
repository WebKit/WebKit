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
    JSImmutableButterflyType,
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
    DirectArgumentsType,
    ScopedArgumentsType,
    ClonedArgumentsType,

    // Start JSArray types.
    ArrayType,
    DerivedArrayType,
    // End JSArray types.

    ArrayBufferType,

    // Start JSArrayBufferView types. Keep in sync with the order of FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW.
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

    // JSScope <- JSWithScope
    //         <- StrictEvalActivation
    //         <- JSSymbolTableObject  <- JSLexicalEnvironment      <- JSModuleEnvironment
    //                                 <- JSSegmentedVariableObject <- JSGlobalLexicalEnvironment
    //                                                              <- JSGlobalObject
    // Start JSScope types.
    // Start environment record types.
    GlobalObjectType,
    GlobalLexicalEnvironmentType,
    LexicalEnvironmentType,
    ModuleEnvironmentType,
    StrictEvalActivationType,
    // End environment record types.
    WithScopeType,
    // End JSScope types.

    RegExpObjectType,
    ProxyObjectType,
    JSMapType,
    JSSetType,
    JSWeakMapType,
    JSWeakSetType,

    WebAssemblyToJSCalleeType,

    LastJSCObjectType = WebAssemblyToJSCalleeType, // This is the last "JSC" Object type. After this, we have embedder's (e.g., WebCore) extended object types.
    MaxJSType = 0b11111111,
};

static const uint32_t FirstTypedArrayType = Int8ArrayType;
static const uint32_t LastTypedArrayType = DataViewType;

// LastObjectType should be MaxJSType (not LastJSCObjectType) since embedders can add their extended object types after the enums listed in JSType.
static const uint32_t FirstObjectType = ObjectType;
static const uint32_t LastObjectType = MaxJSType;

static constexpr uint32_t NumberOfTypedArrayTypes = LastTypedArrayType - FirstTypedArrayType + 1;
static constexpr uint32_t NumberOfTypedArrayTypesExcludingDataView = NumberOfTypedArrayTypes - 1;

static_assert(sizeof(JSType) == sizeof(uint8_t), "sizeof(JSType) is one byte.");
static_assert(LastJSCObjectType < 128, "The highest bit is reserved for embedder's extension.");

} // namespace JSC
