/*
 * Copyright (C) Igalia S.L.
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

#include "B3LowerInt64.h"

#if USE(JSVALUE32_64) && ENABLE(B3_JIT)

#include "B3BasicBlock.h"
#include "B3BlockInsertionSet.h"
#include "B3InsertionSet.h"
#include "B3InsertionSetInlines.h"
#include "B3Procedure.h"
#include "B3PatchpointValue.h"
#include "B3StackmapGenerationParams.h"
#include "B3Type.h"
#include "B3UpsilonValue.h"
#include "B3Value.h"

#include <wtf/DataLog.h>

namespace JSC {
namespace B3 {

namespace B3LowerInt64Internal {
static constexpr bool verbose = true;
}

namespace {

class LowerInt64 {
public:
    LowerInt64(Procedure& proc)
        : m_proc(proc)
        , m_insertionSet(proc)
        , m_blockInsertionSet(proc)
    {
    }

    bool run()
    {
        dataLogLnIf(B3LowerInt64Internal::verbose, "Graph before LowerInt64:", m_proc);

        const Vector<BasicBlock*> blocksInPreOrder = m_proc.blocksInPreOrder();
        m_changed = false;

        m_zero = m_proc.addIntConstant(Origin(), Int32, 0);

        // First, produce empty Int32 mappings for every Int64 node.
        // This way, we never need to re-write indicies when we change control flow below.
        for (BasicBlock *block : blocksInPreOrder) {
            m_block = block;
            for (m_index = 0; m_index < m_block->size(); ++m_index) {
                m_value = m_block->at(m_index);
                if (m_value->type() != Int64)
                    continue;
                auto* hi = insert<Value>(m_index + 1, Identity, m_value->origin(), m_zero);
                auto* lo = insert<Value>(m_index + 1, Identity, m_value->origin(), m_zero);
                m_syntheticValues.add(hi);
                m_syntheticValues.add(lo);
                m_mapping.add(m_value, std::pair<Value*, Value*> { hi, lo });
            }
            m_changed |= !m_insertionSet.isEmpty();
            m_insertionSet.execute(m_block);
        }

        dataLogLnIf(B3LowerInt64Internal::verbose, "Graph after Int64 identities are added:", m_proc);

        // Lower every Int64 node. This might insert new blocks,
        // but new blocks shouldn't need to be visited. We will invalidate
        // the BB index when we insert new values.
        for (BasicBlock *block : blocksInPreOrder) {
            m_block = block;
            processCurrentBlock();
            m_changed |= !m_insertionSet.isEmpty();
            m_insertionSet.execute(m_block);
            m_changed |= m_blockInsertionSet.execute();
        }

        // XXX Validate no more Int64, m_zero.
        m_proc.deleteValue(m_zero);

        if (m_changed) {
            m_proc.resetReachability();
            m_proc.invalidateCFG();
        }

        dataLogLnIf(B3LowerInt64Internal::verbose && m_changed, "Graph after Int64 lowering:", m_proc);

        return m_changed;
    }

private:
    Value* getLo(Value* v) {
        ASSERT(m_mapping.contains(v));
        return std::get<0>(m_mapping.get(v));
    }

    void setLo(Value* lo) {
        Value* ident = getLo(m_value);
        ASSERT(ident->opcode() == Identity);
        ident->child(0) = lo;
    }

    Value* getHi(Value* v) {
        ASSERT(m_mapping.contains(v));
        return std::get<1>(m_mapping.get(v));
    }

    void setHi(Value* hi) {
        Value* ident = getHi(m_value);
        ASSERT(ident->opcode() == Identity);
        ident->child(0) = hi;
    }

    void processCurrentBlock()
    {
        for (m_index = 0; m_index < m_block->size(); ++m_index) {
            m_value = m_block->at(m_index);
            if (m_value->opcode() == Identity && m_syntheticValues.contains(m_value))
                continue;
            processValue();
        }
    }

    Value* truncLow(Value* v)
    {
        return insert<Value>(m_index, Trunc, m_value->origin(), v);
    }

    Value* truncHigh(Value* v)
    {
        // XXX: drop completely, lower to tuple
        auto* patchpoint = insert<PatchpointValue>(m_index, Int32, m_value->origin());
        patchpoint->effects = Effects::none();
        patchpoint->append(v, ValueRep::SomeRegister);
        patchpoint->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.move(params[1].gpr(), params[0].gpr());
        });
        return patchpoint;
    }

    Value* valueHiLo(Value* hi, Value* lo)
    {
        PatchpointValue* patchpoint; // XXX: drop in favor of tuple
        patchpoint = insert<PatchpointValue>(m_index, Int64, m_value->origin());
        patchpoint->effects = Effects::none();
        patchpoint->append(hi, ValueRep::SomeRegister);
        patchpoint->append(lo, ValueRep::SomeRegister);
        Vector<B3::ValueRep, 1> resultConstraints;
        resultConstraints.append(ValueRep::SomeRegister);
        patchpoint->resultConstraints = WTFMove(resultConstraints);
        patchpoint->setGenerator([] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.move(params[1].gpr(), params[0].gpr());
        });
        return patchpoint;
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
        m_changed = true;
        return m_blockInsertionSet.splitForward(m_block, m_index, &m_insertionSet);
    }

    void valueReplaced()
    {
        m_value->replaceWithBottom(m_insertionSet, m_index);
    }

    std::pair<UpsilonValue*, UpsilonValue*> shiftBelow32(BasicBlock* block, Opcode opcode, Value* input, Value* maskedShift)
    {
        Value* outputHi;
        Value* outputLo;
        Value *shiftComplement = appendToBlock<Value>(block, Sub, m_value->origin(), appendToBlock<Const32Value>(block, m_value->origin(), 32), maskedShift);
        Value *inputHi = getHi(input);
        Value *inputLo = getLo(input);

        if (opcode == SShr) {
            Value* shiftedLo = appendToBlock<Value>(block, ZShr, m_value->origin(), inputLo, maskedShift);
            Value* shiftedIntoLoFromHi = appendToBlock<Value>(block, Shl, m_value->origin(), inputHi, shiftComplement);
            outputLo = appendToBlock<Value>(block, BitOr, m_value->origin(), shiftedLo, shiftedIntoLoFromHi);
            outputHi = appendToBlock<Value>(block, SShr, m_value->origin(), inputHi, maskedShift);
        } else if (opcode == ZShr) {
            Value* shiftedLo = appendToBlock<Value>(block, ZShr, m_value->origin(), inputLo, maskedShift);
            Value* shiftedIntoLoFromHi = appendToBlock<Value>(block, Shl, m_value->origin(), inputHi, shiftComplement);
            outputLo = appendToBlock<Value>(block, BitOr, m_value->origin(), shiftedLo, shiftedIntoLoFromHi);
            outputHi = appendToBlock<Value>(block, ZShr, m_value->origin(), inputHi, maskedShift);
        } else {
            RELEASE_ASSERT(opcode == Shl);
            outputLo = appendToBlock<Value>(block, Shl, m_value->origin(), inputLo, maskedShift);
            Value* shiftedHi = appendToBlock<Value>(block, Shl, m_value->origin(), inputHi, maskedShift);
            Value* shiftedIntoHiFromLo = appendToBlock<Value>(block, ZShr, m_value->origin(), inputLo, shiftComplement);
            outputHi = appendToBlock<Value>(block, BitOr, m_value->origin(), shiftedHi, shiftedIntoHiFromLo);
        }
        auto ret = std::pair {appendToBlock<UpsilonValue>(block, m_value->origin(), outputHi), appendToBlock<UpsilonValue>(block, m_value->origin(), outputLo)};
        appendToBlock<Value>(block, Jump, m_value->origin());
        return ret;
    }

    std::pair<UpsilonValue*, UpsilonValue*> shiftAboveEquals32(BasicBlock* block, Opcode opcode, Value* input, Value* maskedShift)
    {
        Value* outputHi;
        Value* outputLo;
        Value* shift = appendToBlock<Value>(block, Sub, m_value->origin(), maskedShift, appendToBlock<Const32Value>(block, m_value->origin(), 32));
        if (opcode == SShr) {
            outputLo = appendToBlock<Value>(block, SShr, m_value->origin(), getHi(input), shift);
            outputHi = appendToBlock<Value>(block, SShr, m_value->origin(), getHi(input), appendToBlock<Const32Value>(block, m_value->origin(), 31));
        } else if (opcode == ZShr) {
            outputLo = appendToBlock<Value>(block, ZShr, m_value->origin(), getHi(input), shift);
            outputHi = appendToBlock<Const32Value>(block, m_value->origin(), 0);
        } else {
            RELEASE_ASSERT(opcode == Shl);
            outputHi = appendToBlock<Value>(block, Shl, m_value->origin(), getLo(input), shift);
            outputLo = appendToBlock<Const32Value>(block, m_value->origin(), 0);
        }
        auto ret = std::pair {appendToBlock<UpsilonValue>(block, m_value->origin(), outputHi), appendToBlock<UpsilonValue>(block, m_value->origin(), outputLo)};
        appendToBlock<Value>(block, Jump, m_value->origin());
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
            auto* left = m_value->child(0);
            auto* right = m_value->child(1);
            setHi(insert<Value>(m_index, m_value->opcode(), m_value->origin(), getHi(left), getHi(right)));
            setLo(insert<Value>(m_index, m_value->opcode(), m_value->origin(), getLo(left), getLo(right)));
            valueReplaced();
            return;
        }
        case Const64: {
            setHi(insert<Const32Value>(m_index, m_value->origin(), (m_value->asInt() >> 32) & 0xffffffff));
            setLo(insert<Const32Value>(m_index, m_value->origin(), m_value->asInt() & 0xffffffff));
            valueReplaced();
            return;
        }
        case ArgumentReg: {
            if (m_value->type() != Int64)
                return;
            // XXX Stitch
            return;
        }

        case Phi : {
            if (m_value->type() != Int64)
                return;
            setHi(insert<Value>(m_index, Phi, Int32, m_value->origin()));
            setLo(insert<Value>(m_index, Phi, Int32, m_value->origin()));
            valueReplaced();
            return;
        }
        case Upsilon: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto* phi = m_value->as<UpsilonValue>()->phi();
            auto* input = m_value->child(0);
            auto* hi = insert<UpsilonValue>(m_index, m_value->origin(), getHi(input));
            auto* lo = insert<UpsilonValue>(m_index, m_value->origin(), getLo(input));
            hi->setPhi(getHi(phi));
            lo->setPhi(getLo(phi));
            setHi(hi);
            setLo(lo);
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
            // The above loop ensured we have all we need, so can allocate the replacement m_value.
            CCallValue* cCall = insert<CCallValue>(m_index, m_value->type(), m_value->origin(), m_value->child(0));
            cCall->effects = m_value->effects();
            Vector<Value*> args;
            RELEASE_ASSERT(cCall->numChildren() == 1);
            for (size_t index = 1; index <= m_value->numChildren(); ++index) {
                Value *child = m_value->child(index);
                if (child->type() != Int64)
                    args.append(child);
                else
                    args.append(valueHiLo(getHi(child), getLo(child)));
            }
            cCall->appendArgs(args);
            m_value->replaceWithIdentity(cCall);
            if (m_value->type() == Int64) {
                setHi(truncHigh(m_value));
                setLo(truncLow(m_value));
            }
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
                if (child->type() == Int64)
                    args.append(valueHiLo(getHi(child), getLo(child)));
                else
                    args.append(child);
            }
            CheckValue* check;
            if (m_value->kind() == Check) {
                RELEASE_ASSERT(m_value->numChildren() == 1);
                check = insert<CheckValue>(m_index, m_value->kind(), m_value->origin(), m_value->child(0));
            } else {
                RELEASE_ASSERT(m_value->numChildren() == 2);
                check = insert<CheckValue>(m_index, m_value->kind(), m_value->origin(), m_value->child(0), m_value->child(1));
            }
            check->clobberEarly(originalCheck->earlyClobbered());
            check->clobberLate(originalCheck->lateClobbered());
            // XXX: m_usedRegisters?
            check->setGenerator(originalCheck->generator());
            const Vector<ValueRep>& reps = originalCheck->reps();
            for (size_t index = 0; index < m_value->numChildren(); ++index) {
                check->append(args[index], reps[index]);
            }
            m_value->replaceWithIdentity(check);
            if (m_value->type() == Int64) {
                setHi(truncHigh(m_value));
                setLo(truncLow(m_value));
            }
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
                if (child->type() == Int64)
                    args.append(valueHiLo(getHi(child), getLo(child)));
                else
                    args.append(child);
            }
            PatchpointValue* patchpoint = insert<PatchpointValue>(m_index, originalPatchpoint->type(), m_value->origin());
            patchpoint->clobberEarly(originalPatchpoint->earlyClobbered());
            patchpoint->clobberLate(originalPatchpoint->lateClobbered());
            // XXX: m_usedRegisters?
            patchpoint->effects = originalPatchpoint->effects;
            patchpoint->resultConstraints = originalPatchpoint->resultConstraints;
            patchpoint->numGPScratchRegisters = originalPatchpoint->numGPScratchRegisters;
            patchpoint->numFPScratchRegisters = originalPatchpoint->numFPScratchRegisters;
            patchpoint->setGenerator(originalPatchpoint->generator());
            const Vector<ValueRep>& reps = originalPatchpoint->reps();
            for (size_t index = 0; index < originalPatchpoint->numChildren(); ++index) {
                patchpoint->append(args[index], reps[index]);
            }
            m_value->replaceWithIdentity(patchpoint);
            if (m_value->type() == Int64) {
                setHi(truncHigh(m_value));
                setLo(truncLow(m_value));
            }
            return;
        }
        case Add:
        case Sub:
        case Mul: {
            if (m_value->type() != Int64)
                return;
            auto* left = m_value->child(0);
            auto* right = m_value->child(1);
            Value* stitchedLeft = valueHiLo(getHi(left), getLo(left));
            Value* stitchedRight = valueHiLo(getHi(right), getLo(right));
            Value* stitched = insert<Value>(m_index, m_value->opcode(), m_value->origin(), stitchedLeft, stitchedRight);
            setHi(truncHigh(stitched));
            setLo(truncLow(stitched));
            valueReplaced();
            return;
        }

        case Branch: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto* input = m_value->child(0);
            Value* testValue = insert<Value>(m_index, BitOr, m_value->origin(), getHi(input), getLo(input));
            Value* branchValue = insert<Value>(m_index, Branch, m_value->origin(), testValue);
            m_value->replaceWithIdentity(branchValue);
            return;
        }

        case Equal:
        case NotEqual: {
            if (m_value->child(0)->type() != Int64)
                return;
            auto* left = m_value->child(0);
            auto* right = m_value->child(1);
            Value *highComparison = insert<Value>(m_index, m_value->opcode(), m_value->origin(), getHi(left), getHi(right));
            Value *lowComparison = insert<Value>(m_index, m_value->opcode(), m_value->origin(), getLo(left), getLo(right));
            Opcode combineOpcode = m_value->opcode() == Equal ? BitAnd : BitOr;
            Value *result = insert<Value>(m_index, combineOpcode, m_value->origin(), highComparison, lowComparison);
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
            auto* left = m_value->child(0);
            auto* right = m_value->child(1);
            Value* highComparison = insert<Value>(m_index, m_value->opcode(), m_value->origin(), getHi(left), getHi(right));
            Value* highEquality = insert<Value>(m_index, Equal, m_value->origin(), getHi(left), getHi(right));
            Value* lowComparison = insert<Value>(m_index, m_value->opcode(), m_value->origin(), getLo(left), getLo(right));
            Value* result = insert<Value>(m_index, BitOr, m_value->origin(), highComparison,
                                          insert<Value>(m_index, BitAnd, m_value->origin(), highEquality, lowComparison));
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
            auto* left = m_value->child(0);
            auto* right = m_value->child(1);
            Opcode highComparisonOpcode = m_value->opcode() == BelowEqual ? Below : Above;
            Value* highComparison = insert<Value>(m_index, highComparisonOpcode, m_value->origin(), getHi(left), getHi(right));
            Value* highEquality = insert<Value>(m_index, Equal, m_value->origin(), getHi(left), getHi(right));
            Value* lowComparison = insert<Value>(m_index, m_value->opcode(), m_value->origin(), getLo(left), getLo(right));
            Value* result = insert<Value>(m_index, BitOr, m_value->origin(), highComparison,
                                          insert<Value>(m_index, BitAnd, m_value->origin(), highEquality, lowComparison));
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
            auto* left = m_value->child(0);
            auto* right = m_value->child(1);
            Value* highComparison = insert<Value>(m_index, m_value->opcode(), m_value->origin(), getHi(left), getHi(right));
            Value* highEquality = insert<Value>(m_index, Equal, m_value->origin(), getHi(left), getHi(right));
            Opcode lowComparisonOpcode = m_value->opcode() == LessThan ? Below : Above;
            Value* lowComparison = insert<Value>(m_index, lowComparisonOpcode, m_value->origin(), getLo(left), getLo(right));
            Value* result = insert<Value>(m_index, BitOr, m_value->origin(), highComparison,
                                          insert<Value>(m_index, BitAnd, m_value->origin(), highEquality, lowComparison));
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
            auto* left = m_value->child(0);
            auto* right = m_value->child(1);
            Opcode highComparisonOpcode = m_value->opcode() == LessEqual ? LessThan : GreaterThan;
            Value* highComparison = insert<Value>(m_index, highComparisonOpcode, m_value->origin(), getHi(left), getHi(right));
            Value* highEquality = insert<Value>(m_index, Equal, m_value->origin(), getHi(left), getHi(right));
            Opcode lowComparisonOpcode = m_value->opcode() == LessEqual ? BelowEqual : AboveEqual;
            Value* lowComparison = insert<Value>(m_index, lowComparisonOpcode, m_value->origin(), getLo(left), getLo(right));
            Value* result = insert<Value>(m_index, BitOr, m_value->origin(), highComparison,
                                          insert<Value>(m_index, BitAnd, m_value->origin(), highEquality, lowComparison));
            m_value->replaceWithIdentity(result);
            return;
        }

        case Return: {
            if (!m_value->numChildren() || m_value->child(0)->type() != Int64)
                return;
            auto* retVal = m_value->child(0);
            PatchpointValue* patchpoint = insert<PatchpointValue>(m_index, Void, m_value->origin());
            patchpoint->effects = Effects::none();
            patchpoint->effects.terminal = true;
            patchpoint->append(getHi(retVal), ValueRep::reg(GPRInfo::returnValueGPR2));
            patchpoint->append(getLo(retVal), ValueRep::reg(GPRInfo::returnValueGPR));
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
            auto* input = m_value->child(0);
            Value *shiftAmount = m_value->child(1);
            BasicBlock* before = splitForward();
            BasicBlock* check = m_blockInsertionSet.insertBefore(m_block);
            BasicBlock* aboveOrEquals32 = m_blockInsertionSet.insertBefore(m_block);
            BasicBlock* below32 = m_blockInsertionSet.insertBefore(m_block);
            BasicBlock* shiftIsZero = m_blockInsertionSet.insertBefore(m_block);

            Value *maskedShift = appendToBlock<Value>(before, B3::BitAnd, m_value->origin(),
                                                          shiftAmount,
                                                          before->replaceLastWithNew<Const32Value>(m_proc, m_value->origin(), 63));
            appendToBlock<Value>(before, Branch, m_value->origin(), maskedShift);
            before->setSuccessors(check, shiftIsZero);
            Value* constant32 = appendToBlock<Const32Value>(check, m_value->origin(), 32);
            appendToBlock<Value>(check, Branch, m_value->origin(),
                                    appendToBlock<Value>(check, Below, m_value->origin(), maskedShift,
                                                            constant32));
            check->setSuccessors(below32, aboveOrEquals32);

            std::pair<UpsilonValue*, UpsilonValue*> resultBelow32 = shiftBelow32(below32, m_value->opcode(), input, maskedShift);
            below32->setSuccessors(m_block);

            std::pair<UpsilonValue*, UpsilonValue*> resultAboveOrEquals32 = shiftAboveEquals32(aboveOrEquals32, m_value->opcode(), input, maskedShift);
            aboveOrEquals32->setSuccessors(m_block);

            std::pair<UpsilonValue*, UpsilonValue*> resultShiftIsZero = std::pair {
                appendToBlock<UpsilonValue>(shiftIsZero, m_value->origin(), getHi(input)),
                appendToBlock<UpsilonValue>(shiftIsZero, m_value->origin(), getLo(input))
            };
            appendToBlock<Value>(shiftIsZero, Jump, m_value->origin());
            shiftIsZero->setSuccessors(m_block);

            std::pair<Value*, Value*> phis = std::pair {
                insert<Value>(m_index, Phi, Int32, m_value->origin()),
                insert<Value>(m_index, Phi, Int32, m_value->origin())
            };
            resultBelow32.first->setPhi(phis.first);
            resultBelow32.second->setPhi(phis.second);

            resultAboveOrEquals32.first->setPhi(phis.first);
            resultAboveOrEquals32.second->setPhi(phis.second);

            resultShiftIsZero.first->setPhi(phis.first);
            resultShiftIsZero.second->setPhi(phis.second);

            setHi(phis.first);
            setLo(phis.second);
            before->updatePredecessorsAfter();
            valueReplaced();
            return;
        }
        case Const32:
        case FramePointer:
        case Jump:
            return;
        case Trunc: {
            m_value->replaceWithIdentity(getLo(m_value));
            return;
        }
        case ZExt32: {
            setHi(insert<Const32Value>(m_index, m_value->origin(), 0));
            setLo(m_value);
            return;
        }
        case Load: {
            if (m_value->type() != Int64)
                return;
            // XXX: TBD
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
    unsigned m_index;
    Value* m_value;
    bool m_changed;

    InsertionSet m_insertionSet;
    BlockInsertionSet m_blockInsertionSet;
    HashMap<Value*, std::pair<Value*, Value*>> m_mapping;
    HashSet<Value*> m_syntheticValues;
};

} // anonymous namespace

bool lowerInt64(Procedure& proc)
{
    PhaseScope phaseScope(proc, "B3::LowerInt64"_s);
    LowerInt64 lowerInt64(proc);
    return lowerInt64.run();
}

} // B3
} // JSC
#endif // USE(JSVALUE32_64) && ENABLE(B3_JIT)
