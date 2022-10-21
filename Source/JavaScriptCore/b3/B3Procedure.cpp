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

#include "config.h"
#include "B3Procedure.h"

#if ENABLE(B3_JIT)

#include "AirCode.h"
#include "AirStackSlotKind.h"
#include "B3BackwardsCFG.h"
#include "B3BackwardsDominators.h"
#include "B3BasicBlockUtils.h"
#include "B3CFG.h"
#include "B3DataSection.h"
#include "B3Dominators.h"
#include "B3NaturalLoops.h"
#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "JITOpaqueByproducts.h"

namespace JSC { namespace B3 {

Procedure::Procedure()
    : m_cfg(new CFG(*this))
    , m_lastPhaseName("initial")
    , m_byproducts(makeUnique<OpaqueByproducts>())
{
    // Initialize all our fields before constructing Air::Code since
    // it looks into our fields.
    m_code = std::unique_ptr<Air::Code>(new Air::Code(*this));
    m_code->setNumEntrypoints(m_numEntrypoints);
}

Procedure::~Procedure()
{
}

void Procedure::printOrigin(PrintStream& out, Origin origin) const
{
    if (m_originPrinter)
        m_originPrinter->run(out, origin);
    else
        out.print(origin);
}

BasicBlock* Procedure::addBlock(double frequency)
{
    std::unique_ptr<BasicBlock> block(new BasicBlock(m_blocks.size(), frequency));
    BasicBlock* result = block.get();
    m_blocks.append(WTFMove(block));
    return result;
}

Air::StackSlot* Procedure::addStackSlot(uint64_t byteSize)
{
    return m_code->addStackSlot(byteSize, Air::StackSlotKind::Locked);
}

Variable* Procedure::addVariable(Type type)
{
    return m_variables.addNew(type); 
}

Type Procedure::addTuple(Vector<Type>&& types)
{
    Type result = Type::tupleFromIndex(m_tuples.size());
    m_tuples.append(WTFMove(types));
    ASSERT(result.isTuple());
    return result;
}

bool Procedure::isValidTuple(Type tuple) const
{
    return tuple.tupleIndex() < m_tuples.size();
}

const Vector<Type>& Procedure::tupleForType(Type tuple) const
{
    return m_tuples[tuple.tupleIndex()];
}

Value* Procedure::clone(Value* value)
{
    std::unique_ptr<Value> clone(value->cloneImpl());
    clone->m_index = UINT_MAX;
    clone->owner = nullptr;
    return m_values.add(WTFMove(clone));
}

Value* Procedure::addIntConstant(Origin origin, Type type, int64_t value)
{
    switch (type.kind()) {
    case Int32:
        return add<Const32Value>(origin, static_cast<int32_t>(value));
    case Int64:
        return add<Const64Value>(origin, value);
    case Double:
        return add<ConstDoubleValue>(origin, static_cast<double>(value));
    case Float:
        return add<ConstFloatValue>(origin, static_cast<float>(value));
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

Value* Procedure::addIntConstant(Value* likeValue, int64_t value)
{
    return addIntConstant(likeValue->origin(), likeValue->type(), value);
}

Value* Procedure::addConstant(Origin origin, Type type, uint64_t bits)
{
    switch (type.kind()) {
    case Int32:
        return add<Const32Value>(origin, static_cast<int32_t>(bits));
    case Int64:
        return add<Const64Value>(origin, bits);
    case Float:
        return add<ConstFloatValue>(origin, bitwise_cast<float>(static_cast<int32_t>(bits)));
    case Double:
        return add<ConstDoubleValue>(origin, bitwise_cast<double>(bits));
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

Value* Procedure::addBottom(Origin origin, Type type)
{
    if (type.isTuple())
        return add<BottomTupleValue>(origin, type);
    return addIntConstant(origin, type, 0);
}

Value* Procedure::addBottom(Value* value)
{
    return addBottom(value->origin(), value->type());
}

Value* Procedure::addBoolConstant(Origin origin, TriState triState)
{
    int32_t value = 0;
    switch (triState) {
    case TriState::False:
        value = 0;
        break;
    case TriState::True:
        value = 1;
        break;
    case TriState::Indeterminate:
        return nullptr;
    }

    return addIntConstant(origin, Int32, value);
}

void Procedure::resetValueOwners()
{
    for (BasicBlock* block : *this) {
        for (Value* value : *block)
            value->owner = block;
    }
}

void Procedure::resetReachability()
{
    recomputePredecessors(m_blocks);
    
    // The common case is that this does not find any dead blocks.
    bool foundDead = false;
    for (auto& block : m_blocks) {
        if (isBlockDead(block.get())) {
            foundDead = true;
            break;
        }
    }
    if (!foundDead)
        return;
    
    resetValueOwners();

    for (Value* value : values()) {
        if (UpsilonValue* upsilon = value->as<UpsilonValue>()) {
            if (isBlockDead(upsilon->phi()->owner))
                upsilon->replaceWithNop();
        }
    }
    
    for (auto& block : m_blocks) {
        if (isBlockDead(block.get())) {
            for (Value* value : *block)
                deleteValue(value);
            block = nullptr;
        }
    }
}

void Procedure::invalidateCFG()
{
    m_dominators = nullptr;
    m_naturalLoops = nullptr;
    m_backwardsCFG = nullptr;
    m_backwardsDominators = nullptr;
}

void Procedure::dump(PrintStream& out) const
{
    out.print("Opt Level: ", optLevel(), "\n");
    IndexSet<Value*> valuesInBlocks;
    for (BasicBlock* block : *this) {
        out.print(deepDump(*this, block));
        valuesInBlocks.addAll(*block);
    }
    bool didPrint = false;
    for (Value* value : values()) {
        if (valuesInBlocks.contains(value))
            continue;

        if (!didPrint) {
            dataLog(tierName, "Orphaned values:\n");
            didPrint = true;
        }
        dataLog(tierName, "    ", deepDump(*this, value), "\n");
    }
    if (hasQuirks())
        out.print(tierName, "Has Quirks: True\n");
    if (variables().size()) {
        out.print(tierName, "Variables:\n");
        for (Variable* variable : variables())
            out.print(tierName, "    ", deepDump(variable), "\n");
    }
    if (stackSlots().size()) {
        out.print(tierName, "Stack slots:\n");
        for (Air::StackSlot* slot : stackSlots())
            out.print(tierName, "    ", pointerDump(slot), ": ", deepDump(slot), "\n");
    }
    if (m_byproducts->count())
        out.print(*m_byproducts);
}

Vector<BasicBlock*> Procedure::blocksInPreOrder()
{
    return B3::blocksInPreOrder(at(0));
}

Vector<BasicBlock*> Procedure::blocksInPostOrder()
{
    return B3::blocksInPostOrder(at(0));
}

void Procedure::deleteVariable(Variable* variable)
{
    m_variables.remove(variable);
}

void Procedure::deleteValue(Value* value)
{
    m_values.remove(value);
}

void Procedure::deleteOrphans()
{
    IndexSet<Value*> valuesInBlocks;
    for (BasicBlock* block : *this)
        valuesInBlocks.addAll(*block);

    // Since this method is not on any hot path, we do it conservatively: first a pass to
    // identify the values to be removed, and then a second pass to remove them. This avoids any
    // risk of the value iteration being broken by removals.
    Vector<Value*, 16> toRemove;
    for (Value* value : values()) {
        if (!valuesInBlocks.contains(value))
            toRemove.append(value);
        else if (UpsilonValue* upsilon = value->as<UpsilonValue>()) {
            if (!valuesInBlocks.contains(upsilon->phi()))
                upsilon->replaceWithNop();
        }
    }

    for (Value* value : toRemove)
        deleteValue(value);
}

Dominators& Procedure::dominators()
{
    if (!m_dominators)
        m_dominators = makeUnique<Dominators>(*this);
    return *m_dominators;
}

NaturalLoops& Procedure::naturalLoops()
{
    if (!m_naturalLoops)
        m_naturalLoops = makeUnique<NaturalLoops>(*this);
    return *m_naturalLoops;
}

BackwardsCFG& Procedure::backwardsCFG()
{
    if (!m_backwardsCFG)
        m_backwardsCFG = makeUnique<BackwardsCFG>(*this);
    return *m_backwardsCFG;
}

BackwardsDominators& Procedure::backwardsDominators()
{
    if (!m_backwardsDominators)
        m_backwardsDominators = makeUnique<BackwardsDominators>(*this);
    return *m_backwardsDominators;
}

void Procedure::addFastConstant(const ValueKey& constant)
{
    RELEASE_ASSERT(constant.isConstant());
    m_fastConstants.add(constant);
}

bool Procedure::isFastConstant(const ValueKey& constant)
{
    if (!constant)
        return false;
    return m_fastConstants.contains(constant);
}

void* Procedure::addDataSection(size_t size)
{
    if (!size)
        return nullptr;
    std::unique_ptr<DataSection> dataSection = makeUnique<DataSection>(size);
    void* result = dataSection->data();
    m_byproducts->add(WTFMove(dataSection));
    return result;
}

unsigned Procedure::callArgAreaSizeInBytes() const
{
    return code().callArgAreaSizeInBytes();
}

void Procedure::requestCallArgAreaSizeInBytes(unsigned size)
{
    code().requestCallArgAreaSizeInBytes(size);
}

void Procedure::pinRegister(Reg reg)
{
    code().pinRegister(reg);
}

void Procedure::setOptLevel(unsigned optLevel)
{
    m_optLevel = optLevel;
    code().setOptLevel(optLevel);
}

unsigned Procedure::frameSize() const
{
    return code().frameSize();
}

RegisterAtOffsetList Procedure::calleeSaveRegisterAtOffsetList() const
{
    return code().calleeSaveRegisterAtOffsetList();
}

Value* Procedure::addValueImpl(Value* value)
{
    return m_values.add(std::unique_ptr<Value>(value));
}

void Procedure::setBlockOrderImpl(Vector<BasicBlock*>& blocks)
{
    IndexSet<BasicBlock*> blocksSet;
    blocksSet.addAll(blocks);

    for (BasicBlock* block : *this) {
        if (!blocksSet.contains(block))
            blocks.append(block);
    }

    // Place blocks into this's block list by first leaking all of the blocks and then readopting
    // them.
    for (auto& entry : m_blocks)
        entry.release();

    m_blocks.resize(blocks.size());
    for (unsigned i = 0; i < blocks.size(); ++i) {
        BasicBlock* block = blocks[i];
        block->m_index = i;
        m_blocks[i] = std::unique_ptr<BasicBlock>(block);
    }
}

void Procedure::setWasmBoundsCheckGenerator(RefPtr<WasmBoundsCheckGenerator> generator)
{
    code().setWasmBoundsCheckGenerator(generator);
}

RegisterSetBuilder Procedure::mutableGPRs()
{
    return code().mutableGPRs();
}

void Procedure::setNumEntrypoints(unsigned numEntrypoints)
{
    m_numEntrypoints = numEntrypoints;
    m_code->setNumEntrypoints(numEntrypoints);
}

void Procedure::freeUnneededB3ValuesAfterLowering()
{
    // We cannot clear m_stackSlots() or m_tuples here, as they are unfortunately modified and read respectively by Air.
    m_variables.clearAll();
    m_blocks.clear();
    m_cfg = nullptr;
    m_dominators = nullptr;
    m_naturalLoops = nullptr;
    m_backwardsCFG = nullptr;
    m_backwardsDominators = nullptr;
    m_fastConstants.clear();

    if (m_code->shouldPreserveB3Origins())
        return;

    BitVector valuesToPreserve;
    valuesToPreserve.ensureSize(m_values.size());
    for (Value* value : m_values) {
        switch (value->opcode()) {
        // Ideally we would also be able to get rid of all of those.
        // But Air currently relies on these origins being preserved, see https://bugs.webkit.org/show_bug.cgi?id=194040
        case WasmBoundsCheck:
            valuesToPreserve.quickSet(value->index());
            break;
        case CCall:
        case Patchpoint:
        case CheckAdd:
        case CheckSub:
        case CheckMul:
        case Check:
            valuesToPreserve.quickSet(value->index());
            for (Value* child : value->children())
                valuesToPreserve.quickSet(child->index());
            break;
        default:
            break;
        }
    }
    for (Value* value : m_values) {
        if (!valuesToPreserve.quickGet(value->index()))
            m_values.remove(value);
    }
    m_values.packIndices();
}

void Procedure::setShouldDumpIR()
{
    m_shouldDumpIR = true;
    m_code->forcePreservationOfB3Origins();
}

void Procedure::setNeedsPCToOriginMap()
{ 
    m_needsPCToOriginMap = true;
    m_code->forcePreservationOfB3Origins();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
