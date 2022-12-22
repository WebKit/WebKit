/*
 * Copyright (C) 2008-2022 Apple Inc. All Rights Reserved.
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
#include "CallFrame.h"

#include "CodeBlock.h"
#include "ExecutableAllocator.h"
#include "InlineCallFrame.h"
#include "JSCInlines.h"
#include "JSWebAssemblyInstance.h"
#include "LLIntPCRanges.h"
#include "VMEntryRecord.h"
#include "VMEntryScope.h"
#include "WasmContextInlines.h"
#include "WasmInstance.h"
#include <wtf/StringPrintStream.h>

namespace JSC {

bool CallFrame::callSiteBitsAreBytecodeOffset() const
{
    ASSERT(codeBlock());
    switch (codeBlock()->jitType()) {
    case JITType::InterpreterThunk:
    case JITType::BaselineJIT:
        return true;
    case JITType::None:
    case JITType::HostCallThunk:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    default:
        return false;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool CallFrame::callSiteBitsAreCodeOriginIndex() const
{
    ASSERT(codeBlock());
    switch (codeBlock()->jitType()) {
    case JITType::DFGJIT:
    case JITType::FTLJIT:
        return true;
    case JITType::None:
    case JITType::HostCallThunk:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    default:
        return false;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

unsigned CallFrame::callSiteAsRawBits() const
{
    return this[static_cast<int>(CallFrameSlot::argumentCountIncludingThis)].tag();
}

SUPPRESS_ASAN unsigned CallFrame::unsafeCallSiteAsRawBits() const
{
    return this[static_cast<int>(CallFrameSlot::argumentCountIncludingThis)].unsafeTag();
}

CallSiteIndex CallFrame::callSiteIndex() const
{
    return CallSiteIndex::fromBits(callSiteAsRawBits());
}

SUPPRESS_ASAN CallSiteIndex CallFrame::unsafeCallSiteIndex() const
{
    return CallSiteIndex::fromBits(unsafeCallSiteAsRawBits());
}

const JSInstruction* CallFrame::currentVPC() const
{
    ASSERT(callSiteBitsAreBytecodeOffset());
    return codeBlock()->instructions().at(callSiteBitsAsBytecodeOffset()).ptr();
}

void CallFrame::setCurrentVPC(const JSInstruction* vpc)
{
    CallSiteIndex callSite(codeBlock()->bytecodeIndex(vpc));
    this[static_cast<int>(CallFrameSlot::argumentCountIncludingThis)].tag() = callSite.bits();
    ASSERT(currentVPC() == vpc);
}

unsigned CallFrame::callSiteBitsAsBytecodeOffset() const
{
    ASSERT(codeBlock());
    ASSERT(callSiteBitsAreBytecodeOffset());
    return callSiteIndex().bits();
}

BytecodeIndex CallFrame::bytecodeIndex() const
{
    if (callee().isWasm())
        return callSiteIndex().bytecodeIndex();
    if (!codeBlock())
        return BytecodeIndex(0);
#if ENABLE(DFG_JIT)
    if (callSiteBitsAreCodeOriginIndex()) {
        ASSERT(codeBlock());
        CodeOrigin codeOrigin = this->codeOrigin();
        for (InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame(); inlineCallFrame;) {
            codeOrigin = inlineCallFrame->directCaller;
            inlineCallFrame = codeOrigin.inlineCallFrame();
        }
        return codeOrigin.bytecodeIndex();
    }
#endif
    ASSERT(callSiteBitsAreBytecodeOffset());
    return callSiteIndex().bytecodeIndex();
}

CodeOrigin CallFrame::codeOrigin() const
{
    if (!codeBlock())
        return CodeOrigin(BytecodeIndex(0));
#if ENABLE(DFG_JIT)
    if (callSiteBitsAreCodeOriginIndex()) {
        CallSiteIndex index = callSiteIndex();
        ASSERT(codeBlock()->canGetCodeOrigin(index));
        return codeBlock()->codeOrigin(index);
    }
#endif
    return CodeOrigin(callSiteIndex().bytecodeIndex());
}

Register* CallFrame::topOfFrameInternal()
{
    CodeBlock* codeBlock = this->codeBlock();
    ASSERT(codeBlock);
    return registers() + codeBlock->stackPointerOffset();
}

bool CallFrame::isAnyWasmCallee() const
{
    CalleeBits callee = this->callee();
    if (callee.isWasm())
        return true;

    ASSERT(callee.isCell());
    if (!!callee.rawPtr() && isWebAssemblyModule(callee.asCell()))
        return true;

    return false;
}

CallFrame* CallFrame::callerFrame(EntryFrame*& currEntryFrame) const
{
    if (callerFrameOrEntryFrame() == currEntryFrame) {
        VMEntryRecord* currVMEntryRecord = vmEntryRecord(currEntryFrame);
        currEntryFrame = currVMEntryRecord->prevTopEntryFrame();
        return currVMEntryRecord->prevTopCallFrame();
    }
    return static_cast<CallFrame*>(callerFrameOrEntryFrame());
}

SUPPRESS_ASAN CallFrame* CallFrame::unsafeCallerFrame(EntryFrame*& currEntryFrame) const
{
    if (unsafeCallerFrameOrEntryFrame() == currEntryFrame) {
        VMEntryRecord* currVMEntryRecord = vmEntryRecord(currEntryFrame);
        currEntryFrame = currVMEntryRecord->unsafePrevTopEntryFrame();
        return currVMEntryRecord->unsafePrevTopCallFrame();
    }
    return static_cast<CallFrame*>(unsafeCallerFrameOrEntryFrame());
}

SourceOrigin CallFrame::callerSourceOrigin(VM& vm)
{
    RELEASE_ASSERT(callee().isCell());
    SourceOrigin sourceOrigin;
    bool haveSkippedFirstFrame = false;
    StackVisitor::visit(this, vm, [&](StackVisitor& visitor) {
        if (!std::exchange(haveSkippedFirstFrame, true))
            return IterationStatus::Continue;

        switch (visitor->codeType()) {
        case StackVisitor::Frame::CodeType::Function:
            // Skip the builtin functions since they should not pass the source origin to the dynamic code generation calls.
            // Consider the following code.
            //
            // [ "42 + 44" ].forEach(eval);
            //
            // In the above case, the eval function will be interpreted as the indirect call to eval inside forEach function.
            // At that time, the generated eval code should have the source origin to the original caller of the forEach function
            // instead of the source origin of the forEach function.
            if (static_cast<FunctionExecutable*>(visitor->codeBlock()->ownerExecutable())->isBuiltinFunction())
                return IterationStatus::Continue;
            FALLTHROUGH;

        case StackVisitor::Frame::CodeType::Eval:
        case StackVisitor::Frame::CodeType::Module:
        case StackVisitor::Frame::CodeType::Global:
            sourceOrigin = visitor->codeBlock()->ownerExecutable()->sourceOrigin();
            return IterationStatus::Done;

        case StackVisitor::Frame::CodeType::Native:
            return IterationStatus::Continue;

        case StackVisitor::Frame::CodeType::Wasm:
            // FIXME: Should return the source origin for WASM.
            return IterationStatus::Done;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return IterationStatus::Done;
    });
    return sourceOrigin;
}

JSGlobalObject* CallFrame::globalObjectOfClosestCodeBlock(VM& vm, CallFrame* callFrame)
{
    // FIXME: We need to handle JSONP interpretation case in ProgramExecutable since it does not have vm.topCallFrame.
    // rdar://83691438
    JSGlobalObject* globalObject = nullptr;
    StackVisitor::visit(callFrame, vm, [&](StackVisitor& visitor) {
        if (visitor->isWasmFrame()) {
            globalObject = visitor->callFrame()->lexicalGlobalObject(vm);
            return IterationStatus::Done;
        }
        if (auto* codeBlock = visitor->codeBlock()) {
            if (codeBlock->codeType() == CodeType::FunctionCode && static_cast<FunctionExecutable*>(codeBlock->ownerExecutable())->isBuiltinFunction())
                return IterationStatus::Continue;
            globalObject = codeBlock->globalObject();
            return IterationStatus::Done;
        }
        ASSERT(visitor->codeType() == StackVisitor::Frame::CodeType::Native);
        return IterationStatus::Continue;
    });
    if (globalObject)
        return globalObject;
    if (vm.entryScope)
        return vm.entryScope->globalObject();
    return nullptr;
}

String CallFrame::friendlyFunctionName()
{
    if (this->isAnyWasmCallee())
        return emptyString();

    CodeBlock* codeBlock = this->codeBlock();
    if (!codeBlock)
        return emptyString();

    switch (codeBlock->codeType()) {
    case EvalCode:
        return "eval code"_s;
    case ModuleCode:
        return "module code"_s;
    case GlobalCode:
        return "global code"_s;
    case FunctionCode:
        if (jsCallee())
            return getCalculatedDisplayName(codeBlock->vm(), jsCallee());
        return emptyString();
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

void CallFrame::dump(PrintStream& out) const
{
    if (!this->isAnyWasmCallee()) {
        if (CodeBlock* codeBlock = this->codeBlock()) {
            out.print(codeBlock->inferredName(), "#", codeBlock->hashAsStringIfPossible(), " [", codeBlock->jitType(), " ", bytecodeIndex(), "]");

            out.print("(");
            thisValue().dumpForBacktrace(out);

            for (size_t i = 0; i < argumentCount(); ++i) {
                out.print(", ");
                JSValue value = argument(i);
                value.dumpForBacktrace(out);
            }

            out.print(")");

            return;
        }
    }

    out.print(RawPointer(returnPCForInspection()));
}

const char* CallFrame::describeFrame()
{
    constexpr size_t bufferSize = 200;
    static char* buffer = nullptr;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        buffer = static_cast<char*>(fastZeroedMalloc(bufferSize + 1));
    });

    WTF::StringPrintStream stringStream;

    dump(stringStream);

    strncpy(buffer, stringStream.toCString().data(), bufferSize);
    buffer[bufferSize] = '\0';

    return buffer;
}

void CallFrame::convertToStackOverflowFrame(VM& vm, CodeBlock* codeBlockToKeepAliveUntilFrameIsUnwound)
{
    ASSERT(!isEmptyTopLevelCallFrameForDebugger());
    ASSERT(codeBlockToKeepAliveUntilFrameIsUnwound->inherits<CodeBlock>());

    EntryFrame* entryFrame = vm.topEntryFrame;
    CallFrame* throwOriginFrame = this;
    do {
        throwOriginFrame = throwOriginFrame->callerFrame(entryFrame);
    } while (throwOriginFrame && throwOriginFrame->callee().isWasm());

    JSObject* originCallee = throwOriginFrame ? throwOriginFrame->jsCallee() : vmEntryRecord(vm.topEntryFrame)->callee();
    JSObject* stackOverflowCallee = originCallee->globalObject()->stackOverflowFrameCallee();

    setCodeBlock(codeBlockToKeepAliveUntilFrameIsUnwound);
    setCallee(stackOverflowCallee);
    setArgumentCountIncludingThis(0);
}

#if ENABLE(WEBASSEMBLY)
JSGlobalObject* CallFrame::lexicalGlobalObjectFromWasmCallee(VM& vm) const
{
    return vm.wasmContext.load()->owner<JSWebAssemblyInstance>()->globalObject();
}
#endif

bool isFromJSCode(void* returnAddress)
{
    UNUSED_PARAM(returnAddress);
#if ENABLE(JIT)
    if (isJITPC(returnAddress))
        return true;
#endif
#if ENABLE(C_LOOP)
    return true;
#else
    return LLInt::isLLIntPC(returnAddress);
#endif
}

} // namespace JSC
