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

#include <dfg/DFGJITCodeGenerator.h>

namespace JSC { namespace DFG {

class SpeculativeJIT;

// This enum describes the types of additional recovery that
// may need be performed should a speculation check fail.
enum SpeculationRecoveryType {
    SpeculativeAdd
};

// === SpeculationRecovery ===
//
// This class provides additional information that may be associated with a
// speculation check - for example 
class SpeculationRecovery {
public:
    SpeculationRecovery(SpeculationRecoveryType type, GPRReg dest, GPRReg src)
        : m_type(type)
        , m_dest(dest)
        , m_src(src)
    {
    }

    SpeculationRecoveryType type() { return m_type; }
    GPRReg dest() { return m_dest; }
    GPRReg src() { return m_src; }

private:
    // Indicates the type of additional recovery to be performed.
    SpeculationRecoveryType m_type;
    // different recovery types may required different additional information here.
    GPRReg m_dest;
    GPRReg m_src;
};

// === SpeculationCheck ===
//
// This structure records a bail-out from the speculative path,
// which will need to be linked in to the non-speculative one.
struct SpeculationCheck {
    SpeculationCheck(MacroAssembler::Jump, SpeculativeJIT*, unsigned recoveryIndex = 0);

    // The location of the jump out from the speculative path, 
    // and the node we were generating code for.
    MacroAssembler::Jump m_check;
    NodeIndex m_nodeIndex;
    // Used to record any additional recovery to be performed; this
    // value is an index into the SpeculativeJIT's m_speculationRecoveryList
    // array, offset by 1. (m_recoveryIndex == 0) means no recovery.
    unsigned m_recoveryIndex;

    struct RegisterInfo {
        NodeIndex nodeIndex;
        DataFormat format;
        bool isSpilled;
    };
    RegisterInfo m_gprInfo[GPRInfo::numberOfRegisters];
    RegisterInfo m_fprInfo[FPRInfo::numberOfRegisters];
};
typedef SegmentedVector<SpeculationCheck, 16> SpeculationCheckVector;


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
    friend struct SpeculationCheck;
public:
    SpeculativeJIT(JITCompiler& jit)
        : JITCodeGenerator(jit, true)
        , m_compileOkay(true)
    {
    }

    bool compile();

    // Retrieve the list of bail-outs from the speculative path,
    // and additional recovery information.
    SpeculationCheckVector& speculationChecks()
    {
        return m_speculationChecks;
    }
    SpeculationRecovery* speculationRecovery(size_t index)
    {
        // SpeculationCheck::m_recoveryIndex is offset by 1,
        // 0 means no recovery.
        return index ? &m_speculationRecoveryList[index - 1] : 0;
    }

    // Called by the speculative operand types, below, to fill operand to
    // machine registers, implicitly generating speculation checks as needed.
    GPRReg fillSpeculateInt(NodeIndex, DataFormat& returnFormat);
    GPRReg fillSpeculateIntStrict(NodeIndex);
    FPRReg fillSpeculateDouble(NodeIndex);
    GPRReg fillSpeculateCell(NodeIndex);

private:
    void compile(Node&);
    void compile(BasicBlock&);

    void checkArgumentTypes();
    void initializeVariableTypes();

    bool isInteger(NodeIndex nodeIndex)
    {
        Node& node = m_jit.graph()[nodeIndex];
        if (node.hasInt32Result())
            return true;

        if (isInt32Constant(nodeIndex))
            return true;

        VirtualRegister virtualRegister = node.virtualRegister();
        GenerationInfo& info = m_generationInfo[virtualRegister];
        
        return (info.registerFormat() | DataFormatJS) == DataFormatJSInteger
            || (info.spillFormat() | DataFormatJS) == DataFormatJSInteger;
    }
    
    bool shouldSpeculateInteger(NodeIndex nodeIndex)
    {
        if (isInteger(nodeIndex))
            return true;
        
        if (isInt32Prediction(m_jit.graph().getPrediction(m_jit.graph()[nodeIndex])))
            return true;
        
        return false;
    }

    bool shouldSpeculateDouble(NodeIndex nodeIndex)
    {
        Node& node = m_jit.graph()[nodeIndex];
        VirtualRegister virtualRegister = node.virtualRegister();
        GenerationInfo& info = m_generationInfo[virtualRegister];

        if ((info.registerFormat() | DataFormatJS) == DataFormatJSDouble
            || (info.spillFormat() | DataFormatJS) == DataFormatJSDouble)
            return true;
        
        if (isDoublePrediction(m_jit.graph().getPrediction(node)))
            return true;
        
        return false;
    }
    
    bool shouldSpeculateInteger(NodeIndex op1, NodeIndex op2)
    {
        return !(shouldSpeculateDouble(op1) || shouldSpeculateDouble(op2)) && (shouldSpeculateInteger(op1) || shouldSpeculateInteger(op2));
    }

    bool compare(Node&, MacroAssembler::RelationalCondition, MacroAssembler::DoubleCondition, Z_DFGOperation_EJJ);
    void compilePeepHoleIntegerBranch(Node&, NodeIndex branchNodeIndex, JITCompiler::RelationalCondition);
    void compilePeepHoleDoubleBranch(Node&, NodeIndex branchNodeIndex, JITCompiler::DoubleCondition, Z_DFGOperation_EJJ);
    
    JITCompiler::Jump convertToDouble(GPRReg value, FPRReg result, GPRReg tmp);

    // Add a speculation check without additional recovery.
    void speculationCheck(MacroAssembler::Jump jumpToFail)
    {
        if (!m_compileOkay)
            return;
        m_speculationChecks.append(SpeculationCheck(jumpToFail, this));
    }
    // Add a speculation check with additional recovery.
    void speculationCheck(MacroAssembler::Jump jumpToFail, const SpeculationRecovery& recovery)
    {
        if (!m_compileOkay)
            return;
        m_speculationRecoveryList.append(recovery);
        m_speculationChecks.append(SpeculationCheck(jumpToFail, this, m_speculationRecoveryList.size()));
    }

    // Called when we statically determine that a speculation will fail.
    void terminateSpeculativeExecution()
    {
        m_compileOkay = false;
    }

    template<bool strict>
    GPRReg fillSpeculateIntInternal(NodeIndex, DataFormat& returnFormat);

    // It is possible, during speculative generation, to reach a situation in which we
    // can statically determine a speculation will fail (for example, when two nodes
    // will make conflicting speculations about the same operand). In such cases this
    // flag is cleared, indicating no further code generation should take place.
    bool m_compileOkay;
    // This vector tracks bail-outs from the speculative path to the non-speculative one.
    SpeculationCheckVector m_speculationChecks;
    // Some bail-outs need to record additional information recording specific recovery
    // to be performed (for example, on detected overflow from an add, we may need to
    // reverse the addition if an operand is being overwritten).
    Vector<SpeculationRecovery, 16> m_speculationRecoveryList;
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


// === SpeculationCheckIndexIterator ===
//
// This class is used by the non-speculative JIT to check which
// nodes require entry points from the speculative path.
class SpeculationCheckIndexIterator {
public:
    SpeculationCheckIndexIterator(SpeculationCheckVector& speculationChecks)
        : m_speculationChecks(speculationChecks)
        , m_iter(m_speculationChecks.begin())
        , m_end(m_speculationChecks.end())
    {
    }

    bool hasCheckAtIndex(NodeIndex nodeIndex)
    {
        while (m_iter != m_end) {
            NodeIndex current = m_iter->m_nodeIndex;
            if (current >= nodeIndex)
                return current == nodeIndex;
            ++m_iter;
        }
        return false;
    }

private:
    SpeculationCheckVector& m_speculationChecks;
    SpeculationCheckVector::Iterator m_iter;
    SpeculationCheckVector::Iterator m_end;
};


} } // namespace JSC::DFG

#endif
#endif

