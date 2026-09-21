/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "B3SpecializeSelect.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3BlockInsertionSet.h"
#include "B3InsertionSetInlines.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace B3 {

namespace {

namespace B3SpecializeSelectInternal {
static constexpr bool verbose = false;
}

class SpecializeSelect {
public:
    SpecializeSelect(Procedure& proc)
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

        // specialize() always inserts blocks, so this is true exactly when one fired.
        bool changed = m_blockInsertionSet.execute();
        if (changed) {
            m_proc.resetReachability();
            m_proc.invalidateCFG();
        }

        return changed;
    }

private:
    void processCurrentBlock()
    {
        // A Check can only be specialized against a Select that precedes it in the same block, and
        // Selects are far rarer than values, so keep everything else off the per-value path.
        bool sawCandidateSelect = false;

        for (m_index = 0; m_index < m_block->size(); ++m_index) {
            m_value = m_block->at(m_index);
            switch (m_value->opcode()) {
            case Select:
                if (!sawCandidateSelect)
                    sawCandidateSelect = hasConstantArm(m_value);
                break;

            case Check: {
                if (!sawCandidateSelect)
                    break;
                Value* select = findSpecializableSelect();
                if (!select)
                    break;
                specialize(select);
                // Splitting moved everything above the Check, Selects included, into the new predecessor.
                sawCandidateSelect = false;
                break;
            }

            default:
                break;
            }
        }

        m_insertionSet.execute(m_block);
    }

    static bool hasConstantArm(Value* select)
    {
        return select->child(1)->isConstant() || select->child(2)->isConstant();
    }

    // Find the Select to specialize against the Check at m_index: it must be reachable from the
    // Check's predicate via children, must lie close enough to the Check in this block, and
    // everything from it up to the Check must be cloneable.
    //
    // FIXME: Make this handle Branch as well as Check. A terminal cannot be reached by splitting
    // the block; the then/else blocks have to be inserted instead, which doubles the Phis and the
    // Upsilons.
    Value* findSpecializableSelect()
    {
        // The bound counts back over non-free values, skipping the Nops that earlier phases leave
        // behind, so it reflects "this many useful operations back".
        constexpr unsigned selectSpecializationBound = 5;
        unsigned startIndex = 0;
        unsigned remaining = selectSpecializationBound;
        for (unsigned i = m_index; i--;) {
            if (!m_block->at(i)->isFree()) {
                if (!remaining) {
                    startIndex = i + 1;
                    break;
                }
                --remaining;
            }
        }

        Value* select = nullptr;
        unsigned selectIndex = 0;
        m_value->child(0)->walk(
            [&] (Value* value) -> Value::WalkStatus {
                auto window = m_block->values().subspan(startIndex, m_index + 1 - startIndex);
                auto iter = std::ranges::find(window, value);
                if (iter == window.end())
                    return Value::IgnoreChildren;

                if (value->opcode() == Select && hasConstantArm(value)) {
                    select = value;
                    selectIndex = startIndex + (iter - window.begin());
                    return Value::Stop;
                }

                return Value::Continue;
            });

        if (!select)
            return nullptr;

        for (unsigned i = selectIndex; i <= m_index; ++i) {
            if (m_block->at(i)->kind().isCloningForbidden())
                return nullptr;
        }

        return select;
    }

    // Turn this:
    //
    //     @a = Select(@p, @x, 42)
    //     @b = Add(@a, 35)
    //     Check(@b)
    //
    // into this:
    //
    //     Branch(@p, #truecase, #falsecase)
    //
    //   BB#truecase:
    //     @b_truecase = Add(@x, 35)
    //     Check(@b_truecase)
    //     Upsilon(@x, ^a)
    //     Upsilon(@b_truecase, ^b)
    //     Jump(#continuation)
    //
    //   BB#falsecase:
    //     @b_falsecase = Add(42, 35)
    //     Check(@b_falsecase)
    //     Upsilon(42, ^a)
    //     Upsilon(@b_falsecase, ^b)
    //     Jump(#continuation)
    //
    //   BB#continuation:
    //     @a = Phi()
    //     @b = Phi()
    //
    // One arm replaces every use of the Select with a constant, and the Check consumes that
    // constant transitively, so constant folding can kill most of that arm.
    void specialize(Value* source)
    {
        if (B3SpecializeSelectInternal::verbose)
            dataLog("Specializing select: ", deepDump(m_proc, source), "\n");

        // This mutates m_index to account for the fact that m_block got the front of it chopped
        // off.
        BasicBlock* predecessor = m_blockInsertionSet.splitForward(m_block, m_index, &m_insertionSet);

        // Splitting commits the insertion set, which changes the exact position of the source.
        // That's why we do the search after splitting.
        size_t startIndex = predecessor->values().reverseFind(source);
        RELEASE_ASSERT(startIndex != notFound);

        // By BasicBlock convention, caseIndex == 0 => then, caseIndex == 1 => else.
        static constexpr unsigned numCases = 2;
        BasicBlock* cases[numCases];
        for (unsigned i = 0; i < numCases; ++i)
            cases[i] = m_blockInsertionSet.insertBefore(m_block);

        std::array<UncheckedKeyHashMap<Value*, Value*>, numCases> mappings { };

        // Save things we want to know about the source.
        Value* predicate = source->child(0);

        for (unsigned i = 0; i < numCases; ++i)
            mappings[i].add(source, source->child(1 + i));

        auto cloneValue = [&] (Value* value) {
            ASSERT(value != source);

            for (unsigned i = 0; i < numCases; ++i) {
                Value* clone = m_proc.clone(value);
                for (Value*& child : clone->children()) {
                    if (Value* newChild = mappings[i].get(child))
                        child = newChild;
                }
                if (value->type() != Void)
                    mappings[i].add(value, clone);

                cases[i]->append(clone);
                if (value->type() != Void)
                    cases[i]->appendNew<UpsilonValue>(m_proc, value->origin(), clone, value);
            }

            value->replaceWithPhi();
        };

        // The jump that the splitter inserted is of no use to us.
        predecessor->removeLast(m_proc);

        // Handle the source, it's special.
        for (unsigned i = 0; i < numCases; ++i) {
            cases[i]->appendNew<UpsilonValue>(
                m_proc, source->origin(), source->child(1 + i), source);
        }
        source->replaceWithPhi();
        m_insertionSet.insertValue(m_index, source);

        // Now handle all values between the source and the check.
        for (size_t i = startIndex + 1; i < predecessor->size(); ++i) {
            Value* value = predecessor->at(i);
            value->owner = nullptr;

            cloneValue(value);

            if (value->type() != Void)
                m_insertionSet.insertValue(m_index, value);
            else
                m_proc.deleteValue(value);
        }

        // Finally, deal with the check.
        cloneValue(m_value);

        // Remove the values from the predecessor.
        predecessor->values().shrink(startIndex);

        predecessor->appendNew<Value>(m_proc, Branch, source->origin(), predicate);
        predecessor->setSuccessors(FrequentedBlock(cases[0]), FrequentedBlock(cases[1]));

        for (unsigned i = 0; i < numCases; ++i) {
            cases[i]->appendNew<Value>(m_proc, Jump, m_value->origin());
            cases[i]->setSuccessors(FrequentedBlock(m_block));
        }

        predecessor->updatePredecessorsAfter();
    }

    Procedure& m_proc;
    BlockInsertionSet m_blockInsertionSet;
    InsertionSet m_insertionSet;
    BasicBlock* m_block { nullptr };
    unsigned m_index { 0 };
    Value* m_value { nullptr };
};

} // anonymous namespace

bool specializeSelect(Procedure& proc)
{
    PhaseScope phaseScope(proc, "specializeSelect"_s);
    SpecializeSelect specializeSelect(proc);
    return specializeSelect.run();
}

} } // namespace JSC::B3

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(B3_JIT)
