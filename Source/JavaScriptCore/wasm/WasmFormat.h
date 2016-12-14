/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "B3Compilation.h"
#include "B3Type.h"
#include "CodeLocation.h"
#include "Identifier.h"
#include "MacroAssemblerCodeRef.h"
#include "RegisterAtOffsetList.h"
#include "WasmMemoryInformation.h"
#include "WasmOps.h"
#include "WasmPageCount.h"
#include <memory>
#include <wtf/FastMalloc.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

namespace JSC {

class JSFunction;

namespace Wasm {

inline bool isValueType(Type type)
{
    switch (type) {
    case I32:
    case I64:
    case F32:
    case F64:
        return true;
    default:
        break;
    }
    return false;
}
    
struct External {
    enum Kind : uint8_t {
        // FIXME auto-generate this. https://bugs.webkit.org/show_bug.cgi?id=165231
        Function = 0,
        Table = 1,
        Memory = 2,
        Global = 3,
    };
    template<typename Int>
    static bool isValid(Int val)
    {
        switch (val) {
        case Function:
        case Table:
        case Memory:
        case Global:
            return true;
        default:
            return false;
        }
    }
    
    static_assert(Function == 0, "Wasm needs Function to have the value 0");
    static_assert(Table    == 1, "Wasm needs Table to have the value 1");
    static_assert(Memory   == 2, "Wasm needs Memory to have the value 2");
    static_assert(Global   == 3, "Wasm needs Global to have the value 3");
};

struct Signature {
    Type returnType;
    Vector<Type> arguments;
};
    
struct Import {
    Identifier module;
    Identifier field;
    External::Kind kind;
    unsigned kindIndex; // Index in the vector of the corresponding kind.
};

struct Export {
    Identifier field;
    External::Kind kind;
    union {
        uint32_t functionIndex;
        // FIXME implement Table https://bugs.webkit.org/show_bug.cgi?id=165782
        // FIXME implement Memory https://bugs.webkit.org/show_bug.cgi?id=165671
        // FIXME implement Global https://bugs.webkit.org/show_bug.cgi?id=164133
    };
};

struct FunctionLocationInBinary {
    size_t start;
    size_t end;
};

struct Segment {
    uint32_t offset;
    uint32_t sizeInBytes;
    // Bytes are allocated at the end.
    static Segment* make(uint32_t offset, uint32_t sizeInBytes)
    {
        auto allocated = tryFastCalloc(sizeof(Segment) + sizeInBytes, 1);
        Segment* segment;
        if (!allocated.getValue(segment))
            return nullptr;
        segment->offset = offset;
        segment->sizeInBytes = sizeInBytes;
        return segment;
    }
    static void destroy(Segment *segment)
    {
        fastFree(segment);
    }
    uint8_t& byte(uint32_t pos)
    {
        ASSERT(pos < sizeInBytes);
        return *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(this) + sizeof(offset) + sizeof(sizeInBytes) + pos);
    }
    typedef std::unique_ptr<Segment, decltype(&Segment::destroy)> Ptr;
    static Ptr makePtr(Segment* segment)
    {
        return Ptr(segment, &Segment::destroy);
    }
};

struct Element {
    uint32_t offset;
    Vector<uint32_t> functionIndices;
};

class TableInformation {
public:
    TableInformation()
    {
        ASSERT(!*this);
    }

    TableInformation(uint32_t initial, std::optional<uint32_t> maximum, bool isImport)
        : m_initial(initial)
        , m_maximum(maximum)
        , m_isImport(isImport)
        , m_isValid(true)
    {
        ASSERT(*this);
    }

    explicit operator bool() const { return m_isValid; }
    bool isImport() const { return m_isImport; }
    uint32_t initial() const { return m_initial; }
    std::optional<uint32_t> maximum() const { return m_maximum; }

private:
    uint32_t m_initial;
    std::optional<uint32_t> m_maximum;
    bool m_isImport { false };
    bool m_isValid { false };
};

struct ModuleInformation {
    Vector<Signature> signatures;
    Vector<Import> imports;
    Vector<Signature*> importFunctions;
    // FIXME implement import Global https://bugs.webkit.org/show_bug.cgi?id=164133
    Vector<Signature*> internalFunctionSignatures;
    MemoryInformation memory;
    Vector<Export> exports;
    std::optional<uint32_t> startFunctionIndexSpace;
    Vector<Segment::Ptr> data;
    Vector<Element> elements;
    TableInformation tableInformation;

    ~ModuleInformation();
};

struct UnlinkedWasmToWasmCall {
    CodeLocationCall callLocation;
    size_t functionIndex;
};

struct Entrypoint {
    std::unique_ptr<B3::Compilation> compilation;
    RegisterAtOffsetList calleeSaveRegisters;
};

struct WasmInternalFunction {
    CodeLocationDataLabelPtr wasmCalleeMoveLocation;
    CodeLocationDataLabelPtr jsToWasmCalleeMoveLocation;

    Entrypoint wasmEntrypoint;
    Entrypoint jsToWasmEntrypoint;
};

typedef MacroAssemblerCodeRef WasmToJSStub;

// WebAssembly direct calls and call_indirect use indices into "function index space". This space starts with all imports, and then all internal functions.
// CallableFunction and FunctionIndexSpace are only meant as fast lookup tables for these opcodes, and do not own code.
struct CallableFunction {
    CallableFunction() = default;

    CallableFunction(Signature* signature, void* code = nullptr)
        : signature(signature)
        , code(code)
    {
    }

    // FIXME pack this inside a (uniqued) integer (for correctness the parser should unique Signatures),
    // and then pack that integer into the code pointer. https://bugs.webkit.org/show_bug.cgi?id=165511
    Signature* signature { nullptr }; 
    void* code { nullptr };
};
typedef Vector<CallableFunction> FunctionIndexSpace;


struct ImmutableFunctionIndexSpace {
    MallocPtr<CallableFunction> buffer;
    size_t size;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
