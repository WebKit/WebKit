/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "CodeLocation.h"
#include "Identifier.h"
#include "JSString.h"
#include "MacroAssemblerCodeRef.h"
#include "PageCount.h"
#include "RegisterAtOffsetList.h"
#include "WasmMemoryInformation.h"
#include "WasmName.h"
#include "WasmNameSection.h"
#include "WasmOSREntryData.h"
#include "WasmOps.h"
#include "WasmTypeDefinition.h"
#include <cstdint>
#include <limits>
#include <memory>
#include <wtf/Vector.h>

namespace JSC {

class Compilation;

namespace Wasm {

struct CompilationContext;
struct ModuleInformation;
struct UnlinkedHandlerInfo;

using BlockSignature = const TypeDefinition*;

enum class TableElementType : uint8_t {
    Externref,
    Funcref
};

inline bool isValueType(Type type)
{
    switch (type.kind) {
    case TypeKind::I32:
    case TypeKind::I64:
    case TypeKind::F32:
    case TypeKind::F64:
    case TypeKind::Externref:
    case TypeKind::Funcref:
        return true;
    case TypeKind::Ref:
    case TypeKind::RefNull:
        return Options::useWebAssemblyTypedFunctionReferences();
    case TypeKind::V128:
        return Options::useWebAssemblySIMD();
    default:
        break;
    }
    return false;
}

inline JSString* typeToString(VM& vm, TypeKind type)
{
#define TYPE_CASE(macroName, value, b3, inc, wasmName, ...) \
    case TypeKind::macroName: \
        return jsNontrivialString(vm, #wasmName""_s); \

    switch (type) {
        FOR_EACH_WASM_TYPE(TYPE_CASE)
    }

#undef TYPE_CASE

    RELEASE_ASSERT_NOT_REACHED();
}

inline bool isRefType(Type type)
{
    if (Options::useWebAssemblyTypedFunctionReferences())
        return type.isRef() || type.isRefNull();
    return type.isFuncref() || type.isExternref();
}

inline bool isExternref(Type type)
{
    if (Options::useWebAssemblyTypedFunctionReferences())
        return isRefType(type) && type.index == static_cast<TypeIndex>(TypeKind::Externref);
    return type.kind == TypeKind::Externref;
}

inline bool isFuncref(Type type)
{
    if (Options::useWebAssemblyTypedFunctionReferences())
        return isRefType(type) && type.index == static_cast<TypeIndex>(TypeKind::Funcref);
    return type.kind == TypeKind::Funcref;
}

inline bool isI31ref(Type type)
{
    if (!Options::useWebAssemblyGC())
        return false;
    return isRefType(type) && type.index == static_cast<TypeIndex>(TypeKind::I31ref);
}

inline bool isArrayref(Type type)
{
    if (!Options::useWebAssemblyGC())
        return false;
    return isRefType(type) && type.index == static_cast<TypeIndex>(TypeKind::Arrayref);
}

inline Type funcrefType()
{
    if (Options::useWebAssemblyTypedFunctionReferences())
        return Wasm::Type { Wasm::TypeKind::RefNull, static_cast<Wasm::TypeIndex>(Wasm::TypeKind::Funcref) };
    return Types::Funcref;
}

inline Type externrefType()
{
    if (Options::useWebAssemblyTypedFunctionReferences())
        return Wasm::Type { Wasm::TypeKind::RefNull, static_cast<Wasm::TypeIndex>(Wasm::TypeKind::Externref) };
    return Types::Externref;
}

inline bool isRefWithTypeIndex(Type type)
{
    if (!Options::useWebAssemblyTypedFunctionReferences())
        return false;

    return isRefType(type) && !isExternref(type) && !isFuncref(type) && !isI31ref(type) && !isArrayref(type);
}

// Determine if the ref type has a placeholder type index that is used
// for an unresoled recursive reference in a recursion group.
inline bool isRefWithRecursiveReference(Type type)
{
    if (!Options::useWebAssemblyGC())
        return false;

    if (isRefWithTypeIndex(type)) {
        const TypeDefinition& def = TypeInformation::get(type.index);
        if (def.is<Projection>())
            return def.as<Projection>()->isPlaceholder();
    }

    return false;
}

inline bool isTypeIndexHeapType(int32_t heapType)
{
    if (!Options::useWebAssemblyTypedFunctionReferences())
        return false;

    return heapType >= 0;
}

inline bool isSubtypeIndex(TypeIndex sub, TypeIndex parent)
{
    if (sub == parent)
        return true;

    const TypeDefinition& sig = TypeInformation::get(sub).unroll();
    if (sig.is<Subtype>()) {
        const Subtype& subtype = *sig.as<Subtype>();
        const TypeDefinition& parentSig = TypeInformation::get(parent).unroll();
        if (parentSig.is<Subtype>()) {
            if (subtype.displaySize() < parentSig.as<Subtype>()->displaySize())
                return false;
            return parent == subtype.displayType(subtype.displaySize() - parentSig.as<Subtype>()->displaySize() - 1);
        }
        // If not a subtype itself, the parent must be at the top of the display.
        return parent == subtype.displayType(subtype.displaySize() - 1);
    }

    return false;
}

inline bool isSubtype(Type sub, Type parent)
{
    if (sub.isNullable() && !parent.isNullable())
        return false;

    if (isRefWithTypeIndex(sub)) {
        if (TypeInformation::get(sub.index).expand().is<ArrayType>() && isArrayref(parent))
            return true;

        if (TypeInformation::get(sub.index).expand().is<FunctionSignature>() && isFuncref(parent))
            return true;

        if (isRefWithTypeIndex(parent))
            return isSubtypeIndex(sub.index, parent.index);
    }

    if (sub.isRef() && parent.isRefNull() && sub.index == parent.index)
        return true;

    return sub == parent;
}

inline bool isValidHeapTypeKind(TypeKind kind)
{
    switch (kind) {
    case TypeKind::Funcref:
    case TypeKind::Externref:
        return true;
    case TypeKind::I31ref:
    case TypeKind::Arrayref:
        return Options::useWebAssemblyGC();
    default:
        break;
    }
    return false;
}

inline bool isDefaultableType(Type type)
{
    return !type.isRef();
}

enum class ExternalKind : uint8_t {
    // FIXME auto-generate this. https://bugs.webkit.org/show_bug.cgi?id=165231
    Function = 0,
    Table = 1,
    Memory = 2,
    Global = 3,
    Exception = 4,
};

template<typename Int>
inline bool isValidExternalKind(Int val)
{
    switch (val) {
    case static_cast<Int>(ExternalKind::Function):
    case static_cast<Int>(ExternalKind::Table):
    case static_cast<Int>(ExternalKind::Memory):
    case static_cast<Int>(ExternalKind::Global):
    case static_cast<Int>(ExternalKind::Exception):
        return true;
    }
    return false;
}

static_assert(static_cast<int>(ExternalKind::Function) == 0, "Wasm needs Function to have the value 0");
static_assert(static_cast<int>(ExternalKind::Table)    == 1, "Wasm needs Table to have the value 1");
static_assert(static_cast<int>(ExternalKind::Memory)   == 2, "Wasm needs Memory to have the value 2");
static_assert(static_cast<int>(ExternalKind::Global)   == 3, "Wasm needs Global to have the value 3");
static_assert(static_cast<int>(ExternalKind::Exception)   == 4, "Wasm needs Exception to have the value 4");

inline const char* makeString(ExternalKind kind)
{
    switch (kind) {
    case ExternalKind::Function: return "function";
    case ExternalKind::Table: return "table";
    case ExternalKind::Memory: return "memory";
    case ExternalKind::Global: return "global";
    case ExternalKind::Exception: return "tag";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return "?";
}

struct Import {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    const Name module;
    const Name field;
    ExternalKind kind;
    unsigned kindIndex; // Index in the vector of the corresponding kind.
};

struct Export {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    const Name field;
    ExternalKind kind;
    unsigned kindIndex; // Index in the vector of the corresponding kind.
};

String makeString(const Name& characters);

struct GlobalInformation {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    enum InitializationType : uint8_t {
        IsImport,
        FromGlobalImport,
        FromRefFunc,
        FromExpression,
        FromVector,
    };

    enum class BindingMode : uint8_t {
        EmbeddedInInstance = 0,
        Portable,
    };

    Mutability mutability;
    Type type;
    InitializationType initializationType { IsImport };
    BindingMode bindingMode { BindingMode::EmbeddedInInstance };
    union {
        uint64_t initialBitsOrImportNumber;
        v128_t initialVector { };
    } initialBits;
};

struct FunctionData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    size_t start;
    size_t end;
    Vector<uint8_t> data;
};

class I32InitExpr {
    WTF_MAKE_FAST_ALLOCATED;
    enum Type : uint8_t {
        Global,
        Const
    };

    I32InitExpr(Type type, uint32_t bits)
        : m_bits(bits)
        , m_type(type)
    { }

public:
    I32InitExpr() = delete;

    static I32InitExpr globalImport(uint32_t globalImportNumber) { return I32InitExpr(Global, globalImportNumber); }
    static I32InitExpr constValue(uint32_t constValue) { return I32InitExpr(Const, constValue); }

    bool isConst() const { return m_type == Const; }
    bool isGlobalImport() const { return m_type == Global; }
    uint32_t constValue() const
    {
        RELEASE_ASSERT(isConst());
        return m_bits;
    }
    uint32_t globalImportIndex() const
    {
        RELEASE_ASSERT(isGlobalImport());
        return m_bits;
    }

private:
    uint32_t m_bits;
    Type m_type;
};

struct Segment {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    enum class Kind : uint8_t {
        Active,
        Passive,
    };

    Kind kind;
    uint32_t sizeInBytes;
    std::optional<I32InitExpr> offsetIfActive;
    // Bytes are allocated at the end.
    uint8_t& byte(uint32_t pos)
    {
        ASSERT(pos < sizeInBytes);
        return *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(this) + sizeof(Segment) + pos);
    }

    static void destroy(Segment*);
    typedef std::unique_ptr<Segment, decltype(&Segment::destroy)> Ptr;
    static Segment::Ptr create(std::optional<I32InitExpr>, uint32_t, Kind);

    bool isActive() const { return kind == Kind::Active; }
    bool isPassive() const { return kind == Kind::Passive; }
};

struct Element {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    // nullFuncIndex represents the case when an element segment (of type funcref)
    // contains a null element.
    constexpr static uint32_t nullFuncIndex = UINT32_MAX;

    enum class Kind : uint8_t {
        Active,
        Passive,
        Declared,
    };

    Element(Element::Kind kind, TableElementType elementType, std::optional<uint32_t> tableIndex, std::optional<I32InitExpr> initExpr)
        : kind(kind)
        , elementType(elementType)
        , tableIndexIfActive(WTFMove(tableIndex))
        , offsetIfActive(WTFMove(initExpr))
    { }

    Element(Element::Kind kind, TableElementType elemType)
        : Element(kind, elemType, std::nullopt, std::nullopt)
    { }

    uint32_t length() const { return functionIndices.size(); }

    bool isActive() const { return kind == Kind::Active; }
    bool isPassive() const { return kind == Kind::Passive; }

    static bool isNullFuncIndex(uint32_t idx) { return idx == nullFuncIndex; }

    Kind kind;
    TableElementType elementType;
    std::optional<uint32_t> tableIndexIfActive;
    std::optional<I32InitExpr> offsetIfActive;

    // Index may be nullFuncIndex.
    Vector<uint32_t> functionIndices;
};

class TableInformation {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TableInformation()
    {
        ASSERT(!*this);
    }

    TableInformation(uint32_t initial, std::optional<uint32_t> maximum, bool isImport, TableElementType type)
        : m_initial(initial)
        , m_maximum(maximum)
        , m_isImport(isImport)
        , m_isValid(true)
        , m_type(type)
    {
        ASSERT(*this);
    }

    explicit operator bool() const { return m_isValid; }
    bool isImport() const { return m_isImport; }
    uint32_t initial() const { return m_initial; }
    std::optional<uint32_t> maximum() const { return m_maximum; }
    TableElementType type() const { return m_type; }
    Type wasmType() const { return m_type == TableElementType::Funcref ? funcrefType() : externrefType(); }

private:
    uint32_t m_initial;
    std::optional<uint32_t> m_maximum;
    bool m_isImport { false };
    bool m_isValid { false };
    TableElementType m_type;
};
    
struct CustomSection {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    Name name;
    Vector<uint8_t> payload;
};

enum class NameType : uint8_t {
    Module = 0,
    Function = 1,
    Local = 2,
};
    
template<typename Int>
inline bool isValidNameType(Int val)
{
    switch (val) {
    case static_cast<Int>(NameType::Module):
    case static_cast<Int>(NameType::Function):
    case static_cast<Int>(NameType::Local):
        return true;
    }
    return false;
}

struct UnlinkedWasmToWasmCall {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    CodeLocationNearCall<WasmEntryPtrTag> callLocation;
    size_t functionIndexSpace;
};

struct Entrypoint {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    std::unique_ptr<Compilation> compilation;
    RegisterAtOffsetList calleeSaveRegisters;
};

struct InternalFunction {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    Vector<CodeLocationDataLabelPtr<WasmEntryPtrTag>> calleeMoveLocations;
#if ENABLE(WEBASSEMBLY_B3JIT)
    StackMaps stackmaps;
#endif
    Vector<UnlinkedHandlerInfo> exceptionHandlers;
    Entrypoint entrypoint;
    unsigned osrEntryScratchBufferSize { 0 };
};

// WebAssembly direct calls and call_indirect use indices into "function index space". This space starts
// with all imports, and then all internal functions. WasmToWasmImportableFunction and FunctionIndexSpace are only
// meant as fast lookup tables for these opcodes and do not own code.
struct WasmToWasmImportableFunction {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    using LoadLocation = CodePtr<WasmEntryPtrTag>*;
    static ptrdiff_t offsetOfSignatureIndex() { return OBJECT_OFFSETOF(WasmToWasmImportableFunction, typeIndex); }
    static ptrdiff_t offsetOfEntrypointLoadLocation() { return OBJECT_OFFSETOF(WasmToWasmImportableFunction, entrypointLoadLocation); }

    // FIXME: Pack type index and code pointer into one 64-bit value. See <https://bugs.webkit.org/show_bug.cgi?id=165511>.
    TypeIndex typeIndex { TypeDefinition::invalidIndex };
    LoadLocation entrypointLoadLocation;
};
using FunctionIndexSpace = Vector<WasmToWasmImportableFunction>;

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
