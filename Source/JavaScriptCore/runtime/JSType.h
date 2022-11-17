/*
 *  Copyright (C) 2006-2019 Apple Inc. All rights reserved.
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
    StructureType,
    StringType,
    HeapBigIntType,
    LastMaybeFalsyCellPrimitive = HeapBigIntType,
    SymbolType,

    GetterSetterType,
    CustomGetterSetterType,
    APIValueWrapperType,

    NativeExecutableType,

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
    NullSetterFunctionType,
    BooleanObjectType,
    NumberObjectType,
    ErrorInstanceType,
    PureForwardingProxyType,
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
    BigInt64ArrayType,
    BigUint64ArrayType,
    DataViewType,
    // End JSArrayBufferView types.

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

    ModuleNamespaceObjectType,
    ShadowRealmType,
    RegExpObjectType,
    JSDateType,
    ProxyObjectType,
    JSGeneratorType,
    JSAsyncGeneratorType,
    JSArrayIteratorType,
    JSMapIteratorType,
    JSSetIteratorType,
    JSStringIteratorType,
    JSPromiseType,
    JSMapType,
    JSSetType,
    JSWeakMapType,
    JSWeakSetType,
    WebAssemblyModuleType,
    // Start StringObjectType types.
    StringObjectType,
    DerivedStringObjectType,
    // End StringObjectType types.

    LastJSCObjectType = DerivedStringObjectType, // This is the last "JSC" Object type. After this, we have embedder's (e.g., WebCore) extended object types.
    MaxJSType = 0b11111111,
};

static constexpr uint32_t FirstTypedArrayType = Int8ArrayType;
static constexpr uint32_t LastTypedArrayType = DataViewType;
static constexpr uint32_t LastTypedArrayTypeExcludingDataView = LastTypedArrayType - 1;

// LastObjectType should be MaxJSType (not LastJSCObjectType) since embedders can add their extended object types after the enums listed in JSType.
static constexpr uint32_t FirstObjectType = ObjectType;
static constexpr uint32_t LastObjectType = MaxJSType;

static constexpr uint32_t NumberOfTypedArrayTypes = LastTypedArrayType - FirstTypedArrayType + 1;
static constexpr uint32_t NumberOfTypedArrayTypesExcludingDataView = NumberOfTypedArrayTypes - 1;
static constexpr uint32_t NumberOfTypedArrayTypesExcludingBigIntArraysAndDataView = NumberOfTypedArrayTypes - 3;

static_assert(sizeof(JSType) == sizeof(uint8_t), "sizeof(JSType) is one byte.");
static_assert(LastJSCObjectType < 0b11100000, "Embedder can use 0b11100000 or upper.");

inline constexpr bool isTypedArrayType(JSType type)
{
    return (static_cast<uint32_t>(type) - FirstTypedArrayType) < NumberOfTypedArrayTypesExcludingDataView;
}

inline constexpr bool isTypedArrayTypeIncludingDataView(JSType type)
{
    return (static_cast<uint32_t>(type) - FirstTypedArrayType) < NumberOfTypedArrayTypes;
}

inline constexpr bool isObjectType(JSType type) { return type >= ObjectType; }

} // namespace JSC

namespace WTF {

class PrintStream;

void printInternal(PrintStream&, JSC::JSType);

} // namespace WTF
