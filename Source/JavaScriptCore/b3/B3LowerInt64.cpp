/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "B3LowerInt64.h"

#if USE(JSVALUE32_64) && ENABLE(B3_JIT)
#include "AirCCallingConvention.h"
#include "B3BasicBlock.h"
#include "B3BlockInsertionSet.h"
#include "B3Const32Value.h"
#include "B3ConstPtrValue.h"
#include "B3ExtractValue.h"
#include "B3InsertionSet.h"
#include "B3InsertionSetInlines.h"
#include "B3PhaseScope.h"
#include "B3Procedure.h"
#include "B3StackmapGenerationParams.h"
#include "B3Value.h"
#include <wtf/CheckedArithmetic.h>

namespace JSC {
namespace B3 {

namespace B3LowerInt64Internal {
static constexpr bool verbose = false;
}

namespace {
class LowerInt64 {
public:
    LowerInt64(Procedure& proc)
        : m_proc(proc)
        , m_zero(m_proc.addIntConstant(Origin(), Int32, 0))
        , m_insertionSet(proc)
        , m_blockInsertionSet(proc)
    {
    }

    bool run()
    {
        const Vector<BasicBlock*> blocksInPreOrder = m_proc.blocksInPreOrder();

        // Create 2 Int32 Identity Values for every Int64 value, so we always
        // have the replacement of an Int64 available. We can then change what
        // the Identity references in the lowering loop below.
        for (BasicBlock *block : blocksInPreOrder) {
            m_block = block;
            for (m_index = 0; m_index < m_block->size(); ++m_index) {
                m_value = m_block->at(m_index);
                Value* lo;
                Value* hi;
                dataLogLnIf(B3LowerInt64Internal::verbose, "B3LowerInt64: prep for ", deepDump(m_value));
                if ((m_value->type() == Int64) && (m_value->opcode() == Phi)) {
                    // Upsilon expects the target of a ->setPhi() to be a Phi,
                    // not an identity, so do the (trivial) lowering here.
                    lo = insert<Value>(m_index + 1, Phi, Int32, m_value->origin());
                    hi = insert<Value>(m_index + 1, Phi, Int32, m_value->origin());
                } else if (m_value->type() == Int64 || (m_value->opcode() == Upsilon && m_value->child(0)->type() == Int64)) {
                    lo = insert<Value>(m_index + 1, Identity, m_value->origin(), m_zero);
                    hi = insert<Value>(m_index + 1, Identity, m_value->origin(), m_zero);
                } else
                    continue;
                m_syntheticValues.add(lo);
                m_syntheticValues.add(hi);
                dataLogLnIf(B3LowerInt64Internal::verbose, "Map ", *m_value, " -> (", deepDump(lo), ", ", deepDump(hi), ")");
                m_mapping.add(m_value, std::pair<Value*, Value*> { lo, hi });
            }
            m_changed |= !m_insertionSet.isEmpty();
            m_insertionSet.execute(m_block);
        }

        // Lower every Int64 node. This might insert new blocks, but those
        // blocks don't need any lowering.
        for (BasicBlock *block : blocksInPreOrder) {
            m_block = block;
            processCurrentBlock();
            m_changed |= !m_insertionSet.isEmpty();
            m_insertionSet.execute(m_block);
            m_changed |= m_blockInsertionSet.execute();
        }
        InsertionSet dropSynthetic(m_proc);
        for (BasicBlock *block : blocksInPreOrder) {
            for (size_t index = 0; index < block->size(); ++index) {
                Value* value = block->at(index);
                if ((value->opcode() == Identity) && value->child(0) == m_zero)
                    value->replaceWithBottom(dropSynthetic, index);
            }
            dropSynthetic.execute(block);
        }
        m_proc.deleteValue(m_zero);
        if (m_changed) {
            m_proc.resetReachability();
            m_proc.invalidateCFG();
        }
        return m_changed;
    }
private:
    void setMapping(Value* value, Value* lo, Value* hi)
    {
        std::pair<Value*, Value*> m = getMapping(value);
        ASSERT(m.first->opcode() == Identity);
        ASSERT(m.second->opcode() == Identity);
        ASSERT(m.first->child(0) == m_zero);
        ASSERT(m.second->child(0) == m_zero);
        m.first->child(0) = lo;
        m.second->child(0) = hi;
    }

    std::pair<Value*, Value*> getMapping(Value* value)
    {
        RELEASE_ASSERT(m_mapping.contains(value));
        return m_mapping.get(value);
    }

    void processCurrentBlock()
    {
        for (m_index = 0; m_index < m_block->size(); ++m_index) {
            m_value = m_block->at(m_index);
            if (m_value->opcode() == Identity && m_syntheticValues.contains(m_value))
                continue;
            dataLogLnIf(B3LowerInt64Internal::verbose, "LowerInt64: ", deepDump(m_value));
            processValue();
        }
    }

    Value* valueLo(Value* value, std::optional<size_t> providedIndex = { })
    {
        size_t index = m_index;
        if (providedIndex)
            index = *providedIndex;
        return insert<ExtractValue>(index, m_origin, Int32, value, ExtractValue::s_int64LowBits);
    }

    Value* valueHi(Value* value, std::optional<size_t> providedIndex = { })
    {
        size_t index = m_index;
        if (providedIndex)
            index = *providedIndex;
        return insert<ExtractValue>(index, m_origin, Int32, value, ExtractValue::s_int64HighBits);
    }

    Value* valueLoHi(Value* lo, Value* hi, std::optional<size_t> providedIndex = { })
    {
        size_t index = m_index;
        if (providedIndex)
            index = *providedIndex;
        return insert<Value>(index, Stitch, m_origin, lo, hi);
    }

    void valueReplaced()
    {
        m_value->replaceWithBottom(m_insertionSet, m_index);
    }

    template<typename ValueType, typename... Arguments>
    ValueType* insert(size_t index, Arguments... arguments)
    {
        return m_insertionSet.insert<ValueType>(index, arguments...);
    }

    template<typename ValueType, typename... Arguments>
    ValueType* appendToBlock(BasicBlock* block, Arguments... arguments)
    {
        return block->appendNew<ValueType>(m_proc, arguments...);
    }

    BasicBlock* splitForward()
    {
        return m_blockInsertionSet.splitForward(m_block, m_index, &m_insertionSet);
    }

    std::pair<UpsilonValue*, UpsilonValue*> shiftBelow32(BasicBlock* block, Opcode opcode, std::pair<Value*, Value*> input, Value* maskedShift)
    {
        Value* outputHi;
        Value* outputLo;
        Value* shiftComplement = appendToBlock<Value>(block, Sub, m_origin, appendToBlock<Const32Value>(block, m_origin, 32), maskedShift);
        Value* inputHi = input.second;
        Value* inputLo = input.first;

        if (opcode == SShr) {
            Value* shiftedLo = appendToBlock<Value>(block, ZShr, m_origin, inputLo, maskedShift);
            Value* shiftedIntoLoFromHi = appendToBlock<Value>(block, Shl, m_origin, inputHi, shiftComplement);
            outputLo = appendToBlock<Value>(block, BitOr, m_origin, shiftedLo, shiftedIntoLoFromHi);
            outputHi = appendToBlock<Value>(block, SShr, m_origin, inputHi, maskedShift);
        } else if (opcode == ZShr) {
            Value* shiftedLo = appendToBlock<Value>(block, ZShr, m_origin, inputLo, maskedShift);
            Value* shiftedIntoLoFromHi = appendToBlock<Value>(block, Shl, m_origin, inputHi, shiftComplement);
            outputLo = appendToBlock<Value>(block, BitOr, m_origin, shiftedLo, shiftedIntoLoFromHi);
            outputHi = appendToBlock<Value>(block, ZShr, m_origin, inputHi, maskedShift);
        } else {
            RELEASE_ASSERT(opcode == Shl);
            outputLo = appendToBlock<Value>(block, Shl, m_origin, inputLo, maskedShift);
            Value* shiftedHi = appendToBlock<Value>(block, Shl, m_origin, inputHi, maskedShift);
            Value* shiftedIntoHiFromLo = appendToBlock<Value>(block, ZShr, m_origin, inputLo, shiftComplement);
            outputHi = appendToBlock<Value>(block, BitOr, m_origin, shiftedHi, shiftedIntoHiFromLo);
        }
        auto ret = std::pair { appendToBlock<UpsilonValue>(block, m_origin, outputLo), appendToBlock<UpsilonValue>(block, m_origin, outputHi) };
        appendToBlock<Value>(block, Jump, m_origin);
        return ret;
    }

    std::pair<UpsilonValue*, UpsilonValue*> shiftAboveEquals32(BasicBlock* block, Opcode opcode, std::pair<Value*, Value*> input, Value* maskedShift)
    {
        Value* outputHi;
        Value* outputLo;
        Value* shift = appendToBlock<Value>(block, Sub, m_origin, maskedShift, appendToBlock<Const32Value>(block, m_origin, 32));
        if (opcode == SShr) {
            outputLo = appendToBlock<Value>(block, SShr, m_origin, input.second, shift);
            outputHi = appendToBlock<Value>(block, SShr, m_origin, input.second, appendToBlock<Const32Value>(block, m_origin, 31));
        } else if (opcode == ZShr) {
            outputLo = appendToBlock<Value>(block, ZShr, m_origin, input.second, shift);
            outputHi = appendToBlock<Const32Value>(block, m_origin, 0);
        } else {
            RELEASE_ASSERT(opcode == Shl);
            outputHi = appendToBlock<Value>(block, Shl, m_origin, input.first, shift);
            outputLo = appendToBlock<Const32Value>(block, m_origin, 0);
        }
        auto ret = std::pair { appendToBlock<UpsilonValue>(block, m_origin, outputLo), appendToBlock<UpsilonValue>(block, m_origin, outputHi) };
        appendToBlock<Value>(block, Jump, m_origin);
        return ret;
    }

    std::pair<UpsilonValue*, UpsilonValue*> rotateBelow32(BasicBlock* block, Opcode opcode, std::pair<Value*, Value*> input, Value* maskedRotation)
    {
        Value* outputHi;
        Value* outputLo;
        Value* rotationComplement = appendToBlock<Value>(block, Sub, m_origin, appendToBlock<Const32Value>(block, m_origin, 32), maskedRotation);
        Value* inputHi = input.second;
        Value* inputLo = input.first;

        if (opcode == RotR) {
            Value* rotatedLo = appendToBlock<Value>(block, ZShr, m_origin, inputLo, maskedRotation);
            Value* rotatedIntoLoFromHi = appendToBlock<Value>(block, Shl, m_origin, inputHi, rotationComplement);
            outputLo = appendToBlock<Value>(block, BitOr, m_origin, rotatedLo, rotatedIntoLoFromHi);
            Value* rotatedHi = appendToBlock<Value>(block, ZShr, m_origin, inputHi, maskedRotation);
            Value* rotatedIntoHiFromLo = appendToBlock<Value>(block, Shl, m_origin, inputLo, rotationComplement);
            outputHi = appendToBlock<Value>(block, BitOr, m_origin, rotatedHi, rotatedIntoHiFromLo);
        } else {
            ASSERT(opcode == RotL);
            Value* rotatedLo = appendToBlock<Value>(block, Shl, m_origin, inputLo, maskedRotation);
            Value* rotatedIntoLoFromHi = appendToBlock<Value>(block, ZShr, m_origin, inputHi, rotationComplement);
            outputLo = appendToBlock<Value>(block, BitOr, m_origin, rotatedLo, rotatedIntoLoFromHi);
            Value* rotatedHi = appendToBlock<Value>(block, Shl, m_origin, inputHi, maskedRotation);
            Value* rotatedIntoHiFromLo = appendToBlock<Value>(block, ZShr, m_origin, inputLo, rotationComplement);
            outputHi = appendToBlock<Value>(block, BitOr, m_origin, rotatedHi, rotatedIntoHiFromLo);
        }
        auto ret = std::pair { appendToBlock<UpsilonValue>(block, m_origin, outputLo), appendToBlock<UpsilonValue>(block, m_origin, outputHi) };
        appendToBlock<Value>(block, Jump, m_origin);
        return ret;
    }

    std::pair<UpsilonValue*, UpsilonValue*> rotateAboveEquals32(BasicBlock* block, Opcode opcode, std::pair<Value*, Value*> input, Value* maskedRotation)
    {
        Value* outputHi;
        Value* outputLo;
        Value* inputHi = input.second;
        Value* inputLo = input.first;
        Value* rotation = appendToBlock<Value>(block, Sub, m_origin, maskedRotation, appendToBlock<Const32Value>(block, m_origin, 32));
        Value* rotationComplement = appendToBlock<Value>(block, Sub, m_origin, appendToBlock<Const32Value>(block, m_origin, 32), rotation);
        if (opcode == RotR) {
            Value* rotatedIntoLoFromHi = appendToBlock<Value>(block, ZShr, m_origin, inputHi, rotation);
            Value* rotatedIntoLoFromLo = appendToBlock<Value>(block, Shl, m_origin, inputLo, rotationComplement);
            outputLo = appendToBlock<Value>(block, BitOr, m_origin, rotatedIntoLoFromHi, rotatedIntoLoFromLo);
            Value* rotatedIntoHiFromLo = appendToBlock<Value>(block, ZShr, m_origin, inputLo, rotation);
            Value* rotatedIntoHiFromHi = appendToBlock<Value>(block, Shl, m_origin, inputHi, rotationComplement);
            outputHi = appendToBlock<Value>(block, BitOr, m_origin, rotatedIntoHiFromLo, rotatedIntoHiFromHi);
        } else {
            ASSERT(opcode == RotL);
            Value* rotatedIntoLoFromHi = appendToBlock<Value>(block, Shl, m_origin, inputHi, rotation);
            Value* rotatedIntoLoFromLo = appendToBlock<Value>(block, ZShr, m_origin, inputLo, rotationComplement);
            outputLo = appendToBlock<Value>(block, BitOr, m_origin, rotatedIntoLoFromHi, rotatedIntoLoFromLo);
            Value* rotatedIntoHiFromLo = appendToBlock<Value>(block, Shl, m_origin, inputLo, rotation);
            Value* rotatedIntoHiFromHi = appendToBlock<Value>(block, ZShr, m_origin, inputHi, rotationComplement);
            outputHi = appendToBlock<Value>(block, BitOr, m_origin, rotatedIntoHiFromLo, rotatedIntoHiFromHi);
        }
        auto ret = std::pair { appendToBlock<UpsilonValue>(block, m_origin, outputLo), appendToBlock<UpsilonValue>(block, m_origin, outputHi) };
        appendToBlock<Value>(block, Jump, m_origin);
        return ret;
    }

    template <class Fn>
    Value* unaryCCall(Fn&& function, Type type, Value* arg)
    {
        Value* functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(function));
        return m_insertionSet.insert<CCallValue>(m_index, type, m_origin, Effects::none(), functionAddress, arg);
    }

    void splitRange(const HeapRange& original, HeapRange&lo, HeapRange&hi)
    {
        if (original.distance() == 1) {
            // XXX: testb3 uses single-byte range for Int64 access.
            hi = HeapRange(original.begin() + bytesForWidth(Width32));
            lo = original;
        } else if (!original || (original == HeapRange::top())) {
            hi = original;
            lo = original;
        } else {
            hi = HeapRange(original.begin() + bytesForWidth(Width32), original.end());
            lo = HeapRange(original.begin(), original.begin() + bytesForWidth(Width32));
            RELEASE_ASSERT(!hi.overlaps(lo));
        }
    }

    bool hasInt64Arg()
    {
        if (m_value->type() == Int64)
            return true;
        if (m_value->type().isTuple()) {
            for (auto type : m_proc.tupleForType(m_value->type())) {
                if (type == Int64)
                    return true;
            }
        }
        for (size_t index = 0; index < m_value->numChildren(); ++index) {
            if (m_value->child(index)->type() == Int64)
                return true;
            ASSERT(!m_value->child(index)->type().isTuple());
        }
        return false;
    }

    void processValue()
    {
        switch (m_value->opcode()) {
        case BitOr:
        case BitXor:
        case BitAnd: {
            if (m_value->type() != Int64)
                return;
            auto left = getMapping(m_value->child(0));
            auto right = getMapping(m_value->child(1));
            Value* hi = insert<Value>(m_index, m_value->opcode(), m_origin, left.second, right.second);
            Value* lo = insert<Value>(m_index, m_value->opcode(), m_origin, left.first, right.first);
            setMapping(m_value, lo, hi);
            valueReplaced();
            return;
        }
        case Const64: {
            Value* hi = insert<Const32Value>(m_index, m_origin, (m_value->asInt() >> 32) & 0xffffffff);
            Value* lo = insert<Const32Value>(m_index, m_origin, m_value->asInt() & 0xffffffff);
            setMapping(m_value, lo, hi);
            valueReplaced();
            return;
        }
        case Phi : {
            return;
        }
        case Upsilon: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto phi = getMapping(m_value->as<UpsilonValue>()->phi());
            auto input = getMapping(m_value->child(0));
            UpsilonValue* lo = insert<UpsilonValue>(m_index, m_origin, input.first);
            lo->setPhi(phi.first);
            UpsilonValue* hi = insert<UpsilonValue>(m_index, m_origin, input.second);
            hi->setPhi(phi.second);
            setMapping(m_value, lo, hi);
            valueReplaced();
            return;
        }
        case CCall: {
            if (!hasInt64Arg())
                return;
            Vector<Value*> args;
            size_t gprCount = 0;
            for (size_t index = 1; index < m_value->numChildren(); ++index) {
                Value* child = m_value->child(index);
                if (child->type() == Int32 || child->type() == Int64) {
                    if (gprCount < GPRInfo::numberOfArgumentRegisters
                        && Air::cCallArgumentEvenRegisterAlignment(child->type())
                        && (gprCount % 2)) {
                        args.append(insert<Const32Value>(m_index, m_origin, 0));
                        ++gprCount;
                    }
                    if (child->type() == Int32)
                        args.append(child);
                    else {
                        auto childParts = getMapping(child);

                        args.append(childParts.first);
                        args.append(childParts.second);
                    }
                    gprCount += Air::cCallArgumentRegisterCount(child->type());
                } else {
                    args.append(child);
                }
            }
            Type returnType = m_value->type();
            if (returnType == Int64)
                returnType = m_proc.addTuple({ Int32, Int32 });
            CCallValue* cCall = insert<CCallValue>(m_index, returnType, m_origin, m_value->child(0));
            RELEASE_ASSERT(cCall->numChildren() == 1);
            cCall->effects = m_value->effects();
            cCall->appendArgs(args);
            if (m_value->type() == Int64) {
                ASSERT(isARM_THUMB2());
                setMapping(m_value, valueLo(cCall, m_index + 1), valueHi(cCall, m_index + 1));
                valueReplaced();
            } else
                m_value->replaceWithIdentity(cCall);
            return;
        }
        case Check: {
            if (!hasInt64Arg())
                return;

            CheckValue* originalCheck = m_value->as<CheckValue>();
            // Putting together Int64 values with valueLoHi needs to preceed the
            // insertion of the PatchpointValue.
            Vector<Value*> args;
            for (size_t index = 0; index < m_value->numChildren(); ++index) {
                Value* child = m_value->child(index);
                if (child->type() == Int64) {
                    auto childParts = getMapping(child);
                    // The rep should have been correctly assigned when the
                    // Patchpoint was created, here we simply piece together the
                    // 64-bit value that it expects.
                    args.append(valueLoHi(childParts.first, childParts.second));
                } else
                    args.append(child);
            }
            CheckValue* check;
            if (m_value->kind() == Check) {
                RELEASE_ASSERT(m_value->numChildren() == 1);
                check = insert<CheckValue>(m_index, m_value->kind(), m_origin, m_value->child(0));
            } else {
                RELEASE_ASSERT(m_value->numChildren() == 2);
                check = insert<CheckValue>(m_index, m_value->kind(), m_origin, m_value->child(0), m_value->child(1));
            }
            check->clobberEarly(originalCheck->earlyClobbered());
            check->clobberLate(originalCheck->lateClobbered());
            // XXX: m_usedRegisters?
            check->setGenerator(originalCheck->generator());
            const Vector<ValueRep>& reps = originalCheck->reps();
            for (size_t index = 0; index < m_value->numChildren(); ++index)
                check->append(args[index], reps[index]);
            m_value->replaceWithIdentity(check);
            if (m_value->type() == Int64)
                setMapping(m_value, valueLo(m_value), valueHi(m_value));
            return;
        }
        case Patchpoint: {
            if (!hasInt64Arg())
                return;

            PatchpointValue* originalPatchpoint = m_value->as<PatchpointValue>();

            // (Int64, Int64, double) Patchpoint(Int32, Int64, double) -> (Int32, Int32, double, Int32, Int32) Patchpoint(Int32, Int32, double, Int32)
            // Int64 results are lowered to (Int32, Int32)
            Vector<Value*> args;
            Vector<Value*> highArgs;
            Vector<ValueRep> highReps;
            for (size_t index = 0; index < originalPatchpoint->numChildren(); ++index) {
                Value* child = originalPatchpoint->child(index);
                if (child->type() == Int64) {
                    auto childParts = getMapping(child);
                    auto rep = originalPatchpoint->reps()[index];
                    // If you already know you want a particular register, it should not be an Int64 value.
                    ASSERT(rep.isStack() || rep.isStackArgument()
                        || rep.kind() == ValueRep::SomeRegister
                        || rep.kind() == ValueRep::SomeLateRegister
                        || rep.isAny());
                    args.append(childParts.first);
                    highArgs.append(childParts.second);

                    if (rep.isStack())
                        rep = B3::ValueRep::stack(checkedSum<intptr_t>(rep.offsetFromFP(), static_cast<intptr_t>(bytesForWidth(Width32))));
                    else if (rep.isStackArgument())
                        rep = B3::ValueRep::stackArgument(checkedSum<intptr_t>(rep.offsetFromSP(), static_cast<intptr_t>(bytesForWidth(Width32))));

                    highReps.append(rep);
                } else
                    args.append(child);
            }

            const auto originalReturnType = originalPatchpoint->type();
            auto returnType = originalReturnType;

            if (originalReturnType.isTuple()) {
                Vector<Type> newTupleType;
                unsigned int64Count = 0;
                for (auto type : m_proc.tupleForType(originalReturnType)) {
                    if (type == Int64) {
                        newTupleType.append(Int32);
                        ++int64Count;
                        continue;
                    }
                    newTupleType.append(type);
                }
                for (unsigned i = 0; i < int64Count; ++i)
                    newTupleType.append(Int32);
                returnType = m_proc.addTuple(WTFMove(newTupleType));
            } else if (originalReturnType == Int64)
                returnType = m_proc.addTuple({ Int32, Int32 });

            PatchpointValue* patchpoint = insert<PatchpointValue>(m_index + 1, returnType, m_origin);

            patchpoint->clobberEarly(originalPatchpoint->earlyClobbered());
            patchpoint->clobberLate(originalPatchpoint->lateClobbered());
            patchpoint->effects = originalPatchpoint->effects;
            patchpoint->resultConstraints = originalPatchpoint->resultConstraints;
            patchpoint->numGPScratchRegisters = originalPatchpoint->numGPScratchRegisters;
            patchpoint->numFPScratchRegisters = originalPatchpoint->numFPScratchRegisters;
            patchpoint->setGenerator(originalPatchpoint->generator());

            const Vector<ValueRep>& reps = originalPatchpoint->reps();
            for (size_t index = 0; index < originalPatchpoint->numChildren(); ++index)
                patchpoint->append(args[index], reps[index]);
            for (size_t index = 0; index < highArgs.size(); ++index)
                patchpoint->append(highArgs[index], highReps[index]);

            if (originalReturnType.isTuple()) {
                auto originalTuple = m_proc.tupleForType(originalReturnType);
                m_rewrittenTupleResults.add(originalPatchpoint, patchpoint);
                valueReplaced();

                for (size_t index = 0; index < originalTuple.size(); ++index) {
                    if (originalTuple[index] == Int64)
                        patchpoint->resultConstraints.append(patchpoint->resultConstraints[index]);
                }

                return;
            }

            if (originalReturnType == Int64) {
                ASSERT(patchpoint->resultConstraints.size() == 1);
                m_rewrittenTupleResults.add(originalPatchpoint, patchpoint);
                auto rep = patchpoint->resultConstraints[0];
                ASSERT(rep.isStack() || rep.isStackArgument()
                    || rep.kind() == ValueRep::SomeRegister
                    || rep.kind() == ValueRep::SomeLateRegister
                    || rep.isAny());
                if (rep.isStack())
                    rep = B3::ValueRep::stack(checkedSum<intptr_t>(rep.offsetFromFP(), static_cast<intptr_t>(bytesForWidth(Width32))));
                else if (rep.isStackArgument())
                    rep = B3::ValueRep::stackArgument(checkedSum<intptr_t>(rep.offsetFromSP(), static_cast<intptr_t>(bytesForWidth(Width32))));
                patchpoint->resultConstraints.append(rep);
                setMapping(m_value, valueLo(patchpoint, m_index + 1), valueHi(patchpoint, m_index + 1));
                valueReplaced();
                return;
            }

            m_value->replaceWithIdentity(patchpoint);
            return;
        }
        case Add:
        case Sub:
        case Mul: {
            if (m_value->type() != Int64)
                return;
            auto left = getMapping(m_value->child(0));
            auto right = getMapping(m_value->child(1));
            Value* stitchedLeft = valueLoHi(left.first, left.second, m_index + 1);
            Value* stitchedRight = valueLoHi(right.first, right.second, m_index + 1);
            Value* stitched = insert<Value>(m_index + 1, m_value->opcode(), m_origin, stitchedLeft, stitchedRight);
            setMapping(m_value, valueLo(stitched, m_index + 1), valueHi(stitched, m_index + 1));
            valueReplaced();
            return;
        }

        case CheckAdd:
        case CheckSub:
        case CheckMul: {
            if (m_value->type() != Int64)
                return;
            RELEASE_ASSERT_NOT_REACHED(); // XXX: TBD
        }
        case Branch: {
            if (m_value->child(0)->type() != Int64)
                return;
            std::pair<Value*, Value*> input = getMapping(m_value->child(0));
            Value* testValue = insert<Value>(m_index, BitOr, m_origin, input.first, input.second);
            Value* branchValue = insert<Value>(m_index, Branch, m_origin, testValue);
            m_value->replaceWithIdentity(branchValue);
            return;
        }
        case Equal:
        case NotEqual: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto left = getMapping(m_value->child(0));
            auto right = getMapping(m_value->child(1));
            Value* highComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.second, right.second);
            Value* lowComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.first, right.first);
            Opcode combineOpcode = m_value->opcode() == Equal ? BitAnd : BitOr;
            Value* result = insert<Value>(m_index, combineOpcode, m_origin, highComparison, lowComparison);
            m_value->replaceWithIdentity(result);
            return;
        }

        // To verify the lowering of the comparisons, you can use Z3:
        // (declare-const left (_ BitVec 64))
        // (declare-const right (_ BitVec 64))
        // (define-fun low ((x (_ BitVec 64))) (_ BitVec 32)
        //   ((_ extract 31 0) x))
        // (define-fun high ((x (_ BitVec 64))) (_ BitVec 32)
        //   ((_ extract 63 32) x))

        // ;; Below
        // (push)
        // (assert
        //   (not
        //    (= (bvult left right)
        //       (or (bvult (high left) (high right))
        //           (and (= (high left) (high right))
        //                (bvult (low left) (low right)))))))
        // (check-sat)
        // (pop)

        // ;; Above
        // (push)
        // (assert
        //   (not
        //    (= (bvugt left right)
        //       (or (bvugt (high left) (high right))
        //        (and (= (high left) (high right))
        //             (bvugt (low left) (low right)))))))
        // (check-sat)
        // (pop)
        case Below:
        case Above: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto left = getMapping(m_value->child(0));
            auto right = getMapping(m_value->child(1));
            Value* highComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.second, right.second);
            Value* highEquality = insert<Value>(m_index, Equal, m_origin, left.second, right.second);
            Value* lowComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.first, right.first);
            Value* result = insert<Value>(m_index, BitOr, m_origin, highComparison, insert<Value>(m_index, BitAnd, m_origin, highEquality, lowComparison));
            m_value->replaceWithIdentity(result);
            return;
        }
        // ;; BelowEqual
        // (push)
        // (assert
        //   (not
        //    (= (bvule left right)
        //       (or (bvult (high left) (high right))
        //        (and (= (high left) (high right))
        //             (bvule (low left) (low right)))))))
        // (check-sat)
        // (pop)

        // ;; AboveEqual
        // (push)
        // (assert
        //   (not
        //    (= (bvuge left right)
        //       (or (bvugt (high left) (high right))
        //        (and (= (high left) (high right))
        //             (bvuge (low left) (low right)))))))
        // (check-sat)
        // (pop)
        case BelowEqual:
        case AboveEqual: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto left = getMapping(m_value->child(0));
            auto right = getMapping(m_value->child(1));
            Opcode highComparisonOpcode = m_value->opcode() == BelowEqual ? Below : Above;
            Value* highComparison = insert<Value>(m_index, highComparisonOpcode, m_origin, left.second, right.second);
            Value* highEquality = insert<Value>(m_index, Equal, m_origin, left.second, right.second);
            Value* lowComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.first, right.first);
            Value* result = insert<Value>(m_index, BitOr, m_origin, highComparison, insert<Value>(m_index, BitAnd, m_origin, highEquality, lowComparison));
            m_value->replaceWithIdentity(result);
            return;
        }

        // ;; JSC LessThan
        // (push)
        // (assert
        //   (not
        //    (= (bvslt left right)
        //       (or (bvslt (high left) (high right))
        //        (and (= (high left) (high right))
        //             (bvult (low left) (low right)))))))
        // (check-sat)
        // (pop)

        // ;; JSC GreaterThan: signed comparison
        // (push)
        // (assert
        //   (not
        //    (= (bvsgt left right)
        //       (or (bvsgt (high left) (high right))
        //        (and (= (high left) (high right))
        //             (bvugt (low left) (low right)))))))
        // (check-sat)
        // (pop)
        case LessThan:
        case GreaterThan: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto left = getMapping(m_value->child(0));
            auto right = getMapping(m_value->child(1));
            Value* highComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.second, right.second);
            Value* highEquality = insert<Value>(m_index, Equal, m_origin, left.second, right.second);
            Opcode lowComparisonOpcode = m_value->opcode() == LessThan ? Below : Above;
            Value* lowComparison = insert<Value>(m_index, lowComparisonOpcode, m_origin, left.first, right.first);
            Value* result = insert<Value>(m_index, BitOr, m_origin, highComparison, insert<Value>(m_index, BitAnd, m_origin, highEquality, lowComparison));
            m_value->replaceWithIdentity(result);
            return;
        }

        // ;; LessEqual
        // (push)
        // (assert
        //   (not
        //    (= (bvsle left right)
        //       (or (bvslt (high left) (high right))
        //        (and (= (high left) (high right))
        //             (bvule (low left) (low right)))))))
        // (check-sat)
        // (pop)

        // ;; GreaterEqual
        // (push)
        // (assert
        //   (not
        //    (= (bvsge left right)
        //       (or (bvsgt (high left) (high right))
        //        (and (= (high left) (high right))
        //             (bvuge (low left) (low right)))))))
        // (check-sat)
        // (pop)
        case LessEqual:
        case GreaterEqual: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto left = getMapping(m_value->child(0));
            auto right = getMapping(m_value->child(1));
            Opcode highComparisonOpcode = m_value->opcode() == LessEqual ? LessThan : GreaterThan;
            Value* highComparison = insert<Value>(m_index, highComparisonOpcode, m_origin, left.second, right.second);
            Value* highEquality = insert<Value>(m_index, Equal, m_origin, left.second, right.second);
            Opcode lowComparisonOpcode = m_value->opcode() == LessEqual ? BelowEqual : AboveEqual;
            Value* lowComparison = insert<Value>(m_index, lowComparisonOpcode, m_origin, left.first, right.first);
            Value* result = insert<Value>(m_index, BitOr, m_origin, highComparison, insert<Value>(m_index, BitAnd, m_origin, highEquality, lowComparison));
            m_value->replaceWithIdentity(result);
            return;
        }

        case Return: {
            if (!m_value->numChildren() || m_value->child(0)->type() != Int64)
                return;
            std::pair<Value*, Value*> retValue = getMapping(m_value->child(0));
            PatchpointValue* patchpoint = insert<PatchpointValue>(m_index + 1, Void, m_origin);
            patchpoint->effects = Effects::none();
            patchpoint->effects.terminal = true;
            patchpoint->append(retValue.second, ValueRep::reg(GPRInfo::returnValueGPR2));
            patchpoint->append(retValue.first, ValueRep::reg(GPRInfo::returnValueGPR));
            patchpoint->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                params.context().code->emitEpilogue(jit);
            });
            valueReplaced();
            return;
        }
        case SShr:
        case Shl:
        case ZShr: {
            if (m_value->type() != Int64)
                return;
            auto input = getMapping(m_value->child(0));
            Value* shiftAmount = m_value->child(1);
            BasicBlock* before = splitForward();
            BasicBlock* check = m_blockInsertionSet.insertBefore(m_block);
            BasicBlock* aboveOrEquals32 = m_blockInsertionSet.insertBefore(m_block);
            BasicBlock* below32 = m_blockInsertionSet.insertBefore(m_block);
            BasicBlock* shiftIsZero = m_blockInsertionSet.insertBefore(m_block);

            Value* maskedShift = appendToBlock<Value>(before, B3::BitAnd, m_origin, shiftAmount, before->replaceLastWithNew<Const32Value>(m_proc, m_origin, 63));
            appendToBlock<Value>(before, Branch, m_origin, maskedShift);
            before->setSuccessors(check, shiftIsZero);
            Value* constant32 = appendToBlock<Const32Value>(check, m_origin, 32);
            appendToBlock<Value>(check, Branch, m_origin, appendToBlock<Value>(check, Below, m_origin, maskedShift, constant32));
            check->setSuccessors(below32, aboveOrEquals32);

            std::pair<UpsilonValue*, UpsilonValue*> resultBelow32 = shiftBelow32(below32, m_value->opcode(), input, maskedShift);
            below32->setSuccessors(m_block);

            std::pair<UpsilonValue*, UpsilonValue*> resultAboveOrEquals32 = shiftAboveEquals32(aboveOrEquals32, m_value->opcode(), input, maskedShift);
            aboveOrEquals32->setSuccessors(m_block);

            std::pair<UpsilonValue*, UpsilonValue*> resultShiftIsZero = std::pair {
                appendToBlock<UpsilonValue>(shiftIsZero, m_origin, input.first),
                appendToBlock<UpsilonValue>(shiftIsZero, m_origin, input.second)
            };
            appendToBlock<Value>(shiftIsZero, Jump, m_origin);
            shiftIsZero->setSuccessors(m_block);

            std::pair<Value*, Value*> phis = std::pair {
                insert<Value>(m_index, Phi, Int32, m_origin),
                insert<Value>(m_index, Phi, Int32, m_origin)
            };
            resultBelow32.first->setPhi(phis.first);
            resultBelow32.second->setPhi(phis.second);

            resultAboveOrEquals32.first->setPhi(phis.first);
            resultAboveOrEquals32.second->setPhi(phis.second);

            resultShiftIsZero.first->setPhi(phis.first);
            resultShiftIsZero.second->setPhi(phis.second);

            setMapping(m_value, phis.first, phis.second);
            before->updatePredecessorsAfter();
            return;
        }
        case RotR:
        case RotL: {
            if (m_value->type() != Int64)
                return;
            auto input = getMapping(m_value->child(0));
            Value* rotationAmount = m_value->child(1);
            BasicBlock* before = splitForward();
            BasicBlock* check32 = m_blockInsertionSet.insertBefore(m_block);
            BasicBlock* swap = m_blockInsertionSet.insertBefore(m_block);
            BasicBlock* rotationIsZero = m_blockInsertionSet.insertBefore(m_block);
            BasicBlock* check = m_blockInsertionSet.insertBefore(m_block);
            BasicBlock* aboveOrEquals32 = m_blockInsertionSet.insertBefore(m_block);
            BasicBlock* below32 = m_blockInsertionSet.insertBefore(m_block);
            Value* maskedRotation = appendToBlock<Value>(before, B3::BitAnd, m_origin, rotationAmount, before->replaceLastWithNew<Const32Value>(m_proc, m_origin, 63));

            appendToBlock<Value>(before, Branch, m_origin, maskedRotation);
            before->setSuccessors(check32, rotationIsZero);

            std::pair<UpsilonValue*, UpsilonValue*> resultRotationIsZero = std::pair {
                appendToBlock<UpsilonValue>(rotationIsZero, m_origin, input.first),
                appendToBlock<UpsilonValue>(rotationIsZero, m_origin, input.second)
            };

            appendToBlock<Value>(rotationIsZero, Jump, m_origin);
            rotationIsZero->setSuccessors(m_block);

            Value* constant32 = appendToBlock<Const32Value>(check32, m_origin, 32);
            Value* isRotationBy32 = appendToBlock<Value>(check32, Equal, m_origin, rotationAmount, constant32);
            appendToBlock<Value>(check32, Branch, m_origin, isRotationBy32);
            check32->setSuccessors(swap, check);

            appendToBlock<Value>(swap, Stitch, m_origin, input.first, input.second);
            std::pair<UpsilonValue*, UpsilonValue*> resultRotationBy32 = std::pair {
                appendToBlock<UpsilonValue>(swap, m_origin, input.second),
                appendToBlock<UpsilonValue>(swap, m_origin, input.first)
            };
            appendToBlock<Value>(swap, Jump, m_origin);
            swap->setSuccessors(m_block);

            appendToBlock<Value>(check, Branch, m_origin, appendToBlock<Value>(check, Below, m_origin, maskedRotation, constant32));
            check->setSuccessors(below32, aboveOrEquals32);

            std::pair<UpsilonValue*, UpsilonValue*> resultBelow32 = rotateBelow32(below32, m_value->opcode(), input, maskedRotation);
            below32->setSuccessors(m_block);


            std::pair<UpsilonValue*, UpsilonValue*> resultAboveOrEquals32 = rotateAboveEquals32(aboveOrEquals32, m_value->opcode(), input, maskedRotation);
            aboveOrEquals32->setSuccessors(m_block);

            std::pair<Value*, Value*> phis = std::pair {
                insert<Value>(m_index, Phi, Int32, m_origin),
                insert<Value>(m_index, Phi, Int32, m_origin)
            };
            resultBelow32.first->setPhi(phis.first);
            resultBelow32.second->setPhi(phis.second);

            resultAboveOrEquals32.first->setPhi(phis.first);
            resultAboveOrEquals32.second->setPhi(phis.second);

            resultRotationIsZero.first->setPhi(phis.first);
            resultRotationIsZero.second->setPhi(phis.second);

            resultRotationBy32.first->setPhi(phis.first);
            resultRotationBy32.second->setPhi(phis.second);

            setMapping(m_value, phis.first, phis.second);
            before->updatePredecessorsAfter();
            return;

        }
        case Clz: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto input = getMapping(m_value->child(0));
            Value* thirtyTwo = insert<Const32Value>(m_index, m_origin, 32);
            Value* clzHi = insert<Value>(m_index, Clz, m_origin, input.second);
            Value* useHi = insert<Value>(m_index, Below, m_origin, clzHi, thirtyTwo);
            Value* clzLo = insert<Value>(m_index, Clz, m_origin, input.first);
            Value* clzIfLo = insert<Value>(m_index, Add, m_origin, clzLo, thirtyTwo);
            Value* result = insert<Value>(m_index, Select, m_origin, useHi, clzHi, clzIfLo);
            setMapping(m_value, result, insert<Const32Value>(m_index, m_origin, 0));
            valueReplaced();
            return;
        }
        case Extract: {
            auto originalTuple = m_value->child(0);
            auto index = m_value->as<ExtractValue>()->index();
            if (originalTuple->type() == Int64) {
                auto input = getMapping(originalTuple);
                m_value->replaceWithIdentity(index ? input.second : input.first);
                return;
            }
            auto originalTupleType = m_proc.tupleForType(originalTuple->type());
            if (!m_rewrittenTupleResults.contains(originalTuple))
                return;
            auto tuple = m_rewrittenTupleResults.get(originalTuple);
            if (originalTupleType[index] != Int64) {
                m_value->child(0) = tuple;
                return;
            }
            int highBitsIndex = 0;
            for (int i = 0; i < index; ++i) {
                ASSERT(i < static_cast<int>(originalTupleType.size()));
                if (originalTupleType[i] == Int64)
                    ++highBitsIndex;
            }
            ASSERT(originalTupleType.size() + highBitsIndex < m_proc.tupleForType(tuple->type()).size());
            Value* hi = insert<ExtractValue>(m_index, m_origin, Int32, tuple, m_proc.tupleForType(originalTuple->type()).size() + highBitsIndex);
            Value* lo = insert<ExtractValue>(m_index, m_origin, Int32, tuple, index);
            valueReplaced();
            setMapping(m_value, lo, hi);
            return;
        }
        case Abs:
        case BottomTuple:
        case Ceil:
        case Const32:
        case ConstDouble:
        case ConstFloat:
        case Div:
        case DoubleToFloat:
        case EntrySwitch:
        case EqualOrUnordered:
        case Fence:
        case FloatToDouble:
        case Floor:
        case FMax:
        case FMin:
        case FramePointer:
        case ArgumentReg:
        case Load8Z:
        case Load8S:
        case Load16Z:
        case Load16S:
        case Mod:
        case Oops:
        case SExt8:
        case SExt16:
        case SlotBase:
        case Sqrt:
        case Store8:
        case Store16:
        case UDiv:
        case UMod:
        case Jump:
        case Nop:
            return;
        case BitwiseCast: {
            if (m_value->type() == Int64) {
                setMapping(m_value, valueLo(m_value, m_index + 1), valueHi(m_value, m_index + 1));
            } else if (m_value->child(0)->type() == Int64) {
                auto input = getMapping(m_value->child(0));
                Value* cast = insert<Value>(m_index, BitwiseCast, m_origin, valueLoHi(input.first, input.second));
                m_value->replaceWithIdentity(cast);
            }

            return;
        }
        case Switch: {
            if (m_value->child(0)->type() != Int64)
                return;
            RELEASE_ASSERT_NOT_REACHED(); // XXX: TBD
        }
        case Trunc: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto input = getMapping(m_value->child(0));
            m_value->replaceWithIdentity(input.first);
            return;
        }
        case TruncHigh: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto input = getMapping(m_value->child(0));
            m_value->replaceWithIdentity(input.second);
            return;
        }
        case Stitch: {
            setMapping(m_value, m_value->child(0), m_value->child(1));
            valueReplaced();
            return;
        }
        case Identity: {
            if (m_value->type() != Int64)
                return;
            auto input = getMapping(m_value->child(0));
            setMapping(m_value, input.first, input.second);
            valueReplaced();
            return;
        }
        case Select: {
            Value* selector = m_value->child(0);
            if (selector->type() == Int64) {
                auto selectorPair = getMapping(selector);
                selector = insert<Value>(m_index, BitOr, m_origin, selectorPair.first, selectorPair.second);
            }
            if (m_value->child(1)->type() != Int64) {
                if (selector == m_value->child(0))
                    return;
                m_value->replaceWithIdentity(insert<Value>(m_index, Select, m_origin, selector, m_value->child(1), m_value->child(2)));
                return;
            }
            auto left = getMapping(m_value->child(1));
            auto right = getMapping(m_value->child(2));
            Value* selectHi = insert<Value>(m_index, Select, m_origin, selector, left.second, right.second);
            Value* selectLo = insert<Value>(m_index, Select, m_origin, selector, left.first, right.first);
            setMapping(m_value, selectLo, selectHi);
            valueReplaced();
            return;
        }
        case ZExt32: {
            Value* hi = insert<Const32Value>(m_index, m_origin, 0);
            setMapping(m_value, m_value->child(0), hi);
            valueReplaced();
            return;
        }
        case SExt32: {
            Value* signBit = insert<Value>(m_index, BitAnd, m_origin, m_value->child(0), insert<Const32Value>(m_index, m_origin, 0x80000000));
            Value* hi = insert<Value>(m_index, SShr, m_origin, signBit, insert<Const32Value>(m_index, m_origin, 31));
            setMapping(m_value, m_value->child(0), hi);
            valueReplaced();
            return;
        }
        case SExt8To64:
        case SExt16To64: {
            size_t inputBits = 8;
            if (m_value->opcode() == SExt16To64)
                inputBits = 16;
            uint32_t signBitMask = 1 << (inputBits - 1);
            uint32_t inputMask = (1 << inputBits) - 1;
            Value* signBit = insert<Value>(m_index, BitAnd, m_origin, m_value->child(0), insert<Const32Value>(m_index, m_origin, signBitMask));
            Value* ones = insert<Const32Value>(m_index, m_origin, 0xffffffff);
            Value* zero = insert<Const32Value>(m_index, m_origin, 0);
            Value* extension = insert<Value>(m_index, Select, m_origin, signBit, ones, zero);
            Value* shiftedExtension = insert<Value>(m_index, Shl, m_origin, extension, insert<Const32Value>(m_index, m_origin, inputBits));
            Value* maskedInput = insert<Value>(m_index, BitAnd, m_origin, m_value->child(0), insert<Const32Value>(m_index, m_origin, inputMask));
            Value* lo = insert<Value>(m_index, BitOr, m_origin, maskedInput, shiftedExtension);
            Value* hi = extension;
            setMapping(m_value, lo, hi);
            valueReplaced();
            return;
        }
        case Load: {
            // Lower Int64 loads to Int32 loads.
            //
            // If loads of Doubles/Floats would fault on unaligned access,
            // lower those to Int32 loads as well.
            MemoryValue* memory = m_value->as<MemoryValue>();
            if (!hasUnalignedFPMemoryAccess() && m_value->type() == Float) {
                MemoryValue* asInt32 = insert<MemoryValue>(m_index, Load, Int32, m_origin, memory->child(0));
                asInt32->setOffset(memory->offset());
                asInt32->setRange(memory->range());
                asInt32->setFenceRange(memory->fenceRange());
                Value* result = insert<Value>(m_index, BitwiseCast, m_origin, asInt32);
                m_value->replaceWithIdentity(result);
                return;
            }
            if (!(m_value->type() == Int64 || (!hasUnalignedFPMemoryAccess() && m_value->type() == Double)))
                return;
            HeapRange rangeHi, rangeLo;
            splitRange(memory->range(), rangeLo, rangeHi);

            HeapRange fenceRangeHi, fenceRangeLo;
            splitRange(memory->fenceRange(), fenceRangeLo, fenceRangeHi);

            // Assumes little-endian arch.
            CheckedInt32 offsetHi = CheckedInt32(memory->offset()) + CheckedInt32(bytesForWidth(Width32));
            RELEASE_ASSERT(!offsetHi.hasOverflowed());

            MemoryValue* hi = insert<MemoryValue>(m_index, Load, Int32, m_origin, memory->child(0));
            hi->setOffset(offsetHi);
            hi->setRange(rangeHi);
            hi->setFenceRange(fenceRangeHi);

            MemoryValue* lo = insert<MemoryValue>(m_index, Load, Int32, m_origin, memory->child(0));
            lo->setOffset(memory->offset());
            lo->setRange(rangeLo);
            lo->setFenceRange(fenceRangeLo);
            if (m_value->type() == Double) {
                Value* result = insert<Value>(m_index, BitwiseCast, m_origin, valueLoHi(lo, hi));
                m_value->replaceWithIdentity(result);
            } else {
                setMapping(m_value, lo, hi);
                valueReplaced();
            }
            return;
        }
        case Store: {
            // Lower Int64 stores to Int32 stores.
            //
            // If stores of Doubles/Floats would fault on unaligned access,
            // lower those to Int32 stores as well.
            MemoryValue* memory = m_value->as<MemoryValue>();
            std::pair<Value*, Value*> value;
            if (!hasUnalignedFPMemoryAccess() && m_value->child(0)->type() == Double) {
                Value* asInt64 = insert<Value>(m_index, BitwiseCast, m_origin, m_value->child(0));
                value = { valueLo(asInt64), valueHi(asInt64) };
            } else if (!hasUnalignedFPMemoryAccess() && m_value->child(0)->type() == Float) {
                Value* asInt32 = insert<Value>(m_index, BitwiseCast, m_origin, m_value->child(0));
                MemoryValue* store = insert<MemoryValue>(m_index, Store, m_origin, asInt32, memory->child(1));
                store->setOffset(memory->offset());
                store->setRange(memory->range());
                store->setFenceRange(memory->fenceRange());
                valueReplaced();
                return;
            } else if (m_value->child(0)->type() == Int64)
                value = getMapping(m_value->child(0));
            else
                return;

            HeapRange rangeHi, rangeLo;
            splitRange(memory->range(), rangeLo, rangeHi);

            HeapRange fenceRangeHi, fenceRangeLo;
            splitRange(memory->fenceRange(), fenceRangeLo, fenceRangeHi);


            MemoryValue* hi = insert<MemoryValue>(m_index, Store, m_origin, value.second, memory->child(1));
            CheckedInt32 offsetHi = CheckedInt32(memory->offset()) + CheckedInt32(bytesForWidth(Width32));
            RELEASE_ASSERT(!offsetHi.hasOverflowed());
            hi->setOffset(offsetHi);
            hi->setRange(rangeHi);
            hi->setFenceRange(fenceRangeHi);

            MemoryValue* lo = insert<MemoryValue>(m_index, Store, m_origin, value.first, memory->child(1));
            lo->setOffset(memory->offset());
            lo->setRange(rangeLo);
            lo->setFenceRange(fenceRangeLo);
            valueReplaced();
            return;
        }
        case IToD: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto input = getMapping(m_value->child(0));
            Value* arg = valueLoHi(input.first, input.second);
            m_value->replaceWithIdentity(unaryCCall(Math::f64_convert_s_i64, m_value->type(), arg));
            return;
        }
        case IToF: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto input = getMapping(m_value->child(0));
            Value* arg = valueLoHi(input.first, input.second);
            m_value->replaceWithIdentity(unaryCCall(Math::f32_convert_s_i64, m_value->type(), arg));
            return;
        }
        case Neg: {
            if (m_value->type() != Int64)
                return;
            auto input = getMapping(m_value->child(0));
            Value* zero32 = insert<Const32Value>(m_index, m_origin, 0);
            Value* zero = valueLoHi(zero32, zero32);
            m_value->replaceWithIdentity(insert<Value>(m_index, Sub, m_origin, zero, valueLoHi(input.first, input.second)));
            setMapping(m_value, valueLo(m_value, m_index + 1), valueHi(m_value, m_index + 1));
            return;
        }
        default: {
            dataLogLn("Cannot lower ", deepDump(m_value));
            RELEASE_ASSERT_NOT_REACHED();
        }
        }
    }
    Procedure& m_proc;
    Value* m_zero;
    BasicBlock* m_block;
    Value* m_value;
    unsigned m_index;
    Origin m_origin;
    InsertionSet m_insertionSet;
    BlockInsertionSet m_blockInsertionSet;
    bool m_changed;
    UncheckedKeyHashMap<Value*, Value*> m_rewrittenTupleResults;
    UncheckedKeyHashMap<Value*, std::pair<Value*, Value*>> m_mapping;
    HashSet<Value*> m_syntheticValues;
};

} // anonymous namespace

bool lowerInt64(Procedure& proc)
{
    PhaseScope phaseScope(proc, "B3::lowerInt64"_s);
    LowerInt64 lowerInt64(proc);
    return lowerInt64.run();
}

} // B3
} // JSC
#endif // USE(JSVALUE32_64) && ENABLE(B3_JIT)
