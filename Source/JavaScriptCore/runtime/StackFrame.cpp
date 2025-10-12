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
#include "FunctionExecutable.h"
#include "JSCellInlines.h"
#include "JSFunctionInlines.h"
#include <wtf/text/MakeString.h>

namespace JSC {

StackFrame::StackFrame(VM& vm, JSCell* owner, JSCell* callee)
    : m_frameData(JSFrameData {
        WriteBarrier<JSCell>(vm, owner, callee),
        WriteBarrier<CodeBlock>(),
        BytecodeIndex()
    })
{
}

StackFrame::StackFrame(VM& vm, JSCell* owner, JSCell* callee, CodeBlock* codeBlock, BytecodeIndex bytecodeIndex)
    : m_frameData(JSFrameData {
        WriteBarrier<JSCell>(vm, owner, callee),
        WriteBarrier<CodeBlock>(vm, owner, codeBlock),
        bytecodeIndex
    })
{
}

StackFrame::StackFrame(VM& vm, JSCell* owner, JSCell* callee, CodeBlock* codeBlock, BytecodeIndex bytecodeIndex, bool isAsyncFrame)
    : m_frameData(JSFrameData {
        WriteBarrier<JSCell>(vm, owner, callee),
        WriteBarrier<CodeBlock>(vm, owner, codeBlock),
        bytecodeIndex,
        isAsyncFrame
    })
{
}

StackFrame::StackFrame(VM& vm, JSCell* owner, CodeBlock* codeBlock, BytecodeIndex bytecodeIndex)
    : m_frameData(JSFrameData {
        WriteBarrier<JSCell>(),
        WriteBarrier<CodeBlock>(vm, owner, codeBlock),
        bytecodeIndex
    })
{
}

StackFrame::StackFrame(Wasm::IndexOrName indexOrName)
    : m_frameData(WasmFrameData { WTFMove(indexOrName), 0 })
{
}

StackFrame::StackFrame(Wasm::IndexOrName indexOrName, size_t functionIndex)
    : m_frameData(WasmFrameData { WTFMove(indexOrName), functionIndex })
{
}

StackFrame::StackFrame(VM& vm, JSCell* owner, JSCell* callee, bool isAsyncFrame)
    : m_frameData(JSFrameData {
        WriteBarrier<JSCell>(vm, owner, callee),
        WriteBarrier<CodeBlock>(),
        BytecodeIndex(),
        isAsyncFrame
    })
{
}

bool StackFrame::hasBytecodeIndex() const
{
    if (auto* jsFrame = std::get_if<JSFrameData>(&m_frameData))
        return !!jsFrame->bytecodeIndex;
    return false;
}

BytecodeIndex StackFrame::bytecodeIndex() const
{
    ASSERT(hasBytecodeIndex());
    return std::get<JSFrameData>(m_frameData).bytecodeIndex;
}

template<typename Visitor>
void StackFrame::visitAggregate(Visitor& visitor)
{
    WTF::switchOn(m_frameData,
        [&visitor](const JSFrameData& jsFrame) {
            if (jsFrame.callee)
                visitor.append(jsFrame.callee);
            if (jsFrame.codeBlock)
                visitor.append(jsFrame.codeBlock);
        },
        [](const WasmFrameData&) { }
    );
}
template void StackFrame::visitAggregate(AbstractSlotVisitor&);
template void StackFrame::visitAggregate(SlotVisitor&);

bool StackFrame::isMarked(VM& vm) const
{
    return WTF::switchOn(m_frameData,
        [&vm](const JSFrameData& jsFrame) {
            return (!jsFrame.callee || vm.heap.isMarked(jsFrame.callee.get())) && (!jsFrame.codeBlock || vm.heap.isMarked(jsFrame.codeBlock.get()));
        },
        [](const WasmFrameData&) { return true; }
    );
}

SourceID StackFrame::sourceID() const
{
    if (auto* jsFrame = std::get_if<JSFrameData>(&m_frameData)) {
        if (!jsFrame->codeBlock)
            return noSourceID;
        return jsFrame->codeBlock->ownerExecutable()->sourceID();
    }
    return noSourceID;
}

static String processSourceURL(VM& vm, const JSC::StackFrame& frame, const String& sourceURL)
{
    if (vm.clientData && (!protocolIsInHTTPFamily(sourceURL) && !protocolIs(sourceURL, "blob"_s))) {
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
    return WTF::switchOn(m_frameData,
        [&vm, this](const JSFrameData& jsFrame) -> String {
            if (isAsyncFrameWithoutCodeBlock()) {
                ASSERT(jsFrame.callee);
                ASSERT(!jsFrame.codeBlock);
                JSFunction* calleeFn = jsDynamicCast<JSFunction*>(jsFrame.callee.get());
                return processSourceURL(vm, *this, calleeFn->jsExecutable()->sourceURL());
            }

            if (!jsFrame.codeBlock)
                return "[native code]"_s;
            return processSourceURL(vm, *this, jsFrame.codeBlock->ownerExecutable()->sourceURL());
        },
        [](const WasmFrameData& wasmFrame) -> String {
            auto moduleName = wasmFrame.functionIndexOrName.moduleName();
            if (moduleName.empty())
                return makeString("wasm-function["_s, wasmFrame.functionIndex, ']');
            return makeString(moduleName, ":wasm-function["_s, wasmFrame.functionIndex, ']');
        }
    );
}

String StackFrame::sourceURLStripped(VM& vm) const
{
    return WTF::switchOn(m_frameData,
        [&vm, this](const JSFrameData& jsFrame) -> String {
            if (isAsyncFrameWithoutCodeBlock()) {
                ASSERT(jsFrame.callee);
                ASSERT(!jsFrame.codeBlock);
                JSFunction* calleeFn = jsDynamicCast<JSFunction*>(jsFrame.callee.get());
                return processSourceURL(vm, *this, calleeFn->jsExecutable()->sourceURLStripped());
            }

            if (!jsFrame.codeBlock)
                return "[native code]"_s;
            return processSourceURL(vm, *this, jsFrame.codeBlock->ownerExecutable()->sourceURLStripped());
        },
        [](const WasmFrameData& wasmFrame) -> String {
            auto moduleName = wasmFrame.functionIndexOrName.moduleName();
            if (moduleName.empty())
                return makeString("wasm-function["_s, wasmFrame.functionIndex, ']');
            return makeString(moduleName, ":wasm-function["_s, wasmFrame.functionIndex, ']');
        }
    );
}

String StackFrame::functionName(VM& vm) const
{
    return WTF::switchOn(m_frameData,
        [&vm](const JSFrameData& jsFrame) -> String {
            if (jsFrame.codeBlock) {
                switch (jsFrame.codeBlock->codeType()) {
                case EvalCode:
                    return "eval code"_s;
                case ModuleCode:
                    return "module code"_s;
                case GlobalCode:
                    return "global code"_s;
                case FunctionCode:
                    break;
                }
            }
            String name;
            if (jsFrame.callee && jsFrame.callee->isObject())
                name = getCalculatedDisplayName(vm, jsCast<JSObject*>(jsFrame.callee.get())).impl();
            else if (jsFrame.codeBlock) {
                if (auto* executable = jsDynamicCast<FunctionExecutable*>(jsFrame.codeBlock->ownerExecutable()))
                    name = executable->ecmaName().impl();
            }

            if (name.isNull())
                return emptyString();

            if (jsFrame.m_isAsyncFrame)
                return makeString("async "_s, name);

            return name;
        },
        [](const WasmFrameData& wasmFrame) -> String {
            if (wasmFrame.functionIndexOrName.isEmpty() || !wasmFrame.functionIndexOrName.nameSection())
                return "wasm-stub"_s;
            if (wasmFrame.functionIndexOrName.isIndex())
                return WTF::toString(wasmFrame.functionIndexOrName.index());
            return WTF::toString(wasmFrame.functionIndexOrName.name()->span());
        }
    );
}

LineColumn StackFrame::computeLineAndColumn() const
{
    if (auto* jsFrame = std::get_if<JSFrameData>(&m_frameData)) {
        if (!jsFrame->codeBlock)
            return { };
        auto lineColumn = jsFrame->codeBlock->lineColumnForBytecodeIndex(jsFrame->bytecodeIndex);

        ScriptExecutable* executable = jsFrame->codeBlock->ownerExecutable();
        if (std::optional<int> overrideLineNumber = executable->overrideLineNumber(jsFrame->codeBlock->vm()))
            lineColumn.line = overrideLineNumber.value();

        return lineColumn;
    }
    return { };
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
