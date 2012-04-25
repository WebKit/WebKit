/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef DFGJITCompiler_h
#define DFGJITCompiler_h

#if ENABLE(DFG_JIT)

#include <assembler/LinkBuffer.h>
#include <assembler/MacroAssembler.h>
#include <bytecode/CodeBlock.h>
#include <dfg/DFGCCallHelpers.h>
#include <dfg/DFGFPRInfo.h>
#include <dfg/DFGGPRInfo.h>
#include <dfg/DFGGraph.h>
#include <dfg/DFGRegisterBank.h>
#include <jit/JITCode.h>

namespace JSC {

class AbstractSamplingCounter;
class CodeBlock;
class JSGlobalData;

namespace DFG {

class JITCodeGenerator;
class NodeToRegisterMap;
class SpeculativeJIT;
class SpeculationRecovery;

struct EntryLocation;
struct OSRExit;

// === CallLinkRecord ===
//
// A record of a call out from JIT code that needs linking to a helper function.
// Every CallLinkRecord contains a reference to the call instruction & the function
// that it needs to be linked to.
struct CallLinkRecord {
    CallLinkRecord(MacroAssembler::Call call, FunctionPtr function)
        : m_call(call)
        , m_function(function)
    {
    }

    MacroAssembler::Call m_call;
    FunctionPtr m_function;
};

class CallBeginToken {
public:
    CallBeginToken()
#if !ASSERT_DISABLED
        : m_codeOriginIndex(UINT_MAX)
#endif
    {
    }
    
    explicit CallBeginToken(unsigned codeOriginIndex)
#if !ASSERT_DISABLED
        : m_codeOriginIndex(codeOriginIndex)
#endif
    {
        UNUSED_PARAM(codeOriginIndex);
    }
    
    void assertCodeOriginIndex(unsigned codeOriginIndex) const
    {
        ASSERT_UNUSED(codeOriginIndex, codeOriginIndex < UINT_MAX);
        ASSERT_UNUSED(codeOriginIndex, codeOriginIndex == m_codeOriginIndex);
    }

private:
#if !ASSERT_DISABLED
    unsigned m_codeOriginIndex;
#endif
};

// === CallExceptionRecord ===
//
// A record of a call out from JIT code that might throw an exception.
// Calls that might throw an exception also record the Jump taken on exception
// (unset if not present) and code origin used to recover handler/source info.
struct CallExceptionRecord {
    CallExceptionRecord(MacroAssembler::Call call, CodeOrigin codeOrigin, CallBeginToken token)
        : m_call(call)
        , m_codeOrigin(codeOrigin)
        , m_token(token)
    {
    }

    CallExceptionRecord(MacroAssembler::Call call, MacroAssembler::Jump exceptionCheck, CodeOrigin codeOrigin, CallBeginToken token)
        : m_call(call)
        , m_exceptionCheck(exceptionCheck)
        , m_codeOrigin(codeOrigin)
        , m_token(token)
    {
    }

    MacroAssembler::Call m_call;
    MacroAssembler::Jump m_exceptionCheck;
    CodeOrigin m_codeOrigin;
    CallBeginToken m_token;
};

struct PropertyAccessRecord {
    enum RegisterMode { RegistersFlushed, RegistersInUse };
    
#if USE(JSVALUE64)
    PropertyAccessRecord(CodeOrigin codeOrigin, MacroAssembler::DataLabelPtr deltaCheckImmToCall, MacroAssembler::Call functionCall, MacroAssembler::PatchableJump deltaCallToStructCheck, MacroAssembler::DataLabelCompact deltaCallToLoadOrStore, MacroAssembler::Label deltaCallToSlowCase, MacroAssembler::Label deltaCallToDone, int8_t baseGPR, int8_t valueGPR, int8_t scratchGPR, RegisterMode registerMode = RegistersInUse)
#elif USE(JSVALUE32_64)
    PropertyAccessRecord(CodeOrigin codeOrigin, MacroAssembler::DataLabelPtr deltaCheckImmToCall, MacroAssembler::Call functionCall, MacroAssembler::PatchableJump deltaCallToStructCheck, MacroAssembler::DataLabelCompact deltaCallToTagLoadOrStore, MacroAssembler::DataLabelCompact deltaCallToPayloadLoadOrStore, MacroAssembler::Label deltaCallToSlowCase, MacroAssembler::Label deltaCallToDone, int8_t baseGPR, int8_t valueTagGPR, int8_t valueGPR, int8_t scratchGPR, RegisterMode registerMode = RegistersInUse)
#endif
        : m_codeOrigin(codeOrigin)
        , m_deltaCheckImmToCall(deltaCheckImmToCall)
        , m_functionCall(functionCall)
        , m_deltaCallToStructCheck(deltaCallToStructCheck)
#if USE(JSVALUE64)
        , m_deltaCallToLoadOrStore(deltaCallToLoadOrStore)
#elif USE(JSVALUE32_64)
        , m_deltaCallToTagLoadOrStore(deltaCallToTagLoadOrStore)
        , m_deltaCallToPayloadLoadOrStore(deltaCallToPayloadLoadOrStore)
#endif
        , m_deltaCallToSlowCase(deltaCallToSlowCase)
        , m_deltaCallToDone(deltaCallToDone)
        , m_baseGPR(baseGPR)
#if USE(JSVALUE32_64)
        , m_valueTagGPR(valueTagGPR)
#endif
        , m_valueGPR(valueGPR)
        , m_scratchGPR(scratchGPR)
        , m_registerMode(registerMode)
    {
    }

    CodeOrigin m_codeOrigin;
    MacroAssembler::DataLabelPtr m_deltaCheckImmToCall;
    MacroAssembler::Call m_functionCall;
    MacroAssembler::PatchableJump m_deltaCallToStructCheck;
#if USE(JSVALUE64)
    MacroAssembler::DataLabelCompact m_deltaCallToLoadOrStore;
#elif USE(JSVALUE32_64)
    MacroAssembler::DataLabelCompact m_deltaCallToTagLoadOrStore;
    MacroAssembler::DataLabelCompact m_deltaCallToPayloadLoadOrStore;
#endif
    MacroAssembler::Label m_deltaCallToSlowCase;
    MacroAssembler::Label m_deltaCallToDone;
    int8_t m_baseGPR;
#if USE(JSVALUE32_64)
    int8_t m_valueTagGPR;
#endif
    int8_t m_valueGPR;
    int8_t m_scratchGPR;
    RegisterMode m_registerMode;
};

// === JITCompiler ===
//
// DFG::JITCompiler is responsible for generating JIT code from the dataflow graph.
// It does so by delegating to the speculative & non-speculative JITs, which
// generate to a MacroAssembler (which the JITCompiler owns through an inheritance
// relationship). The JITCompiler holds references to information required during
// compilation, and also records information used in linking (e.g. a list of all
// call to be linked).
class JITCompiler : public CCallHelpers {
public:
    JITCompiler(Graph& dfg)
        : CCallHelpers(&dfg.m_globalData, dfg.m_codeBlock)
        , m_graph(dfg)
        , m_currentCodeOriginIndex(0)
    {
    }

    bool compile(JITCode& entry);
    bool compileFunction(JITCode& entry, MacroAssemblerCodePtr& entryWithArityCheck);

    // Accessors for properties.
    Graph& graph() { return m_graph; }
    
    // Get a token for beginning a call, and set the current code origin index in
    // the call frame.
    CallBeginToken beginCall()
    {
        unsigned codeOriginIndex = m_currentCodeOriginIndex++;
        store32(TrustedImm32(codeOriginIndex), tagFor(static_cast<VirtualRegister>(RegisterFile::ArgumentCount)));
        return CallBeginToken(codeOriginIndex);
    }

    // Notify the JIT of a call that does not require linking.
    void notifyCall(Call functionCall, CodeOrigin codeOrigin, CallBeginToken token)
    {
        m_exceptionChecks.append(CallExceptionRecord(functionCall, codeOrigin, token));
    }

    // Add a call out from JIT code, without an exception check.
    Call appendCall(const FunctionPtr& function)
    {
        Call functionCall = call();
        m_calls.append(CallLinkRecord(functionCall, function));
        return functionCall;
    }

    // Add a call out from JIT code, with an exception check.
    void addExceptionCheck(Call functionCall, CodeOrigin codeOrigin, CallBeginToken token)
    {
        move(TrustedImm32(m_exceptionChecks.size()), GPRInfo::nonPreservedNonReturnGPR);
        m_exceptionChecks.append(CallExceptionRecord(functionCall, emitExceptionCheck(), codeOrigin, token));
    }
    
    // Add a call out from JIT code, with a fast exception check that tests if the return value is zero.
    void addFastExceptionCheck(Call functionCall, CodeOrigin codeOrigin, CallBeginToken token)
    {
        move(TrustedImm32(m_exceptionChecks.size()), GPRInfo::nonPreservedNonReturnGPR);
        Jump exceptionCheck = branchTestPtr(Zero, GPRInfo::returnValueGPR);
        m_exceptionChecks.append(CallExceptionRecord(functionCall, exceptionCheck, codeOrigin, token));
    }
    
    // Helper methods to get predictions
    PredictedType getPrediction(Node& node) { return node.prediction(); }
    PredictedType getPrediction(NodeIndex nodeIndex) { return getPrediction(graph()[nodeIndex]); }
    PredictedType getPrediction(Edge nodeUse) { return getPrediction(nodeUse.index()); }

#if USE(JSVALUE32_64)
    void* addressOfDoubleConstant(NodeIndex nodeIndex)
    {
        ASSERT(m_graph.isNumberConstant(nodeIndex));
        unsigned constantIndex = graph()[nodeIndex].constantNumber();
        return &(codeBlock()->constantRegister(FirstConstantRegisterIndex + constantIndex));
    }
#endif

    void addPropertyAccess(const PropertyAccessRecord& record)
    {
        m_propertyAccesses.append(record);
    }

    void addJSCall(Call fastCall, Call slowCall, DataLabelPtr targetToCheck, CallLinkInfo::CallType callType, CodeOrigin codeOrigin)
    {
        m_jsCalls.append(JSCallRecord(fastCall, slowCall, targetToCheck, callType, codeOrigin));
    }
    
    void addWeakReference(JSCell* target)
    {
        m_codeBlock->appendWeakReference(target);
    }
    
    void addWeakReferenceTransition(JSCell* codeOrigin, JSCell* from, JSCell* to)
    {
        m_codeBlock->appendWeakReferenceTransition(codeOrigin, from, to);
    }
    
    template<typename T>
    Jump branchWeakPtr(RelationalCondition cond, T left, JSCell* weakPtr)
    {
        Jump result = branchPtr(cond, left, TrustedImmPtr(weakPtr));
        addWeakReference(weakPtr);
        return result;
    }
    
    void noticeOSREntry(BasicBlock& basicBlock, JITCompiler::Label blockHead, LinkBuffer& linkBuffer)
    {
#if DFG_ENABLE(OSR_ENTRY)
        OSREntryData* entry = codeBlock()->appendDFGOSREntryData(basicBlock.bytecodeBegin, linkBuffer.offsetOf(blockHead));
        
        entry->m_expectedValues = basicBlock.valuesAtHead;
        
        // Fix the expected values: in our protocol, a dead variable will have an expected
        // value of (None, []). But the old JIT may stash some values there. So we really
        // need (Top, TOP).
        for (size_t argument = 0; argument < basicBlock.variablesAtHead.numberOfArguments(); ++argument) {
            NodeIndex nodeIndex = basicBlock.variablesAtHead.argument(argument);
            if (nodeIndex == NoNode || !m_graph[nodeIndex].shouldGenerate())
                entry->m_expectedValues.argument(argument).makeTop();
        }
        for (size_t local = 0; local < basicBlock.variablesAtHead.numberOfLocals(); ++local) {
            NodeIndex nodeIndex = basicBlock.variablesAtHead.local(local);
            if (nodeIndex == NoNode || !m_graph[nodeIndex].shouldGenerate())
                entry->m_expectedValues.local(local).makeTop();
            else if (m_graph[nodeIndex].variableAccessData()->shouldUseDoubleFormat())
                entry->m_localsForcedDouble.set(local);
        }
#else
        UNUSED_PARAM(basicBlock);
        UNUSED_PARAM(blockHead);
        UNUSED_PARAM(linkBuffer);
#endif
    }

private:
    // Internal implementation to compile.
    void compileEntry();
    void compileBody(SpeculativeJIT&);
    void link(LinkBuffer&);

    void exitSpeculativeWithOSR(const OSRExit&, SpeculationRecovery*);
    void linkOSRExits();
    
    // The dataflow graph currently being generated.
    Graph& m_graph;

    // Vector of calls out from JIT code, including exception handler information.
    // Count of the number of CallRecords with exception handlers.
    Vector<CallLinkRecord> m_calls;
    Vector<CallExceptionRecord> m_exceptionChecks;
    
    struct JSCallRecord {
        JSCallRecord(Call fastCall, Call slowCall, DataLabelPtr targetToCheck, CallLinkInfo::CallType callType, CodeOrigin codeOrigin)
            : m_fastCall(fastCall)
            , m_slowCall(slowCall)
            , m_targetToCheck(targetToCheck)
            , m_callType(callType)
            , m_codeOrigin(codeOrigin)
        {
        }
        
        Call m_fastCall;
        Call m_slowCall;
        DataLabelPtr m_targetToCheck;
        CallLinkInfo::CallType m_callType;
        CodeOrigin m_codeOrigin;
    };
    
    Vector<PropertyAccessRecord, 4> m_propertyAccesses;
    Vector<JSCallRecord, 4> m_jsCalls;
    unsigned m_currentCodeOriginIndex;
};

} } // namespace JSC::DFG

#endif
#endif

