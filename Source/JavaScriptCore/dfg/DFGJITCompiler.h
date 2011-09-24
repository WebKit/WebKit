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

#include <assembler/MacroAssembler.h>
#include <bytecode/CodeBlock.h>
#include <dfg/DFGGraph.h>
#include <dfg/DFGRegisterBank.h>
#include <jit/JITCode.h>

#include <dfg/DFGFPRInfo.h>
#include <dfg/DFGGPRInfo.h>

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

#ifndef NDEBUG
typedef void (*V_DFGDebugOperation_EP)(ExecState*, void*);
#endif

#if ENABLE(DFG_VERBOSE_SPECULATION_FAILURE)
struct SpeculationFailureDebugInfo {
    CodeBlock* codeBlock;
    unsigned debugOffset;
};
#endif

// === CallRecord ===
//
// A record of a call out from JIT code to a helper function.
// Every CallRecord contains a reference to the call instruction & the function
// that it needs to be linked to. Calls that might throw an exception also record
// the Jump taken on exception (unset if not present), and ExceptionInfo (presently
// an unsigned, bytecode index) used to recover handler/source info.
struct CallRecord {
    // Constructor for a call with no exception handler.
    CallRecord(MacroAssembler::Call call, FunctionPtr function)
        : m_call(call)
        , m_function(function)
        , m_handlesExceptions(false)
    {
    }

    // Constructor for a call with an exception handler.
    CallRecord(MacroAssembler::Call call, FunctionPtr function, MacroAssembler::Jump exceptionCheck, CodeOrigin codeOrigin)
        : m_call(call)
        , m_function(function)
        , m_exceptionCheck(exceptionCheck)
        , m_codeOrigin(codeOrigin)
        , m_handlesExceptions(true)
    {
    }

    // Constructor for a call that may cause exceptions, but which are handled
    // through some mechanism other than the in-line exception handler.
    CallRecord(MacroAssembler::Call call, FunctionPtr function, CodeOrigin codeOrigin)
        : m_call(call)
        , m_function(function)
        , m_codeOrigin(codeOrigin)
        , m_handlesExceptions(true)
    {
    }

    MacroAssembler::Call m_call;
    FunctionPtr m_function;
    MacroAssembler::Jump m_exceptionCheck;
    CodeOrigin m_codeOrigin;
    bool m_handlesExceptions;
};

// === JITCompiler ===
//
// DFG::JITCompiler is responsible for generating JIT code from the dataflow graph.
// It does so by delegating to the speculative & non-speculative JITs, which
// generate to a MacroAssembler (which the JITCompiler owns through an inheritance
// relationship). The JITCompiler holds references to information required during
// compilation, and also records information used in linking (e.g. a list of all
// call to be linked).
class JITCompiler : public MacroAssembler {
public:
    JITCompiler(JSGlobalData* globalData, Graph& dfg, CodeBlock* codeBlock)
        : m_globalData(globalData)
        , m_graph(dfg)
        , m_codeBlock(codeBlock)
        , m_exceptionCheckCount(0)
    {
    }

    void compile(JITCode& entry);
    void compileFunction(JITCode& entry, MacroAssemblerCodePtr& entryWithArityCheck);

    // Accessors for properties.
    Graph& graph() { return m_graph; }
    CodeBlock* codeBlock() { return m_codeBlock; }
    JSGlobalData* globalData() { return m_globalData; }
    AssemblerType_T& assembler() { return m_assembler; }

#if CPU(X86_64) || CPU(X86)
    void preserveReturnAddressAfterCall(GPRReg reg)
    {
        pop(reg);
    }

    void restoreReturnAddressBeforeReturn(GPRReg reg)
    {
        push(reg);
    }

    void restoreReturnAddressBeforeReturn(Address address)
    {
        push(address);
    }

    void emitGetFromCallFrameHeaderPtr(RegisterFile::CallFrameHeaderEntry entry, GPRReg to)
    {
        loadPtr(Address(GPRInfo::callFrameRegister, entry * sizeof(Register)), to);
    }
    void emitPutToCallFrameHeader(GPRReg from, RegisterFile::CallFrameHeaderEntry entry)
    {
        storePtr(from, Address(GPRInfo::callFrameRegister, entry * sizeof(Register)));
    }

    void emitPutImmediateToCallFrameHeader(void* value, RegisterFile::CallFrameHeaderEntry entry)
    {
        storePtr(TrustedImmPtr(value), Address(GPRInfo::callFrameRegister, entry * sizeof(Register)));
    }
#endif

    static Address addressForGlobalVar(GPRReg global, int32_t varNumber)
    {
        return Address(global, varNumber * sizeof(Register));
    }

    static Address tagForGlobalVar(GPRReg global, int32_t varNumber)
    {
        return Address(global, varNumber * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
    }

    static Address payloadForGlobalVar(GPRReg global, int32_t varNumber)
    {
        return Address(global, varNumber * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
    }

    static Address addressFor(VirtualRegister virtualRegister)
    {
        return Address(GPRInfo::callFrameRegister, virtualRegister * sizeof(Register));
    }

    static Address tagFor(VirtualRegister virtualRegister)
    {
        return Address(GPRInfo::callFrameRegister, virtualRegister * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
    }

    static Address payloadFor(VirtualRegister virtualRegister)
    {
        return Address(GPRInfo::callFrameRegister, virtualRegister * sizeof(Register) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
    }

    Jump branchIfNotObject(GPRReg structureReg)
    {
        return branch8(Below, Address(structureReg, Structure::typeInfoTypeOffset()), TrustedImm32(ObjectType));
    }

    // Notify the JIT of a call that does not require linking.
    void notifyCall(Call call, CodeOrigin codeOrigin)
    {
        m_calls.append(CallRecord(call, FunctionPtr(), codeOrigin));
    }

    // Add a call out from JIT code, without an exception check.
    void appendCall(const FunctionPtr& function)
    {
        m_calls.append(CallRecord(call(), function));
        // FIXME: should be able to JIT_ASSERT here that globalData->exception is null on return back to JIT code.
    }

    // Add a call out from JIT code, with an exception check.
    Call appendCallWithExceptionCheck(const FunctionPtr& function, CodeOrigin codeOrigin)
    {
        Call functionCall = call();
#if USE(JSVALUE64)
        Jump exceptionCheck = branchTestPtr(NonZero, AbsoluteAddress(&globalData()->exception));
#elif USE(JSVALUE32_64)
        Jump exceptionCheck = branch32(NotEqual, AbsoluteAddress(reinterpret_cast<char*>(&globalData()->exception) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), TrustedImm32(JSValue::EmptyValueTag));
#endif
        m_calls.append(CallRecord(functionCall, function, exceptionCheck, codeOrigin));
        return functionCall;
    }
    
    // Add a call out from JIT code, with a fast exception check that tests if the return value is zero.
    Call appendCallWithFastExceptionCheck(const FunctionPtr& function, CodeOrigin codeOrigin)
    {
        Call functionCall = call();
        Jump exceptionCheck = branchTestPtr(Zero, GPRInfo::returnValueGPR);
        m_calls.append(CallRecord(functionCall, function, exceptionCheck, codeOrigin));
        return functionCall;
    }
    
#ifndef NDEBUG
    // Add a debug call. This call has no effect on JIT code execution state.
    void debugCall(V_DFGDebugOperation_EP function, void* argument)
    {
        for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i)
            storePtr(GPRInfo::toRegister(i), m_globalData->debugDataBuffer + i);
        for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
            move(TrustedImmPtr(m_globalData->debugDataBuffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
            storeDouble(FPRInfo::toRegister(i), GPRInfo::regT0);
        }
        move(TrustedImmPtr(argument), GPRInfo::argumentGPR1);
        move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        move(TrustedImmPtr(reinterpret_cast<void*>(function)), GPRInfo::regT0);
        call(GPRInfo::regT0);
        for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
            move(TrustedImmPtr(m_globalData->debugDataBuffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
            loadDouble(GPRInfo::regT0, FPRInfo::toRegister(i));
        }
        for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i)
            loadPtr(m_globalData->debugDataBuffer + i, GPRInfo::toRegister(i));
    }
#endif

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

#if USE(JSVALUE32_64)
    void* addressOfDoubleConstant(NodeIndex nodeIndex)
    {
        ASSERT(isNumberConstant(nodeIndex));
        unsigned constantIndex = graph()[nodeIndex].constantNumber();
        return &(codeBlock()->constantRegister(FirstConstantRegisterIndex + constantIndex));
    }

    void emitLoadTag(NodeIndex, GPRReg tag);
    void emitLoadPayload(NodeIndex, GPRReg payload);

    void emitLoad(const JSValue&, GPRReg tag, GPRReg payload);
    void emitLoad(NodeIndex, GPRReg tag, GPRReg payload);
    void emitLoad2(NodeIndex index1, GPRReg tag1, GPRReg payload1, NodeIndex index2, GPRReg tag2, GPRReg payload2);

    void emitLoadDouble(NodeIndex, FPRReg value);
    void emitLoadInt32ToDouble(NodeIndex, FPRReg value);

    void emitStore(NodeIndex, GPRReg tag, GPRReg payload);
    void emitStore(NodeIndex, const JSValue constant);
    void emitStoreInt32(NodeIndex, GPRReg payload, bool indexIsInt32 = false);
    void emitStoreInt32(NodeIndex, TrustedImm32 payload, bool indexIsInt32 = false);
    void emitStoreCell(NodeIndex, GPRReg payload, bool indexIsCell = false);
    void emitStoreBool(NodeIndex, GPRReg payload, bool indexIsBool = false);
    void emitStoreDouble(NodeIndex, FPRReg value);
#endif

    // These methods JIT generate dynamic, debug-only checks - akin to ASSERTs.
#if ENABLE(DFG_JIT_ASSERT)
    void jitAssertIsInt32(GPRReg);
    void jitAssertIsJSInt32(GPRReg);
    void jitAssertIsJSNumber(GPRReg);
    void jitAssertIsJSDouble(GPRReg);
    void jitAssertIsCell(GPRReg);
#else
    void jitAssertIsInt32(GPRReg) {}
    void jitAssertIsJSInt32(GPRReg) {}
    void jitAssertIsJSNumber(GPRReg) {}
    void jitAssertIsJSDouble(GPRReg) {}
    void jitAssertIsCell(GPRReg) {}
#endif

    // These methods convert between doubles, and doubles boxed and JSValues.
#if USE(JSVALUE64)
    GPRReg boxDouble(FPRReg fpr, GPRReg gpr)
    {
        moveDoubleToPtr(fpr, gpr);
        subPtr(GPRInfo::tagTypeNumberRegister, gpr);
        return gpr;
    }
    FPRReg unboxDouble(GPRReg gpr, FPRReg fpr)
    {
        jitAssertIsJSDouble(gpr);
        addPtr(GPRInfo::tagTypeNumberRegister, gpr);
        movePtrToDouble(gpr, fpr);
        return fpr;
    }
#elif USE(JSVALUE32_64)
    // FIXME: The box/unbox of doubles could be improved without exchanging data through memory,
    // for example on x86 some SSE instructions can help do this.
    void boxDouble(FPRReg fpr, GPRReg tagGPR, GPRReg payloadGPR, VirtualRegister virtualRegister)
    {
        storeDouble(fpr, addressFor(virtualRegister));
        load32(tagFor(virtualRegister), tagGPR);
        load32(payloadFor(virtualRegister), payloadGPR);
    }
    void unboxDouble(GPRReg tagGPR, GPRReg payloadGPR, FPRReg fpr, VirtualRegister virtualRegister)
    {
        jitAssertIsJSDouble(tagGPR);
        store32(tagGPR, tagFor(virtualRegister));
        store32(payloadGPR, payloadFor(virtualRegister));
        loadDouble(addressFor(virtualRegister), fpr);
    }
#endif

#if ENABLE(SAMPLING_COUNTERS)
    // Debug profiling tool.
    static void emitCount(MacroAssembler&, AbstractSamplingCounter&, uint32_t increment = 1);
    void emitCount(AbstractSamplingCounter& counter, uint32_t increment = 1)
    {
        emitCount(*this, counter, increment);
    }
#endif

#if ENABLE(SAMPLING_FLAGS)
    void setSamplingFlag(int32_t flag);
    void clearSamplingFlag(int32_t flag);
#endif

#if USE(JSVALUE64)
    void addPropertyAccess(JITCompiler::Call functionCall, int16_t deltaCheckImmToCall, int16_t deltaCallToStructCheck, int16_t deltaCallToLoadOrStore, int16_t deltaCallToSlowCase, int16_t deltaCallToDone, int8_t baseGPR, int8_t valueGPR, int8_t scratchGPR)
    {
        m_propertyAccesses.append(PropertyAccessRecord(functionCall, deltaCheckImmToCall, deltaCallToStructCheck, deltaCallToLoadOrStore, deltaCallToSlowCase, deltaCallToDone,  baseGPR, valueGPR, scratchGPR));
    }
#elif USE(JSVALUE32_64)
    void addPropertyAccess(JITCompiler::Call functionCall, int16_t deltaCheckImmToCall, int16_t deltaCallToStructCheck, int16_t deltaCallToLoadOrStore, int16_t deltaCallToSlowCase, int16_t deltaCallToDone, int8_t baseGPR, int8_t valueTagGPR, int8_t valueGPR, int8_t scratchGPR)
    {
        m_propertyAccesses.append(PropertyAccessRecord(functionCall, deltaCheckImmToCall, deltaCallToStructCheck, deltaCallToLoadOrStore, deltaCallToSlowCase, deltaCallToDone,  baseGPR, valueTagGPR, valueGPR, scratchGPR));
    }
#endif

    void addMethodGet(Call slowCall, DataLabelPtr structToCompare, DataLabelPtr protoObj, DataLabelPtr protoStructToCompare, DataLabelPtr putFunction)
    {
        m_methodGets.append(MethodGetRecord(slowCall, structToCompare, protoObj, protoStructToCompare, putFunction));
    }
    
    void addJSCall(Call fastCall, Call slowCall, DataLabelPtr targetToCheck, bool isCall, CodeOrigin codeOrigin)
    {
        m_jsCalls.append(JSCallRecord(fastCall, slowCall, targetToCheck, isCall, codeOrigin));
    }
    
    void noticeOSREntry(BasicBlock& basicBlock)
    {
#if ENABLE(DFG_OSR_ENTRY)
        OSREntryData* entry = codeBlock()->appendDFGOSREntryData(basicBlock.bytecodeBegin, differenceBetween(m_startOfCode, label()));
        
        unsigned lastLiveArgument = 0;
        unsigned lastLiveLocal = 0;
        
        for (unsigned i = 0; i < basicBlock.m_arguments.size(); ++i) {
            if (basicBlock.m_arguments[i].value != NoNode)
                lastLiveArgument = i;
        }
        
        for (unsigned i = 0; i < basicBlock.m_locals.size(); ++i) {
            if (basicBlock.m_locals[i].value != NoNode)
                lastLiveLocal = i;
        }
        
        if (lastLiveArgument) {
            entry->m_liveArguments.resize(lastLiveArgument + 1);
            entry->m_liveArguments.clearAll();
            
            for (unsigned i = 0; i <= lastLiveArgument; ++i) {
                if (basicBlock.m_arguments[i].value != NoNode)
                    entry->m_liveArguments.set(i);
            }
        } else
            entry->m_liveArguments.clearAll();
        
        if (lastLiveLocal) {
            entry->m_liveVariables.resize(lastLiveLocal + 1);
            entry->m_liveVariables.clearAll();
            
            for (unsigned i = 0; i <= lastLiveLocal; ++i) {
                if (basicBlock.m_locals[i].value != NoNode)
                    entry->m_liveVariables.set(i);
            }
        } else
            entry->m_liveVariables.clearAll();
#else
        UNUSED_PARAM(basicBlock);
#endif
    }

private:
    // Internal implementation to compile.
    void compileEntry();
    void compileBody();
    void link(LinkBuffer&);

    // These methods used in linking the speculative & non-speculative paths together.
    void fillNumericToDouble(NodeIndex, FPRReg, GPRReg temporary);
    void fillInt32ToInteger(NodeIndex, GPRReg);
    void fillToJS(NodeIndex, GPRReg);
    
    void exitSpeculativeWithOSR(const OSRExit&, SpeculationRecovery*, Vector<BytecodeAndMachineOffset>& decodedCodeMap);
    void linkOSRExits(SpeculativeJIT&);

    // The globalData, used to access constants such as the vPtrs.
    JSGlobalData* m_globalData;

    // The dataflow graph currently being generated.
    Graph& m_graph;

    // The codeBlock currently being generated, used to access information such as constant values, immediates.
    CodeBlock* m_codeBlock;

    // Vector of calls out from JIT code, including exception handler information.
    // Count of the number of CallRecords with exception handlers.
    Vector<CallRecord> m_calls;
    unsigned m_exceptionCheckCount;
    
    // JIT code map for OSR entrypoints.
    Label m_startOfCode;

    struct PropertyAccessRecord {
#if USE(JSVALUE64)
        PropertyAccessRecord(Call functionCall, int16_t deltaCheckImmToCall, int16_t deltaCallToStructCheck, int16_t deltaCallToLoadOrStore, int16_t deltaCallToSlowCase, int16_t deltaCallToDone, int8_t baseGPR, int8_t valueGPR, int8_t scratchGPR)
#elif USE(JSVALUE32_64)
        PropertyAccessRecord(Call functionCall, int16_t deltaCheckImmToCall, int16_t deltaCallToStructCheck, int16_t deltaCallToLoadOrStore, int16_t deltaCallToSlowCase, int16_t deltaCallToDone, int8_t baseGPR, int8_t valueTagGPR, int8_t valueGPR, int8_t scratchGPR)
#endif
            : m_functionCall(functionCall)
            , m_deltaCheckImmToCall(deltaCheckImmToCall)
            , m_deltaCallToStructCheck(deltaCallToStructCheck)
            , m_deltaCallToLoadOrStore(deltaCallToLoadOrStore)
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

        JITCompiler::Call m_functionCall;
        int16_t m_deltaCheckImmToCall;
        int16_t m_deltaCallToStructCheck;
        int16_t m_deltaCallToLoadOrStore;
        int16_t m_deltaCallToSlowCase;
        int16_t m_deltaCallToDone;
        int8_t m_baseGPR;
#if USE(JSVALUE32_64)
        int8_t m_valueTagGPR;
#endif
        int8_t m_valueGPR;
        int8_t m_scratchGPR;
    };
    
    struct MethodGetRecord {
        MethodGetRecord(Call slowCall, DataLabelPtr structToCompare, DataLabelPtr protoObj, DataLabelPtr protoStructToCompare, DataLabelPtr putFunction)
            : m_slowCall(slowCall)
            , m_structToCompare(structToCompare)
            , m_protoObj(protoObj)
            , m_protoStructToCompare(protoStructToCompare)
            , m_putFunction(putFunction)
        {
        }
        
        Call m_slowCall;
        DataLabelPtr m_structToCompare;
        DataLabelPtr m_protoObj;
        DataLabelPtr m_protoStructToCompare;
        DataLabelPtr m_putFunction;
    };
    
    struct JSCallRecord {
        JSCallRecord(Call fastCall, Call slowCall, DataLabelPtr targetToCheck, bool isCall, CodeOrigin codeOrigin)
            : m_fastCall(fastCall)
            , m_slowCall(slowCall)
            , m_targetToCheck(targetToCheck)
            , m_isCall(isCall)
            , m_codeOrigin(codeOrigin)
        {
        }
        
        Call m_fastCall;
        Call m_slowCall;
        DataLabelPtr m_targetToCheck;
        bool m_isCall;
        CodeOrigin m_codeOrigin;
    };

    Vector<PropertyAccessRecord, 4> m_propertyAccesses;
    Vector<MethodGetRecord, 4> m_methodGets;
    Vector<JSCallRecord, 4> m_jsCalls;
};

} } // namespace JSC::DFG

#endif
#endif

