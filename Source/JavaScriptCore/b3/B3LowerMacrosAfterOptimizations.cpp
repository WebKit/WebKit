/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#include "B3LowerMacrosAfterOptimizations.h"

#if ENABLE(B3_JIT)

#include "AirArg.h"
#include "B3BasicBlockInlines.h"
#include "B3BlockInsertionSet.h"
#include "B3BulkMemoryValue.h"
#include "B3CCallValue.h"
#include "B3Const32Value.h"
#include "B3ConstDoubleValue.h"
#include "B3ConstFloatValue.h"
#include "B3ConstPtrValue.h"
#include "B3InsertionSetInlines.h"
#include "B3MemoryValueInlines.h"
#include "B3Operations.h"
#include "B3PhaseScope.h"
#include "B3SIMDValue.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

using Arg = Air::Arg;
using Code = Air::Code;
using Tmp = Air::Tmp;

namespace {

class LowerMacrosAfterOptimizations {
public:
    LowerMacrosAfterOptimizations(Procedure& proc)
        : m_proc(proc)
        , m_blockInsertionSet(proc)
        , m_insertionSet(proc)
    {
    }

    bool run()
    {
        for (BasicBlock* block : m_proc) {
            m_block = block;
            processCurrentBlock();
        }
        m_changed |= m_blockInsertionSet.execute();
        if (m_changed) {
            m_proc.resetReachability();
            m_proc.invalidateCFG();
        }
        return m_changed;
    }
    
private:
    void processCurrentBlock()
    {
        for (m_index = 0; m_index < m_block->size(); ++m_index) {
            m_value = m_block->at(m_index);
            m_origin = m_value->origin();
            switch (m_value->opcode()) {
            case Abs: {
                // ARM supports this instruction natively.
                if (isARM64())
                    break;

                Value* mask = nullptr;
                if (m_value->type() == Double)
                    mask = m_insertionSet.insert<ConstDoubleValue>(m_index, m_origin, std::bit_cast<double>(~(1ll << 63)));
                else if (m_value->type() == Float)
                    mask = m_insertionSet.insert<ConstFloatValue>(m_index, m_origin, std::bit_cast<float>(~(1 << 31)));
                else
                    RELEASE_ASSERT_NOT_REACHED();
                Value* result = m_insertionSet.insert<Value>(m_index, BitAnd, m_origin, m_value->child(0), mask);
                m_value->replaceWithIdentity(result);
                break;
            }
            case Ceil: {
                if (MacroAssembler::supportsFloatingPointRounding())
                    break;

                Value* functionAddress = nullptr;
                if (m_value->type() == Double)
                    functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(Math::ceilDouble));
                else if (m_value->type() == Float)
                    functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(Math::ceilFloat));
                else
                    RELEASE_ASSERT_NOT_REACHED();

                Value* result = m_insertionSet.insert<CCallValue>(m_index,
                    m_value->type(),
                    m_origin,
                    Effects::none(),
                    functionAddress,
                    m_value->child(0));
                m_value->replaceWithIdentity(result);
                break;
            }
            case Floor: {
                if (MacroAssembler::supportsFloatingPointRounding())
                    break;

                Value* functionAddress = nullptr;
                if (m_value->type() == Double)
                    functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(Math::floorDouble));
                else if (m_value->type() == Float)
                    functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(Math::floorFloat));
                else
                    RELEASE_ASSERT_NOT_REACHED();

                Value* result = m_insertionSet.insert<CCallValue>(m_index,
                    m_value->type(),
                    m_origin,
                    Effects::none(),
                    functionAddress,
                    m_value->child(0));
                m_value->replaceWithIdentity(result);
                break;
            }
            case FTrunc: {
                if (MacroAssembler::supportsFloatingPointRounding())
                    break;

                Value* functionAddress = nullptr;
                if (m_value->type() == Double)
                    functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(Math::truncDouble));
                else if (m_value->type() == Float)
                    functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(Math::truncFloat));
                else
                    RELEASE_ASSERT_NOT_REACHED();

                Value* result = m_insertionSet.insert<CCallValue>(m_index,
                    m_value->type(),
                    m_origin,
                    Effects::none(),
                    functionAddress,
                    m_value->child(0));
                m_value->replaceWithIdentity(result);
                break;
            }

            case Neg: {
                if (!m_value->type().isFloat())
                    break;
                
                // X86 is odd in that it requires this.
                if (!isX86())
                    break;

                Value* mask = nullptr;
                if (m_value->type() == Double)
                    mask = m_insertionSet.insert<ConstDoubleValue>(m_index, m_origin, -0.0);
                else {
                    RELEASE_ASSERT(m_value->type() == Float);
                    mask = m_insertionSet.insert<ConstFloatValue>(m_index, m_origin, -0.0f);
                }

                Value* result = m_insertionSet.insert<Value>(
                    m_index, BitXor, m_origin, m_value->child(0), mask);
                m_value->replaceWithIdentity(result);
                break;
            }

            case PurifyNaN: {
                if (m_value->type() == Double) {
                    auto* pureNaN = m_insertionSet.insert<ConstDoubleValue>(m_index, m_value->origin(), PNaN);
                    auto* compare = m_insertionSet.insert<Value>(m_index, Equal, m_value->origin(), m_value->child(0), m_value->child(0));
                    auto* result = m_insertionSet.insert<Value>(m_index, Select, m_value->origin(), compare, m_value->child(0), pureNaN);
                    m_value->replaceWithIdentity(result);
                } else {
                    auto* pureNaN = m_insertionSet.insert<ConstFloatValue>(m_index, m_value->origin(), static_cast<float>(PNaN));
                    auto* compare = m_insertionSet.insert<Value>(m_index, Equal, m_value->origin(), m_value->child(0), m_value->child(0));
                    auto* result = m_insertionSet.insert<Value>(m_index, Select, m_value->origin(), compare, m_value->child(0), pureNaN);
                    m_value->replaceWithIdentity(result);
                }
                break;
            }

            case RotL: {
                // ARM64 doesn't have a rotate left.
                if (isARM64()) {
                    Value* newShift = m_insertionSet.insert<Value>(m_index, Neg, m_value->origin(), m_value->child(1));
                    Value* rotate = m_insertionSet.insert<Value>(m_index, RotR, m_value->origin(), m_value->child(0), newShift);
                    m_value->replaceWithIdentity(rotate);
                    break;
                }
                break;
            }

            case MemoryMove: {
                Value* functionAddress;
                if (m_value->as<BulkMemoryValue>()->isGCSafe())
                    functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(operationGCSafeMemoryMove));
                else
                    functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(operationMemoryMove));
                Value* result = m_insertionSet.insert<CCallValue>(
                    m_index,
                    m_value->type(),
                    m_origin,
                    m_value->effects(),
                    functionAddress,
                    m_value->child(0),
                    m_value->child(1),
                    m_value->child(2));
                m_value->replaceWithIdentity(result);
                break;
            }

            case MemoryFill8:
            case MemoryFill16:
            case MemoryFill32:
            case MemoryFill64:
            case MemoryFill128:
                lowerMemoryFill();
                break;

            default:
                break;
            }
        }
        m_insertionSet.execute(m_block);
    }

    // Short fills are inlined as a pair of overlapping stores of the widest width that fits, or a
    // loop of paired widest stores whose final pair ends exactly at the end. Every store writes the
    // same repeating pattern, so the overlap is harmless. Long fills call libc, which fills large
    // ranges faster than a store loop does.
    void lowerMemoryFill()
    {
        constexpr uint64_t maxInlineByteCount = 512;

        auto* memoryFill = m_value->as<BulkMemoryValue>();
        Value* destination = memoryFill->child(0);
        Value* count = memoryFill->child(2);
        HeapRange range = memoryFill->writeRange();
        uint64_t elementSize = bytesForWidth(memoryFill->fillWidth());
        Width unitWidth = memoryFill->widestFillStoreWidth(m_proc);
        int32_t unitSize = bytesForWidth(unitWidth);

        BasicBlock* before = m_blockInsertionSet.splitForward(m_block, m_index, &m_insertionSet);
        BasicBlock* done = m_block;
        before->replaceLastWithNew<Value>(m_proc, Nop, m_origin);

        std::optional<uint64_t> constantByteCount;
        Value* byteCount = count;
        if (count->hasInt() && static_cast<uint64_t>(count->asInt()) <= UINT64_MAX / elementSize) {
            constantByteCount = static_cast<uint64_t>(count->asInt()) * elementSize;
            byteCount = before->appendIntConstant(m_proc, m_origin, pointerType(), *constantByteCount);
        } else if (elementSize > 1)
            byteCount = before->appendNew<Value>(m_proc, Shl, m_origin, count, before->appendNew<Const32Value>(m_proc, m_origin, std::countr_zero(elementSize)));

        auto addBlock = [&] {
            return m_blockInsertionSet.insertBefore(done);
        };
        auto jump = [&](BasicBlock* block, BasicBlock* target) {
            block->appendNew<Value>(m_proc, Jump, m_origin);
            block->setSuccessors(FrequentedBlock(target));
        };
        auto branchIfByteCountIsAtLeast = [&](BasicBlock* block, uint64_t limit, BasicBlock* taken, BasicBlock* notTaken) {
            if (constantByteCount) {
                jump(block, *constantByteCount >= limit ? taken : notTaken);
                return;
            }
            Value* condition = block->appendNew<Value>(m_proc, AboveEqual, m_origin, byteCount, block->appendIntConstant(m_proc, m_origin, pointerType(), limit));
            block->appendNew<Value>(m_proc, Branch, m_origin, condition);
            block->setSuccessors(FrequentedBlock(taken), FrequentedBlock(notTaken));
        };
        auto splat = [&](BasicBlock* block, Width width) {
            return memoryFill->splatFillPattern(m_proc, width, [&](Value* value) {
                block->append(value);
            });
        };
        auto store = [&](BasicBlock* block, Width width, Value* value, Value* pointer, int32_t offset) {
            block->appendNew<MemoryValue>(m_proc, storeOpcode(width == Width128 ? FP : GP, width), m_origin, value, pointer, offset, range);
        };

        BasicBlock* callBlock = addBlock();
        BasicBlock* inlineBlock = addBlock();
        branchIfByteCountIsAtLeast(before, maxInlineByteCount + 1, callBlock, inlineBlock);

        emitMemoryFillCall(callBlock, memoryFill, byteCount);
        jump(callBlock, done);

        Value* end = inlineBlock->appendNew<Value>(m_proc, Add, m_origin, destination, byteCount);
        BasicBlock* wideBlock = addBlock();
        BasicBlock* narrowBlock = elementSize < static_cast<uint64_t>(unitSize) ? addBlock() : done;
        branchIfByteCountIsAtLeast(inlineBlock, unitSize, wideBlock, narrowBlock);

        {
            Value* pattern = splat(wideBlock, unitWidth);
            BasicBlock* pairBlock = addBlock();
            BasicBlock* loopPreheader = addBlock();
            BasicBlock* loopBody = addBlock();
            BasicBlock* loopExit = addBlock();
            branchIfByteCountIsAtLeast(wideBlock, 2 * unitSize, loopPreheader, pairBlock);

            store(pairBlock, unitWidth, pattern, destination, 0);
            store(pairBlock, unitWidth, pattern, end, -unitSize);
            jump(pairBlock, done);

            Value* last = loopPreheader->appendNew<Value>(m_proc, Sub, m_origin, end, loopPreheader->appendIntConstant(m_proc, m_origin, pointerType(), 2 * unitSize));
            UpsilonValue* initialCursor = loopPreheader->appendNew<UpsilonValue>(m_proc, m_origin, destination);
            jump(loopPreheader, loopBody);

            Value* cursor = loopBody->appendNew<Value>(m_proc, Phi, pointerType(), m_origin);
            initialCursor->setPhi(cursor);
            store(loopBody, unitWidth, pattern, cursor, 0);
            store(loopBody, unitWidth, pattern, cursor, unitSize);
            Value* nextCursor = loopBody->appendNew<Value>(m_proc, Add, m_origin, cursor, loopBody->appendIntConstant(m_proc, m_origin, pointerType(), 2 * unitSize));
            loopBody->appendNew<UpsilonValue>(m_proc, m_origin, nextCursor, cursor);
            loopBody->appendNew<Value>(m_proc, Branch, m_origin, loopBody->appendNew<Value>(m_proc, Below, m_origin, nextCursor, last));
            loopBody->setSuccessors(FrequentedBlock(loopBody), FrequentedBlock(loopExit));

            store(loopExit, unitWidth, pattern, last, 0);
            store(loopExit, unitWidth, pattern, last, unitSize);
            jump(loopExit, done);
        }

        BasicBlock* check = narrowBlock;
        for (uint64_t size = unitSize / 2; size >= elementSize; size /= 2) {
            Width width = widthForBytes(size);
            BasicBlock* sizeBlock = addBlock();
            BasicBlock* next = size > elementSize ? addBlock() : done;
            branchIfByteCountIsAtLeast(check, size, sizeBlock, next);

            Value* pattern = splat(sizeBlock, width);
            store(sizeBlock, width, pattern, destination, 0);
            // A fill shorter than two elements is exactly one element.
            if (size > elementSize)
                store(sizeBlock, width, pattern, end, -static_cast<int32_t>(size));
            jump(sizeBlock, done);
            check = next;
        }

        m_value->replaceWithNop();
        before->updatePredecessorsAfter();
        m_changed = true;
    }

    void emitMemoryFillCall(BasicBlock* block, BulkMemoryValue* memoryFill, Value* byteCount)
    {
        Value* destination = memoryFill->child(0);
        Value* pattern = memoryFill->child(1);
        Value* count = memoryFill->child(2);
        auto call = [&](auto operation, auto... arguments) {
            Value* functionAddress = block->appendNew<ConstPtrValue>(m_proc, m_origin, tagCFunction<OperationPtrTag>(operation));
            block->appendNew<CCallValue>(m_proc, Void, m_origin, memoryFill->effects(), functionAddress, destination, arguments...);
        };

        // A pattern whose bytes are all the same is a byte fill, and memset is faster than any
        // wider pattern fill. A byte fill does not keep words whole, though.
        if (memoryFill->fillWidth() > Width8 && !memoryFill->isGCSafe()) {
            if (auto byte = repeatedFillByte(memoryFill)) {
                call(operationMemoryFill8, block->appendIntConstant(m_proc, m_origin, Int32, *byte), byteCount);
                return;
            }
        }

        switch (memoryFill->opcode()) {
        case MemoryFill8:
            call(operationMemoryFill8, pattern, count);
            return;
        case MemoryFill16:
            call(operationMemoryFill16, pattern, count);
            return;
        case MemoryFill32:
            call(operationMemoryFill32, pattern, count);
            return;
        case MemoryFill64:
            if (memoryFill->isGCSafe())
                call(operationGCSafeMemoryFill64, pattern, count);
            else
                call(operationMemoryFill64, pattern, count);
            return;
        case MemoryFill128: {
            Value* lane0;
            Value* lane1;
            if (pattern->hasV128()) {
                lane0 = block->appendIntConstant(m_proc, m_origin, Int64, pattern->asV128().u64x2[0]);
                lane1 = block->appendIntConstant(m_proc, m_origin, Int64, pattern->asV128().u64x2[1]);
            } else {
                lane0 = block->appendNew<SIMDValue>(m_proc, m_origin, VectorExtractLane, Int64, SIMDLane::i64x2, SIMDSignMode::None, uint8_t { 0 }, pattern);
                lane1 = block->appendNew<SIMDValue>(m_proc, m_origin, VectorExtractLane, Int64, SIMDLane::i64x2, SIMDSignMode::None, uint8_t { 1 }, pattern);
            }
            call(operationMemoryFill128, lane0, lane1, count);
            return;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    static std::optional<uint8_t> repeatedFillByte(BulkMemoryValue* memoryFill)
    {
        Value* pattern = memoryFill->child(1);
        Width width = memoryFill->fillWidth();
        uint64_t bits;
        if (pattern->hasV128()) {
            v128_t vector = pattern->asV128();
            if (vector.u64x2[0] != vector.u64x2[1])
                return std::nullopt;
            bits = vector.u64x2[0];
            width = Width64;
        } else if (pattern->hasInt())
            bits = pattern->asInt();
        else
            return std::nullopt;

        uint8_t byte = bits;
        if ((bits & mask(width)) != ((byte * 0x0101010101010101ULL) & mask(width)))
            return std::nullopt;
        return byte;
    }

    Procedure& m_proc;
    BlockInsertionSet m_blockInsertionSet;
    InsertionSet m_insertionSet;
    BasicBlock* m_block;
    unsigned m_index;
    Value* m_value;
    Origin m_origin;
    bool m_changed { false };
};

bool lowerMacrosImpl(Procedure& proc)
{
    LowerMacrosAfterOptimizations lowerMacros(proc);
    return lowerMacros.run();
}

} // anonymous namespace

bool lowerMacrosAfterOptimizations(Procedure& proc)
{
    PhaseScope phaseScope(proc, "lowerMacrosAfterOptimizations"_s);
    bool result = lowerMacrosImpl(proc);
    if (shouldValidateIR())
        RELEASE_ASSERT(!lowerMacrosImpl(proc));
    return result;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

