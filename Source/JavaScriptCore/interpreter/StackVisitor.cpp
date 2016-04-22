/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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
#include "Executable.h"
#include "InlineCallFrame.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include <wtf/DataLog.h>

namespace JSC {

StackVisitor::StackVisitor(CallFrame* startFrame)
{
    m_frame.m_index = 0;
    CallFrame* topFrame;
    if (startFrame) {
        m_frame.m_VMEntryFrame = startFrame->vm().topVMEntryFrame;
        topFrame = startFrame->vm().topCallFrame;
    } else {
        m_frame.m_VMEntryFrame = 0;
        topFrame = 0;
    }
    m_frame.m_callerIsVMEntryFrame = false;
    readFrame(topFrame);

    // Find the frame the caller wants to start unwinding from.
    while (m_frame.callFrame() && m_frame.callFrame() != startFrame)
        gotoNextFrame();
}

void StackVisitor::gotoNextFrame()
{
#if ENABLE(DFG_JIT)
    if (m_frame.isInlinedFrame()) {
        InlineCallFrame* inlineCallFrame = m_frame.inlineCallFrame();
        CodeOrigin* callerCodeOrigin = inlineCallFrame->getCallerSkippingTailCalls();
        if (!callerCodeOrigin) {
            while (inlineCallFrame) {
                readInlinedFrame(m_frame.callFrame(), &inlineCallFrame->directCaller);
                inlineCallFrame = m_frame.inlineCallFrame();
            }
            m_frame.m_VMEntryFrame = m_frame.m_CallerVMEntryFrame;
            readFrame(m_frame.callerFrame());
        } else
            readInlinedFrame(m_frame.callFrame(), callerCodeOrigin);
        return;
    }
#endif // ENABLE(DFG_JIT)
    m_frame.m_VMEntryFrame = m_frame.m_CallerVMEntryFrame;
    readFrame(m_frame.callerFrame());
}

void StackVisitor::unwindToMachineCodeBlockFrame()
{
#if ENABLE(DFG_JIT)
    while (m_frame.isInlinedFrame())
        gotoNextFrame();
#endif
}

void StackVisitor::readFrame(CallFrame* callFrame)
{
    if (!callFrame) {
        m_frame.setToEnd();
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
    if (!codeOrigin.inlineCallFrame) {
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
    m_frame.m_CallerVMEntryFrame = m_frame.m_VMEntryFrame;
    m_frame.m_callerFrame = callFrame->callerFrame(m_frame.m_CallerVMEntryFrame);
    m_frame.m_callerIsVMEntryFrame = m_frame.m_CallerVMEntryFrame != m_frame.m_VMEntryFrame;
    m_frame.m_callee = callFrame->callee();
    m_frame.m_codeBlock = callFrame->codeBlock();
    m_frame.m_bytecodeOffset = !m_frame.codeBlock() ? 0
        : codeOrigin ? codeOrigin->bytecodeIndex
        : callFrame->bytecodeOffset();
#if ENABLE(DFG_JIT)
    m_frame.m_inlineCallFrame = 0;
#endif
}

#if ENABLE(DFG_JIT)
static int inlinedFrameOffset(CodeOrigin* codeOrigin)
{
    InlineCallFrame* inlineCallFrame = codeOrigin->inlineCallFrame;
    int frameOffset = inlineCallFrame ? inlineCallFrame->stackOffset : 0;
    return frameOffset;
}

void StackVisitor::readInlinedFrame(CallFrame* callFrame, CodeOrigin* codeOrigin)
{
    ASSERT(codeOrigin);

    int frameOffset = inlinedFrameOffset(codeOrigin);
    bool isInlined = !!frameOffset;
    if (isInlined) {
        InlineCallFrame* inlineCallFrame = codeOrigin->inlineCallFrame;

        m_frame.m_callFrame = callFrame;
        m_frame.m_inlineCallFrame = inlineCallFrame;
        if (inlineCallFrame->argumentCountRegister.isValid())
            m_frame.m_argumentCountIncludingThis = callFrame->r(inlineCallFrame->argumentCountRegister.offset()).unboxedInt32();
        else
            m_frame.m_argumentCountIncludingThis = inlineCallFrame->arguments.size();
        m_frame.m_codeBlock = inlineCallFrame->baselineCodeBlock.get();
        m_frame.m_bytecodeOffset = codeOrigin->bytecodeIndex;

        JSFunction* callee = inlineCallFrame->calleeForCallFrame(callFrame);
        m_frame.m_callee = callee;
        ASSERT(m_frame.callee());

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
    if (!isJSFrame())
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

String StackVisitor::Frame::functionName()
{
    String traceLine;
    JSObject* callee = this->callee();

    switch (codeType()) {
    case CodeType::Eval:
        traceLine = ASCIILiteral("eval code");
        break;
    case CodeType::Module:
        traceLine = ASCIILiteral("module code");
        break;
    case CodeType::Native:
        if (callee)
            traceLine = getCalculatedDisplayName(callFrame(), callee).impl();
        break;
    case CodeType::Function:
        traceLine = getCalculatedDisplayName(callFrame(), callee).impl();
        break;
    case CodeType::Global:
        traceLine = ASCIILiteral("global code");
        break;
    }
    return traceLine.isNull() ? emptyString() : traceLine;
}

String StackVisitor::Frame::sourceURL()
{
    String traceLine;

    switch (codeType()) {
    case CodeType::Eval:
    case CodeType::Module:
    case CodeType::Function:
    case CodeType::Global: {
        String sourceURL = codeBlock()->ownerScriptExecutable()->sourceURL();
        if (!sourceURL.isEmpty())
            traceLine = sourceURL.impl();
        break;
    }
    case CodeType::Native:
        traceLine = ASCIILiteral("[native code]");
        break;
    }
    return traceLine.isNull() ? emptyString() : traceLine;
}

String StackVisitor::Frame::toString()
{
    StringBuilder traceBuild;
    String functionName = this->functionName();
    String sourceURL = this->sourceURL();
    traceBuild.append(functionName);
    if (!sourceURL.isEmpty()) {
        if (!functionName.isEmpty())
            traceBuild.append('@');
        traceBuild.append(sourceURL);
        if (isJSFrame()) {
            unsigned line = 0;
            unsigned column = 0;
            computeLineAndColumn(line, column);
            traceBuild.append(':');
            traceBuild.appendNumber(line);
            traceBuild.append(':');
            traceBuild.appendNumber(column);
        }
    }
    return traceBuild.toString().impl();
}

intptr_t StackVisitor::Frame::sourceID()
{
    if (CodeBlock* codeBlock = this->codeBlock())
        return codeBlock->ownerScriptExecutable()->sourceID();
    return noSourceID;
}

ClonedArguments* StackVisitor::Frame::createArguments()
{
    ASSERT(m_callFrame);
    CallFrame* physicalFrame = m_callFrame;
    ClonedArguments* arguments;
    ArgumentsMode mode;
    if (Options::useFunctionDotArguments())
        mode = ArgumentsMode::Cloned;
    else
        mode = ArgumentsMode::FakeValues;
#if ENABLE(DFG_JIT)
    if (isInlinedFrame()) {
        ASSERT(m_inlineCallFrame);
        arguments = ClonedArguments::createWithInlineFrame(physicalFrame, physicalFrame, m_inlineCallFrame, mode);
    } else 
#endif
        arguments = ClonedArguments::createWithMachineFrame(physicalFrame, physicalFrame, mode);
    return arguments;
}

void StackVisitor::Frame::computeLineAndColumn(unsigned& line, unsigned& column)
{
    CodeBlock* codeBlock = this->codeBlock();
    if (!codeBlock) {
        line = 0;
        column = 0;
        return;
    }

    int divot = 0;
    int unusedStartOffset = 0;
    int unusedEndOffset = 0;
    unsigned divotLine = 0;
    unsigned divotColumn = 0;
    retrieveExpressionInfo(divot, unusedStartOffset, unusedEndOffset, divotLine, divotColumn);

    line = divotLine + codeBlock->ownerScriptExecutable()->firstLine();
    column = divotColumn + (divotLine ? 1 : codeBlock->firstLineColumnOffset());

    if (codeBlock->ownerScriptExecutable()->hasOverrideLineNumber())
        line = codeBlock->ownerScriptExecutable()->overrideLineNumber();
}

void StackVisitor::Frame::retrieveExpressionInfo(int& divot, int& startOffset, int& endOffset, unsigned& line, unsigned& column)
{
    CodeBlock* codeBlock = this->codeBlock();
    codeBlock->unlinkedCodeBlock()->expressionRangeForBytecodeOffset(bytecodeOffset(), divot, startOffset, endOffset, line, column);
    divot += codeBlock->sourceOffset();
}

void StackVisitor::Frame::setToEnd()
{
    m_callFrame = 0;
#if ENABLE(DFG_JIT)
    m_inlineCallFrame = 0;
#endif
}

static void printIndents(int levels)
{
    while (levels--)
        dataLogFString("   ");
}

template<typename... Types>
void log(unsigned indent, const Types&... values)
{
    printIndents(indent);
    dataLog(values...);
}

template<typename... Types>
void logF(unsigned indent, const char* format, const Types&... values)
{
    printIndents(indent);

#if COMPILER(GCC_OR_CLANG)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wmissing-format-attribute"
#endif

    dataLogF(format, values...);

#if COMPILER(GCC_OR_CLANG)
#pragma GCC diagnostic pop
#endif
}

void StackVisitor::Frame::print(int indent)
{
    if (!this->callFrame()) {
        log(indent, "frame 0x0\n");
        return;
    }

    CodeBlock* codeBlock = this->codeBlock();
    logF(indent, "frame %p {\n", this->callFrame());

    {
        indent++;

        CallFrame* callFrame = m_callFrame;
        CallFrame* callerFrame = this->callerFrame();
        void* returnPC = callFrame->hasReturnPC() ? callFrame->returnPC().value() : nullptr;

        log(indent, "name: ", functionName(), "\n");
        log(indent, "sourceURL: ", sourceURL(), "\n");

        bool isInlined = false;
#if ENABLE(DFG_JIT)
        isInlined = isInlinedFrame();
        log(indent, "isInlinedFrame: ", isInlinedFrame(), "\n");
        if (isInlinedFrame())
            logF(indent, "InlineCallFrame: %p\n", m_inlineCallFrame);
#endif

        logF(indent, "callee: %p\n", callee());
        logF(indent, "returnPC: %p\n", returnPC);
        logF(indent, "callerFrame: %p\n", callerFrame);
        unsigned locationRawBits = callFrame->callSiteAsRawBits();
        logF(indent, "rawLocationBits: %u 0x%x\n", locationRawBits, locationRawBits);
        logF(indent, "codeBlock: %p ", codeBlock);
        if (codeBlock)
            dataLog(*codeBlock);
        dataLog("\n");
        if (codeBlock && !isInlined) {
            indent++;

            if (callFrame->callSiteBitsAreBytecodeOffset()) {
                unsigned bytecodeOffset = callFrame->bytecodeOffset();
                log(indent, "bytecodeOffset: ", bytecodeOffset, " of ", codeBlock->instructions().size(), "\n");
#if ENABLE(DFG_JIT)
            } else {
                log(indent, "hasCodeOrigins: ", codeBlock->hasCodeOrigins(), "\n");
                if (codeBlock->hasCodeOrigins()) {
                    CallSiteIndex callSiteIndex = callFrame->callSiteIndex();
                    log(indent, "callSiteIndex: ", callSiteIndex.bits(), " of ", codeBlock->codeOrigins().size(), "\n");

                    JITCode::JITType jitType = codeBlock->jitType();
                    if (jitType != JITCode::FTLJIT) {
                        JITCode* jitCode = codeBlock->jitCode().get();
                        logF(indent, "jitCode: %p start %p end %p\n", jitCode, jitCode->start(), jitCode->end());
                    }
                }
#endif
            }
            unsigned line = 0;
            unsigned column = 0;
            computeLineAndColumn(line, column);
            log(indent, "line: ", line, "\n");
            log(indent, "column: ", column, "\n");

            indent--;
        }
        indent--;
    }
    log(indent, "}\n");
}

} // namespace JSC
