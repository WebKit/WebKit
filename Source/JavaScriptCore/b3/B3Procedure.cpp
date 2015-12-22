/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "B3BasicBlockInlines.h"
#include "B3BasicBlockUtils.h"
#include "B3BlockWorklist.h"
#include "B3CFG.h"
#include "B3DataSection.h"
#include "B3Dominators.h"
#include "B3OpaqueByproducts.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

Procedure::Procedure()
    : m_cfg(new CFG(*this))
    , m_lastPhaseName("initial")
    , m_byproducts(std::make_unique<OpaqueByproducts>())
    , m_code(new Air::Code(*this))
{
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
    m_blocks.append(WTF::move(block));
    return result;
}

Value* Procedure::addIntConstant(Origin origin, Type type, int64_t value)
{
    switch (type) {
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

Value* Procedure::addBoolConstant(Origin origin, TriState triState)
{
    int32_t value = 0;
    switch (triState) {
    case FalseTriState:
        value = 0;
        break;
    case TrueTriState:
        value = 1;
        break;
    case MixedTriState:
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
    B3::resetReachability(
        m_blocks,
        [&] (BasicBlock* deleted) {
            // Gotta delete the values in this block.
            for (Value* value : *deleted)
                deleteValue(value);
        });
}

void Procedure::invalidateCFG()
{
    m_dominators = nullptr;
}

void Procedure::dump(PrintStream& out) const
{
    for (BasicBlock* block : *this)
        out.print(deepDump(*this, block));
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

void Procedure::deleteValue(Value* value)
{
    ASSERT(m_values[value->index()].get() == value);
    m_valueIndexFreeList.append(value->index());
    m_values[value->index()] = nullptr;
}

Dominators& Procedure::dominators()
{
    if (!m_dominators)
        m_dominators = std::make_unique<Dominators>(*this);
    return *m_dominators;
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
    std::unique_ptr<DataSection> dataSection = std::make_unique<DataSection>(size);
    void* result = dataSection->data();
    m_byproducts->add(WTF::move(dataSection));
    return result;
}

unsigned Procedure::callArgAreaSize() const
{
    return code().callArgAreaSize();
}

void Procedure::requestCallArgAreaSize(unsigned size)
{
    code().requestCallArgAreaSize(size);
}

unsigned Procedure::frameSize() const
{
    return code().frameSize();
}

const RegisterAtOffsetList& Procedure::calleeSaveRegisters() const
{
    return code().calleeSaveRegisters();
}

size_t Procedure::addValueIndex()
{
    if (m_valueIndexFreeList.isEmpty()) {
        size_t index = m_values.size();
        m_values.append(nullptr);
        return index;
    }
    
    return m_valueIndexFreeList.takeLast();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
