/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "AirArg.h"
#include "AirBasicBlock.h"
#include "AirDisassembler.h"
#include "AirSpecial.h"
#include "AirStackSlot.h"
#include "AirTmp.h"
#include "B3SparseCollection.h"
#include "GPRInfo.h"
#include "MacroAssembler.h"
#include "RegisterAtOffsetList.h"
#include "StackAlignment.h"
#include <wtf/HashSet.h>
#include <wtf/IndexMap.h>
#include <wtf/SmallSet.h>
#include <wtf/WeakRandom.h>

namespace JSC {

class CCallHelpers;

namespace B3 {

class Procedure;

#if !ASSERT_ENABLED
IGNORE_RETURN_TYPE_WARNINGS_BEGIN
#endif

namespace Air {

class GenerateAndAllocateRegisters;
class BlockInsertionSet;
class CCallSpecial;
class CFG;
class Code;
class Disassembler;

typedef void WasmBoundsCheckGeneratorFunction(CCallHelpers&, GPRReg);
typedef SharedTask<WasmBoundsCheckGeneratorFunction> WasmBoundsCheckGenerator;

typedef void PrologueGeneratorFunction(CCallHelpers&, Code&);
typedef SharedTask<PrologueGeneratorFunction> PrologueGenerator;

extern const char* const tierName;

// This is an IR that is very close to the bare metal. It requires about 40x more bytes than the
// generated machine code - for example if you're generating 1MB of machine code, you need about
// 40MB of Air.

class Code {
    WTF_MAKE_NONCOPYABLE(Code);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~Code();

    Procedure& proc() { return m_proc; }
    
    const Vector<Reg>& regsInPriorityOrder(Bank bank) const
    {
        switch (bank) {
        case GP:
            return m_gpRegsInPriorityOrder;
        case FP:
            return m_fpRegsInPriorityOrder;
        }
        ASSERT_NOT_REACHED();
    }
    
    // This is the set of registers that Air is allowed to emit code to mutate. It's derived from
    // regsInPriorityOrder. Any registers not in this set are said to be "pinned".
    RegisterSet mutableRegs() const { return m_mutableRegs.toRegisterSet().includeWholeRegisterWidth(); }
    
    bool isPinned(Reg reg) const { return !mutableRegs().contains(reg, IgnoreVectors); }
    void pinRegister(Reg);
    
    void setOptLevel(unsigned optLevel) { m_optLevel = optLevel; }
    unsigned optLevel() const { return m_optLevel; }
    
    bool needsUsedRegisters() const;

    JS_EXPORT_PRIVATE BasicBlock* addBlock(double frequency = 1);

    // Note that you can rely on stack slots always getting indices that are larger than the index
    // of any prior stack slot. In fact, all stack slots you create in the future will have an index
    // that is >= stackSlots().size().
    JS_EXPORT_PRIVATE StackSlot* addStackSlot(uint64_t byteSize, StackSlotKind);

    JS_EXPORT_PRIVATE Special* addSpecial(std::unique_ptr<Special>);

    // This is the special you need to make a C call!
    CCallSpecial* cCallSpecial();

    Tmp newTmp(Bank bank)
    {
        switch (bank) {
        case GP:
            return Tmp::gpTmpForIndex(m_numGPTmps++);
        case FP:
            return Tmp::fpTmpForIndex(m_numFPTmps++);
        }
        ASSERT_NOT_REACHED();
    }

    unsigned numTmps(Bank bank)
    {
        switch (bank) {
        case GP:
            return m_numGPTmps;
        case FP:
            return m_numFPTmps;
        }
        ASSERT_NOT_REACHED();
    }
    
    template<typename Func>
    void forEachTmp(const Func& func)
    {
        for (unsigned bankIndex = 0; bankIndex < numBanks; ++bankIndex) {
            Bank bank = static_cast<Bank>(bankIndex);
            unsigned numTmps = this->numTmps(bank);
            for (unsigned i = 0; i < numTmps; ++i)
                func(Tmp::tmpForIndex(bank, i));
        }
    }

    unsigned callArgAreaSizeInBytes() const { return m_callArgAreaSize; }

    // You can call this before code generation to force a minimum call arg area size.
    void requestCallArgAreaSizeInBytes(unsigned size)
    {
        m_callArgAreaSize = std::max(
            m_callArgAreaSize,
            static_cast<unsigned>(WTF::roundUpToMultipleOf(stackAlignmentBytes(), size)));
    }

    unsigned frameSize() const { return m_frameSize; }

    // Only phases that do stack allocation are allowed to set this. Currently, only
    // Air::allocateStack() does this.
    void setFrameSize(unsigned frameSize)
    {
        m_frameSize = frameSize;
    }

    // Note that this is not the same thing as proc().numEntrypoints(). This value here may be zero
    // until we lower EntrySwitch.
    unsigned numEntrypoints() const { return m_entrypoints.size(); }
    const Vector<FrequentedBlock>& entrypoints() const { return m_entrypoints; }
    const FrequentedBlock& entrypoint(unsigned index) const { return m_entrypoints[index]; }
    bool isEntrypoint(BasicBlock*) const;
    // Note: It is only valid to call this function after LowerEntrySwitch.
    std::optional<unsigned> entrypointIndex(BasicBlock*) const;

    // Note: We allow this to be called even before we set m_entrypoints just for convenience to users of this API.
    // However, if you call this before setNumEntrypoints, setNumEntrypoints will overwrite this value.
    void setPrologueForEntrypoint(unsigned entrypointIndex, Ref<PrologueGenerator>&& generator)
    {
        m_prologueGenerators[entrypointIndex] = WTFMove(generator);
    }
    const Ref<PrologueGenerator>& prologueGeneratorForEntrypoint(unsigned entrypointIndex)
    {
        return m_prologueGenerators[entrypointIndex];
    }

    void setNumEntrypoints(unsigned);

    // This is used by lowerEntrySwitch().
    template<typename Vector>
    void setEntrypoints(Vector&& vector)
    {
        m_entrypoints = std::forward<Vector>(vector);
        RELEASE_ASSERT(m_entrypoints.size() == m_prologueGenerators.size());
    }
    
    MacroAssembler::Label entrypointLabel(unsigned index) const
    {
        return m_entrypointLabels[index];
    }
    
    // This is used by generate().
    template<typename Vector>
    void setEntrypointLabels(Vector&& vector)
    {
        m_entrypointLabels = std::forward<Vector>(vector);
        RELEASE_ASSERT(m_entrypointLabels.size() == m_prologueGenerators.size());
    }
    
    void setStackIsAllocated(bool value)
    {
        m_stackIsAllocated = value;
    }
    
    bool stackIsAllocated() const { return m_stackIsAllocated; }
    
    // This sets the callee save registers.
    void setCalleeSaveRegisterAtOffsetList(RegisterAtOffsetList&&, StackSlot*);

    // This returns the correctly offset list of callee save registers.
    RegisterAtOffsetList calleeSaveRegisterAtOffsetList() const;
    
    // This just tells you what the callee saves are.
    RegisterSetBuilder calleeSaveRegisters() const { return m_calleeSaveRegisters; }

    // Recomputes predecessors and deletes unreachable blocks.
    JS_EXPORT_PRIVATE void resetReachability();
    
    JS_EXPORT_PRIVATE void dump(PrintStream&) const;

    unsigned size() const { return m_blocks.size(); }
    BasicBlock* at(unsigned index) const { return m_blocks[index].get(); }
    BasicBlock* operator[](unsigned index) const { return at(index); }

    // This is used by phases that optimize the block list. You shouldn't use this unless you really know
    // what you're doing.
    Vector<std::unique_ptr<BasicBlock>>& blockList() { return m_blocks; }

    // Finds the smallest index' such that at(index') != null and index' >= index.
    JS_EXPORT_PRIVATE unsigned findFirstBlockIndex(unsigned index) const;

    // Finds the smallest index' such that at(index') != null and index' > index.
    unsigned findNextBlockIndex(unsigned index) const;

    BasicBlock* findNextBlock(BasicBlock*) const;

    class iterator {
    public:
        iterator()
            : m_code(nullptr)
            , m_index(0)
        {
        }

        iterator(const Code& code, unsigned index)
            : m_code(&code)
            , m_index(m_code->findFirstBlockIndex(index))
        {
        }

        BasicBlock* operator*()
        {
            return m_code->at(m_index);
        }

        iterator& operator++()
        {
            m_index = m_code->findFirstBlockIndex(m_index + 1);
            return *this;
        }

        bool operator==(const iterator& other) const
        {
            return m_index == other.m_index;
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

    private:
        const Code* m_code;
        unsigned m_index;
    };

    iterator begin() const { return iterator(*this, 0); }
    iterator end() const { return iterator(*this, size()); }

    const SparseCollection<StackSlot>& stackSlots() const { return m_stackSlots; }
    SparseCollection<StackSlot>& stackSlots() { return m_stackSlots; }

    const SparseCollection<Special>& specials() const { return m_specials; }
    SparseCollection<Special>& specials() { return m_specials; }

    template<typename Callback>
    void forAllTmps(const Callback& callback) const
    {
        for (unsigned i = m_numGPTmps; i--;)
            callback(Tmp::gpTmpForIndex(i));
        for (unsigned i = m_numFPTmps; i--;)
            callback(Tmp::fpTmpForIndex(i));
    }

    void addFastTmp(Tmp);

    template<typename Functor>
    void forEachFastTmp(const Functor& functor) const
    {
        for (Tmp tmp : m_fastTmps)
            functor(tmp);
    }
    
    CFG& cfg() const { return *m_cfg; }
    
    void* addDataSection(size_t);
    
    // The name has to be a string literal, since we don't do any memory management for the string.
    void setLastPhaseName(const char* name)
    {
        m_lastPhaseName = name;
    }

    const char* lastPhaseName() const { return m_lastPhaseName; }

    void setWasmBoundsCheckGenerator(RefPtr<WasmBoundsCheckGenerator> generator)
    {
        m_wasmBoundsCheckGenerator = generator;
    }

    RefPtr<WasmBoundsCheckGenerator> wasmBoundsCheckGenerator() const { return m_wasmBoundsCheckGenerator; }

    // This is a hash of the code. You can use this if you want to put code into a hashtable, but
    // it's mainly for validating the results from JSAir.
    unsigned jsHash() const;

    bool shouldPreserveB3Origins() const { return m_preserveB3Origins; }
    void forcePreservationOfB3Origins() { m_preserveB3Origins = true; }

    void setDisassembler(std::unique_ptr<Disassembler>&& disassembler)
    {
        m_disassembler = WTFMove(disassembler);
        forcePreservationOfB3Origins();
    }
    Disassembler* disassembler() { return m_disassembler.get(); }

    RegisterSet mutableGPRs();
    ScalarRegisterSet pinnedRegisters() const { return m_pinnedRegs; }
    
    void emitDefaultPrologue(CCallHelpers&);
    void emitEpilogue(CCallHelpers&);

    std::unique_ptr<GenerateAndAllocateRegisters> m_generateAndAllocateRegisters;

    void setForceIRCRegisterAllocation() { m_forceIRC = true; }
    bool forceIRCRegisterAllocation() { return m_forceIRC; }
    
private:
    friend class ::JSC::B3::Procedure;
    friend class BlockInsertionSet;
    
    Code(Procedure&);

    void setRegsInPriorityOrder(Bank, const Vector<Reg>&);
    
    Vector<Reg>& regsInPriorityOrderImpl(Bank bank)
    {
        switch (bank) {
        case GP:
            return m_gpRegsInPriorityOrder;
        case FP:
            return m_fpRegsInPriorityOrder;
        }
        ASSERT_NOT_REACHED();
    }

    Procedure& m_proc; // Some meta-data, like byproducts, is stored in the Procedure.
    Vector<Reg> m_gpRegsInPriorityOrder;
    Vector<Reg> m_fpRegsInPriorityOrder;
    ScalarRegisterSet m_mutableRegs;
    ScalarRegisterSet m_pinnedRegs;
    SparseCollection<StackSlot> m_stackSlots;
    Vector<std::unique_ptr<BasicBlock>> m_blocks;
    SparseCollection<Special> m_specials;
    std::unique_ptr<CFG> m_cfg;
    SmallSet<Tmp, TmpHash, 2> m_fastTmps;
    CCallSpecial* m_cCallSpecial { nullptr };
    unsigned m_numGPTmps { 0 };
    unsigned m_numFPTmps { 0 };
    unsigned m_frameSize { 0 };
    unsigned m_callArgAreaSize { 0 };
    unsigned m_optLevel { defaultOptLevel() };
    bool m_stackIsAllocated { false };
    bool m_preserveB3Origins { true };
    bool m_forceIRC { false };
    RegisterAtOffsetList m_uncorrectedCalleeSaveRegisterAtOffsetList;
    RegisterSetBuilder m_calleeSaveRegisters;
    StackSlot* m_calleeSaveStackSlot { nullptr };
    Vector<FrequentedBlock> m_entrypoints; // This is empty until after lowerEntrySwitch().
    Vector<MacroAssembler::Label> m_entrypointLabels; // This is empty until code generation.
    Vector<Ref<PrologueGenerator>, 1> m_prologueGenerators;
    RefPtr<WasmBoundsCheckGenerator> m_wasmBoundsCheckGenerator;
    const char* m_lastPhaseName;
    std::unique_ptr<Disassembler> m_disassembler;
    Ref<PrologueGenerator> m_defaultPrologueGenerator;
};

} } } // namespace JSC::B3::Air

#if !ASSERT_ENABLED
IGNORE_RETURN_TYPE_WARNINGS_END
#endif

#endif // ENABLE(B3_JIT)
