/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/BytecodeIndex.h>
#include <JavaScriptCore/Heap.h>
#include <JavaScriptCore/LineColumn.h>
#include <JavaScriptCore/SlotVisitorMacros.h>
#include <JavaScriptCore/VM.h>
#include <JavaScriptCore/WasmIndexOrName.h>
#include <JavaScriptCore/WriteBarrier.h>
#include <limits.h>
#include <wtf/Variant.h>

namespace JSC {

class CodeBlock;
class JSObject;

struct JSFrameData {
    WriteBarrier<JSCell> callee;
    WriteBarrier<CodeBlock> codeBlock;
    BytecodeIndex bytecodeIndex;
    bool m_isAsyncFrame { false };
};

struct WasmFrameData {
    Wasm::IndexOrName functionIndexOrName;
    size_t functionIndex { 0 };
};

class StackFrame {
public:
    using FrameData = Variant<JSFrameData, WasmFrameData>;

    StackFrame(VM&, JSCell* owner, JSCell* callee);
    StackFrame(VM&, JSCell* owner, JSCell* callee, CodeBlock*, BytecodeIndex);
    StackFrame(VM&, JSCell* owner, JSCell* callee, CodeBlock*, BytecodeIndex, bool isAsyncFrame);
    StackFrame(VM&, JSCell* owner, CodeBlock*, BytecodeIndex);
    StackFrame(VM&, JSCell* owner, JSCell* callee, bool isAsyncFrame);
    StackFrame(Wasm::IndexOrName);
    StackFrame(Wasm::IndexOrName, size_t functionIndex);
    StackFrame() = default;

    bool hasLineAndColumnInfo() const
    {
        if (auto* jsFrame = std::get_if<JSFrameData>(&m_frameData))
            return !!jsFrame->codeBlock;
        return false;
    }

    CodeBlock* codeBlock() const
    {
        if (auto* jsFrame = std::get_if<JSFrameData>(&m_frameData))
            return jsFrame->codeBlock.get();
        return nullptr;
    }

    bool isAsyncFrameWithoutCodeBlock() const
    {
        if (auto* jsFrame = std::get_if<JSFrameData>(&m_frameData))
            return jsFrame->m_isAsyncFrame && !codeBlock();
        return false;
    }

    LineColumn computeLineAndColumn() const;
    String functionName(VM&) const;
    SourceID sourceID() const;
    String sourceURL(VM&) const;
    String sourceURLStripped(VM&) const;
    String toString(VM&) const;

    bool hasBytecodeIndex() const;
    BytecodeIndex bytecodeIndex() const;

    template<typename Visitor>
    void visitAggregate(Visitor&);

    bool isMarked(VM&) const;

private:
    FrameData m_frameData { JSFrameData { } };
};

} // namespace JSC
