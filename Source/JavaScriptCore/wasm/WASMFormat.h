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

#ifndef WASMFormat_h
#define WASMFormat_h

#if ENABLE(WEBASSEMBLY)

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class JSFunction;

enum class WASMType : uint8_t {
    I32,
    F32,
    F64,
    NumberOfTypes
};

enum class WASMExpressionType : uint8_t {
    I32,
    F32,
    F64,
    Void,
    NumberOfExpressionTypes
};

struct WASMSignature {
    WASMExpressionType returnType;
    Vector<WASMType> arguments;
};

struct WASMFunctionImport {
    String functionName;
};

struct WASMFunctionImportSignature {
    uint32_t signatureIndex;
    uint32_t functionImportIndex;
};

struct WASMFunctionDeclaration {
    uint32_t signatureIndex;
};

struct WASMFunctionPointerTable {
    uint32_t signatureIndex;
    Vector<uint32_t> functionIndices;
    Vector<JSFunction*> functions;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // WASMFormat_h
