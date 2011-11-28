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
#include <dfg/DFGAssemblyHelpers.h>
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

// === CallExceptionRecord ===
//
// A record of a call out from JIT code that might throw an exception.
// Calls that might throw an exception also record the Jump taken on exception
// (unset if not present) and code origin used to recover handler/source info.
struct CallExceptionRecord {
    CallExceptionRecord(MacroAssembler::Call call, CodeOrigin codeOrigin)
        : m_call(call)
        , m_codeOrigin(codeOrigin)
    {
    }

    CallExceptionRecord(MacroAssembler::Call call, MacroAssembler::Jump exceptionCheck, CodeOrigin codeOrigin)
        : m_call(call)
        , m_exceptionCheck(exceptionCheck)
        , m_codeOrigin(codeOrigin)
    {
    }

    MacroAssembler::Call m_call;
    MacroAssembler::Jump m_exceptionCheck;
    CodeOrigin m_codeOrigin;
};

struct PropertyAccessRecord {
#if USE(JSVALUE64)
    PropertyAccessRecord(MacroAssembler::DataLabelPtr deltaCheckImmToCall, MacroAssembler::Call functionCall, MacroAssembler::Jump deltaCallToStructCheck, MacroAssembler::DataLabelCompact deltaCallToLoadOrStore, MacroAssembler::Label deltaCallToSlowCase, MacroAssembler::Label deltaCallToDone, int8_t baseGPR, int8_t valueGPR, int8_t scratchGPR)
#elif USE(JSVALUE32_64)
    PropertyAccessRecord(MacroAssembler::DataLabelPtr deltaCheckImmToCall, MacroAssembler::Call functionCall, MacroAssembler::Jump deltaCallToStructCheck, MacroAssembler::DataLabelCompact deltaCallToTagLoadOrStore, MacroAssembler::DataLabelCompact deltaCallToPayloadLoadOrStore, MacroAssembler::Label deltaCallToSlowCase, MacroAssembler::Label deltaCallToDone, int8_t baseGPR, int8_t valueTagGPR, int8_t valueGPR, int8_t scratchGPR)
#endif
        : m_deltaCheckImmToCall(deltaCheckImmToCall)
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
    {
    }

    MacroAssembler::DataLabelPtr m_deltaCheckImmToCall;
    MacroAssembler::Call m_functionCall;
    MacroAssembler::Jump m_deltaCallToStructCheck;
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
};

// === JITCompiler ===
//
// DFG::JITCompiler is responsible for generating JIT code from the dataflow graph.
// It does so by delegating to the speculative & non-speculative JITs, which
// generate to a MacroAssembler (which the JITCompiler owns through an inheritance
// relationship). The JITCompiler holds references to information required during
// compilation, and also records information used in linking (e.g. a list of all
// call to be linked).
class JITCompiler : public AssemblyHelpers {
public:
    JITCompiler(JSGlobalData* globalData, Graph& dfg, CodeBlock* codeBlock)
        : AssemblyHelpers(globalData, codeBlock)
        , m_graph(dfg)
    {
    }

    void compile(JITCode& entry);
    void compileFunction(JITCode& entry, MacroAssemblerCodePtr& entryWithArityCheck);

    // Accessors for properties.
    Graph& graph() { return m_graph; }

    // Notify the JIT of a call that does not require linking.
    void notifyCall(Call functionCall, CodeOrigin codeOrigin)
    {
        m_exceptionChecks.append(CallExceptionRecord(functionCall, codeOrigin));
    }

    // Add a call out from JIT code, without an exception check.
    Call appendCall(const FunctionPtr& function)
    {
        Call functionCall = call();
        m_calls.append(CallLinkRecord(functionCall, function));
        return functionCall;
    }

    // Add a call out from JIT code, with an exception check.
    Call addExceptionCheck(Call functionCall, CodeOrigin codeOrigin)
    {
#if USE(JSVALUE64)
        Jump exceptionCheck = branchTestPtr(NonZero, AbsoluteAddress(&globalData()->exception));
#elif USE(JSVALUE32_64)
        Jump exceptionCheck = branch32(NotEqual, AbsoluteAddress(reinterpret_cast<char*>(&globalData()->exception) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), TrustedImm32(JSValue::EmptyValueTag));
#endif
        m_exceptionChecks.append(CallExceptionRecord(functionCall, exceptionCheck, codeOrigin));
        return functionCall;
    }
    
    // Add a call out from JIT code, with a fast exception check that tests if the return value is zero.
    Call addFastExceptionCheck(Call functionCall, CodeOrigin codeOrigin)
    {
        Jump exceptionCheck = branchTestPtr(Zero, GPRInfo::returnValueGPR);
        m_exceptionChecks.append(CallExceptionRecord(functionCall, exceptionCheck, codeOrigin));
        return functionCall;
    }
    
    // Helper methods to check nodes for constants.
    bool isConstant(NodeIndex nodeIndex) { return graph().isConstant(nodeIndex); }
    bool isJSConstant(NodeIndex nodeIndex) { return graph().isJSConstant(nodeIndex); }
    bool isInt32Constant(NodeIndex nodeIndex) { return graph().isInt32Constant(codeBlock(), nodeIndex); }
    bool isDoubleConstant(NodeIndex nodeIndex) { return graph().isDoubleConstant(codeBlock(), nodeIndex); }
    bool isNumberConstant(NodeIndex nodeIndex) { return graph().isNumberConstant(codeBlock(), nodeIndex); }
    bool isBooleanConstant(NodeIndex nodeIndex) { return graph().isBooleanConstant(codeBlock(), nodeIndex); }
    bool isFunctionConstant(NodeIndex nodeIndex) { return graph().isFunctionConstant(codeBlock(), nodeIndex); }
    // Helper methods get constant values from nodes.
    JSValue valueOfJSConstant(NodeIndex nodeIndex) { return graph().valueOfJSConstant(codeBlock(), nodeIndex); }
    int32_t valueOfInt32Constant(NodeIndex nodeIndex) { return graph().valueOfInt32Constant(codeBlock(), nodeIndex); }
    double valueOfNumberConstant(NodeIndex nodeIndex) { return graph().valueOfNumberConstant(codeBlock(), nodeIndex); }
    bool valueOfBooleanConstant(NodeIndex nodeIndex) { return graph().valueOfBooleanConstant(codeBlock(), nodeIndex); }
    JSFunction* valueOfFunctionConstant(NodeIndex nodeIndex) { return graph().valueOfFunctionConstant(codeBlock(), nodeIndex); }
    
    // Helper methods to get predictions
    PredictedType getPrediction(Node& node) { return node.prediction(); }
    PredictedType getPrediction(NodeIndex nodeIndex) { return getPrediction(graph()[nodeIndex]); }

#if USE(JSVALUE32_64)
    void* addressOfDoubleConstant(NodeIndex nodeIndex)
    {
        ASSERT(isNumberConstant(nodeIndex));
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
            if (basicBlock.variablesAtHead.argument(argument) == NoNode)
                entry->m_expectedValues.argument(argument).makeTop();
        }
        for (size_t local = 0; local < basicBlock.variablesAtHead.numberOfLocals(); ++local) {
            if (basicBlock.variablesAtHead.local(local) == NoNode)
                entry->m_expectedValues.local(local).makeTop();
        }
#else
        UNUSED_PARAM(basicBlock);
        UNUSED_PARAM(blockHead);
        UNUSED_PARAM(linkBuffer);
#endif
    }

    ValueProfile* valueProfileFor(NodeIndex nodeIndex)
    {
        if (nodeIndex == NoNode)
            return 0;
        
        return m_graph.valueProfileFor(nodeIndex, baselineCodeBlockFor(m_graph[nodeIndex].codeOrigin));
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
};

} } // namespace JSC::DFG

#endif
#endif

