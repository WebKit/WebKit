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
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class JSFunction;

namespace Wasm {

enum Type : uint8_t {
    Void,
    I32,
    I64,
    F32,
    F64,
    LastValueType = F64,
};

static_assert(I32 == 1, "Wasm needs I32 to have the value 1");
static_assert(I64 == 2, "Wasm needs I64 to have the value 2");
static_assert(F32 == 3, "Wasm needs F32 to have the value 3");
static_assert(F64 == 4, "Wasm needs F64 to have the value 4");

inline B3::Type toB3Type(Type type)
{
    switch (type) {
    case I32: return B3::Int32;
    case I64: return B3::Int64;
    case F32: return B3::Float;
    case F64: return B3::Double;
    case Void: return B3::Void;
    default: break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

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

const char* toString(Type);
    
struct External {
    enum Kind : uint8_t {
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
    String module;
    String field;
    External::Kind kind;
    union {
        Signature* functionSignature;
        // FIXME implement Table https://bugs.webkit.org/show_bug.cgi?id=164135
        // FIXME implement Memory https://bugs.webkit.org/show_bug.cgi?id=164134
        // FIXME implement Global https://bugs.webkit.org/show_bug.cgi?id=164133
    };
};

struct FunctionInformation {
    Signature* signature;
    size_t start;
    size_t end;
};

class Memory;

struct Export {
    String field;
    External::Kind kind;
    union {
        Signature* functionSignature;
        // FIXME implement Table https://bugs.webkit.org/show_bug.cgi?id=164135
        // FIXME implement Memory https://bugs.webkit.org/show_bug.cgi?id=164134
        // FIXME implement Global https://bugs.webkit.org/show_bug.cgi?id=164133
    };
};

struct ModuleInformation {
    Vector<Signature> signatures;
    Vector<Import> imports;
    Vector<FunctionInformation> functions;
    std::unique_ptr<Memory> memory;
    Vector<Export> exports;

    ~ModuleInformation();
};

struct FunctionCompilation;
typedef Vector<std::unique_ptr<FunctionCompilation>> CompiledFunctions;

struct UnlinkedCall {
    CodeLocationCall callLocation;
    size_t functionIndex;
};

struct FunctionCompilation {
    Vector<UnlinkedCall> unlinkedCalls;
    std::unique_ptr<B3::Compilation> code;
    std::unique_ptr<B3::Compilation> jsEntryPoint;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
