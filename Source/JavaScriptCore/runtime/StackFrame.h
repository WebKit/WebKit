/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "Strong.h"
#include "WasmIndexOrName.h"
#include <limits.h>

namespace JSC {

class CodeBlock;
class JSObject;

class StackFrame {
public:
    StackFrame()
        : m_bytecodeOffset(UINT_MAX)
    { }

    StackFrame(VM& vm, JSCell* callee)
        : m_callee(vm, callee)
        , m_bytecodeOffset(UINT_MAX)
    { }

    StackFrame(VM& vm, JSCell* callee, CodeBlock* codeBlock, unsigned bytecodeOffset)
        : m_callee(vm, callee)
        , m_codeBlock(vm, codeBlock)
        , m_bytecodeOffset(bytecodeOffset)
    { }

    static StackFrame wasm(Wasm::IndexOrName indexOrName)
    {
        StackFrame result;
        result.m_isWasmFrame = true;
        result.m_wasmFunctionIndexOrName = indexOrName;
        return result;
    }

    bool hasLineAndColumnInfo() const { return !!m_codeBlock; }
    
    void computeLineAndColumn(unsigned& line, unsigned& column) const;
    String functionName(VM&) const;
    intptr_t sourceID() const;
    String sourceURL() const;
    String toString(VM&) const;

    bool hasBytecodeOffset() const { return m_bytecodeOffset != UINT_MAX && !m_isWasmFrame; }
    unsigned bytecodeOffset()
    {
        ASSERT(hasBytecodeOffset());
        return m_bytecodeOffset;
    }


private:
    Strong<JSCell> m_callee { };
    Strong<CodeBlock> m_codeBlock { };
    union {
        unsigned m_bytecodeOffset;
        Wasm::IndexOrName m_wasmFunctionIndexOrName;
    };
    bool m_isWasmFrame { false };
};

} // namespace JSC

