/*
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
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
#include "StackVisitor.h"

#include "ClonedArguments.h"
#include "DebuggerPrimitives.h"
#include "ExecutableBaseInlines.h"
#include "InlineCallFrame.h"
#include "JSCInlines.h"
#include "NativeCallee.h"
#include "RegisterAtOffsetList.h"
#include "WasmCallee.h"
#include "WasmIndexOrName.h"
#include "WebAssemblyFunction.h"
#include <wtf/text/MakeString.h>

namespace JSC {

StackVisitor::StackVisitor(CallFrame* startFrame, VM& vm, bool skipFirstFrame)
{
    m_frame.m_index = 0;
    m_frame.m_isWasmFrame = false;
    m_frame.m_wasmDistanceFromDeepestInlineFrame = 0;
    CallFrame* topFrame;
    if (startFrame) {
        ASSERT(!vm.topCallFrame || static_cast<void*>(vm.topCallFrame) != vm.topEntryFrame);

        m_frame.m_entryFrame = vm.topEntryFrame;
        topFrame = vm.topCallFrame;

        if (topFrame && (skipFirstFrame || topFrame->isPartiallyInitializedFrame())) {
            topFrame = topFrame->callerFrame(m_frame.m_entryFrame);
            m_topEntryFrameIsEmpty = (m_frame.m_entryFrame != vm.topEntryFrame);
            if (startFrame == vm.topCallFrame)
                startFrame = topFrame;
        }

    } else {
        m_frame.m_entryFrame = nullptr;
        topFrame = nullptr;
    }
    m_frame.m_callerIsEntryFrame = false;
    readFrame(topFrame);

    // Find the frame the caller wants to start unwinding from.
    while (m_frame.callFrame() && m_frame.callFrame() != startFrame)
        gotoNextFrame();
}

void StackVisitor::gotoNextFrame()
{
    m_frame.m_index++;
#if ENABLE(DFG_JIT)
    if (m_frame.isInlinedDFGFrame()) {
        InlineCallFrame* inlineCallFrame = m_frame.inlineCallFrame();
        CodeOrigin* callerCodeOrigin = inlineCallFrame->getCallerSkippingTailCalls();
        if (!callerCodeOrigin) {
            while (inlineCallFrame) {
                readInlinedFrame(m_frame.callFrame(), &inlineCallFrame->directCaller);
                inlineCallFrame = m_frame.inlineCallFrame();
            }
            m_frame.m_entryFrame = m_frame.m_callerEntryFrame;
            readFrame(m_frame.callerFrame());
        } else
            readInlinedFrame(m_frame.callFrame(), callerCodeOrigin);
        return;
    }
#endif // ENABLE(DFG_JIT)
    m_frame.m_entryFrame = m_frame.m_callerEntryFrame;
    readFrame(m_frame.callerFrame());
}

void StackVisitor::unwindToMachineCodeBlockFrame()
{
#if ENABLE(DFG_JIT)
    if (m_frame.isInlinedDFGFrame()) {
        CodeOrigin codeOrigin = m_frame.inlineCallFrame()->directCaller;
        while (codeOrigin.inlineCallFrame())
            codeOrigin = codeOrigin.inlineCallFrame()->directCaller;
        readNonInlinedFrame(m_frame.callFrame(), &codeOrigin);
    }
#endif
}

void StackVisitor::readFrame(CallFrame* callFrame)
{
    if (!callFrame) {
        m_frame.setToEnd();
        return;
    }

    if (callFrame->isNativeCalleeFrame()) {
        readInlinableNativeCalleeFrame(callFrame);
        return;
    }

#if !ENABLE(DFG_JIT)
    readNonInlinedFrame(callFrame);

#else // !ENABLE(DFG_JIT)
    // If the frame doesn't have a code block, then it's not a DFG frame.
    // Hence, we're not at an inlined frame.
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (!codeBlock) {
        readNonInlinedFrame(callFrame);
        return;
    }

    // If the code block does not have any code origins, then there's no
    // inlining. Hence, we're not at an inlined frame.
    if (!codeBlock->hasCodeOrigins()) {
        readNonInlinedFrame(callFrame);
        return;
    }

    CallSiteIndex index = callFrame->callSiteIndex();
    ASSERT(codeBlock->canGetCodeOrigin(index));
    if (!codeBlock->canGetCodeOrigin(index)) {
        // See assertion above. In release builds, we try to protect ourselves
        // from crashing even though stack walking will be goofed up.
        m_frame.setToEnd();
        return;
    }

    CodeOrigin codeOrigin = codeBlock->codeOrigin(index);
    if (!codeOrigin.inlineCallFrame()) {
        readNonInlinedFrame(callFrame, &codeOrigin);
        return;
    }

    readInlinedFrame(callFrame, &codeOrigin);
#endif // !ENABLE(DFG_JIT)
}

void StackVisitor::readNonInlinedFrame(CallFrame* callFrame, CodeOrigin* codeOrigin)
{
    m_frame.m_callFrame = callFrame;
    m_frame.m_argumentCountIncludingThis = callFrame->argumentCountIncludingThis();
    m_frame.m_callerEntryFrame = m_frame.m_entryFrame;
    m_frame.m_callerFrame = callFrame->callerFrame(m_frame.m_callerEntryFrame);
    m_frame.m_callerIsEntryFrame = m_frame.m_callerEntryFrame != m_frame.m_entryFrame;
    m_frame.m_isWasmFrame = false;
    m_frame.m_callee = callFrame->callee();
#if ENABLE(DFG_JIT)
    m_frame.m_inlineDFGCallFrame = nullptr;
#endif
    m_frame.m_wasmDistanceFromDeepestInlineFrame = 0;

    m_frame.m_codeBlock = callFrame->isNativeCalleeFrame() ? nullptr : callFrame->codeBlock();
    m_frame.m_bytecodeIndex = !m_frame.codeBlock() ? BytecodeIndex(0)
        : codeOrigin ? codeOrigin->bytecodeIndex()
        : callFrame->bytecodeIndex();

    RELEASE_ASSERT(!callFrame->isNativeCalleeFrame());
}

void StackVisitor::readInlinableNativeCalleeFrame(CallFrame* callFrame)
{
    RELEASE_ASSERT(callFrame->callee().isNativeCallee());
    auto& callee = *callFrame->callee().asNativeCallee();
    switch (callee.category()) {
    case NativeCallee::Category::Wasm: {
#if ENABLE(WEBASSEMBLY)
        auto& wasmCallee = static_cast<Wasm::Callee&>(callee);
        auto depth = m_frame.m_wasmDistanceFromDeepestInlineFrame;
        m_frame.m_isWasmFrame = true;
        m_frame.m_callFrame = callFrame;
        m_frame.m_argumentCountIncludingThis = callFrame->argumentCountIncludingThis();
        m_frame.m_callerEntryFrame = m_frame.m_entryFrame;
        m_frame.m_callerFrame = callFrame->callerFrame(m_frame.m_callerEntryFrame);
        m_frame.m_callerIsEntryFrame = m_frame.m_callerEntryFrame != m_frame.m_entryFrame;
        m_frame.m_callee = callFrame->callee();
        m_frame.m_codeBlock = nullptr;
        m_frame.m_wasmDistanceFromDeepestInlineFrame = 0;

        m_frame.m_wasmFunctionIndexOrName = wasmCallee.indexOrName();

#if ENABLE(WEBASSEMBLY_OMGJIT)
        bool canInline = isAnyOMG(wasmCallee.compilationMode());
        if (!canInline)
            return;

        const auto& omgCallee = *static_cast<const Wasm::OptimizingJITCallee*>(&wasmCallee);
        bool isInlined = false;
        auto origin = omgCallee.getOrigin(callFrame->callSiteIndex().bits(), depth, isInlined);
        if (!isInlined)
            return;

        // The callerFrame just needs to be non-null to indicate that we
        // haven't reached the last frame yet.
        m_frame.m_callerFrame = callFrame;
        m_frame.m_wasmDistanceFromDeepestInlineFrame = depth + 1;
        m_frame.m_wasmFunctionIndexOrName = origin;
#else
        UNUSED_VARIABLE(depth);
#endif
#else
        UNUSED_PARAM(callFrame);
#endif
        break;
    }
    case NativeCallee::Category::InlineCache: {
        m_frame.m_callFrame = callFrame;
        m_frame.m_argumentCountIncludingThis = callFrame->argumentCountIncludingThis();
        m_frame.m_callerEntryFrame = m_frame.m_entryFrame;
        m_frame.m_callerFrame = callFrame->callerFrame(m_frame.m_callerEntryFrame);
        m_frame.m_callerIsEntryFrame = m_frame.m_callerEntryFrame != m_frame.m_entryFrame;
        m_frame.m_isWasmFrame = false;
        m_frame.m_callee = callFrame->callee();
#if ENABLE(DFG_JIT)
        m_frame.m_inlineDFGCallFrame = nullptr;
#endif
        m_frame.m_wasmDistanceFromDeepestInlineFrame = 0;

        m_frame.m_codeBlock = nullptr;
        m_frame.m_bytecodeIndex = BytecodeIndex(0);
        break;
    }
    }
}

#if ENABLE(DFG_JIT)
static int inlinedFrameOffset(CodeOrigin* codeOrigin)
{
    InlineCallFrame* inlineCallFrame = codeOrigin->inlineCallFrame();
    int frameOffset = inlineCallFrame ? inlineCallFrame->stackOffset : 0;
    return frameOffset;
}

void StackVisitor::readInlinedFrame(CallFrame* callFrame, CodeOrigin* codeOrigin)
{
    ASSERT(codeOrigin);
    m_frame.m_isWasmFrame = false;
    m_frame.m_wasmDistanceFromDeepestInlineFrame = 0;

    int frameOffset = inlinedFrameOffset(codeOrigin);
    bool isInlined = !!frameOffset;
    if (isInlined) {
        InlineCallFrame* inlineCallFrame = codeOrigin->inlineCallFrame();

        m_frame.m_callFrame = callFrame;
        m_frame.m_inlineDFGCallFrame = inlineCallFrame;
        if (inlineCallFrame->argumentCountRegister.isValid())
            m_frame.m_argumentCountIncludingThis = callFrame->r(inlineCallFrame->argumentCountRegister).unboxedInt32();
        else
            m_frame.m_argumentCountIncludingThis = inlineCallFrame->argumentCountIncludingThis;
        m_frame.m_codeBlock = inlineCallFrame->baselineCodeBlock.get();
        m_frame.m_bytecodeIndex = codeOrigin->bytecodeIndex();

        JSFunction* callee = inlineCallFrame->calleeForCallFrame(callFrame);
        m_frame.m_callee = callee;
        ASSERT(!!m_frame.callee().rawPtr());

        // The callerFrame just needs to be non-null to indicate that we
        // haven't reached the last frame yet. Setting it to the root
        // frame (i.e. the callFrame that this inlined frame is called from)
        // would work just fine.
        m_frame.m_callerFrame = callFrame;
        return;
    }

    readNonInlinedFrame(callFrame, codeOrigin);
}
#endif // ENABLE(DFG_JIT)

StackVisitor::Frame::CodeType StackVisitor::Frame::codeType() const
{
    if (isNativeCalleeFrame()) {
        auto* nativeCallee = callee().asNativeCallee();
        switch (nativeCallee->category()) {
        case NativeCallee::Category::Wasm:
            return CodeType::Wasm;
        case NativeCallee::Category::InlineCache:
            return CodeType::Native;
        }
        return CodeType::Native;
    }

    if (!codeBlock())
        return CodeType::Native;

    switch (codeBlock()->codeType()) {
    case EvalCode:
        return CodeType::Eval;
    case ModuleCode:
        return CodeType::Module;
    case FunctionCode:
        return CodeType::Function;
    case GlobalCode:
        return CodeType::Global;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return CodeType::Global;
}

#if ENABLE(ASSEMBLER)
std::optional<RegisterAtOffsetList> StackVisitor::Frame::calleeSaveRegistersForUnwinding()
{
    if (!NUMBER_OF_CALLEE_SAVES_REGISTERS)
        return std::nullopt;

    if (isInlinedDFGFrame())
        return std::nullopt;

    if (isNativeCalleeFrame()) {
        auto* nativeCallee = callee().asNativeCallee();
        switch (nativeCallee->category()) {
        case NativeCallee::Category::Wasm: {
#if ENABLE(WEBASSEMBLY)
            auto* wasmCallee = static_cast<Wasm::Callee*>(nativeCallee);
            if (auto* calleeSaveRegisters = wasmCallee->calleeSaveRegisters())
                return *calleeSaveRegisters;
#endif // ENABLE(WEBASSEMBLY)
            break;
        }
        case NativeCallee::Category::InlineCache: {
            break;
        }
        }
        return std::nullopt;
    }

    if (CodeBlock* codeBlock = this->codeBlock())
        return *codeBlock->jitCode()->calleeSaveRegisters();

    return std::nullopt;
}
#endif // ENABLE(ASSEMBLER)

String StackVisitor::Frame::functionName() const
{
    String traceLine;

    switch (codeType()) {
    case CodeType::Wasm:
        traceLine = makeString(m_wasmFunctionIndexOrName);
        break;
    case CodeType::Eval:
        traceLine = "eval code"_s;
        break;
    case CodeType::Module:
        traceLine = "module code"_s;
        break;
    case CodeType::Native: {
        JSCell* callee = this->callee().asCell();
        if (callee)
            traceLine = getCalculatedDisplayName(callFrame()->deprecatedVM(), jsCast<JSObject*>(callee)).impl();
        break;
    }
    case CodeType::Function: 
        traceLine = getCalculatedDisplayName(callFrame()->deprecatedVM(), jsCast<JSObject*>(this->callee().asCell())).impl();
        break;
    case CodeType::Global:
        traceLine = "global code"_s;
        break;
    }
    return traceLine.isNull() ? emptyString() : traceLine;
}

String StackVisitor::Frame::sourceURL() const
{
    String traceLine;

    switch (codeType()) {
    case CodeType::Eval:
    case CodeType::Module:
    case CodeType::Function:
    case CodeType::Global: {
        String sourceURL = codeBlock()->ownerExecutable()->sourceURL();
        if (!sourceURL.isEmpty())
            traceLine = sourceURL.impl();
        break;
    }
    case CodeType::Native:
        traceLine = "[native code]"_s;
        break;
    case CodeType::Wasm:
        traceLine = "[wasm code]"_s;
        break;
    }
    return traceLine.isNull() ? emptyString() : traceLine;
}

String StackVisitor::Frame::preRedirectURL() const
{
    String traceLine;

    switch (codeType()) {
    case CodeType::Eval:
    case CodeType::Module:
    case CodeType::Function:
    case CodeType::Global: {
        String preRedirectURL = codeBlock()->ownerExecutable()->preRedirectURL();
        if (!preRedirectURL.isEmpty())
            traceLine = preRedirectURL.impl();
        break;
    }
    case CodeType::Native:
    case CodeType::Wasm:
        break;
    }

    return traceLine.isNull() ? emptyString() : traceLine;
}

String StackVisitor::Frame::toString() const
{
    String functionName = this->functionName();
    String sourceURL = this->sourceURL();
    auto separator = !sourceURL.isEmpty() && !functionName.isEmpty() ? "@"_s : ""_s;

    if (sourceURL.isEmpty() || !hasLineAndColumnInfo())
        return makeString(functionName, separator, sourceURL);

    auto lineColumn = computeLineAndColumn();
    return makeString(functionName, separator, sourceURL, ':', lineColumn.line, ':', lineColumn.column);
}

SourceID StackVisitor::Frame::sourceID()
{
    if (CodeBlock* codeBlock = this->codeBlock())
        return codeBlock->ownerExecutable()->sourceID();
    return noSourceID;
}

ClonedArguments* StackVisitor::Frame::createArguments(VM& vm)
{
    ASSERT(m_callFrame);
    CallFrame* physicalFrame = m_callFrame;
    // FIXME: Revisit JSGlobalObject.
    // https://bugs.webkit.org/show_bug.cgi?id=203204
    JSGlobalObject* globalObject = physicalFrame->lexicalGlobalObject(vm);
    ClonedArguments* arguments;
    ArgumentsMode mode;
    if (Options::useFunctionDotArguments())
        mode = ArgumentsMode::Cloned;
    else
        mode = ArgumentsMode::FakeValues;
#if ENABLE(DFG_JIT)
    if (isInlinedDFGFrame()) {
        ASSERT(m_inlineDFGCallFrame);
        arguments = ClonedArguments::createWithInlineFrame(globalObject, physicalFrame, m_inlineDFGCallFrame, mode);
    } else 
#endif
        arguments = ClonedArguments::createWithMachineFrame(globalObject, physicalFrame, mode);
    return arguments;
}

bool StackVisitor::Frame::hasLineAndColumnInfo() const
{
    return !!codeBlock();
}

LineColumn StackVisitor::Frame::computeLineAndColumn() const
{
    CodeBlock* codeBlock = this->codeBlock();
    if (!codeBlock)
        return { };

    auto lineColumn = codeBlock->lineColumnForBytecodeIndex(bytecodeIndex());

    if (std::optional<int> overrideLineNumber = codeBlock->ownerExecutable()->overrideLineNumber(codeBlock->vm()))
        lineColumn.line = overrideLineNumber.value();

    return lineColumn;
}

void StackVisitor::Frame::setToEnd()
{
    m_callFrame = nullptr;
#if ENABLE(DFG_JIT)
    m_inlineDFGCallFrame = nullptr;
#endif
    m_isWasmFrame = false;
}

bool StackVisitor::Frame::isImplementationVisibilityPrivate() const
{
    ImplementationVisibility implementationVisibility = [&] () -> ImplementationVisibility {
        if (auto* codeBlock = this->codeBlock()) {
            if (auto* executable = codeBlock->ownerExecutable())
                return executable->implementationVisibility();
            return ImplementationVisibility::Public;
        }

#if ENABLE(WEBASSEMBLY)
        if (isNativeCalleeFrame())
            return callee().asNativeCallee()->implementationVisibility();
#endif

        if (callee().isCell()) {
            if (auto* callee = this->callee().asCell()) {
                if (auto* jsFunction = jsDynamicCast<JSFunction*>(callee)) {
                    if (auto* executable = jsFunction->executable())
                        return executable->implementationVisibility();
                    return ImplementationVisibility::Public;
                }
            }
        }

        return ImplementationVisibility::Public;
    }();
    switch (implementationVisibility) {
    case ImplementationVisibility::Public:
        return false;

    case ImplementationVisibility::Private:
    case ImplementationVisibility::PrivateRecursive:
        return !Options::showPrivateScriptsInStackTraces();
    }

    ASSERT_NOT_REACHED();
    return false;
}

void StackVisitor::Frame::dump(PrintStream& out, Indenter indent) const
{
    dump(out, indent, [] (PrintStream&) { });
}

void StackVisitor::Frame::dump(PrintStream& out, Indenter indent, WTF::Function<void(PrintStream&)> prefix) const
{
    if (!this->callFrame()) {
        out.print(indent, "frame 0x0\n");
        return;
    }

    CodeBlock* codeBlock = this->codeBlock();
    out.print(indent);
    prefix(out);
    out.print("frame ", RawPointer(this->callFrame()), " {\n");

    {
        indent++;

        CallFrame* callFrame = m_callFrame;
        CallFrame* callerFrame = this->callerFrame();
        const void* returnPC = callFrame->hasReturnPC() ? callFrame->returnPCForInspection() : nullptr;

        out.print(indent, "name: ", functionName(), "\n");
        out.print(indent, "sourceURL: ", sourceURL(), "\n");

        bool isInlined = false;
#if ENABLE(DFG_JIT)
        isInlined = isInlinedDFGFrame();
        out.print(indent, "isInlinedDFGFrame: ", isInlinedDFGFrame(), "\n");
        if (isInlinedDFGFrame())
            out.print(indent, "InlineCallFrame: ", RawPointer(m_inlineDFGCallFrame), "\n");
#endif

        out.print(indent, "callee: ", RawPointer(callee().rawPtr()), "\n");
        out.print(indent, "returnPC: ", RawPointer(returnPC), "\n");
        out.print(indent, "callerFrame: ", RawPointer(callerFrame), "\n");
        uintptr_t locationRawBits = callFrame->callSiteAsRawBits();
        out.print(indent, "rawLocationBits: ", locationRawBits,
            " ", RawHex(locationRawBits), "\n");
        out.print(indent, "codeBlock: ", RawPointer(codeBlock));
        if (codeBlock)
            out.print(" ", *codeBlock);
        out.print("\n");
        if (codeBlock && !isInlined) {
            indent++;

            if (callFrame->callSiteBitsAreBytecodeOffset()) {
                BytecodeIndex bytecodeIndex = callFrame->bytecodeIndex();
                out.print(indent, bytecodeIndex, " of ", codeBlock->instructions().size(), "\n");
#if ENABLE(DFG_JIT)
            } else {
                out.print(indent, "hasCodeOrigins: ", codeBlock->hasCodeOrigins(), "\n");
                if (codeBlock->hasCodeOrigins()) {
                    CallSiteIndex callSiteIndex = callFrame->callSiteIndex();
                    out.print(indent, "callSiteIndex: ", callSiteIndex.bits(), " of ", codeBlock->codeOrigins().size(), "\n");

                    JITType jitType = codeBlock->jitType();
                    if (jitType != JITType::FTLJIT) {
                        JITCode* jitCode = codeBlock->jitCode().get();
                        out.print(indent, "jitCode: ", RawPointer(jitCode),
                            " start ", RawPointer(jitCode->start()),
                            " end ", RawPointer(jitCode->end()), "\n");
                    }
                }
#endif
            }
            auto lineColumn = computeLineAndColumn();
            out.print(indent, "line: ", lineColumn.line, "\n");
            out.print(indent, "column: ", lineColumn.column, "\n");

            indent--;
        }
        out.print(indent, "EntryFrame: ", RawPointer(m_entryFrame), "\n");
        indent--;
    }
    out.print(indent, "}\n");
}

} // namespace JSC
