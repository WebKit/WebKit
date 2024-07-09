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
#include "B3BasicBlock.h"
#include "B3BlockInsertionSet.h"
#include "B3InsertionSet.h"
#include "B3InsertionSetInlines.h"
#include "B3PhaseScope.h"
#include "B3Procedure.h"
#include "B3StackmapGenerationParams.h"
#include "B3Value.h"

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
                Value* hi;
                Value* lo;
                dataLogLnIf(B3LowerInt64Internal::verbose, "B3LowerInt64: prep for ", deepDump(m_value));
                if ((m_value->type() == Int64) && (m_value->opcode() == Phi)) {
                    // Upsilon expects the target of a ->setPhi() to be a Phi,
                    // not an identity, so do the (trivial) lowering here.
                    hi = insert<Value>(m_index + 1, Phi, Int32, m_value->origin());
                    lo = insert<Value>(m_index + 1, Phi, Int32, m_value->origin());
                } else if (m_value->type() == Int64 || (m_value->opcode() == Upsilon && m_value->child(0)->type() == Int64)) {
                    hi = insert<Value>(m_index + 1, Identity, m_value->origin(), m_zero);
                    lo = insert<Value>(m_index + 1, Identity, m_value->origin(), m_zero);
                } else
                    continue;
                m_syntheticValues.add(hi);
                m_syntheticValues.add(lo);
                dataLogLnIf(B3LowerInt64Internal::verbose, "Map ", *m_value, " -> (", deepDump(hi), ", ", deepDump(lo), ")");
                m_mapping.add(m_value, std::pair<Value*, Value*> { hi, lo });
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

        m_proc.deleteValue(m_zero);
        if (m_changed) {
            m_proc.resetReachability();
            m_proc.invalidateCFG();
        }
        return m_changed;
    }
private:
    void setMapping(Value* value, Value* hi, Value* lo)
    {
        std::pair<Value*, Value*> m = getMapping(value);
        ASSERT(m.first->opcode() == Identity);
        ASSERT(m.second->opcode() == Identity);
        ASSERT(m.first->child(0) == m_zero);
        ASSERT(m.second->child(0) == m_zero);
        m.first->child(0) = hi;
        m.second->child(0) = lo;
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

    Value* valueLo(Value* value)
    {
        return insert<Value>(m_index, Trunc, m_origin, value);
    }

    Value* valueHi(Value* value)
    {
        return insert<Value>(m_index, TruncHigh, m_origin, value);
    }

    Value* valueHiLo(Value* hi, Value* lo)
    {
        return insert<Value>(m_index, Stitch, m_origin, hi, lo);
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
        Value* inputHi = input.first;
        Value* inputLo = input.second;

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
        auto ret = std::pair { appendToBlock<UpsilonValue>(block, m_origin, outputHi), appendToBlock<UpsilonValue>(block, m_origin, outputLo) };
        appendToBlock<Value>(block, Jump, m_origin);
        return ret;
    }

    std::pair<UpsilonValue*, UpsilonValue*> shiftAboveEquals32(BasicBlock* block, Opcode opcode, std::pair<Value*, Value*> input, Value* maskedShift)
    {
        Value* outputHi;
        Value* outputLo;
        Value* shift = appendToBlock<Value>(block, Sub, m_origin, maskedShift, appendToBlock<Const32Value>(block, m_origin, 32));
        if (opcode == SShr) {
            outputLo = appendToBlock<Value>(block, SShr, m_origin, input.first, shift);
            outputHi = appendToBlock<Value>(block, SShr, m_origin, input.first, appendToBlock<Const32Value>(block, m_origin, 31));
        } else if (opcode == ZShr) {
            outputLo = appendToBlock<Value>(block, ZShr, m_origin, input.first, shift);
            outputHi = appendToBlock<Const32Value>(block, m_origin, 0);
        } else {
            RELEASE_ASSERT(opcode == Shl);
            outputHi = appendToBlock<Value>(block, Shl, m_origin, input.second, shift);
            outputLo = appendToBlock<Const32Value>(block, m_origin, 0);
        }
        auto ret = std::pair { appendToBlock<UpsilonValue>(block, m_origin, outputHi), appendToBlock<UpsilonValue>(block, m_origin, outputLo) };
        appendToBlock<Value>(block, Jump, m_origin);
        return ret;
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
            Value* hi = insert<Value>(m_index, m_value->opcode(), m_origin, left.first, right.first);
            Value* lo = insert<Value>(m_index, m_value->opcode(), m_origin, left.second, right.second);
            setMapping(m_value, hi, lo);
            valueReplaced();
            return;
        }
        case Const64: {
            Value* hi = insert<Const32Value>(m_index, m_origin, (m_value->asInt() >> 32) & 0xffffffff);
            Value* lo = insert<Const32Value>(m_index, m_origin, m_value->asInt() & 0xffffffff);
            setMapping(m_value, hi, lo);
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
            UpsilonValue* hi = insert<UpsilonValue>(m_index, m_origin, input.first);
            hi->setPhi(phi.first);
            UpsilonValue* lo = insert<UpsilonValue>(m_index, m_origin, input.second);
            lo->setPhi(phi.second);
            setMapping(m_value, hi, lo);
            valueReplaced();
            return;
        }
        case CCall: {
            bool hasInt64Arg = false;
            for (size_t index = 0; index < m_value->numChildren(); ++index) {
                if (m_value->child(index)->type() != Int64)
                    continue;
                hasInt64Arg = true;
            }
            if (!hasInt64Arg && m_value->type() != Int64)
                return;
            CCallValue* cCall = insert<CCallValue>(m_index, m_value->type(), m_origin, m_value->child(0));
            cCall->effects = m_value->effects();
            Vector<Value*> args;
            RELEASE_ASSERT(cCall->numChildren() == 1);
            for (size_t index = 1; index < m_value->numChildren(); ++index) {
                Value* child = m_value->child(index);
                if (child->type() != Int64)
                    args.append(child);
                else {
                    auto childParts = getMapping(child);
                    args.append(valueHiLo(childParts.first, childParts.second));
                }
            }
            cCall->appendArgs(args);
            m_value->replaceWithIdentity(cCall);
            if (m_value->type() == Int64)
                setMapping(m_value, valueHi(m_value), valueLo(m_value));
            return;
        }
        case Check: {
            bool hasInt64Arg = false;
            for (size_t index = 0; index < m_value->numChildren(); ++index) {
                if (m_value->child(index)->type() != Int64)
                    continue;
                hasInt64Arg = true;
            }
            if (!hasInt64Arg && m_value->type() != Int64)
                return;

            CheckValue* originalCheck = m_value->as<CheckValue>();
            // Putting together Int64 values with valueHiLo needs to preceed the
            // insertion of the PatchpointValue.
            Vector<Value*> args;
            for (size_t index = 0; index < m_value->numChildren(); ++index) {
                Value* child = m_value->child(index);
                if (child->type() == Int64) {
                    auto childParts = getMapping(child);
                    // The rep should have been correctly assigned when the
                    // Patchpoint was created, here we simply piece together the
                    // 64-bit value that it expects.
                    args.append(valueHiLo(childParts.first, childParts.second));
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
                setMapping(m_value, valueHi(m_value), valueLo(m_value));
            return;
        }
        case Patchpoint: {
            bool hasInt64Arg = false;
            for (size_t index = 0; index < m_value->numChildren(); ++index) {
                if (m_value->child(index)->type() != Int64)
                    continue;
                hasInt64Arg = true;
            }
            if (!hasInt64Arg && m_value->type() != Int64)
                return;
            PatchpointValue* originalPatchpoint = m_value->as<PatchpointValue>();

            // Putting together Int64 values with valueHiLo needs to preceed the
            // insertion of the PatchpointValue.
            Vector<Value*> args;
            for (size_t index = 0; index < originalPatchpoint->numChildren(); ++index) {
                Value* child = originalPatchpoint->child(index);
                if (child->type() == Int64) {
                    auto childParts = getMapping(child);
                    // The rep should have been correctly assigned when the
                    // Patchpoint was created, here we simply piece together the
                    // 64-bit value that it expects.
                    args.append(valueHiLo(childParts.first, childParts.second));
                } else
                    args.append(child);
            }
            PatchpointValue* patchpoint = insert<PatchpointValue>(m_index, originalPatchpoint->type(), m_origin);
            patchpoint->clobberEarly(originalPatchpoint->earlyClobbered());
            patchpoint->clobberLate(originalPatchpoint->lateClobbered());
            // XXX: m_usedRegisters?
            patchpoint->effects = originalPatchpoint->effects;
            patchpoint->resultConstraints = originalPatchpoint->resultConstraints;
            patchpoint->numGPScratchRegisters = originalPatchpoint->numGPScratchRegisters;
            patchpoint->numFPScratchRegisters = originalPatchpoint->numFPScratchRegisters;
            patchpoint->setGenerator(originalPatchpoint->generator());
            const Vector<ValueRep>& reps = originalPatchpoint->reps();
            for (size_t index = 0; index < originalPatchpoint->numChildren(); ++index)
                patchpoint->append(args[index], reps[index]);
            m_value->replaceWithIdentity(patchpoint);
            if (m_value->type() == Int64)
                setMapping(m_value, valueHi(m_value), valueLo(m_value));
            return;
        }
        case Add:
        case Sub: {
            if (m_value->type() != Int64)
                return;
            auto left = getMapping(m_value->child(0));
            auto right = getMapping(m_value->child(1));
            Value* stitchedLeft = valueHiLo(left.first, left.second);
            Value* stitchedRight = valueHiLo(right.first, right.second);
            Value* stitched = insert<Value>(m_index, m_value->opcode(), m_origin, stitchedLeft, stitchedRight);
            m_value->replaceWithIdentity(stitched);
            setMapping(m_value, valueHi(stitched), valueLo(stitched));
            return;
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
            Value* highComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.first, right.first);
            Value* lowComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.second, right.second);
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
            Value* highComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.first, right.first);
            Value* highEquality = insert<Value>(m_index, Equal, m_origin, left.first, right.first);
            Value* lowComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.second, right.second);
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
            Value* highComparison = insert<Value>(m_index, highComparisonOpcode, m_origin, left.first, right.first);
            Value* highEquality = insert<Value>(m_index, Equal, m_origin, left.first, right.first);
            Value* lowComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.second, right.second);
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
            Value* highComparison = insert<Value>(m_index, m_value->opcode(), m_origin, left.first, right.first);
            Value* highEquality = insert<Value>(m_index, Equal, m_origin, left.first, right.first);
            Opcode lowComparisonOpcode = m_value->opcode() == LessThan ? Below : Above;
            Value* lowComparison = insert<Value>(m_index, lowComparisonOpcode, m_origin, left.second, right.second);
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
            Value* highComparison = insert<Value>(m_index, highComparisonOpcode, m_origin, left.first, right.first);
            Value* highEquality = insert<Value>(m_index, Equal, m_origin, left.first, right.first);
            Opcode lowComparisonOpcode = m_value->opcode() == LessEqual ? BelowEqual : AboveEqual;
            Value* lowComparison = insert<Value>(m_index, lowComparisonOpcode, m_origin, left.second, right.second);
            Value* result = insert<Value>(m_index, BitOr, m_origin, highComparison, insert<Value>(m_index, BitAnd, m_origin, highEquality, lowComparison));
            m_value->replaceWithIdentity(result);
            return;
        }

        case Return: {
            if (!m_value->numChildren() || m_value->child(0)->type() != Int64)
                return;
            std::pair<Value*, Value*> retValue = getMapping(m_value->child(0));
            PatchpointValue* patchpoint = insert<PatchpointValue>(m_index, Void, m_origin);
            patchpoint->effects = Effects::none();
            patchpoint->effects.terminal = true;
            patchpoint->append(retValue.first, ValueRep::reg(GPRInfo::returnValueGPR2));
            patchpoint->append(retValue.second, ValueRep::reg(GPRInfo::returnValueGPR));
            patchpoint->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                params.context().code->emitEpilogue(jit);
            });
            m_value->replaceWithIdentity(patchpoint);
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
        case Const32:
        case FramePointer:
        case ArgumentReg:
        case Jump:
            return;
        case Trunc: {
            auto input = getMapping(m_value->child(0));
            m_value->replaceWithIdentity(input.second);
            return;
        }
        case ZExt32: {
            Value* hi = insert<Const32Value>(m_index, m_origin, 0);
            setMapping(m_value, hi, m_value->child(0));
            valueReplaced();
            return;
        }
        case Load: {
            if (m_value->type() != Int64)
                return;
            MemoryValue* memory = m_value->as<MemoryValue>();
            if (memory->range() != HeapRange::top())
                RELEASE_ASSERT_NOT_REACHED(); // XXX: TBD
            if (memory->fenceRange() != HeapRange())
                RELEASE_ASSERT_NOT_REACHED(); // XXX: TBD
            MemoryValue* hi = insert<MemoryValue>(m_index, Load, Int32, m_origin, memory->child(0));
            // XXX: overflow!
            hi->setOffset(memory->offset() + bytesForWidth(Width32));
            // Assumes little-endian arch.
            MemoryValue* lo = insert<MemoryValue>(m_index, Load, Int32, m_origin, memory->child(0));
            lo->setOffset(memory->offset());
            setMapping(m_value, hi, lo);
            valueReplaced();
            return;
        }
        case Store: {
            if (m_value->child(0)->type() != Int64)
                return;
            RELEASE_ASSERT_NOT_REACHED(); // XXX: TBD
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
    HashMap<Value*, std::pair<Value*, Value*>> m_mapping;
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
