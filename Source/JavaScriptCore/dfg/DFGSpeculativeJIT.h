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

#ifndef DFGSpeculativeJIT_h
#define DFGSpeculativeJIT_h

#if ENABLE(DFG_JIT)

#include "DFGAbstractState.h"
#include "DFGJITCodeGenerator.h"
#include "DFGOSRExit.h"
#include "ValueRecovery.h"

namespace JSC { namespace DFG {

class SpeculativeJIT;

enum ValueSourceKind {
    SourceNotSet,
    ValueInRegisterFile,
    Int32InRegisterFile,
    CellInRegisterFile,
    BooleanInRegisterFile,
    HaveNode
};

class ValueSource {
public:
    ValueSource()
        : m_nodeIndex(nodeIndexFromKind(SourceNotSet))
    {
    }
    
    explicit ValueSource(ValueSourceKind valueSourceKind)
        : m_nodeIndex(nodeIndexFromKind(valueSourceKind))
    {
        ASSERT(kind() != SourceNotSet);
        ASSERT(kind() != HaveNode);
    }
    
    explicit ValueSource(NodeIndex nodeIndex)
        : m_nodeIndex(nodeIndex)
    {
        ASSERT(kind() == HaveNode);
    }
    
    static ValueSource forPrediction(PredictedType prediction)
    {
        if (isInt32Prediction(prediction))
            return ValueSource(Int32InRegisterFile);
        if (isArrayPrediction(prediction) || isByteArrayPrediction(prediction))
            return ValueSource(CellInRegisterFile);
        if (isBooleanPrediction(prediction))
            return ValueSource(BooleanInRegisterFile);
        return ValueSource(ValueInRegisterFile);
    }
    
    bool isSet() const
    {
        return kindFromNodeIndex(m_nodeIndex) != SourceNotSet;
    }
    
    ValueSourceKind kind() const
    {
        return kindFromNodeIndex(m_nodeIndex);
    }
    
    NodeIndex nodeIndex() const
    {
        ASSERT(kind() == HaveNode);
        return m_nodeIndex;
    }
    
#ifndef NDEBUG
    void dump(FILE* out) const;
#endif
    
private:
    static NodeIndex nodeIndexFromKind(ValueSourceKind kind)
    {
        ASSERT(kind >= SourceNotSet && kind < HaveNode);
        return NoNode - kind;
    }
    
    static ValueSourceKind kindFromNodeIndex(NodeIndex nodeIndex)
    {
        unsigned kind = static_cast<unsigned>(NoNode - nodeIndex);
        if (kind >= static_cast<unsigned>(HaveNode))
            return HaveNode;
        return static_cast<ValueSourceKind>(kind);
    }
    
    NodeIndex m_nodeIndex;
};
    
// === SpeculativeJIT ===
//
// The SpeculativeJIT is used to generate a fast, but potentially
// incomplete code path for the dataflow. When code generating
// we may make assumptions about operand types, dynamically check,
// and bail-out to an alternate code path if these checks fail.
// Importantly, the speculative code path cannot be reentered once
// a speculative check has failed. This allows the SpeculativeJIT
// to propagate type information (including information that has
// only speculatively been asserted) through the dataflow.
class SpeculativeJIT : public JITCodeGenerator {
    friend struct OSRExit;
public:
    SpeculativeJIT(JITCompiler&);

    bool compile();
    void linkOSREntries(LinkBuffer&);

    // Called by the speculative operand types, below, to fill operand to
    // machine registers, implicitly generating speculation checks as needed.
    GPRReg fillSpeculateInt(NodeIndex, DataFormat& returnFormat);
    GPRReg fillSpeculateIntStrict(NodeIndex);
    FPRReg fillSpeculateDouble(NodeIndex);
    GPRReg fillSpeculateCell(NodeIndex);
    GPRReg fillSpeculateBoolean(NodeIndex);

private:
    friend class JITCodeGenerator;
    
    void compile(Node&);
    void compileMovHint(Node&);
    void compile(BasicBlock&);

    void checkArgumentTypes();

    bool isInteger(NodeIndex nodeIndex)
    {
        Node& node = at(nodeIndex);
        if (node.hasInt32Result())
            return true;

        if (isInt32Constant(nodeIndex))
            return true;

        VirtualRegister virtualRegister = node.virtualRegister();
        GenerationInfo& info = m_generationInfo[virtualRegister];
        
        return info.isJSInteger();
    }
    
    bool compare(Node&, MacroAssembler::RelationalCondition, MacroAssembler::DoubleCondition, S_DFGOperation_EJJ);
    bool compilePeepHoleBranch(Node&, MacroAssembler::RelationalCondition, MacroAssembler::DoubleCondition, S_DFGOperation_EJJ);
    void compilePeepHoleIntegerBranch(Node&, NodeIndex branchNodeIndex, JITCompiler::RelationalCondition);
    void compilePeepHoleDoubleBranch(Node&, NodeIndex branchNodeIndex, JITCompiler::DoubleCondition);
    void compilePeepHoleObjectEquality(Node&, NodeIndex branchNodeIndex, void* vptr, PredictionChecker);
    void compileObjectEquality(Node&, void* vptr, PredictionChecker);
    void compileValueAdd(Node&);
    void compileObjectOrOtherLogicalNot(NodeIndex value, void* vptr, bool needSpeculationCheck);
    void compileLogicalNot(Node&);
    void emitObjectOrOtherBranch(NodeIndex value, BlockIndex taken, BlockIndex notTaken, void *vptr, bool needSpeculationCheck);
    void emitBranch(Node&);
    
    void compileGetCharCodeAt(Node&);
    void compileGetByValOnString(Node&);
    void compileValueToInt32(Node&);
    void compileGetByValOnByteArray(Node&);
    void compilePutByValForByteArray(GPRReg base, GPRReg property, Node&);
    
    // It is acceptable to have structure be equal to scratch, so long as you're fine
    // with the structure GPR being clobbered.
    template<typename T>
    void emitAllocateJSFinalObject(T structure, GPRReg resultGPR, GPRReg scratchGPR, MacroAssembler::JumpList& slowPath)
    {
        MarkedSpace::SizeClass* sizeClass = &m_jit.globalData()->heap.sizeClassForObject(sizeof(JSFinalObject));
        
        m_jit.loadPtr(&sizeClass->firstFreeCell, resultGPR);
        slowPath.append(m_jit.branchTestPtr(MacroAssembler::Zero, resultGPR));
        
        // The object is half-allocated: we have what we know is a fresh object, but
        // it's still on the GC's free list.
        
        // Ditch the structure by placing it into the structure slot, so that we can reuse
        // scratchGPR.
        m_jit.storePtr(structure, MacroAssembler::Address(resultGPR, JSObject::structureOffset()));
        
        // Now that we have scratchGPR back, remove the object from the free list
        m_jit.loadPtr(MacroAssembler::Address(resultGPR), scratchGPR);
        m_jit.storePtr(scratchGPR, &sizeClass->firstFreeCell);
        
        // Initialize the object's vtable
        m_jit.storePtr(MacroAssembler::TrustedImmPtr(m_jit.globalData()->jsFinalObjectVPtr), MacroAssembler::Address(resultGPR));
        
        // Initialize the object's inheritorID.
        m_jit.storePtr(MacroAssembler::TrustedImmPtr(0), MacroAssembler::Address(resultGPR, JSObject::offsetOfInheritorID()));
        
        // Initialize the object's property storage pointer.
        m_jit.addPtr(MacroAssembler::TrustedImm32(sizeof(JSObject)), resultGPR, scratchGPR);
        m_jit.storePtr(scratchGPR, MacroAssembler::Address(resultGPR, JSFinalObject::offsetOfPropertyStorage()));
    }

#if USE(JSVALUE64) 
    JITCompiler::Jump convertToDouble(GPRReg value, FPRReg result, GPRReg tmp);
#elif USE(JSVALUE32_64)
    JITCompiler::Jump convertToDouble(JSValueOperand&, FPRReg result);
#endif

    // Add a speculation check without additional recovery.
    void speculationCheck(JSValueSource jsValueSource, NodeIndex nodeIndex, MacroAssembler::Jump jumpToFail)
    {
        if (!m_compileOkay)
            return;
        m_jit.codeBlock()->appendOSRExit(OSRExit(jsValueSource, m_jit.valueProfileFor(nodeIndex), jumpToFail, this));
    }
    // Add a speculation check with additional recovery.
    void speculationCheck(JSValueSource jsValueSource, NodeIndex nodeIndex, MacroAssembler::Jump jumpToFail, const SpeculationRecovery& recovery)
    {
        if (!m_compileOkay)
            return;
        m_jit.codeBlock()->appendSpeculationRecovery(recovery);
        m_jit.codeBlock()->appendOSRExit(OSRExit(jsValueSource, m_jit.valueProfileFor(nodeIndex), jumpToFail, this, m_jit.codeBlock()->numberOfSpeculationRecoveries()));
    }

    // Called when we statically determine that a speculation will fail.
    void terminateSpeculativeExecution(JSValueRegs jsValueRegs, NodeIndex nodeIndex)
    {
#if DFG_ENABLE(DEBUG_VERBOSE)
        fprintf(stderr, "SpeculativeJIT was terminated.\n");
#endif
        if (!m_compileOkay)
            return;
        speculationCheck(jsValueRegs, nodeIndex, m_jit.jump());
        m_compileOkay = false;
    }

    template<bool strict>
    GPRReg fillSpeculateIntInternal(NodeIndex, DataFormat& returnFormat);
    
    // It is possible, during speculative generation, to reach a situation in which we
    // can statically determine a speculation will fail (for example, when two nodes
    // will make conflicting speculations about the same operand). In such cases this
    // flag is cleared, indicating no further code generation should take place.
    bool m_compileOkay;
    
    // Tracking for which nodes are currently holding the values of arguments and bytecode
    // operand-indexed variables.
    
    ValueSource valueSourceForOperand(int operand)
    {
        return valueSourceReferenceForOperand(operand);
    }
    
    void setNodeIndexForOperand(NodeIndex nodeIndex, int operand)
    {
        valueSourceReferenceForOperand(operand) = ValueSource(nodeIndex);
    }
    
    // Call this with care, since it both returns a reference into an array
    // and potentially resizes the array. So it would not be right to call this
    // twice and then perform operands on both references, since the one from
    // the first call may no longer be valid.
    ValueSource& valueSourceReferenceForOperand(int operand)
    {
        if (operandIsArgument(operand)) {
            int argument = operand + m_arguments.size() + RegisterFile::CallFrameHeaderSize;
            return m_arguments[argument];
        }
        
        if ((unsigned)operand >= m_variables.size())
            m_variables.resize(operand + 1);
        
        return m_variables[operand];
    }
    
    Vector<ValueSource, 0> m_arguments;
    Vector<ValueSource, 0> m_variables;
    int m_lastSetOperand;
    CodeOrigin m_codeOriginForOSR;
    
    AbstractState m_state;
    
    ValueRecovery computeValueRecoveryFor(const ValueSource&);

    ValueRecovery computeValueRecoveryFor(int operand)
    {
        return computeValueRecoveryFor(valueSourceForOperand(operand));
    }
};


// === Speculative Operand types ===
//
// SpeculateIntegerOperand, SpeculateStrictInt32Operand and SpeculateCellOperand.
//
// These are used to lock the operands to a node into machine registers within the
// SpeculativeJIT. The classes operate like those provided by the JITCodeGenerator,
// however these will perform a speculative check for a more restrictive type than
// we can statically determine the operand to have. If the operand does not have
// the requested type, a bail-out to the non-speculative path will be taken.

class SpeculateIntegerOperand {
public:
    explicit SpeculateIntegerOperand(SpeculativeJIT* jit, NodeIndex index)
        : m_jit(jit)
        , m_index(index)
        , m_gprOrInvalid(InvalidGPRReg)
#ifndef NDEBUG
        , m_format(DataFormatNone)
#endif
    {
        ASSERT(m_jit);
        if (jit->isFilled(index))
            gpr();
    }

    ~SpeculateIntegerOperand()
    {
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }

    NodeIndex index() const
    {
        return m_index;
    }

    DataFormat format()
    {
        gpr(); // m_format is set when m_gpr is locked.
        ASSERT(m_format == DataFormatInteger || m_format == DataFormatJSInteger);
        return m_format;
    }

    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillSpeculateInt(index(), m_format);
        return m_gprOrInvalid;
    }

private:
    SpeculativeJIT* m_jit;
    NodeIndex m_index;
    GPRReg m_gprOrInvalid;
    DataFormat m_format;
};

class SpeculateStrictInt32Operand {
public:
    explicit SpeculateStrictInt32Operand(SpeculativeJIT* jit, NodeIndex index)
        : m_jit(jit)
        , m_index(index)
        , m_gprOrInvalid(InvalidGPRReg)
    {
        ASSERT(m_jit);
        if (jit->isFilled(index))
            gpr();
    }

    ~SpeculateStrictInt32Operand()
    {
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }

    NodeIndex index() const
    {
        return m_index;
    }

    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillSpeculateIntStrict(index());
        return m_gprOrInvalid;
    }
    
    void use()
    {
        m_jit->use(m_index);
    }

private:
    SpeculativeJIT* m_jit;
    NodeIndex m_index;
    GPRReg m_gprOrInvalid;
};

class SpeculateDoubleOperand {
public:
    explicit SpeculateDoubleOperand(SpeculativeJIT* jit, NodeIndex index)
        : m_jit(jit)
        , m_index(index)
        , m_fprOrInvalid(InvalidFPRReg)
    {
        ASSERT(m_jit);
        if (jit->isFilled(index))
            fpr();
    }

    ~SpeculateDoubleOperand()
    {
        ASSERT(m_fprOrInvalid != InvalidFPRReg);
        m_jit->unlock(m_fprOrInvalid);
    }

    NodeIndex index() const
    {
        return m_index;
    }

    FPRReg fpr()
    {
        if (m_fprOrInvalid == InvalidFPRReg)
            m_fprOrInvalid = m_jit->fillSpeculateDouble(index());
        return m_fprOrInvalid;
    }

private:
    SpeculativeJIT* m_jit;
    NodeIndex m_index;
    FPRReg m_fprOrInvalid;
};

class SpeculateCellOperand {
public:
    explicit SpeculateCellOperand(SpeculativeJIT* jit, NodeIndex index)
        : m_jit(jit)
        , m_index(index)
        , m_gprOrInvalid(InvalidGPRReg)
    {
        ASSERT(m_jit);
        if (jit->isFilled(index))
            gpr();
    }

    ~SpeculateCellOperand()
    {
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }

    NodeIndex index() const
    {
        return m_index;
    }

    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillSpeculateCell(index());
        return m_gprOrInvalid;
    }
    
    void use()
    {
        m_jit->use(m_index);
    }

private:
    SpeculativeJIT* m_jit;
    NodeIndex m_index;
    GPRReg m_gprOrInvalid;
};

class SpeculateBooleanOperand {
public:
    explicit SpeculateBooleanOperand(SpeculativeJIT* jit, NodeIndex index)
        : m_jit(jit)
        , m_index(index)
        , m_gprOrInvalid(InvalidGPRReg)
    {
        ASSERT(m_jit);
        if (jit->isFilled(index))
            gpr();
    }
    
    ~SpeculateBooleanOperand()
    {
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }
    
    NodeIndex index() const
    {
        return m_index;
    }
    
    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillSpeculateBoolean(index());
        return m_gprOrInvalid;
    }
    
    void use()
    {
        m_jit->use(m_index);
    }

private:
    SpeculativeJIT* m_jit;
    NodeIndex m_index;
    GPRReg m_gprOrInvalid;
};

inline SpeculativeJIT::SpeculativeJIT(JITCompiler& jit)
    : JITCodeGenerator(jit)
    , m_compileOkay(true)
    , m_arguments(jit.codeBlock()->m_numParameters)
    , m_variables(jit.graph().m_localVars)
    , m_lastSetOperand(std::numeric_limits<int>::max())
    , m_state(m_jit.codeBlock(), m_jit.graph())
{
}

} } // namespace JSC::DFG

#endif
#endif

