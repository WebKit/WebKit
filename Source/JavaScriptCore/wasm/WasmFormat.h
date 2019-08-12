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

#include "B3Type.h"
#include "CodeLocation.h"
#include "Identifier.h"
#include "MacroAssemblerCodeRef.h"
#include "RegisterAtOffsetList.h"
#include "WasmMemoryInformation.h"
#include "WasmName.h"
#include "WasmNameSection.h"
#include "WasmOps.h"
#include "WasmPageCount.h"
#include "WasmSignature.h"
#include <limits>
#include <memory>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

namespace JSC {

namespace B3 {
class Compilation;
}

namespace Wasm {

struct CompilationContext;
struct ModuleInformation;

enum class TableElementType : uint8_t {
    Anyref,
    Funcref
};

inline bool isValueType(Type type)
{
    switch (type) {
    case I32:
    case I64:
    case F32:
    case F64:
        return true;
    case Anyref:
    case Funcref:
        return Options::useWebAssemblyReferences();
    default:
        break;
    }
    return false;
}

inline bool isSubtype(Type sub, Type parent)
{
    if (sub == parent)
        return true;
    return sub == Funcref && parent == Anyref;
}
    
enum class ExternalKind : uint8_t {
    // FIXME auto-generate this. https://bugs.webkit.org/show_bug.cgi?id=165231
    Function = 0,
    Table = 1,
    Memory = 2,
    Global = 3,
};

template<typename Int>
inline bool isValidExternalKind(Int val)
{
    switch (val) {
    case static_cast<Int>(ExternalKind::Function):
    case static_cast<Int>(ExternalKind::Table):
    case static_cast<Int>(ExternalKind::Memory):
    case static_cast<Int>(ExternalKind::Global):
        return true;
    }
    return false;
}

static_assert(static_cast<int>(ExternalKind::Function) == 0, "Wasm needs Function to have the value 0");
static_assert(static_cast<int>(ExternalKind::Table)    == 1, "Wasm needs Table to have the value 1");
static_assert(static_cast<int>(ExternalKind::Memory)   == 2, "Wasm needs Memory to have the value 2");
static_assert(static_cast<int>(ExternalKind::Global)   == 3, "Wasm needs Global to have the value 3");

inline const char* makeString(ExternalKind kind)
{
    switch (kind) {
    case ExternalKind::Function: return "function";
    case ExternalKind::Table: return "table";
    case ExternalKind::Memory: return "memory";
    case ExternalKind::Global: return "global";
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

struct Global {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    enum Mutability : uint8_t {
        // FIXME auto-generate this. https://bugs.webkit.org/show_bug.cgi?id=165231
        Mutable = 1,
        Immutable = 0
    };

    enum InitializationType {
        IsImport,
        FromGlobalImport,
        FromRefFunc,
        FromExpression
    };

    Mutability mutability;
    Type type;
    InitializationType initializationType { IsImport };
    uint64_t initialBitsOrImportNumber { 0 };
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
    uint32_t sizeInBytes;
    I32InitExpr offset;
    // Bytes are allocated at the end.
    uint8_t& byte(uint32_t pos)
    {
        ASSERT(pos < sizeInBytes);
        return *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(this) + sizeof(Segment) + pos);
    }
    static Segment* create(I32InitExpr, uint32_t);
    static void destroy(Segment*);
    typedef std::unique_ptr<Segment, decltype(&Segment::destroy)> Ptr;
    static Ptr adoptPtr(Segment*);
};

struct Element {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    Element(uint32_t tableIndex, I32InitExpr offset)
        : tableIndex(tableIndex)
        , offset(offset)
    { }

    uint32_t tableIndex;
    I32InitExpr offset;
    Vector<uint32_t> functionIndices;
};

class TableInformation {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TableInformation()
    {
        ASSERT(!*this);
    }

    TableInformation(uint32_t initial, Optional<uint32_t> maximum, bool isImport, TableElementType type)
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
    Optional<uint32_t> maximum() const { return m_maximum; }
    TableElementType type() const { return m_type; }
    Wasm::Type wasmType() const { return m_type == TableElementType::Funcref ? Type::Funcref : Type::Anyref; }

private:
    uint32_t m_initial;
    Optional<uint32_t> m_maximum;
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
    std::unique_ptr<B3::Compilation> compilation;
    RegisterAtOffsetList calleeSaveRegisters;
};

struct InternalFunction {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    CodeLocationDataLabelPtr<WasmEntryPtrTag> calleeMoveLocation;
    Entrypoint entrypoint;
};

// WebAssembly direct calls and call_indirect use indices into "function index space". This space starts
// with all imports, and then all internal functions. WasmToWasmImportableFunction and FunctionIndexSpace are only
// meant as fast lookup tables for these opcodes and do not own code.
struct WasmToWasmImportableFunction {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    using LoadLocation = MacroAssemblerCodePtr<WasmEntryPtrTag>*;
    static ptrdiff_t offsetOfSignatureIndex() { return OBJECT_OFFSETOF(WasmToWasmImportableFunction, signatureIndex); }
    static ptrdiff_t offsetOfEntrypointLoadLocation() { return OBJECT_OFFSETOF(WasmToWasmImportableFunction, entrypointLoadLocation); }

    // FIXME: Pack signature index and code pointer into one 64-bit value. See <https://bugs.webkit.org/show_bug.cgi?id=165511>.
    SignatureIndex signatureIndex { Signature::invalidIndex };
    LoadLocation entrypointLoadLocation;
};
using FunctionIndexSpace = Vector<WasmToWasmImportableFunction>;

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
