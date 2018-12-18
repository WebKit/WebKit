/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(DFG_JIT)

#include "BlockDirectory.h"
#include "DFGAbstractInterpreter.h"
#include "DFGGenerationInfo.h"
#include "DFGInPlaceAbstractState.h"
#include "DFGJITCompiler.h"
#include "DFGOSRExit.h"
#include "DFGOSRExitJumpPlaceholder.h"
#include "DFGRegisterBank.h"
#include "DFGSilentRegisterSavePlan.h"
#include "JITMathIC.h"
#include "JITOperations.h"
#include "PutKind.h"
#include "SpillRegistersMode.h"
#include "StructureStubInfo.h"
#include "ValueRecovery.h"
#include "VirtualRegister.h"

namespace JSC { namespace DFG {

class GPRTemporary;
class JSValueOperand;
class SlowPathGenerator;
class SpeculativeJIT;
class SpeculateInt32Operand;
class SpeculateStrictInt32Operand;
class SpeculateDoubleOperand;
class SpeculateCellOperand;
class SpeculateBooleanOperand;

enum GeneratedOperandType { GeneratedOperandTypeUnknown, GeneratedOperandInteger, GeneratedOperandJSValue};

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
class SpeculativeJIT {
    WTF_MAKE_FAST_ALLOCATED;

    friend struct OSRExit;
private:
    typedef JITCompiler::TrustedImm32 TrustedImm32;
    typedef JITCompiler::Imm32 Imm32;
    typedef JITCompiler::ImmPtr ImmPtr;
    typedef JITCompiler::TrustedImm64 TrustedImm64;
    typedef JITCompiler::Imm64 Imm64;

    // These constants are used to set priorities for spill order for
    // the register allocator.
#if USE(JSVALUE64)
    enum SpillOrder {
        SpillOrderConstant = 1, // no spill, and cheap fill
        SpillOrderSpilled  = 2, // no spill
        SpillOrderJS       = 4, // needs spill
        SpillOrderCell     = 4, // needs spill
        SpillOrderStorage  = 4, // needs spill
        SpillOrderInteger  = 5, // needs spill and box
        SpillOrderBoolean  = 5, // needs spill and box
        SpillOrderDouble   = 6, // needs spill and convert
    };
#elif USE(JSVALUE32_64)
    enum SpillOrder {
        SpillOrderConstant = 1, // no spill, and cheap fill
        SpillOrderSpilled  = 2, // no spill
        SpillOrderJS       = 4, // needs spill
        SpillOrderStorage  = 4, // needs spill
        SpillOrderDouble   = 4, // needs spill
        SpillOrderInteger  = 5, // needs spill and box
        SpillOrderCell     = 5, // needs spill and box
        SpillOrderBoolean  = 5, // needs spill and box
    };
#endif

    enum UseChildrenMode { CallUseChildren, UseChildrenCalledExplicitly };
    
public:
    SpeculativeJIT(JITCompiler&);
    ~SpeculativeJIT();

    VM& vm()
    {
        return *m_jit.vm();
    }

    struct TrustedImmPtr {
        template <typename T>
        explicit TrustedImmPtr(T* value)
            : m_value(value)
        {
            static_assert(!std::is_base_of<JSCell, T>::value, "To use a GC pointer, the graph must be aware of it. Use SpeculativeJIT::TrustedImmPtr::weakPointer instead.");
        }

        explicit TrustedImmPtr(RegisteredStructure structure)
            : m_value(structure.get())
        { }
        
        explicit TrustedImmPtr(std::nullptr_t)
            : m_value(nullptr)
        { }

        explicit TrustedImmPtr(FrozenValue* value)
            : m_value(value->cell())
        {
            RELEASE_ASSERT(value->value().isCell());
        }

        explicit TrustedImmPtr(size_t value)
            : m_value(bitwise_cast<void*>(value))
        {
        }

        static TrustedImmPtr weakPointer(Graph& graph, JSCell* cell)
        {     
            graph.m_plan.weakReferences().addLazily(cell);
            return TrustedImmPtr(bitwise_cast<size_t>(cell));
        }

        template<typename Key>
        static TrustedImmPtr weakPoisonedPointer(Graph& graph, JSCell* cell)
        {     
            graph.m_plan.weakReferences().addLazily(cell);
            return TrustedImmPtr(bitwise_cast<size_t>(cell) ^ Key::key());
        }

        operator MacroAssembler::TrustedImmPtr() const { return m_value; }
        operator MacroAssembler::TrustedImm() const { return m_value; }

        intptr_t asIntptr()
        {
            return m_value.asIntptr();
        }

    private:
        MacroAssembler::TrustedImmPtr m_value;
    };

    bool compile();
    
    void createOSREntries();
    void linkOSREntries(LinkBuffer&);

    BasicBlock* nextBlock()
    {
        for (BlockIndex resultIndex = m_block->index + 1; ; resultIndex++) {
            if (resultIndex >= m_jit.graph().numBlocks())
                return 0;
            if (BasicBlock* result = m_jit.graph().block(resultIndex))
                return result;
        }
    }
    
#if USE(JSVALUE64)
    GPRReg fillJSValue(Edge);
#elif USE(JSVALUE32_64)
    bool fillJSValue(Edge, GPRReg&, GPRReg&, FPRReg&);
#endif
    GPRReg fillStorage(Edge);

    // lock and unlock GPR & FPR registers.
    void lock(GPRReg reg)
    {
        m_gprs.lock(reg);
    }
    void lock(FPRReg reg)
    {
        m_fprs.lock(reg);
    }
    void unlock(GPRReg reg)
    {
        m_gprs.unlock(reg);
    }
    void unlock(FPRReg reg)
    {
        m_fprs.unlock(reg);
    }

    // Used to check whether a child node is on its last use,
    // and its machine registers may be reused.
    bool canReuse(Node* node)
    {
        return generationInfo(node).useCount() == 1;
    }
    bool canReuse(Node* nodeA, Node* nodeB)
    {
        return nodeA == nodeB && generationInfo(nodeA).useCount() == 2;
    }
    bool canReuse(Edge nodeUse)
    {
        return canReuse(nodeUse.node());
    }
    GPRReg reuse(GPRReg reg)
    {
        m_gprs.lock(reg);
        return reg;
    }
    FPRReg reuse(FPRReg reg)
    {
        m_fprs.lock(reg);
        return reg;
    }

    // Allocate a gpr/fpr.
    GPRReg allocate()
    {
#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
        m_jit.addRegisterAllocationAtOffset(m_jit.debugOffset());
#endif
        VirtualRegister spillMe;
        GPRReg gpr = m_gprs.allocate(spillMe);
        if (spillMe.isValid()) {
#if USE(JSVALUE32_64)
            GenerationInfo& info = generationInfoFromVirtualRegister(spillMe);
            if ((info.registerFormat() & DataFormatJS))
                m_gprs.release(info.tagGPR() == gpr ? info.payloadGPR() : info.tagGPR());
#endif
            spill(spillMe);
        }
        return gpr;
    }
    GPRReg allocate(GPRReg specific)
    {
#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
        m_jit.addRegisterAllocationAtOffset(m_jit.debugOffset());
#endif
        VirtualRegister spillMe = m_gprs.allocateSpecific(specific);
        if (spillMe.isValid()) {
#if USE(JSVALUE32_64)
            GenerationInfo& info = generationInfoFromVirtualRegister(spillMe);
            RELEASE_ASSERT(info.registerFormat() != DataFormatJSDouble);
            if ((info.registerFormat() & DataFormatJS))
                m_gprs.release(info.tagGPR() == specific ? info.payloadGPR() : info.tagGPR());
#endif
            spill(spillMe);
        }
        return specific;
    }
    GPRReg tryAllocate()
    {
        return m_gprs.tryAllocate();
    }
    FPRReg fprAllocate()
    {
#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
        m_jit.addRegisterAllocationAtOffset(m_jit.debugOffset());
#endif
        VirtualRegister spillMe;
        FPRReg fpr = m_fprs.allocate(spillMe);
        if (spillMe.isValid())
            spill(spillMe);
        return fpr;
    }

    // Check whether a VirtualRegsiter is currently in a machine register.
    // We use this when filling operands to fill those that are already in
    // machine registers first (by locking VirtualRegsiters that are already
    // in machine register before filling those that are not we attempt to
    // avoid spilling values we will need immediately).
    bool isFilled(Node* node)
    {
        return generationInfo(node).registerFormat() != DataFormatNone;
    }
    bool isFilledDouble(Node* node)
    {
        return generationInfo(node).registerFormat() == DataFormatDouble;
    }

    // Called on an operand once it has been consumed by a parent node.
    void use(Node* node)
    {
        if (!node->hasResult())
            return;
        GenerationInfo& info = generationInfo(node);

        // use() returns true when the value becomes dead, and any
        // associated resources may be freed.
        if (!info.use(*m_stream))
            return;

        // Release the associated machine registers.
        DataFormat registerFormat = info.registerFormat();
#if USE(JSVALUE64)
        if (registerFormat == DataFormatDouble)
            m_fprs.release(info.fpr());
        else if (registerFormat != DataFormatNone)
            m_gprs.release(info.gpr());
#elif USE(JSVALUE32_64)
        if (registerFormat == DataFormatDouble)
            m_fprs.release(info.fpr());
        else if (registerFormat & DataFormatJS) {
            m_gprs.release(info.tagGPR());
            m_gprs.release(info.payloadGPR());
        } else if (registerFormat != DataFormatNone)
            m_gprs.release(info.gpr());
#endif
    }
    void use(Edge nodeUse)
    {
        use(nodeUse.node());
    }
    
    RegisterSet usedRegisters();
    
    bool masqueradesAsUndefinedWatchpointIsStillValid(const CodeOrigin& codeOrigin)
    {
        return m_jit.graph().masqueradesAsUndefinedWatchpointIsStillValid(codeOrigin);
    }
    bool masqueradesAsUndefinedWatchpointIsStillValid()
    {
        return masqueradesAsUndefinedWatchpointIsStillValid(m_currentNode->origin.semantic);
    }

    void compileStoreBarrier(Node*);

    // Called by the speculative operand types, below, to fill operand to
    // machine registers, implicitly generating speculation checks as needed.
    GPRReg fillSpeculateInt32(Edge, DataFormat& returnFormat);
    GPRReg fillSpeculateInt32Strict(Edge);
    GPRReg fillSpeculateInt52(Edge, DataFormat desiredFormat);
    FPRReg fillSpeculateDouble(Edge);
    GPRReg fillSpeculateCell(Edge);
    GPRReg fillSpeculateBoolean(Edge);
    GeneratedOperandType checkGeneratedTypeForToInt32(Node*);

    void addSlowPathGenerator(std::unique_ptr<SlowPathGenerator>);
    void addSlowPathGeneratorLambda(Function<void()>&&);
    void runSlowPathGenerators(PCToCodeOriginMapBuilder&);
    
    void compile(Node*);
    void noticeOSRBirth(Node*);
    void bail(AbortReason);
    void compileCurrentBlock();

    void checkArgumentTypes();

    void clearGenerationInfo();

    // These methods are used when generating 'unexpected'
    // calls out from JIT code to C++ helper routines -
    // they spill all live values to the appropriate
    // slots in the JSStack without changing any state
    // in the GenerationInfo.
    SilentRegisterSavePlan silentSavePlanForGPR(VirtualRegister spillMe, GPRReg source);
    SilentRegisterSavePlan silentSavePlanForFPR(VirtualRegister spillMe, FPRReg source);
    void silentSpill(const SilentRegisterSavePlan&);
    void silentFill(const SilentRegisterSavePlan&);

    template<typename CollectionType>
    void silentSpill(const CollectionType& savePlans)
    {
        for (unsigned i = 0; i < savePlans.size(); ++i)
            silentSpill(savePlans[i]);
    }

    template<typename CollectionType>
    void silentFill(const CollectionType& savePlans)
    {
        for (unsigned i = savePlans.size(); i--;)
            silentFill(savePlans[i]);
    }

    template<typename CollectionType>
    void silentSpillAllRegistersImpl(bool doSpill, CollectionType& plans, GPRReg exclude, GPRReg exclude2 = InvalidGPRReg, FPRReg fprExclude = InvalidFPRReg)
    {
        ASSERT(plans.isEmpty());
        for (gpr_iterator iter = m_gprs.begin(); iter != m_gprs.end(); ++iter) {
            GPRReg gpr = iter.regID();
            if (iter.name().isValid() && gpr != exclude && gpr != exclude2) {
                SilentRegisterSavePlan plan = silentSavePlanForGPR(iter.name(), gpr);
                if (doSpill)
                    silentSpill(plan);
                plans.append(plan);
            }
        }
        for (fpr_iterator iter = m_fprs.begin(); iter != m_fprs.end(); ++iter) {
            if (iter.name().isValid() && iter.regID() != fprExclude) {
                SilentRegisterSavePlan plan = silentSavePlanForFPR(iter.name(), iter.regID());
                if (doSpill)
                    silentSpill(plan);
                plans.append(plan);
            }
        }
    }
    template<typename CollectionType>
    void silentSpillAllRegistersImpl(bool doSpill, CollectionType& plans, NoResultTag)
    {
        silentSpillAllRegistersImpl(doSpill, plans, InvalidGPRReg, InvalidGPRReg, InvalidFPRReg);
    }
    template<typename CollectionType>
    void silentSpillAllRegistersImpl(bool doSpill, CollectionType& plans, FPRReg exclude)
    {
        silentSpillAllRegistersImpl(doSpill, plans, InvalidGPRReg, InvalidGPRReg, exclude);
    }
    template<typename CollectionType>
    void silentSpillAllRegistersImpl(bool doSpill, CollectionType& plans, JSValueRegs exclude)
    {
#if USE(JSVALUE32_64)
        silentSpillAllRegistersImpl(doSpill, plans, exclude.tagGPR(), exclude.payloadGPR());
#else
        silentSpillAllRegistersImpl(doSpill, plans, exclude.gpr());
#endif
    }
    
    void silentSpillAllRegisters(GPRReg exclude, GPRReg exclude2 = InvalidGPRReg, FPRReg fprExclude = InvalidFPRReg)
    {
        silentSpillAllRegistersImpl(true, m_plans, exclude, exclude2, fprExclude);
    }
    void silentSpillAllRegisters(FPRReg exclude)
    {
        silentSpillAllRegisters(InvalidGPRReg, InvalidGPRReg, exclude);
    }
    void silentSpillAllRegisters(JSValueRegs exclude)
    {
#if USE(JSVALUE64)
        silentSpillAllRegisters(exclude.payloadGPR());
#else
        silentSpillAllRegisters(exclude.payloadGPR(), exclude.tagGPR());
#endif
    }

    void silentFillAllRegisters()
    {
        while (!m_plans.isEmpty()) {
            SilentRegisterSavePlan& plan = m_plans.last();
            silentFill(plan);
            m_plans.removeLast();
        }
    }

    // These methods convert between doubles, and doubles boxed and JSValues.
#if USE(JSVALUE64)
    GPRReg boxDouble(FPRReg fpr, GPRReg gpr)
    {
        return m_jit.boxDouble(fpr, gpr);
    }
    FPRReg unboxDouble(GPRReg gpr, GPRReg resultGPR, FPRReg fpr)
    {
        return m_jit.unboxDouble(gpr, resultGPR, fpr);
    }
    GPRReg boxDouble(FPRReg fpr)
    {
        return boxDouble(fpr, allocate());
    }
    
    void boxInt52(GPRReg sourceGPR, GPRReg targetGPR, DataFormat);
#elif USE(JSVALUE32_64)
    void boxDouble(FPRReg fpr, GPRReg tagGPR, GPRReg payloadGPR)
    {
        m_jit.boxDouble(fpr, tagGPR, payloadGPR);
    }
    void unboxDouble(GPRReg tagGPR, GPRReg payloadGPR, FPRReg fpr, FPRReg scratchFPR)
    {
        m_jit.unboxDouble(tagGPR, payloadGPR, fpr, scratchFPR);
    }
#endif
    void boxDouble(FPRReg fpr, JSValueRegs regs)
    {
        m_jit.boxDouble(fpr, regs);
    }

    // Spill a VirtualRegister to the JSStack.
    void spill(VirtualRegister spillMe)
    {
        GenerationInfo& info = generationInfoFromVirtualRegister(spillMe);

#if USE(JSVALUE32_64)
        if (info.registerFormat() == DataFormatNone) // it has been spilled. JS values which have two GPRs can reach here
            return;
#endif
        // Check the GenerationInfo to see if this value need writing
        // to the JSStack - if not, mark it as spilled & return.
        if (!info.needsSpill()) {
            info.setSpilled(*m_stream, spillMe);
            return;
        }

        DataFormat spillFormat = info.registerFormat();
        switch (spillFormat) {
        case DataFormatStorage: {
            // This is special, since it's not a JS value - as in it's not visible to JS
            // code.
            m_jit.storePtr(info.gpr(), JITCompiler::addressFor(spillMe));
            info.spill(*m_stream, spillMe, DataFormatStorage);
            return;
        }

        case DataFormatInt32: {
            m_jit.store32(info.gpr(), JITCompiler::payloadFor(spillMe));
            info.spill(*m_stream, spillMe, DataFormatInt32);
            return;
        }

#if USE(JSVALUE64)
        case DataFormatDouble: {
            m_jit.storeDouble(info.fpr(), JITCompiler::addressFor(spillMe));
            info.spill(*m_stream, spillMe, DataFormatDouble);
            return;
        }

        case DataFormatInt52:
        case DataFormatStrictInt52: {
            m_jit.store64(info.gpr(), JITCompiler::addressFor(spillMe));
            info.spill(*m_stream, spillMe, spillFormat);
            return;
        }
            
        default:
            // The following code handles JSValues, int32s, and cells.
            RELEASE_ASSERT(spillFormat == DataFormatCell || spillFormat & DataFormatJS);
            
            GPRReg reg = info.gpr();
            // We need to box int32 and cell values ...
            // but on JSVALUE64 boxing a cell is a no-op!
            if (spillFormat == DataFormatInt32)
                m_jit.or64(GPRInfo::tagTypeNumberRegister, reg);
            
            // Spill the value, and record it as spilled in its boxed form.
            m_jit.store64(reg, JITCompiler::addressFor(spillMe));
            info.spill(*m_stream, spillMe, (DataFormat)(spillFormat | DataFormatJS));
            return;
#elif USE(JSVALUE32_64)
        case DataFormatCell:
        case DataFormatBoolean: {
            m_jit.store32(info.gpr(), JITCompiler::payloadFor(spillMe));
            info.spill(*m_stream, spillMe, spillFormat);
            return;
        }

        case DataFormatDouble: {
            // On JSVALUE32_64 boxing a double is a no-op.
            m_jit.storeDouble(info.fpr(), JITCompiler::addressFor(spillMe));
            info.spill(*m_stream, spillMe, DataFormatDouble);
            return;
        }

        default:
            // The following code handles JSValues.
            RELEASE_ASSERT(spillFormat & DataFormatJS);
            m_jit.store32(info.tagGPR(), JITCompiler::tagFor(spillMe));
            m_jit.store32(info.payloadGPR(), JITCompiler::payloadFor(spillMe));
            info.spill(*m_stream, spillMe, spillFormat);
            return;
#endif
        }
    }
    
    bool isKnownInteger(Node* node) { return m_state.forNode(node).isType(SpecInt32Only); }
    bool isKnownCell(Node* node) { return m_state.forNode(node).isType(SpecCell); }
    
    bool isKnownNotInteger(Node* node) { return !(m_state.forNode(node).m_type & SpecInt32Only); }
    bool isKnownNotNumber(Node* node) { return !(m_state.forNode(node).m_type & SpecFullNumber); }
    bool isKnownNotCell(Node* node) { return !(m_state.forNode(node).m_type & SpecCell); }
    bool isKnownNotOther(Node* node) { return !(m_state.forNode(node).m_type & SpecOther); }
    
    UniquedStringImpl* identifierUID(unsigned index)
    {
        return m_jit.graph().identifiers()[index];
    }

    // Spill all VirtualRegisters back to the JSStack.
    void flushRegisters()
    {
        for (gpr_iterator iter = m_gprs.begin(); iter != m_gprs.end(); ++iter) {
            if (iter.name().isValid()) {
                spill(iter.name());
                iter.release();
            }
        }
        for (fpr_iterator iter = m_fprs.begin(); iter != m_fprs.end(); ++iter) {
            if (iter.name().isValid()) {
                spill(iter.name());
                iter.release();
            }
        }
    }

    // Used to ASSERT flushRegisters() has been called prior to
    // calling out from JIT code to a C helper function.
    bool isFlushed()
    {
        for (gpr_iterator iter = m_gprs.begin(); iter != m_gprs.end(); ++iter) {
            if (iter.name().isValid())
                return false;
        }
        for (fpr_iterator iter = m_fprs.begin(); iter != m_fprs.end(); ++iter) {
            if (iter.name().isValid())
                return false;
        }
        return true;
    }

#if USE(JSVALUE64)
    static MacroAssembler::Imm64 valueOfJSConstantAsImm64(Node* node)
    {
        return MacroAssembler::Imm64(JSValue::encode(node->asJSValue()));
    }
#endif

    // Helper functions to enable code sharing in implementations of bit/shift ops.
    void bitOp(NodeType op, int32_t imm, GPRReg op1, GPRReg result)
    {
        switch (op) {
        case ArithBitAnd:
            m_jit.and32(Imm32(imm), op1, result);
            break;
        case ArithBitOr:
            m_jit.or32(Imm32(imm), op1, result);
            break;
        case ArithBitXor:
            m_jit.xor32(Imm32(imm), op1, result);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    void bitOp(NodeType op, GPRReg op1, GPRReg op2, GPRReg result)
    {
        switch (op) {
        case ArithBitAnd:
            m_jit.and32(op1, op2, result);
            break;
        case ArithBitOr:
            m_jit.or32(op1, op2, result);
            break;
        case ArithBitXor:
            m_jit.xor32(op1, op2, result);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    void shiftOp(NodeType op, GPRReg op1, int32_t shiftAmount, GPRReg result)
    {
        switch (op) {
        case BitRShift:
            m_jit.rshift32(op1, Imm32(shiftAmount), result);
            break;
        case BitLShift:
            m_jit.lshift32(op1, Imm32(shiftAmount), result);
            break;
        case BitURShift:
            m_jit.urshift32(op1, Imm32(shiftAmount), result);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    void shiftOp(NodeType op, GPRReg op1, GPRReg shiftAmount, GPRReg result)
    {
        switch (op) {
        case BitRShift:
            m_jit.rshift32(op1, shiftAmount, result);
            break;
        case BitLShift:
            m_jit.lshift32(op1, shiftAmount, result);
            break;
        case BitURShift:
            m_jit.urshift32(op1, shiftAmount, result);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    
    // Returns the index of the branch node if peephole is okay, UINT_MAX otherwise.
    unsigned detectPeepHoleBranch()
    {
        // Check that no intervening nodes will be generated.
        for (unsigned index = m_indexInBlock + 1; index < m_block->size() - 1; ++index) {
            Node* node = m_block->at(index);
            if (!node->shouldGenerate())
                continue;
            // Check if it's a Phantom that can be safely ignored.
            if (node->op() == Phantom && !node->child1())
                continue;
            return UINT_MAX;
        }

        // Check if the lastNode is a branch on this node.
        Node* lastNode = m_block->terminal();
        return lastNode->op() == Branch && lastNode->child1() == m_currentNode ? m_block->size() - 1 : UINT_MAX;
    }
    
    void compileCheckTraps(Node*);

    void compileMovHint(Node*);
    void compileMovHintAndCheck(Node*);

    void cachedGetById(CodeOrigin, JSValueRegs base, JSValueRegs result, unsigned identifierNumber, JITCompiler::Jump slowPathTarget, SpillRegistersMode, AccessType);
    void cachedPutById(CodeOrigin, GPRReg baseGPR, JSValueRegs valueRegs, GPRReg scratchGPR, unsigned identifierNumber, PutKind, JITCompiler::Jump slowPathTarget = JITCompiler::Jump(), SpillRegistersMode = NeedToSpill);

#if USE(JSVALUE64)
    void cachedGetById(CodeOrigin, GPRReg baseGPR, GPRReg resultGPR, unsigned identifierNumber, JITCompiler::Jump slowPathTarget, SpillRegistersMode, AccessType);
    void cachedGetByIdWithThis(CodeOrigin, GPRReg baseGPR, GPRReg thisGPR, GPRReg resultGPR, unsigned identifierNumber, const JITCompiler::JumpList& slowPathTarget = JITCompiler::JumpList());
#elif USE(JSVALUE32_64)
    void cachedGetById(CodeOrigin, GPRReg baseTagGPROrNone, GPRReg basePayloadGPR, GPRReg resultTagGPR, GPRReg resultPayloadGPR, unsigned identifierNumber, JITCompiler::Jump slowPathTarget, SpillRegistersMode, AccessType);
    void cachedGetByIdWithThis(CodeOrigin, GPRReg baseTagGPROrNone, GPRReg basePayloadGPR, GPRReg thisTagGPROrNone, GPRReg thisPayloadGPR, GPRReg resultTagGPR, GPRReg resultPayloadGPR, unsigned identifierNumber, const JITCompiler::JumpList& slowPathTarget = JITCompiler::JumpList());
#endif

    void compileDeleteById(Node*);
    void compileDeleteByVal(Node*);
    void compilePushWithScope(Node*);
    void compileGetById(Node*, AccessType);
    void compileGetByIdFlush(Node*, AccessType);
    void compileInById(Node*);
    void compileInByVal(Node*);
    
    void nonSpeculativeNonPeepholeCompareNullOrUndefined(Edge operand);
    void nonSpeculativePeepholeBranchNullOrUndefined(Edge operand, Node* branchNode);
    
    void nonSpeculativePeepholeBranch(Node*, Node* branchNode, MacroAssembler::RelationalCondition, S_JITOperation_EJJ helperFunction);
    void nonSpeculativeNonPeepholeCompare(Node*, MacroAssembler::RelationalCondition, S_JITOperation_EJJ helperFunction);
    
    void nonSpeculativePeepholeStrictEq(Node*, Node* branchNode, bool invert = false);
    void nonSpeculativeNonPeepholeStrictEq(Node*, bool invert = false);
    bool nonSpeculativeStrictEq(Node*, bool invert = false);
    
    void compileInstanceOfForCells(Node*, JSValueRegs valueGPR, JSValueRegs prototypeGPR, GPRReg resultGPT, GPRReg scratchGPR, GPRReg scratch2GPR, JITCompiler::Jump slowCase = JITCompiler::Jump());
    void compileInstanceOf(Node*);
    void compileInstanceOfCustom(Node*);
    void compileOverridesHasInstance(Node*);

    void compileIsCellWithType(Node*);
    void compileIsTypedArrayView(Node*);

    void emitCall(Node*);

    void emitAllocateButterfly(GPRReg storageGPR, GPRReg sizeGPR, GPRReg scratch1, GPRReg scratch2, GPRReg scratch3, MacroAssembler::JumpList& slowCases);
    void emitInitializeButterfly(GPRReg storageGPR, GPRReg sizeGPR, JSValueRegs emptyValueRegs, GPRReg scratchGPR);
    void compileAllocateNewArrayWithSize(JSGlobalObject*, GPRReg resultGPR, GPRReg sizeGPR, IndexingType, bool shouldConvertLargeSizeToArrayStorage = true);
    
    // Called once a node has completed code generation but prior to setting
    // its result, to free up its children. (This must happen prior to setting
    // the nodes result, since the node may have the same VirtualRegister as
    // a child, and as such will use the same GeneratioInfo).
    void useChildren(Node*);

    // These method called to initialize the GenerationInfo
    // to describe the result of an operation.
    void int32Result(GPRReg reg, Node* node, DataFormat format = DataFormatInt32, UseChildrenMode mode = CallUseChildren)
    {
        if (mode == CallUseChildren)
            useChildren(node);

        VirtualRegister virtualRegister = node->virtualRegister();
        GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

        if (format == DataFormatInt32) {
            m_jit.jitAssertIsInt32(reg);
            m_gprs.retain(reg, virtualRegister, SpillOrderInteger);
            info.initInt32(node, node->refCount(), reg);
        } else {
#if USE(JSVALUE64)
            RELEASE_ASSERT(format == DataFormatJSInt32);
            m_jit.jitAssertIsJSInt32(reg);
            m_gprs.retain(reg, virtualRegister, SpillOrderJS);
            info.initJSValue(node, node->refCount(), reg, format);
#elif USE(JSVALUE32_64)
            RELEASE_ASSERT_NOT_REACHED();
#endif
        }
    }
    void int32Result(GPRReg reg, Node* node, UseChildrenMode mode)
    {
        int32Result(reg, node, DataFormatInt32, mode);
    }
    void int52Result(GPRReg reg, Node* node, DataFormat format, UseChildrenMode mode = CallUseChildren)
    {
        if (mode == CallUseChildren)
            useChildren(node);

        VirtualRegister virtualRegister = node->virtualRegister();
        GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

        m_gprs.retain(reg, virtualRegister, SpillOrderJS);
        info.initInt52(node, node->refCount(), reg, format);
    }
    void int52Result(GPRReg reg, Node* node, UseChildrenMode mode = CallUseChildren)
    {
        int52Result(reg, node, DataFormatInt52, mode);
    }
    void strictInt52Result(GPRReg reg, Node* node, UseChildrenMode mode = CallUseChildren)
    {
        int52Result(reg, node, DataFormatStrictInt52, mode);
    }
    void noResult(Node* node, UseChildrenMode mode = CallUseChildren)
    {
        if (mode == UseChildrenCalledExplicitly)
            return;
        useChildren(node);
    }
    void cellResult(GPRReg reg, Node* node, UseChildrenMode mode = CallUseChildren)
    {
        if (mode == CallUseChildren)
            useChildren(node);

        VirtualRegister virtualRegister = node->virtualRegister();
        m_gprs.retain(reg, virtualRegister, SpillOrderCell);
        GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
        info.initCell(node, node->refCount(), reg);
    }
    void blessedBooleanResult(GPRReg reg, Node* node, UseChildrenMode mode = CallUseChildren)
    {
#if USE(JSVALUE64)
        jsValueResult(reg, node, DataFormatJSBoolean, mode);
#else
        booleanResult(reg, node, mode);
#endif
    }
    void unblessedBooleanResult(GPRReg reg, Node* node, UseChildrenMode mode = CallUseChildren)
    {
#if USE(JSVALUE64)
        blessBoolean(reg);
#endif
        blessedBooleanResult(reg, node, mode);
    }
#if USE(JSVALUE64)
    void jsValueResult(GPRReg reg, Node* node, DataFormat format = DataFormatJS, UseChildrenMode mode = CallUseChildren)
    {
        if (format == DataFormatJSInt32)
            m_jit.jitAssertIsJSInt32(reg);
        
        if (mode == CallUseChildren)
            useChildren(node);

        VirtualRegister virtualRegister = node->virtualRegister();
        m_gprs.retain(reg, virtualRegister, SpillOrderJS);
        GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
        info.initJSValue(node, node->refCount(), reg, format);
    }
    void jsValueResult(GPRReg reg, Node* node, UseChildrenMode mode)
    {
        jsValueResult(reg, node, DataFormatJS, mode);
    }
#elif USE(JSVALUE32_64)
    void booleanResult(GPRReg reg, Node* node, UseChildrenMode mode = CallUseChildren)
    {
        if (mode == CallUseChildren)
            useChildren(node);

        VirtualRegister virtualRegister = node->virtualRegister();
        m_gprs.retain(reg, virtualRegister, SpillOrderBoolean);
        GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
        info.initBoolean(node, node->refCount(), reg);
    }
    void jsValueResult(GPRReg tag, GPRReg payload, Node* node, DataFormat format = DataFormatJS, UseChildrenMode mode = CallUseChildren)
    {
        if (mode == CallUseChildren)
            useChildren(node);

        VirtualRegister virtualRegister = node->virtualRegister();
        m_gprs.retain(tag, virtualRegister, SpillOrderJS);
        m_gprs.retain(payload, virtualRegister, SpillOrderJS);
        GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
        info.initJSValue(node, node->refCount(), tag, payload, format);
    }
    void jsValueResult(GPRReg tag, GPRReg payload, Node* node, UseChildrenMode mode)
    {
        jsValueResult(tag, payload, node, DataFormatJS, mode);
    }
#endif
    void jsValueResult(JSValueRegs regs, Node* node, DataFormat format = DataFormatJS, UseChildrenMode mode = CallUseChildren)
    {
#if USE(JSVALUE64)
        jsValueResult(regs.gpr(), node, format, mode);
#else
        jsValueResult(regs.tagGPR(), regs.payloadGPR(), node, format, mode);
#endif
    }
    void storageResult(GPRReg reg, Node* node, UseChildrenMode mode = CallUseChildren)
    {
        if (mode == CallUseChildren)
            useChildren(node);
        
        VirtualRegister virtualRegister = node->virtualRegister();
        m_gprs.retain(reg, virtualRegister, SpillOrderStorage);
        GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
        info.initStorage(node, node->refCount(), reg);
    }
    void doubleResult(FPRReg reg, Node* node, UseChildrenMode mode = CallUseChildren)
    {
        if (mode == CallUseChildren)
            useChildren(node);

        VirtualRegister virtualRegister = node->virtualRegister();
        m_fprs.retain(reg, virtualRegister, SpillOrderDouble);
        GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
        info.initDouble(node, node->refCount(), reg);
    }
    void initConstantInfo(Node* node)
    {
        ASSERT(node->hasConstant());
        generationInfo(node).initConstant(node, node->refCount());
    }

#define FIRST_ARGUMENT_TYPE typename FunctionTraits<OperationType>::template ArgumentType<0>

    template<typename OperationType, typename ResultRegType, typename... Args>
    std::enable_if_t<
        FunctionTraits<OperationType>::hasResult,
    JITCompiler::Call>
    callOperation(OperationType operation, ResultRegType result, Args... args)
    {
        m_jit.setupArguments<OperationType>(args...);
        return appendCallSetResult(operation, result);
    }

    template<typename OperationType, typename Arg, typename... Args>
    std::enable_if_t<
        !FunctionTraits<OperationType>::hasResult
        && !std::is_same<Arg, NoResultTag>::value,
    JITCompiler::Call>
    callOperation(OperationType operation, Arg arg, Args... args)
    {
        m_jit.setupArguments<OperationType>(arg, args...);
        return appendCall(operation);
    }

    template<typename OperationType, typename... Args>
    std::enable_if_t<
        !FunctionTraits<OperationType>::hasResult,
    JITCompiler::Call>
    callOperation(OperationType operation, NoResultTag, Args... args)
    {
        m_jit.setupArguments<OperationType>(args...);
        return appendCall(operation);
    }

    template<typename OperationType>
    std::enable_if_t<
        !FunctionTraits<OperationType>::hasResult,
    JITCompiler::Call>
    callOperation(OperationType operation)
    {
        m_jit.setupArguments<OperationType>();
        return appendCall(operation);
    }

#undef FIRST_ARGUMENT_TYPE

    JITCompiler::Call callOperationWithCallFrameRollbackOnException(V_JITOperation_ECb operation, void* pointer)
    {
        m_jit.setupArguments<V_JITOperation_ECb>(TrustedImmPtr(pointer));
        return appendCallWithCallFrameRollbackOnException(operation);
    }

    JITCompiler::Call callOperationWithCallFrameRollbackOnException(Z_JITOperation_E operation, GPRReg result)
    {
        m_jit.setupArguments<Z_JITOperation_E>();
        return appendCallWithCallFrameRollbackOnExceptionSetResult(operation, result);
    }
    
#if !defined(NDEBUG) && !CPU(ARM_THUMB2) && !CPU(MIPS)
    void prepareForExternalCall()
    {
        // We're about to call out to a "native" helper function. The helper
        // function is expected to set topCallFrame itself with the ExecState
        // that is passed to it.
        //
        // We explicitly trash topCallFrame here so that we'll know if some of
        // the helper functions are not setting topCallFrame when they should
        // be doing so. Note: the previous value in topcallFrame was not valid
        // anyway since it was not being updated by JIT'ed code by design.

        for (unsigned i = 0; i < sizeof(void*) / 4; i++)
            m_jit.store32(TrustedImm32(0xbadbeef), reinterpret_cast<char*>(&m_jit.vm()->topCallFrame) + i * 4);
    }
#else
    void prepareForExternalCall() { }
#endif

    // These methods add call instructions, optionally setting results, and optionally rolling back the call frame on an exception.
    JITCompiler::Call appendCall(const FunctionPtr<CFunctionPtrTag> function)
    {
        prepareForExternalCall();
        m_jit.emitStoreCodeOrigin(m_currentNode->origin.semantic);
        return m_jit.appendCall(function);
    }

    JITCompiler::Call appendCallWithCallFrameRollbackOnException(const FunctionPtr<CFunctionPtrTag> function)
    {
        JITCompiler::Call call = appendCall(function);
        m_jit.exceptionCheckWithCallFrameRollback();
        return call;
    }

    JITCompiler::Call appendCallWithCallFrameRollbackOnExceptionSetResult(const FunctionPtr<CFunctionPtrTag> function, GPRReg result)
    {
        JITCompiler::Call call = appendCallWithCallFrameRollbackOnException(function);
        if ((result != InvalidGPRReg) && (result != GPRInfo::returnValueGPR))
            m_jit.move(GPRInfo::returnValueGPR, result);
        return call;
    }

    JITCompiler::Call appendCallSetResult(const FunctionPtr<CFunctionPtrTag> function, GPRReg result)
    {
        JITCompiler::Call call = appendCall(function);
        if (result != InvalidGPRReg)
            m_jit.move(GPRInfo::returnValueGPR, result);
        return call;
    }

    JITCompiler::Call appendCallSetResult(const FunctionPtr<CFunctionPtrTag> function, GPRReg result1, GPRReg result2)
    {
        JITCompiler::Call call = appendCall(function);
        m_jit.setupResults(result1, result2);
        return call;
    }

    JITCompiler::Call appendCallSetResult(const FunctionPtr<CFunctionPtrTag> function, JSValueRegs resultRegs)
    {
#if USE(JSVALUE64)
        return appendCallSetResult(function, resultRegs.gpr());
#else
        return appendCallSetResult(function, resultRegs.payloadGPR(), resultRegs.tagGPR());
#endif
    }

#if CPU(X86)
    JITCompiler::Call appendCallSetResult(const FunctionPtr<CFunctionPtrTag> function, FPRReg result)
    {
        JITCompiler::Call call = appendCall(function);
        if (result != InvalidFPRReg) {
            m_jit.assembler().fstpl(0, JITCompiler::stackPointerRegister);
            m_jit.loadDouble(JITCompiler::stackPointerRegister, result);
        }
        return call;
    }
#elif CPU(ARM_THUMB2) && !CPU(ARM_HARDFP)
    JITCompiler::Call appendCallSetResult(const FunctionPtr<CFunctionPtrTag> function, FPRReg result)
    {
        JITCompiler::Call call = appendCall(function);
        if (result != InvalidFPRReg)
            m_jit.assembler().vmov(result, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
        return call;
    }
#else // CPU(X86_64) || (CPU(ARM_THUMB2) && CPU(ARM_HARDFP)) || CPU(ARM64) || CPU(MIPS)
    JITCompiler::Call appendCallSetResult(const FunctionPtr<CFunctionPtrTag> function, FPRReg result)
    {
        JITCompiler::Call call = appendCall(function);
        if (result != InvalidFPRReg)
            m_jit.moveDouble(FPRInfo::returnValueFPR, result);
        return call;
    }
#endif

    void branchDouble(JITCompiler::DoubleCondition cond, FPRReg left, FPRReg right, BasicBlock* destination)
    {
        return addBranch(m_jit.branchDouble(cond, left, right), destination);
    }
    
    void branchDoubleNonZero(FPRReg value, FPRReg scratch, BasicBlock* destination)
    {
        return addBranch(m_jit.branchDoubleNonZero(value, scratch), destination);
    }
    
    template<typename T, typename U>
    void branch32(JITCompiler::RelationalCondition cond, T left, U right, BasicBlock* destination)
    {
        return addBranch(m_jit.branch32(cond, left, right), destination);
    }
    
    template<typename T, typename U>
    void branchTest32(JITCompiler::ResultCondition cond, T value, U mask, BasicBlock* destination)
    {
        return addBranch(m_jit.branchTest32(cond, value, mask), destination);
    }
    
    template<typename T>
    void branchTest32(JITCompiler::ResultCondition cond, T value, BasicBlock* destination)
    {
        return addBranch(m_jit.branchTest32(cond, value), destination);
    }
    
#if USE(JSVALUE64)
    template<typename T, typename U>
    void branch64(JITCompiler::RelationalCondition cond, T left, U right, BasicBlock* destination)
    {
        return addBranch(m_jit.branch64(cond, left, right), destination);
    }
#endif
    
    template<typename T, typename U>
    void branch8(JITCompiler::RelationalCondition cond, T left, U right, BasicBlock* destination)
    {
        return addBranch(m_jit.branch8(cond, left, right), destination);
    }
    
    template<typename T, typename U>
    void branchPtr(JITCompiler::RelationalCondition cond, T left, U right, BasicBlock* destination)
    {
        return addBranch(m_jit.branchPtr(cond, left, right), destination);
    }
    
    template<typename T, typename U>
    void branchTestPtr(JITCompiler::ResultCondition cond, T value, U mask, BasicBlock* destination)
    {
        return addBranch(m_jit.branchTestPtr(cond, value, mask), destination);
    }
    
    template<typename T>
    void branchTestPtr(JITCompiler::ResultCondition cond, T value, BasicBlock* destination)
    {
        return addBranch(m_jit.branchTestPtr(cond, value), destination);
    }
    
    template<typename T, typename U>
    void branchTest8(JITCompiler::ResultCondition cond, T value, U mask, BasicBlock* destination)
    {
        return addBranch(m_jit.branchTest8(cond, value, mask), destination);
    }
    
    template<typename T>
    void branchTest8(JITCompiler::ResultCondition cond, T value, BasicBlock* destination)
    {
        return addBranch(m_jit.branchTest8(cond, value), destination);
    }
    
    enum FallThroughMode {
        AtFallThroughPoint,
        ForceJump
    };
    void jump(BasicBlock* destination, FallThroughMode fallThroughMode = AtFallThroughPoint)
    {
        if (destination == nextBlock()
            && fallThroughMode == AtFallThroughPoint)
            return;
        addBranch(m_jit.jump(), destination);
    }
    
    void addBranch(const MacroAssembler::Jump& jump, BasicBlock* destination)
    {
        m_branches.append(BranchRecord(jump, destination));
    }
    void addBranch(const MacroAssembler::JumpList& jump, BasicBlock* destination);

    void linkBranches();

    void dump(const char* label = 0);

    bool betterUseStrictInt52(Node* node)
    {
        return !generationInfo(node).isInt52();
    }
    bool betterUseStrictInt52(Edge edge)
    {
        return betterUseStrictInt52(edge.node());
    }
    
    bool compare(Node*, MacroAssembler::RelationalCondition, MacroAssembler::DoubleCondition, S_JITOperation_EJJ);
    void compileCompareUnsigned(Node*, MacroAssembler::RelationalCondition);
    bool compilePeepHoleBranch(Node*, MacroAssembler::RelationalCondition, MacroAssembler::DoubleCondition, S_JITOperation_EJJ);
    void compilePeepHoleInt32Branch(Node*, Node* branchNode, JITCompiler::RelationalCondition);
    void compilePeepHoleInt52Branch(Node*, Node* branchNode, JITCompiler::RelationalCondition);
    void compilePeepHoleBooleanBranch(Node*, Node* branchNode, JITCompiler::RelationalCondition);
    void compilePeepHoleDoubleBranch(Node*, Node* branchNode, JITCompiler::DoubleCondition);
    void compilePeepHoleObjectEquality(Node*, Node* branchNode);
    void compilePeepHoleObjectStrictEquality(Edge objectChild, Edge otherChild, Node* branchNode);
    void compilePeepHoleObjectToObjectOrOtherEquality(Edge leftChild, Edge rightChild, Node* branchNode);
    void compileObjectEquality(Node*);
    void compileObjectStrictEquality(Edge objectChild, Edge otherChild);
    void compileObjectToObjectOrOtherEquality(Edge leftChild, Edge rightChild);
    void compileObjectOrOtherLogicalNot(Edge value);
    void compileLogicalNot(Node*);
    void compileLogicalNotStringOrOther(Node*);
    void compileStringEquality(
        Node*, GPRReg leftGPR, GPRReg rightGPR, GPRReg lengthGPR,
        GPRReg leftTempGPR, GPRReg rightTempGPR, GPRReg leftTemp2GPR,
        GPRReg rightTemp2GPR, const JITCompiler::JumpList& fastTrue,
        const JITCompiler::JumpList& fastSlow);
    void compileStringEquality(Node*);
    void compileStringIdentEquality(Node*);
    void compileStringToUntypedEquality(Node*, Edge stringEdge, Edge untypedEdge);
    void compileStringIdentToNotStringVarEquality(Node*, Edge stringEdge, Edge notStringVarEdge);
    void compileStringZeroLength(Node*);
    void compileMiscStrictEq(Node*);

    void compileSymbolEquality(Node*);
    void compileBigIntEquality(Node*);
    void compilePeepHoleSymbolEquality(Node*, Node* branchNode);
    void compileSymbolUntypedEquality(Node*, Edge symbolEdge, Edge untypedEdge);

    void emitObjectOrOtherBranch(Edge value, BasicBlock* taken, BasicBlock* notTaken);
    void emitStringBranch(Edge value, BasicBlock* taken, BasicBlock* notTaken);
    void emitStringOrOtherBranch(Edge value, BasicBlock* taken, BasicBlock* notTaken);
    void emitBranch(Node*);
    
    struct StringSwitchCase {
        StringSwitchCase() { }
        
        StringSwitchCase(StringImpl* string, BasicBlock* target)
            : string(string)
            , target(target)
        {
        }
        
        bool operator<(const StringSwitchCase& other) const
        {
            return stringLessThan(*string, *other.string);
        }
        
        StringImpl* string;
        BasicBlock* target;
    };
    
    void emitSwitchIntJump(SwitchData*, GPRReg value, GPRReg scratch, GPRReg scratch2);
    void emitSwitchImm(Node*, SwitchData*);
    void emitSwitchCharStringJump(SwitchData*, GPRReg value, GPRReg scratch, GPRReg scratch2);
    void emitSwitchChar(Node*, SwitchData*);
    void emitBinarySwitchStringRecurse(
        SwitchData*, const Vector<StringSwitchCase>&, unsigned numChecked,
        unsigned begin, unsigned end, GPRReg buffer, GPRReg length, GPRReg temp,
        unsigned alreadyCheckedLength, bool checkedExactLength);
    void emitSwitchStringOnString(SwitchData*, GPRReg string);
    void emitSwitchString(Node*, SwitchData*);
    void emitSwitch(Node*);
    
    void compileToStringOrCallStringConstructorOrStringValueOf(Node*);
    void compileNumberToStringWithRadix(Node*);
    void compileNumberToStringWithValidRadixConstant(Node*);
    void compileNumberToStringWithValidRadixConstant(Node*, int32_t radix);
    void compileNewStringObject(Node*);
    void compileNewSymbol(Node*);
    
    void compileNewTypedArrayWithSize(Node*);
    
    void compileInt32Compare(Node*, MacroAssembler::RelationalCondition);
    void compileInt52Compare(Node*, MacroAssembler::RelationalCondition);
    void compileBooleanCompare(Node*, MacroAssembler::RelationalCondition);
    void compileDoubleCompare(Node*, MacroAssembler::DoubleCondition);
    void compileStringCompare(Node*, MacroAssembler::RelationalCondition);
    void compileStringIdentCompare(Node*, MacroAssembler::RelationalCondition);
    
    bool compileStrictEq(Node*);

    void compileSameValue(Node*);
    
    void compileAllocatePropertyStorage(Node*);
    void compileReallocatePropertyStorage(Node*);
    void compileNukeStructureAndSetButterfly(Node*);
    void compileGetButterfly(Node*);
    void compileCallDOMGetter(Node*);
    void compileCallDOM(Node*);
    void compileCheckSubClass(Node*);
    void compileNormalizeMapKey(Node*);
    void compileGetMapBucketHead(Node*);
    void compileGetMapBucketNext(Node*);
    void compileSetAdd(Node*);
    void compileMapSet(Node*);
    void compileWeakMapGet(Node*);
    void compileWeakSetAdd(Node*);
    void compileWeakMapSet(Node*);
    void compileLoadKeyFromMapBucket(Node*);
    void compileLoadValueFromMapBucket(Node*);
    void compileExtractValueFromWeakMapGet(Node*);
    void compileGetPrototypeOf(Node*);
    void compileIdentity(Node*);
    
#if USE(JSVALUE32_64)
    template<typename BaseOperandType, typename PropertyOperandType, typename ValueOperandType, typename TagType>
    void compileContiguousPutByVal(Node*, BaseOperandType&, PropertyOperandType&, ValueOperandType&, GPRReg valuePayloadReg, TagType valueTag);
#endif
    void compileDoublePutByVal(Node*, SpeculateCellOperand& base, SpeculateStrictInt32Operand& property);
    bool putByValWillNeedExtraRegister(ArrayMode arrayMode)
    {
        return arrayMode.mayStoreToHole();
    }
    GPRReg temporaryRegisterForPutByVal(GPRTemporary&, ArrayMode);
    GPRReg temporaryRegisterForPutByVal(GPRTemporary& temporary, Node* node)
    {
        return temporaryRegisterForPutByVal(temporary, node->arrayMode());
    }
    
    void compileGetCharCodeAt(Node*);
    void compileGetByValOnString(Node*);
    void compileFromCharCode(Node*); 

    void compileGetByValOnDirectArguments(Node*);
    void compileGetByValOnScopedArguments(Node*);
    
    void compileGetScope(Node*);
    void compileSkipScope(Node*);
    void compileGetGlobalObject(Node*);
    void compileGetGlobalThis(Node*);

    void compileGetArrayLength(Node*);

    void compileCheckTypeInfoFlags(Node*);
    void compileCheckStringIdent(Node*);

    void compileParseInt(Node*);
    
    void compileValueRep(Node*);
    void compileDoubleRep(Node*);
    
    void compileValueToInt32(Node*);
    void compileUInt32ToNumber(Node*);
    void compileDoubleAsInt32(Node*);

    void compileBitwiseNot(Node*);

    template<typename SnippetGenerator, J_JITOperation_EJJ slowPathFunction>
    void emitUntypedBitOp(Node*);
    void compileBitwiseOp(Node*);
    void compileValueBitwiseOp(Node*);

    void emitUntypedRightShiftBitOp(Node*);
    void compileShiftOp(Node*);

    template <typename Generator, typename RepatchingFunction, typename NonRepatchingFunction>
    void compileMathIC(Node*, JITBinaryMathIC<Generator>*, bool needsScratchGPRReg, bool needsScratchFPRReg, RepatchingFunction, NonRepatchingFunction);
    template <typename Generator, typename RepatchingFunction, typename NonRepatchingFunction>
    void compileMathIC(Node*, JITUnaryMathIC<Generator>*, bool needsScratchGPRReg, RepatchingFunction, NonRepatchingFunction);

    void compileArithDoubleUnaryOp(Node*, double (*doubleFunction)(double), double (*operation)(ExecState*, EncodedJSValue));
    void compileValueAdd(Node*);
    void compileValueSub(Node*);
    void compileArithAdd(Node*);
    void compileMakeRope(Node*);
    void compileArithAbs(Node*);
    void compileArithClz32(Node*);
    void compileArithSub(Node*);
    void compileValueNegate(Node*);
    void compileArithNegate(Node*);
    void compileValueMul(Node*);
    void compileArithMul(Node*);
    void compileValueDiv(Node*);
    void compileArithDiv(Node*);
    void compileArithFRound(Node*);
    void compileArithMod(Node*);
    void compileArithPow(Node*);
    void compileArithRounding(Node*);
    void compileArithRandom(Node*);
    void compileArithUnary(Node*);
    void compileArithSqrt(Node*);
    void compileArithMinMax(Node*);
    void compileConstantStoragePointer(Node*);
    void compileGetIndexedPropertyStorage(Node*);
    JITCompiler::Jump jumpForTypedArrayOutOfBounds(Node*, GPRReg baseGPR, GPRReg indexGPR);
    JITCompiler::Jump jumpForTypedArrayIsNeuteredIfOutOfBounds(Node*, GPRReg baseGPR, JITCompiler::Jump outOfBounds);
    void emitTypedArrayBoundsCheck(Node*, GPRReg baseGPR, GPRReg indexGPR);
    void compileGetTypedArrayByteOffset(Node*);
    void compileGetByValOnIntTypedArray(Node*, TypedArrayType);
    void compilePutByValForIntTypedArray(GPRReg base, GPRReg property, Node*, TypedArrayType);
    void compileGetByValOnFloatTypedArray(Node*, TypedArrayType);
    void compilePutByValForFloatTypedArray(GPRReg base, GPRReg property, Node*, TypedArrayType);
    void compileGetByValForObjectWithString(Node*);
    void compileGetByValForObjectWithSymbol(Node*);
    void compilePutByValForCellWithString(Node*, Edge& child1, Edge& child2, Edge& child3);
    void compilePutByValForCellWithSymbol(Node*, Edge& child1, Edge& child2, Edge& child3);
    void compileGetByValWithThis(Node*);
    void compileGetByOffset(Node*);
    void compilePutByOffset(Node*);
    void compileMatchStructure(Node*);
    // If this returns false it means that we terminated speculative execution.
    bool getIntTypedArrayStoreOperand(
        GPRTemporary& value,
        GPRReg property,
#if USE(JSVALUE32_64)
        GPRTemporary& propertyTag,
        GPRTemporary& valueTag,
#endif
        Edge valueUse, JITCompiler::JumpList& slowPathCases, bool isClamped = false);
    void loadFromIntTypedArray(GPRReg storageReg, GPRReg propertyReg, GPRReg resultReg, TypedArrayType);
    void setIntTypedArrayLoadResult(Node*, GPRReg resultReg, TypedArrayType, bool canSpeculate = false);
    template <typename ClassType> void compileNewFunctionCommon(GPRReg, RegisteredStructure, GPRReg, GPRReg, GPRReg, MacroAssembler::JumpList&, size_t, FunctionExecutable*);
    void compileNewFunction(Node*);
    void compileSetFunctionName(Node*);
    void compileNewRegexp(Node*);
    void compileForwardVarargs(Node*);
    void compileLoadVarargs(Node*);
    void compileCreateActivation(Node*);
    void compileCreateDirectArguments(Node*);
    void compileGetFromArguments(Node*);
    void compilePutToArguments(Node*);
    void compileGetArgument(Node*);
    void compileCreateScopedArguments(Node*);
    void compileCreateClonedArguments(Node*);
    void compileCreateRest(Node*);
    void compileSpread(Node*);
    void compileNewArray(Node*);
    void compileNewArrayWithSpread(Node*);
    void compileGetRestLength(Node*);
    void compileArraySlice(Node*);
    void compileArrayIndexOf(Node*);
    void compileArrayPush(Node*);
    void compileNotifyWrite(Node*);
    void compileRegExpExec(Node*);
    void compileRegExpExecNonGlobalOrSticky(Node*);
    void compileRegExpMatchFast(Node*);
    void compileRegExpMatchFastGlobal(Node*);
    void compileRegExpTest(Node*);
    void compileStringReplace(Node*);
    void compileIsObject(Node*);
    void compileIsObjectOrNull(Node*);
    void compileIsFunction(Node*);
    void compileTypeOf(Node*);
    void compileCheckCell(Node*);
    void compileCheckNotEmpty(Node*);
    void compileCheckStructure(Node*);
    void emitStructureCheck(Node*, GPRReg cellGPR, GPRReg tempGPR);
    void compilePutAccessorById(Node*);
    void compilePutGetterSetterById(Node*);
    void compilePutAccessorByVal(Node*);
    void compileGetRegExpObjectLastIndex(Node*);
    void compileSetRegExpObjectLastIndex(Node*);
    void compileLazyJSConstant(Node*);
    void compileMaterializeNewObject(Node*);
    void compileRecordRegExpCachedResult(Node*);
    void compileToObjectOrCallObjectConstructor(Node*);
    void compileResolveScope(Node*);
    void compileResolveScopeForHoistingFuncDeclInEval(Node*);
    void compileGetGlobalVariable(Node*);
    void compilePutGlobalVariable(Node*);
    void compileGetDynamicVar(Node*);
    void compilePutDynamicVar(Node*);
    void compileGetClosureVar(Node*);
    void compilePutClosureVar(Node*);
    void compileCompareEqPtr(Node*);
    void compileDefineDataProperty(Node*);
    void compileDefineAccessorProperty(Node*);
    void compileStringSlice(Node*);
    void compileToLowerCase(Node*);
    void compileThrow(Node*);
    void compileThrowStaticError(Node*);
    void compileGetEnumerableLength(Node*);
    void compileHasGenericProperty(Node*);
    void compileToIndexString(Node*);
    void compilePutByIdFlush(Node*);
    void compilePutById(Node*);
    void compilePutByIdDirect(Node*);
    void compilePutByIdWithThis(Node*);
    void compileHasStructureProperty(Node*);
    void compileGetDirectPname(Node*);
    void compileGetPropertyEnumerator(Node*);
    void compileGetEnumeratorPname(Node*);
    void compileGetExecutable(Node*);
    void compileGetGetter(Node*);
    void compileGetSetter(Node*);
    void compileGetCallee(Node*);
    void compileSetCallee(Node*);
    void compileGetArgumentCountIncludingThis(Node*);
    void compileSetArgumentCountIncludingThis(Node*);
    void compileStrCat(Node*);
    void compileNewArrayBuffer(Node*);
    void compileNewArrayWithSize(Node*);
    void compileNewTypedArray(Node*);
    void compileToThis(Node*);
    void compileObjectKeys(Node*);
    void compileObjectCreate(Node*);
    void compileCreateThis(Node*);
    void compileNewObject(Node*);
    void compileToPrimitive(Node*);
    void compileLogShadowChickenPrologue(Node*);
    void compileLogShadowChickenTail(Node*);
    void compileHasIndexedProperty(Node*);
    void compileExtractCatchLocal(Node*);
    void compileClearCatchLocals(Node*);
    void compileProfileType(Node*);

    void moveTrueTo(GPRReg);
    void moveFalseTo(GPRReg);
    void blessBoolean(GPRReg);
    
    // Allocator for a cell of a specific size.
    template <typename StructureType> // StructureType can be GPR or ImmPtr.
    void emitAllocateJSCell(
        GPRReg resultGPR, const JITAllocator& allocator, GPRReg allocatorGPR, StructureType structure,
        GPRReg scratchGPR, MacroAssembler::JumpList& slowPath)
    {
        m_jit.emitAllocateJSCell(resultGPR, allocator, allocatorGPR, structure, scratchGPR, slowPath);
    }

    // Allocator for an object of a specific size.
    template <typename StructureType, typename StorageType> // StructureType and StorageType can be GPR or ImmPtr.
    void emitAllocateJSObject(
        GPRReg resultGPR, const JITAllocator& allocator, GPRReg allocatorGPR, StructureType structure,
        StorageType storage, GPRReg scratchGPR, MacroAssembler::JumpList& slowPath)
    {
        m_jit.emitAllocateJSObject(
            resultGPR, allocator, allocatorGPR, structure, storage, scratchGPR, slowPath);
    }

    template <typename ClassType, typename StructureType, typename StorageType> // StructureType and StorageType can be GPR or ImmPtr.
    void emitAllocateJSObjectWithKnownSize(
        GPRReg resultGPR, StructureType structure, StorageType storage, GPRReg scratchGPR1,
        GPRReg scratchGPR2, MacroAssembler::JumpList& slowPath, size_t size)
    {
        m_jit.emitAllocateJSObjectWithKnownSize<ClassType>(*m_jit.vm(), resultGPR, structure, storage, scratchGPR1, scratchGPR2, slowPath, size);
    }

    // Convenience allocator for a built-in object.
    template <typename ClassType, typename StructureType, typename StorageType> // StructureType and StorageType can be GPR or ImmPtr.
    void emitAllocateJSObject(GPRReg resultGPR, StructureType structure, StorageType storage,
        GPRReg scratchGPR1, GPRReg scratchGPR2, MacroAssembler::JumpList& slowPath)
    {
        m_jit.emitAllocateJSObject<ClassType>(*m_jit.vm(), resultGPR, structure, storage, scratchGPR1, scratchGPR2, slowPath);
    }

    template <typename ClassType, typename StructureType> // StructureType and StorageType can be GPR or ImmPtr.
    void emitAllocateVariableSizedJSObject(GPRReg resultGPR, StructureType structure, GPRReg allocationSize, GPRReg scratchGPR1, GPRReg scratchGPR2, MacroAssembler::JumpList& slowPath)
    {
        m_jit.emitAllocateVariableSizedJSObject<ClassType>(*m_jit.vm(), resultGPR, structure, allocationSize, scratchGPR1, scratchGPR2, slowPath);
    }

    template<typename ClassType>
    void emitAllocateDestructibleObject(GPRReg resultGPR, RegisteredStructure structure, 
        GPRReg scratchGPR1, GPRReg scratchGPR2, MacroAssembler::JumpList& slowPath)
    {
        m_jit.emitAllocateDestructibleObject<ClassType>(*m_jit.vm(), resultGPR, structure.get(), scratchGPR1, scratchGPR2, slowPath);
    }

    void emitAllocateRawObject(GPRReg resultGPR, RegisteredStructure, GPRReg storageGPR, unsigned numElements, unsigned vectorLength);
    
    void emitGetLength(InlineCallFrame*, GPRReg lengthGPR, bool includeThis = false);
    void emitGetLength(CodeOrigin, GPRReg lengthGPR, bool includeThis = false);
    void emitGetCallee(CodeOrigin, GPRReg calleeGPR);
    void emitGetArgumentStart(CodeOrigin, GPRReg startGPR);
    void emitPopulateSliceIndex(Edge&, GPRReg length, GPRReg result);
    
    // Generate an OSR exit fuzz check. Returns Jump() if OSR exit fuzz is not enabled, or if
    // it's in training mode.
    MacroAssembler::Jump emitOSRExitFuzzCheck();
    
    // Add a speculation check.
    void speculationCheck(ExitKind, JSValueSource, Node*, MacroAssembler::Jump jumpToFail);
    void speculationCheck(ExitKind, JSValueSource, Node*, const MacroAssembler::JumpList& jumpsToFail);

    // Add a speculation check without additional recovery, and with a promise to supply a jump later.
    OSRExitJumpPlaceholder speculationCheck(ExitKind, JSValueSource, Node*);
    OSRExitJumpPlaceholder speculationCheck(ExitKind, JSValueSource, Edge);
    void speculationCheck(ExitKind, JSValueSource, Edge, MacroAssembler::Jump jumpToFail);
    void speculationCheck(ExitKind, JSValueSource, Edge, const MacroAssembler::JumpList& jumpsToFail);
    // Add a speculation check with additional recovery.
    void speculationCheck(ExitKind, JSValueSource, Node*, MacroAssembler::Jump jumpToFail, const SpeculationRecovery&);
    void speculationCheck(ExitKind, JSValueSource, Edge, MacroAssembler::Jump jumpToFail, const SpeculationRecovery&);
    
    void emitInvalidationPoint(Node*);
    
    void unreachable(Node*);
    
    // Called when we statically determine that a speculation will fail.
    void terminateSpeculativeExecution(ExitKind, JSValueRegs, Node*);
    void terminateSpeculativeExecution(ExitKind, JSValueRegs, Edge);
    
    // Helpers for performing type checks on an edge stored in the given registers.
    bool needsTypeCheck(Edge edge, SpeculatedType typesPassedThrough) { return m_interpreter.needsTypeCheck(edge, typesPassedThrough); }
    void typeCheck(JSValueSource, Edge, SpeculatedType typesPassedThrough, MacroAssembler::Jump jumpToFail, ExitKind = BadType);
    
    void speculateCellTypeWithoutTypeFiltering(Edge, GPRReg cellGPR, JSType);
    void speculateCellType(Edge, GPRReg cellGPR, SpeculatedType, JSType);
    
    void speculateInt32(Edge);
#if USE(JSVALUE64)
    void convertAnyInt(Edge, GPRReg resultGPR);
    void speculateAnyInt(Edge);
    void speculateInt32(Edge, JSValueRegs);
    void speculateDoubleRepAnyInt(Edge);
#endif // USE(JSVALUE64)
    void speculateNumber(Edge);
    void speculateRealNumber(Edge);
    void speculateDoubleRepReal(Edge);
    void speculateBoolean(Edge);
    void speculateCell(Edge);
    void speculateCellOrOther(Edge);
    void speculateObject(Edge, GPRReg cell);
    void speculateObject(Edge);
    void speculateArray(Edge, GPRReg cell);
    void speculateArray(Edge);
    void speculateFunction(Edge, GPRReg cell);
    void speculateFunction(Edge);
    void speculateFinalObject(Edge, GPRReg cell);
    void speculateFinalObject(Edge);
    void speculateRegExpObject(Edge, GPRReg cell);
    void speculateRegExpObject(Edge);
    void speculateProxyObject(Edge, GPRReg cell);
    void speculateProxyObject(Edge);
    void speculateDerivedArray(Edge, GPRReg cell);
    void speculateDerivedArray(Edge);
    void speculateMapObject(Edge);
    void speculateMapObject(Edge, GPRReg cell);
    void speculateSetObject(Edge);
    void speculateSetObject(Edge, GPRReg cell);
    void speculateWeakMapObject(Edge);
    void speculateWeakMapObject(Edge, GPRReg cell);
    void speculateWeakSetObject(Edge);
    void speculateWeakSetObject(Edge, GPRReg cell);
    void speculateDataViewObject(Edge);
    void speculateDataViewObject(Edge, GPRReg cell);
    void speculateObjectOrOther(Edge);
    void speculateString(Edge edge, GPRReg cell);
    void speculateStringIdentAndLoadStorage(Edge edge, GPRReg string, GPRReg storage);
    void speculateStringIdent(Edge edge, GPRReg string);
    void speculateStringIdent(Edge);
    void speculateString(Edge);
    void speculateStringOrOther(Edge, JSValueRegs, GPRReg scratch);
    void speculateStringOrOther(Edge);
    void speculateNotStringVar(Edge);
    void speculateNotSymbol(Edge);
    template<typename StructureLocationType>
    void speculateStringObjectForStructure(Edge, StructureLocationType);
    void speculateStringObject(Edge, GPRReg);
    void speculateStringObject(Edge);
    void speculateStringOrStringObject(Edge);
    void speculateSymbol(Edge, GPRReg cell);
    void speculateSymbol(Edge);
    void speculateBigInt(Edge, GPRReg cell);
    void speculateBigInt(Edge);
    void speculateNotCell(Edge, JSValueRegs);
    void speculateNotCell(Edge);
    void speculateOther(Edge, JSValueRegs, GPRReg temp);
    void speculateOther(Edge, JSValueRegs);
    void speculateOther(Edge);
    void speculateMisc(Edge, JSValueRegs);
    void speculateMisc(Edge);
    void speculate(Node*, Edge);
    
    JITCompiler::JumpList jumpSlowForUnwantedArrayMode(GPRReg tempWithIndexingTypeReg, ArrayMode);
    void checkArray(Node*);
    void arrayify(Node*, GPRReg baseReg, GPRReg propertyReg);
    void arrayify(Node*);
    
    template<bool strict>
    GPRReg fillSpeculateInt32Internal(Edge, DataFormat& returnFormat);
    
    void cageTypedArrayStorage(GPRReg);
    
    // It is possible, during speculative generation, to reach a situation in which we
    // can statically determine a speculation will fail (for example, when two nodes
    // will make conflicting speculations about the same operand). In such cases this
    // flag is cleared, indicating no further code generation should take place.
    bool m_compileOkay;
    
    void recordSetLocal(
        VirtualRegister bytecodeReg, VirtualRegister machineReg, DataFormat format)
    {
        m_stream->appendAndLog(VariableEvent::setLocal(bytecodeReg, machineReg, format));
    }
    
    void recordSetLocal(DataFormat format)
    {
        VariableAccessData* variable = m_currentNode->variableAccessData();
        recordSetLocal(variable->local(), variable->machineLocal(), format);
    }

    GenerationInfo& generationInfoFromVirtualRegister(VirtualRegister virtualRegister)
    {
        return m_generationInfo[virtualRegister.toLocal()];
    }
    
    GenerationInfo& generationInfo(Node* node)
    {
        return generationInfoFromVirtualRegister(node->virtualRegister());
    }
    
    GenerationInfo& generationInfo(Edge edge)
    {
        return generationInfo(edge.node());
    }

    // The JIT, while also provides MacroAssembler functionality.
    JITCompiler& m_jit;
    Graph& m_graph;

    // The current node being generated.
    BasicBlock* m_block;
    Node* m_currentNode;
    NodeType m_lastGeneratedNode;
    unsigned m_indexInBlock;

    // Virtual and physical register maps.
    Vector<GenerationInfo, 32> m_generationInfo;
    RegisterBank<GPRInfo> m_gprs;
    RegisterBank<FPRInfo> m_fprs;

    Vector<MacroAssembler::Label> m_osrEntryHeads;
    
    struct BranchRecord {
        BranchRecord(MacroAssembler::Jump jump, BasicBlock* destination)
            : jump(jump)
            , destination(destination)
        {
        }

        MacroAssembler::Jump jump;
        BasicBlock* destination;
    };
    Vector<BranchRecord, 8> m_branches;

    NodeOrigin m_origin;
    
    InPlaceAbstractState m_state;
    AbstractInterpreter<InPlaceAbstractState> m_interpreter;
    
    VariableEventStream* m_stream;
    MinifiedGraph* m_minifiedGraph;
    
    Vector<std::unique_ptr<SlowPathGenerator>, 8> m_slowPathGenerators;
    struct SlowPathLambda {
        Function<void()> generator;
        Node* currentNode;
        unsigned streamIndex;
    };
    Vector<SlowPathLambda> m_slowPathLambdas;
    Vector<SilentRegisterSavePlan> m_plans;
    std::optional<unsigned> m_outOfLineStreamIndex;
};


// === Operand types ===
//
// These classes are used to lock the operands to a node into machine
// registers. These classes implement of pattern of locking a value
// into register at the point of construction only if it is already in
// registers, and otherwise loading it lazily at the point it is first
// used. We do so in order to attempt to avoid spilling one operand
// in order to make space available for another.

class JSValueOperand {
public:
    explicit JSValueOperand(SpeculativeJIT* jit, Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
        : m_jit(jit)
        , m_edge(edge)
#if USE(JSVALUE64)
        , m_gprOrInvalid(InvalidGPRReg)
#elif USE(JSVALUE32_64)
        , m_isDouble(false)
#endif
    {
        ASSERT(m_jit);
        if (!edge)
            return;
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == UntypedUse);
#if USE(JSVALUE64)
        if (jit->isFilled(node()))
            gpr();
#elif USE(JSVALUE32_64)
        m_register.pair.tagGPR = InvalidGPRReg;
        m_register.pair.payloadGPR = InvalidGPRReg;
        if (jit->isFilled(node()))
            fill();
#endif
    }

    explicit JSValueOperand(JSValueOperand&& other)
        : m_jit(other.m_jit)
        , m_edge(other.m_edge)
    {
#if USE(JSVALUE64)
        m_gprOrInvalid = other.m_gprOrInvalid;
#elif USE(JSVALUE32_64)
        m_register.pair.tagGPR = InvalidGPRReg;
        m_register.pair.payloadGPR = InvalidGPRReg;
        m_isDouble = other.m_isDouble;

        if (m_edge) {
            if (m_isDouble)
                m_register.fpr = other.m_register.fpr;
            else
                m_register.pair = other.m_register.pair;
        }
#endif
        other.m_edge = Edge();
#if USE(JSVALUE64)
        other.m_gprOrInvalid = InvalidGPRReg;
#elif USE(JSVALUE32_64)
        other.m_isDouble = false;
#endif
    }

    ~JSValueOperand()
    {
        if (!m_edge)
            return;
#if USE(JSVALUE64)
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
#elif USE(JSVALUE32_64)
        if (m_isDouble) {
            ASSERT(m_register.fpr != InvalidFPRReg);
            m_jit->unlock(m_register.fpr);
        } else {
            ASSERT(m_register.pair.tagGPR != InvalidGPRReg && m_register.pair.payloadGPR != InvalidGPRReg);
            m_jit->unlock(m_register.pair.tagGPR);
            m_jit->unlock(m_register.pair.payloadGPR);
        }
#endif
    }
    
    Edge edge() const
    {
        return m_edge;
    }

    Node* node() const
    {
        return edge().node();
    }

#if USE(JSVALUE64)
    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillJSValue(m_edge);
        return m_gprOrInvalid;
    }
    JSValueRegs jsValueRegs()
    {
        return JSValueRegs(gpr());
    }
#elif USE(JSVALUE32_64)
    bool isDouble() { return m_isDouble; }

    void fill()
    {
        if (m_register.pair.tagGPR == InvalidGPRReg && m_register.pair.payloadGPR == InvalidGPRReg)
            m_isDouble = !m_jit->fillJSValue(m_edge, m_register.pair.tagGPR, m_register.pair.payloadGPR, m_register.fpr);
    }

    GPRReg tagGPR()
    {
        fill();
        ASSERT(!m_isDouble);
        return m_register.pair.tagGPR;
    } 

    GPRReg payloadGPR()
    {
        fill();
        ASSERT(!m_isDouble);
        return m_register.pair.payloadGPR;
    }
    
    JSValueRegs jsValueRegs()
    {
        return JSValueRegs(tagGPR(), payloadGPR());
    }

    GPRReg gpr(WhichValueWord which)
    {
        return jsValueRegs().gpr(which);
    }

    FPRReg fpr()
    {
        fill();
        ASSERT(m_isDouble);
        return m_register.fpr;
    }
#endif

    void use()
    {
        m_jit->use(node());
    }

private:
    SpeculativeJIT* m_jit;
    Edge m_edge;
#if USE(JSVALUE64)
    GPRReg m_gprOrInvalid;
#elif USE(JSVALUE32_64)
    union {
        struct {
            GPRReg tagGPR;
            GPRReg payloadGPR;
        } pair;
        FPRReg fpr;
    } m_register;
    bool m_isDouble;
#endif
};

class StorageOperand {
public:
    explicit StorageOperand(SpeculativeJIT* jit, Edge edge)
        : m_jit(jit)
        , m_edge(edge)
        , m_gprOrInvalid(InvalidGPRReg)
    {
        ASSERT(m_jit);
        ASSERT(edge.useKind() == UntypedUse || edge.useKind() == KnownCellUse);
        if (jit->isFilled(node()))
            gpr();
    }
    
    ~StorageOperand()
    {
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }
    
    Edge edge() const
    {
        return m_edge;
    }
    
    Node* node() const
    {
        return edge().node();
    }
    
    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillStorage(edge());
        return m_gprOrInvalid;
    }
    
    void use()
    {
        m_jit->use(node());
    }
    
private:
    SpeculativeJIT* m_jit;
    Edge m_edge;
    GPRReg m_gprOrInvalid;
};


// === Temporaries ===
//
// These classes are used to allocate temporary registers.
// A mechanism is provided to attempt to reuse the registers
// currently allocated to child nodes whose value is consumed
// by, and not live after, this operation.

enum ReuseTag { Reuse };

class GPRTemporary {
public:
    GPRTemporary();
    GPRTemporary(SpeculativeJIT*);
    GPRTemporary(SpeculativeJIT*, GPRReg specific);
    template<typename T>
    GPRTemporary(SpeculativeJIT* jit, ReuseTag, T& operand)
        : m_jit(jit)
        , m_gpr(InvalidGPRReg)
    {
        if (m_jit->canReuse(operand.node()))
            m_gpr = m_jit->reuse(operand.gpr());
        else
            m_gpr = m_jit->allocate();
    }
    template<typename T1, typename T2>
    GPRTemporary(SpeculativeJIT* jit, ReuseTag, T1& op1, T2& op2)
        : m_jit(jit)
        , m_gpr(InvalidGPRReg)
    {
        if (m_jit->canReuse(op1.node()))
            m_gpr = m_jit->reuse(op1.gpr());
        else if (m_jit->canReuse(op2.node()))
            m_gpr = m_jit->reuse(op2.gpr());
        else if (m_jit->canReuse(op1.node(), op2.node()) && op1.gpr() == op2.gpr())
            m_gpr = m_jit->reuse(op1.gpr());
        else
            m_gpr = m_jit->allocate();
    }
    GPRTemporary(SpeculativeJIT*, ReuseTag, JSValueOperand&, WhichValueWord);

    GPRTemporary(GPRTemporary& other) = delete;

    GPRTemporary(GPRTemporary&& other)
    {
        ASSERT(other.m_jit);
        ASSERT(other.m_gpr != InvalidGPRReg);
        m_jit = other.m_jit;
        m_gpr = other.m_gpr;
        other.m_jit = nullptr;
        other.m_gpr = InvalidGPRReg;
    }

    GPRTemporary& operator=(GPRTemporary&& other)
    {
        ASSERT(!m_jit);
        ASSERT(m_gpr == InvalidGPRReg);
        std::swap(m_jit, other.m_jit);
        std::swap(m_gpr, other.m_gpr);
        return *this;
    }

    void adopt(GPRTemporary&);

    ~GPRTemporary()
    {
        if (m_jit && m_gpr != InvalidGPRReg)
            m_jit->unlock(gpr());
    }

    GPRReg gpr()
    {
        return m_gpr;
    }

private:
    SpeculativeJIT* m_jit;
    GPRReg m_gpr;
};

class JSValueRegsTemporary {
public:
    JSValueRegsTemporary();
    JSValueRegsTemporary(SpeculativeJIT*);
    template<typename T>
    JSValueRegsTemporary(SpeculativeJIT*, ReuseTag, T& operand, WhichValueWord resultRegWord = PayloadWord);
    JSValueRegsTemporary(SpeculativeJIT*, ReuseTag, JSValueOperand&);
    ~JSValueRegsTemporary();
    
    JSValueRegs regs();

private:
#if USE(JSVALUE64)
    GPRTemporary m_gpr;
#else
    GPRTemporary m_payloadGPR;
    GPRTemporary m_tagGPR;
#endif
};

class FPRTemporary {
    WTF_MAKE_NONCOPYABLE(FPRTemporary);
public:
    FPRTemporary(FPRTemporary&&);
    FPRTemporary(SpeculativeJIT*);
    FPRTemporary(SpeculativeJIT*, SpeculateDoubleOperand&);
    FPRTemporary(SpeculativeJIT*, SpeculateDoubleOperand&, SpeculateDoubleOperand&);
#if USE(JSVALUE32_64)
    FPRTemporary(SpeculativeJIT*, JSValueOperand&);
#endif

    ~FPRTemporary()
    {
        if (LIKELY(m_jit))
            m_jit->unlock(fpr());
    }

    FPRReg fpr() const
    {
        ASSERT(m_jit);
        ASSERT(m_fpr != InvalidFPRReg);
        return m_fpr;
    }

protected:
    FPRTemporary(SpeculativeJIT* jit, FPRReg lockedFPR)
        : m_jit(jit)
        , m_fpr(lockedFPR)
    {
    }

private:
    SpeculativeJIT* m_jit;
    FPRReg m_fpr;
};


// === Results ===
//
// These classes lock the result of a call to a C++ helper function.

class GPRFlushedCallResult : public GPRTemporary {
public:
    GPRFlushedCallResult(SpeculativeJIT* jit)
        : GPRTemporary(jit, GPRInfo::returnValueGPR)
    {
    }
};

#if USE(JSVALUE32_64)
class GPRFlushedCallResult2 : public GPRTemporary {
public:
    GPRFlushedCallResult2(SpeculativeJIT* jit)
        : GPRTemporary(jit, GPRInfo::returnValueGPR2)
    {
    }
};
#endif

class FPRResult : public FPRTemporary {
public:
    FPRResult(SpeculativeJIT* jit)
        : FPRTemporary(jit, lockedResult(jit))
    {
    }

private:
    static FPRReg lockedResult(SpeculativeJIT* jit)
    {
        jit->lock(FPRInfo::returnValueFPR);
        return FPRInfo::returnValueFPR;
    }
};

class JSValueRegsFlushedCallResult {
public:
    JSValueRegsFlushedCallResult(SpeculativeJIT* jit)
#if USE(JSVALUE64)
        : m_gpr(jit)
#else
        : m_payloadGPR(jit)
        , m_tagGPR(jit)
#endif
    {
    }

    JSValueRegs regs()
    {
#if USE(JSVALUE64)
        return JSValueRegs { m_gpr.gpr() };
#else
        return JSValueRegs { m_tagGPR.gpr(), m_payloadGPR.gpr() };
#endif
    }

private:
#if USE(JSVALUE64)
    GPRFlushedCallResult m_gpr;
#else
    GPRFlushedCallResult m_payloadGPR;
    GPRFlushedCallResult2 m_tagGPR;
#endif
};


// === Speculative Operand types ===
//
// SpeculateInt32Operand, SpeculateStrictInt32Operand and SpeculateCellOperand.
//
// These are used to lock the operands to a node into machine registers within the
// SpeculativeJIT. The classes operate like those above, however these will
// perform a speculative check for a more restrictive type than we can statically
// determine the operand to have. If the operand does not have the requested type,
// a bail-out to the non-speculative path will be taken.

class SpeculateInt32Operand {
public:
    explicit SpeculateInt32Operand(SpeculativeJIT* jit, Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
        : m_jit(jit)
        , m_edge(edge)
        , m_gprOrInvalid(InvalidGPRReg)
#ifndef NDEBUG
        , m_format(DataFormatNone)
#endif
    {
        ASSERT(m_jit);
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || (edge.useKind() == Int32Use || edge.useKind() == KnownInt32Use));
        if (jit->isFilled(node()))
            gpr();
    }

    ~SpeculateInt32Operand()
    {
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }
    
    Edge edge() const
    {
        return m_edge;
    }

    Node* node() const
    {
        return edge().node();
    }

    DataFormat format()
    {
        gpr(); // m_format is set when m_gpr is locked.
        ASSERT(m_format == DataFormatInt32 || m_format == DataFormatJSInt32);
        return m_format;
    }

    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillSpeculateInt32(edge(), m_format);
        return m_gprOrInvalid;
    }
    
    void use()
    {
        m_jit->use(node());
    }

private:
    SpeculativeJIT* m_jit;
    Edge m_edge;
    GPRReg m_gprOrInvalid;
    DataFormat m_format;
};

class SpeculateStrictInt32Operand {
public:
    explicit SpeculateStrictInt32Operand(SpeculativeJIT* jit, Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
        : m_jit(jit)
        , m_edge(edge)
        , m_gprOrInvalid(InvalidGPRReg)
    {
        ASSERT(m_jit);
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || (edge.useKind() == Int32Use || edge.useKind() == KnownInt32Use));
        if (jit->isFilled(node()))
            gpr();
    }

    ~SpeculateStrictInt32Operand()
    {
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }
    
    Edge edge() const
    {
        return m_edge;
    }

    Node* node() const
    {
        return edge().node();
    }

    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillSpeculateInt32Strict(edge());
        return m_gprOrInvalid;
    }
    
    void use()
    {
        m_jit->use(node());
    }

private:
    SpeculativeJIT* m_jit;
    Edge m_edge;
    GPRReg m_gprOrInvalid;
};

// Gives you a canonical Int52 (i.e. it's left-shifted by 16, low bits zero).
class SpeculateInt52Operand {
public:
    explicit SpeculateInt52Operand(SpeculativeJIT* jit, Edge edge)
        : m_jit(jit)
        , m_edge(edge)
        , m_gprOrInvalid(InvalidGPRReg)
    {
        RELEASE_ASSERT(edge.useKind() == Int52RepUse);
        if (jit->isFilled(node()))
            gpr();
    }
    
    ~SpeculateInt52Operand()
    {
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }
    
    Edge edge() const
    {
        return m_edge;
    }
    
    Node* node() const
    {
        return edge().node();
    }
    
    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillSpeculateInt52(edge(), DataFormatInt52);
        return m_gprOrInvalid;
    }
    
    void use()
    {
        m_jit->use(node());
    }
    
private:
    SpeculativeJIT* m_jit;
    Edge m_edge;
    GPRReg m_gprOrInvalid;
};

// Gives you a strict Int52 (i.e. the payload is in the low 48 bits, high 16 bits are sign-extended).
class SpeculateStrictInt52Operand {
public:
    explicit SpeculateStrictInt52Operand(SpeculativeJIT* jit, Edge edge)
        : m_jit(jit)
        , m_edge(edge)
        , m_gprOrInvalid(InvalidGPRReg)
    {
        RELEASE_ASSERT(edge.useKind() == Int52RepUse);
        if (jit->isFilled(node()))
            gpr();
    }
    
    ~SpeculateStrictInt52Operand()
    {
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }
    
    Edge edge() const
    {
        return m_edge;
    }
    
    Node* node() const
    {
        return edge().node();
    }
    
    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillSpeculateInt52(edge(), DataFormatStrictInt52);
        return m_gprOrInvalid;
    }
    
    void use()
    {
        m_jit->use(node());
    }
    
private:
    SpeculativeJIT* m_jit;
    Edge m_edge;
    GPRReg m_gprOrInvalid;
};

enum OppositeShiftTag { OppositeShift };

class SpeculateWhicheverInt52Operand {
public:
    explicit SpeculateWhicheverInt52Operand(SpeculativeJIT* jit, Edge edge)
        : m_jit(jit)
        , m_edge(edge)
        , m_gprOrInvalid(InvalidGPRReg)
        , m_strict(jit->betterUseStrictInt52(edge))
    {
        RELEASE_ASSERT(edge.useKind() == Int52RepUse);
        if (jit->isFilled(node()))
            gpr();
    }
    
    explicit SpeculateWhicheverInt52Operand(SpeculativeJIT* jit, Edge edge, const SpeculateWhicheverInt52Operand& other)
        : m_jit(jit)
        , m_edge(edge)
        , m_gprOrInvalid(InvalidGPRReg)
        , m_strict(other.m_strict)
    {
        RELEASE_ASSERT(edge.useKind() == Int52RepUse);
        if (jit->isFilled(node()))
            gpr();
    }
    
    explicit SpeculateWhicheverInt52Operand(SpeculativeJIT* jit, Edge edge, OppositeShiftTag, const SpeculateWhicheverInt52Operand& other)
        : m_jit(jit)
        , m_edge(edge)
        , m_gprOrInvalid(InvalidGPRReg)
        , m_strict(!other.m_strict)
    {
        RELEASE_ASSERT(edge.useKind() == Int52RepUse);
        if (jit->isFilled(node()))
            gpr();
    }
    
    ~SpeculateWhicheverInt52Operand()
    {
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }
    
    Edge edge() const
    {
        return m_edge;
    }
    
    Node* node() const
    {
        return edge().node();
    }
    
    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg) {
            m_gprOrInvalid = m_jit->fillSpeculateInt52(
                edge(), m_strict ? DataFormatStrictInt52 : DataFormatInt52);
        }
        return m_gprOrInvalid;
    }
    
    void use()
    {
        m_jit->use(node());
    }
    
    DataFormat format() const
    {
        return m_strict ? DataFormatStrictInt52 : DataFormatInt52;
    }

private:
    SpeculativeJIT* m_jit;
    Edge m_edge;
    GPRReg m_gprOrInvalid;
    bool m_strict;
};

class SpeculateDoubleOperand {
public:
    explicit SpeculateDoubleOperand(SpeculativeJIT* jit, Edge edge)
        : m_jit(jit)
        , m_edge(edge)
        , m_fprOrInvalid(InvalidFPRReg)
    {
        ASSERT(m_jit);
        RELEASE_ASSERT(isDouble(edge.useKind()));
        if (jit->isFilled(node()))
            fpr();
    }

    ~SpeculateDoubleOperand()
    {
        ASSERT(m_fprOrInvalid != InvalidFPRReg);
        m_jit->unlock(m_fprOrInvalid);
    }
    
    Edge edge() const
    {
        return m_edge;
    }

    Node* node() const
    {
        return edge().node();
    }

    FPRReg fpr()
    {
        if (m_fprOrInvalid == InvalidFPRReg)
            m_fprOrInvalid = m_jit->fillSpeculateDouble(edge());
        return m_fprOrInvalid;
    }
    
    void use()
    {
        m_jit->use(node());
    }

private:
    SpeculativeJIT* m_jit;
    Edge m_edge;
    FPRReg m_fprOrInvalid;
};

class SpeculateCellOperand {
    WTF_MAKE_NONCOPYABLE(SpeculateCellOperand);

public:
    explicit SpeculateCellOperand(SpeculativeJIT* jit, Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
        : m_jit(jit)
        , m_edge(edge)
        , m_gprOrInvalid(InvalidGPRReg)
    {
        ASSERT(m_jit);
        if (!edge)
            return;
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || isCell(edge.useKind()));
        if (jit->isFilled(node()))
            gpr();
    }

    explicit SpeculateCellOperand(SpeculateCellOperand&& other)
    {
        m_jit = other.m_jit;
        m_edge = other.m_edge;
        m_gprOrInvalid = other.m_gprOrInvalid;

        other.m_gprOrInvalid = InvalidGPRReg;
        other.m_edge = Edge();
    }

    ~SpeculateCellOperand()
    {
        if (!m_edge)
            return;
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }
    
    Edge edge() const
    {
        return m_edge;
    }

    Node* node() const
    {
        return edge().node();
    }

    GPRReg gpr()
    {
        ASSERT(m_edge);
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillSpeculateCell(edge());
        return m_gprOrInvalid;
    }
    
    void use()
    {
        ASSERT(m_edge);
        m_jit->use(node());
    }

private:
    SpeculativeJIT* m_jit;
    Edge m_edge;
    GPRReg m_gprOrInvalid;
};

class SpeculateBooleanOperand {
public:
    explicit SpeculateBooleanOperand(SpeculativeJIT* jit, Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
        : m_jit(jit)
        , m_edge(edge)
        , m_gprOrInvalid(InvalidGPRReg)
    {
        ASSERT(m_jit);
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == BooleanUse || edge.useKind() == KnownBooleanUse);
        if (jit->isFilled(node()))
            gpr();
    }
    
    ~SpeculateBooleanOperand()
    {
        ASSERT(m_gprOrInvalid != InvalidGPRReg);
        m_jit->unlock(m_gprOrInvalid);
    }
    
    Edge edge() const
    {
        return m_edge;
    }
    
    Node* node() const
    {
        return edge().node();
    }
    
    GPRReg gpr()
    {
        if (m_gprOrInvalid == InvalidGPRReg)
            m_gprOrInvalid = m_jit->fillSpeculateBoolean(edge());
        return m_gprOrInvalid;
    }
    
    void use()
    {
        m_jit->use(node());
    }

private:
    SpeculativeJIT* m_jit;
    Edge m_edge;
    GPRReg m_gprOrInvalid;
};

template<typename StructureLocationType>
void SpeculativeJIT::speculateStringObjectForStructure(Edge edge, StructureLocationType structureLocation)
{
    RegisteredStructure stringObjectStructure =
        m_jit.graph().registerStructure(m_jit.globalObjectFor(m_currentNode->origin.semantic)->stringObjectStructure());
    
    if (!m_state.forNode(edge).m_structure.isSubsetOf(RegisteredStructureSet(stringObjectStructure))) {
        speculationCheck(
            NotStringObject, JSValueRegs(), 0,
            m_jit.branchWeakStructure(
                JITCompiler::NotEqual, structureLocation, stringObjectStructure));
    }
}

#define DFG_TYPE_CHECK_WITH_EXIT_KIND(exitKind, source, edge, typesPassedThrough, jumpToFail) do { \
        JSValueSource _dtc_source = (source);                           \
        Edge _dtc_edge = (edge);                                        \
        SpeculatedType _dtc_typesPassedThrough = typesPassedThrough;    \
        if (!needsTypeCheck(_dtc_edge, _dtc_typesPassedThrough))        \
            break;                                                      \
        typeCheck(_dtc_source, _dtc_edge, _dtc_typesPassedThrough, (jumpToFail), exitKind); \
    } while (0)

#define DFG_TYPE_CHECK(source, edge, typesPassedThrough, jumpToFail) \
    DFG_TYPE_CHECK_WITH_EXIT_KIND(BadType, source, edge, typesPassedThrough, jumpToFail)

} } // namespace JSC::DFG

#endif
