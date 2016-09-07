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
 *
 * =========================================================================
 *
 * Copyright (c) 2015 by the repository authors of
 * WebAssembly/polyfill-prototype-1.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "B3Type.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class JSFunction;

namespace WASM {

enum Type : uint8_t {
    I32 = 1,
    I64,
    F32,
    F64,
    LastValueType = F64,
    Void
};

static_assert(I32 == 1, "WASM needs I32 to have the value 1");
static_assert(I64 == 2, "WASM needs I64 to have the value 2");
static_assert(F32 == 3, "WASM needs F32 to have the value 3");
static_assert(F64 == 4, "WASM needs F64 to have the value 4");

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


struct Signature {
    Type returnType;
    Vector<Type> arguments;
};

struct FunctionImport {
    String functionName;
};

struct FunctionImportSignature {
    uint32_t signatureIndex;
    uint32_t functionImportIndex;
};

struct FunctionDeclaration {
    uint32_t signatureIndex;
};

struct FunctionPointerTable {
    uint32_t signatureIndex;
    Vector<uint32_t> functionIndices;
    Vector<JSFunction*> functions;
};

struct FunctionInformation {
    Signature* signature;
    size_t start;
    size_t end;
};

} } // namespace JSC::WASM

#endif // ENABLE(WEBASSEMBLY)
