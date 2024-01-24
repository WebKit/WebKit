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

#include "config.h"
#include "StackFrame.h"

#include "CodeBlock.h"
#include "DebuggerPrimitives.h"
#include "JSCellInlines.h"
#include <wtf/text/StringConcatenateNumbers.h>

namespace JSC {

StackFrame::StackFrame(VM& vm, JSCell* owner, JSCell* callee)
    : m_callee(vm, owner, callee)
{
}

StackFrame::StackFrame(VM& vm, JSCell* owner, JSCell* callee, CodeBlock* codeBlock, BytecodeIndex bytecodeIndex)
    : m_callee(vm, owner, callee)
    , m_codeBlock(vm, owner, codeBlock)
    , m_bytecodeIndex(bytecodeIndex)
{
}

StackFrame::StackFrame(VM& vm, JSCell* owner, CodeBlock* codeBlock, BytecodeIndex bytecodeIndex)
    : m_codeBlock(vm, owner, codeBlock)
    , m_bytecodeIndex(bytecodeIndex)
{
}

StackFrame::StackFrame(Wasm::IndexOrName indexOrName)
    : m_wasmFunctionIndexOrName(indexOrName)
    , m_isWasmFrame(true)
{
}

SourceID StackFrame::sourceID() const
{
    if (!m_codeBlock)
        return noSourceID;
    return m_codeBlock->ownerExecutable()->sourceID();
}

static String processSourceURL(VM& vm, const JSC::StackFrame& frame, const String& sourceURL)
{
    if (vm.clientData && !sourceURL.startsWithIgnoringASCIICase("http"_s)) {
        String overrideURL = vm.clientData->overrideSourceURL(frame, sourceURL);
        if (!overrideURL.isNull())
            return overrideURL;
    }

    if (!sourceURL.isNull())
        return sourceURL;
    return emptyString();
}

String StackFrame::sourceURL(VM& vm) const
{
    if (m_isWasmFrame)
        return "[wasm code]"_s;

    if (!m_codeBlock)
        return "[native code]"_s;

    return processSourceURL(vm, *this, m_codeBlock->ownerExecutable()->sourceURL());
}

String StackFrame::sourceURLStripped(VM& vm) const
{
    if (m_isWasmFrame)
        return "[wasm code]"_s;

    if (!m_codeBlock)
        return "[native code]"_s;

    return processSourceURL(vm, *this, m_codeBlock->ownerExecutable()->sourceURLStripped());
}

String StackFrame::functionName(VM& vm) const
{
    if (m_isWasmFrame)
        return makeString(m_wasmFunctionIndexOrName);

    if (m_codeBlock) {
        switch (m_codeBlock->codeType()) {
        case EvalCode:
            return "eval code"_s;
        case ModuleCode:
            return "module code"_s;
        case FunctionCode:
            break;
        case GlobalCode:
            return "global code"_s;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    String name;
    if (m_callee) {
        if (m_callee->isObject())
            name = getCalculatedDisplayName(vm, jsCast<JSObject*>(m_callee.get())).impl();

        return name.isNull() ? emptyString() : name;
    }

    if (m_codeBlock) {
        if (auto* executable = jsDynamicCast<FunctionExecutable*>(m_codeBlock->ownerExecutable()))
            name = executable->ecmaName().impl();
    }

    return name.isNull() ? emptyString() : name;
}

LineColumn StackFrame::computeLineAndColumn() const
{
    if (!m_codeBlock)
        return { };

    auto lineColumn = m_codeBlock->lineColumnForBytecodeIndex(m_bytecodeIndex);

    ScriptExecutable* executable = m_codeBlock->ownerExecutable();
    if (std::optional<int> overrideLineNumber = executable->overrideLineNumber(m_codeBlock->vm()))
        lineColumn.line = overrideLineNumber.value();

    return lineColumn;
}

String StackFrame::toString(VM& vm) const
{
    String functionName = this->functionName(vm);
    String sourceURL = this->sourceURLStripped(vm);

    if (sourceURL.isEmpty() || !hasLineAndColumnInfo())
        return makeString(functionName, '@', sourceURL);

    auto lineColumn = computeLineAndColumn();
    return makeString(functionName, '@', sourceURL, ':', lineColumn.line, ':', lineColumn.column);
}

} // namespace JSC
