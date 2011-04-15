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
#include <jit/JITCode.h>

namespace JSC {

class AbstractSamplingCounter;
class CodeBlock;
class JSGlobalData;

namespace DFG {

class JITCodeGenerator;
class NonSpeculativeJIT;
class SpeculativeJIT;
class SpeculationRecovery;

struct EntryLocation;
struct SpeculationCheck;

// Abstracted sequential numbering of available machine registers (as opposed to MacroAssembler::RegisterID,
// which are non-sequential, and not abstracted from the register numbering used by the underlying processor).
enum GPRReg { gpr0, gpr1, gpr2, gpr3, gpr4, gpr5, numberOfGPRs, InvalidGPRReg = 0xFFFFFFFF };
enum FPRReg { fpr0, fpr1, fpr2, fpr3, fpr4, fpr5, numberOfFPRs, InvalidFPRReg = 0xFFFFFFFF };

// GPRReg/FPRReg are enum types to provide type checking at compile time, use these method to iterate.
inline GPRReg next(GPRReg& reg)
{
    ASSERT(reg < numberOfGPRs);
    return reg = static_cast<GPRReg>(reg + 1);
}
inline FPRReg next(FPRReg& reg)
{
    ASSERT(reg < numberOfFPRs);
    return reg = static_cast<FPRReg>(reg + 1);
}

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
    {
    }

    // Constructor for a call with an exception handler.
    CallRecord(MacroAssembler::Call call, FunctionPtr function, MacroAssembler::Jump exceptionCheck, ExceptionInfo exceptionInfo)
        : m_call(call)
        , m_function(function)
        , m_exceptionCheck(exceptionCheck)
        , m_exceptionInfo(exceptionInfo)
    {
    }

    MacroAssembler::Call m_call;
    FunctionPtr m_function;
    MacroAssembler::Jump m_exceptionCheck;
    ExceptionInfo m_exceptionInfo;
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
    {
    }

    void compileFunction(JITCode& entry, MacroAssemblerCodePtr& entryWithArityCheck);

    // Accessors for properties.
    Graph& graph() { return m_graph; }
    CodeBlock* codeBlock() { return m_codeBlock; }
    JSGlobalData* globalData() { return m_globalData; }

#if CPU(X86_64)
    // These registers match the old JIT.
    static const RegisterID timeoutCheckRegister = X86Registers::r12;
    static const RegisterID callFrameRegister = X86Registers::r13;
    static const RegisterID tagTypeNumberRegister = X86Registers::r14;
    static const RegisterID tagMaskRegister = X86Registers::r15;

    // Temporary registers (these correspond to the temporary GPRReg/FPRReg
    // registers i.e. regT0 and grp0 refer to the same thing, grp0 being
    // the abstracted, sequential name, and regT0 being the machine register
    // number in the instruction set, as provided by the MacroAssembler).
    static const RegisterID regT0 = X86Registers::eax;
    static const RegisterID regT1 = X86Registers::edx;
    static const RegisterID regT2 = X86Registers::ecx;
    static const RegisterID regT3 = X86Registers::ebx;
    static const RegisterID regT4 = X86Registers::edi;
    static const RegisterID regT5 = X86Registers::esi;
    static const FPRegisterID fpRegT0 = X86Registers::xmm0;
    static const FPRegisterID fpRegT1 = X86Registers::xmm1;
    static const FPRegisterID fpRegT2 = X86Registers::xmm2;
    static const FPRegisterID fpRegT3 = X86Registers::xmm3;
    static const FPRegisterID fpRegT4 = X86Registers::xmm4;
    static const FPRegisterID fpRegT5 = X86Registers::xmm5;

    // These constants provide both RegisterID & GPRReg style names for the
    // general purpose argument & return value register.
    static const GPRReg argumentGPR0 = gpr4;
    static const GPRReg argumentGPR1 = gpr5;
    static const GPRReg argumentGPR2 = gpr1;
    static const GPRReg argumentGPR3 = gpr2;
    static const RegisterID argumentRegister0 = regT4;
    static const RegisterID argumentRegister1 = regT5;
    static const RegisterID argumentRegister2 = regT1;
    static const RegisterID argumentRegister3 = regT2;
    static const GPRReg returnValueGPR = gpr0;
    static const RegisterID returnValueRegister = regT0;
    static const RegisterID returnValueRegister2 = regT1;

    // These constants provide both FPRegisterID & FPRReg style names for the
    // floating point argument & return value register.
    static const FPRReg argumentFPR0 = fpr0;
    static const FPRReg argumentFPR1 = fpr1;
    static const FPRReg argumentFPR2 = fpr2;
    static const FPRReg argumentFPR3 = fpr3;
    static const FPRegisterID fpArgumentRegister0 = fpRegT0;
    static const FPRegisterID fpArgumentRegister1 = fpRegT1;
    static const FPRegisterID fpArgumentRegister2 = fpRegT2;
    static const FPRegisterID fpArgumentRegister3 = fpRegT3;
    static const FPRReg returnValueFPR = fpr0;
    static const FPRegisterID fpReturnValueRegister = fpRegT0;


    void preserveReturnAddressAfterCall(RegisterID reg)
    {
        pop(reg);
    }

    void restoreReturnAddressBeforeReturn(RegisterID reg)
    {
        push(reg);
    }

    void restoreReturnAddressBeforeReturn(Address address)
    {
        push(address);
    }

    void emitGetFromCallFrameHeaderPtr(RegisterFile::CallFrameHeaderEntry entry, RegisterID to)
    {
        loadPtr(Address(callFrameRegister, entry * sizeof(Register)), to);
    }
    void emitPutToCallFrameHeader(RegisterID from, RegisterFile::CallFrameHeaderEntry entry)
    {
        storePtr(from, Address(callFrameRegister, entry * sizeof(Register)));
    }

    void emitPutImmediateToCallFrameHeader(void* value, RegisterFile::CallFrameHeaderEntry entry)
    {
        storePtr(TrustedImmPtr(value), Address(callFrameRegister, entry * sizeof(Register)));
    }
#endif

    Address addressForArgument(int32_t argument)
    {
        return Address(callFrameRegister, (argument - (m_codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize)) * sizeof(Register));
    }

    static Address addressForGlobalVar(RegisterID global, int32_t varNumber)
    {
        return Address(global, varNumber * sizeof(Register));
    }

    static Address addressFor(VirtualRegister virtualRegister)
    {
        return Address(callFrameRegister, virtualRegister * sizeof(Register));
    }

    // These methods provide mapping from sequential register numbering (GPRReg/FPRReg)
    // to machine register numbering (RegisterID/FPRegisterID).
    static RegisterID gprToRegisterID(GPRReg reg)
    {
        ASSERT(reg < numberOfGPRs);
        static const RegisterID idForRegister[numberOfGPRs] = { regT0, regT1, regT2, regT3, regT4, regT5 };
        return idForRegister[reg];
    }
    static FPRegisterID fprToRegisterID(FPRReg reg)
    {
        ASSERT(reg < numberOfFPRs);
        static const FPRegisterID idForRegister[numberOfFPRs] = { fpRegT0, fpRegT1, fpRegT2, fpRegT3, fpRegT4, fpRegT5 };
        return idForRegister[reg];
    }

    // Add a call out from JIT code, without an exception check.
    void appendCall(const FunctionPtr& function)
    {
        m_calls.append(CallRecord(call(), function));
        // FIXME: should be able to JIT_ASSERT here that globalData->exception is null on return back to JIT code.
    }

    // Add a call out from JIT code, with an exception check.
    void appendCallWithExceptionCheck(const FunctionPtr& function, unsigned exceptionInfo)
    {
        Call functionCall = call();
        Jump exceptionCheck = branchTestPtr(NonZero, AbsoluteAddress(&globalData()->exception));
        m_calls.append(CallRecord(functionCall, function, exceptionCheck, exceptionInfo));
    }

    // Helper methods to check nodes for constants.
    bool isConstant(NodeIndex nodeIndex)
    {
        return graph()[nodeIndex].isConstant();
    }
    bool isInt32Constant(NodeIndex nodeIndex)
    {
        return graph()[nodeIndex].op == Int32Constant;
    }
    bool isDoubleConstant(NodeIndex nodeIndex)
    {
        return graph()[nodeIndex].op == DoubleConstant;
    }
    bool isJSConstant(NodeIndex nodeIndex)
    {
        return graph()[nodeIndex].op == JSConstant;
    }

    // Helper methods get constant values from nodes.
    int32_t valueOfInt32Constant(NodeIndex nodeIndex)
    {
        ASSERT(isInt32Constant(nodeIndex));
        return graph()[nodeIndex].int32Constant();
    }
    double valueOfDoubleConstant(NodeIndex nodeIndex)
    {
        ASSERT(isDoubleConstant(nodeIndex));
        return graph()[nodeIndex].numericConstant();
    }
    JSValue valueOfJSConstant(NodeIndex nodeIndex)
    {
        ASSERT(isJSConstant(nodeIndex));
        unsigned constantIndex = graph()[nodeIndex].constantNumber();
        return codeBlock()->constantRegister(FirstConstantRegisterIndex + constantIndex).get();
    }

    // These methods JIT generate dynamic, debug-only checks - akin to ASSERTs.
#if DFG_JIT_ASSERT
    void jitAssertIsInt32(GPRReg);
    void jitAssertIsJSInt32(GPRReg);
    void jitAssertIsJSNumber(GPRReg);
    void jitAssertIsJSDouble(GPRReg);
#else
    void jitAssertIsInt32(GPRReg) {}
    void jitAssertIsJSInt32(GPRReg) {}
    void jitAssertIsJSNumber(GPRReg) {}
    void jitAssertIsJSDouble(GPRReg) {}
#endif

#if ENABLE(SAMPLING_COUNTERS)
    // Debug profiling tool.
    void emitCount(AbstractSamplingCounter&, uint32_t increment = 1);
#endif

private:
    // These methods used in linking the speculative & non-speculative paths together.
    void fillNumericToDouble(NodeIndex, FPRReg, GPRReg temporary);
    void fillInt32ToInteger(NodeIndex, GPRReg);
    void fillToJS(NodeIndex, GPRReg);
    void jumpFromSpeculativeToNonSpeculative(const SpeculationCheck&, const EntryLocation&, SpeculationRecovery*);
    void linkSpeculationChecks(SpeculativeJIT&, NonSpeculativeJIT&);

    // The globalData, used to access constants such as the vPtrs.
    JSGlobalData* m_globalData;

    // The dataflow graph currently being generated.
    Graph& m_graph;

    // The codeBlock currently being generated, used to access information such as constant values, immediates.
    CodeBlock* m_codeBlock;

    // Vector of calls out from JIT code, including exception handler information.
    Vector<CallRecord> m_calls;
};

} } // namespace JSC::DFG

#endif
#endif

