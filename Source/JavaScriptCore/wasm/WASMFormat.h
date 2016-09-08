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

using B3::Type;
using B3::Int32;
using B3::Int64;
using B3::Float;
using B3::Double;

static_assert(Int32 == 0, "WASM needs B3::Type::Int32 to have the value 0");
static_assert(Int64 == 1, "WASM needs B3::Type::Int64 to have the value 1");
static_assert(Float == 2, "WASM needs B3::Type::Float to have the value 2");
static_assert(Double == 3, "WASM needs B3::Type::Double to have the value 3");

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
    size_t start;
    size_t end;
};

} // namespace WASM

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
