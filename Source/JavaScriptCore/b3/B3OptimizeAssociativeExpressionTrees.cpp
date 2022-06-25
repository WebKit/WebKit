/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "B3OptimizeAssociativeExpressionTrees.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlock.h"
#include "B3Const32Value.h"
#include "B3Const64Value.h"
#include "B3InsertionSetInlines.h"
#include "B3Opcode.h"
#include "B3PhaseScope.h"
#include "B3Procedure.h"
#include "B3Value.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

class OptimizeAssociativeExpressionTrees {
public:
    OptimizeAssociativeExpressionTrees(Procedure& proc)
        : m_proc(proc)
    {
    }

    bool run();

private:
    int64_t neutralElement(Opcode);
    bool isAbsorbingElement(Opcode, int64_t);
    void combineConstants(Opcode, int64_t&, int64_t);
    void emitValue(Opcode, Value*, unsigned numSeen, InsertionSet&, size_t indexInBlock, Vector<Value*, 4>& results);
    bool optimizeRootedTree(Value* root, InsertionSet&, size_t indexInBlock, const Vector<unsigned>& useCounts);

    Procedure& m_proc;
    static constexpr bool verbose { false };
};

int64_t OptimizeAssociativeExpressionTrees::neutralElement(Opcode op)
{
    switch (op) {
    case Add:
    case BitOr:
    case BitXor:
        return 0;
    case Mul:
        return 1;
    case BitAnd:
        return -1;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

bool OptimizeAssociativeExpressionTrees::isAbsorbingElement(Opcode op, int64_t constant)
{
    switch (op) {
    case Add:
    case BitXor:
        return false;
    case Mul:
    case BitAnd:
        return !constant;
    case BitOr:
        return constant == -1;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void OptimizeAssociativeExpressionTrees::combineConstants(Opcode op, int64_t& const1, int64_t const2)
{
    switch (op) {
    case Add:
        const1 += const2;
        break;
    case Mul:
        const1 *= const2;
        break;
    case BitAnd:
        const1 &= const2;
        break;
    case BitOr:
        const1 |= const2;
        break;
    case BitXor:
        const1 ^= const2;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void OptimizeAssociativeExpressionTrees::emitValue(Opcode op, Value* value, unsigned numSeen, InsertionSet& insertionSet, size_t indexInBlock, Vector<Value*, 4>& results)
{
    switch (op) {
    case Add:
        if (numSeen > 1) {
            Value* constNumSeen;
            if (value->type() == Int32)
                constNumSeen = insertionSet.insert<Const32Value>(indexInBlock, value->origin(), numSeen);
            else
                constNumSeen = insertionSet.insert<Const64Value>(indexInBlock, value->origin(), static_cast<int64_t>(numSeen));
            results.append(insertionSet.insert<Value>(indexInBlock, Mul, value->origin(), value, constNumSeen));
        } else
            results.append(value);
        break;
    case Mul:
        for (unsigned i = 0; i < numSeen; ++i)
            results.append(value);
        break;
    case BitAnd:
    case BitOr:
        results.append(value);
        break;
    case BitXor:
        if (numSeen % 2)
            results.append(value);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

bool OptimizeAssociativeExpressionTrees::optimizeRootedTree(Value* root, InsertionSet& insertionSet, size_t indexInBlock, const Vector<unsigned>& useCounts)
{
    Opcode op = root->opcode();
    if ((root->child(0)->opcode() != op || useCounts[root->child(0)->index()] > 1)
        && (root->child(1)->opcode() != op || useCounts[root->child(1)->index()] > 1)) {
        // This is a trivial expression tree of size two, we have nothing to do here that B3ReduceStrength cannot do better than us.
        return false;
    }

    // We proceed in three steps:
    // - gather all the leaves of the expression tree
    // - sort them, and combine as many as possible
    // - make a balanced binary tree of them

    Vector<Value*, 4> leaves;
    Vector<Value*, 3> worklist = { root->child(0), root->child(1) };
    int64_t constant = neutralElement(op);
    unsigned numVisited = 0;
    while (!worklist.isEmpty()) {
        Value* val = worklist.takeLast();
        if (val->opcode() == op && useCounts[val->index()] < 2) {
            worklist.append(val->child(0));
            worklist.append(val->child(1));
        } else if (val->hasInt()) {
            combineConstants(op, constant, val->asInt());
            numVisited++;
        } else {
            numVisited++;
            leaves.append(val);
        }
    }
    if (isAbsorbingElement(op, constant)) {
        Value* newRoot;
        if (root->type() == Int32)
            newRoot = insertionSet.insert<Const32Value>(indexInBlock, root->origin(), static_cast<int32_t>(constant));
        else
            newRoot = insertionSet.insert<Const64Value>(indexInBlock, root->origin(), constant);
        root->replaceWithIdentity(newRoot);
        return true;
    }
    if (numVisited < 4) {
        // This is a nearly-trivial expression of size 3. B3ReduceStrength is still able to deal with such expressions competently, and there is no possible win from balancing them.
        return false;
    }

    std::sort(leaves.begin(), leaves.end(), [](Value* x, Value* y) {
        return x->index() < y->index();
    });
    Vector<Value*, 4> optLeaves;
    Value* lastValue = nullptr;
    unsigned numSeen = 0;
    for (Value* value : leaves) {
        if (lastValue == value)
            numSeen++;
        else {
            if (lastValue)
                emitValue(op, lastValue, numSeen, insertionSet, indexInBlock, optLeaves);
            lastValue = value;
            numSeen = 1;
        }
    }
    if (lastValue)
        emitValue(op, lastValue, numSeen, insertionSet, indexInBlock, optLeaves);

    // optLeaves can be empty for trees of BitXor where all leaves happen an even number of times.
    // In that case, we make the whole tree equivalent to the neutral element (which is 0 for BitXor).
    if (constant != neutralElement(op) || optLeaves.isEmpty()) {
        if (root->type() == Int32)
            optLeaves.append(insertionSet.insert<Const32Value>(indexInBlock, root->origin(), static_cast<int32_t>(constant)));
        else
            optLeaves.append(insertionSet.insert<Const64Value>(indexInBlock, root->origin(), constant));
    }

    if (verbose) {
        dataLog("      Expression tree rooted at ", *root, " (", root->opcode(), ") with leaves (numVisited = ", numVisited, ") ");
        for (Value* leaf : leaves)
            dataLog(" ", *leaf);
        dataLog(" =>");
        for (Value* leaf : optLeaves)
            dataLog(" ", *leaf);
        dataLog("\n");
    }

    // Finally we can build the balanced binary tree
    unsigned leafIndex = 0;
    while (leafIndex + 1 < optLeaves.size()) {
        optLeaves.append(insertionSet.insert<Value>(indexInBlock, op, root->origin(), optLeaves[leafIndex], optLeaves[leafIndex + 1]));
        leafIndex += 2;
    }
    ASSERT(leafIndex == optLeaves.size() - 1);
    root->replaceWithIdentity(optLeaves[leafIndex]);
    return true;
}

bool OptimizeAssociativeExpressionTrees::run()
{
    bool changed = false;

    // We proceed in two phases.
    // In the first one we compute the use counts of each value (of an interesting opcode), and find potential roots of interesting expression trees.
    // In the second one we optimize each such expression tree in turn.
    // We need the use counts to avoid duplicating code.

    m_proc.resetValueOwners();

    Vector<unsigned> useCounts(m_proc.values().size(), 0); // Mapping from Value::m_index to use counts.
    HashSet<Value*> expressionTreeRoots;
    HashSet<BasicBlock*> rootOwners;

    for (BasicBlock* block : m_proc) {
        for (Value* value : *block) {
            for (Value* child : value->children()) {
                if (!child->isInteger())
                    continue;
                switch (child->opcode()) {
                case Mul:
                case Add:
                case BitAnd:
                case BitOr:
                case BitXor:
                    useCounts[child->index()]++;
                    if (child->opcode() != value->opcode() || useCounts[child->index()] > 1) {
                        expressionTreeRoots.add(child);
                        rootOwners.add(child->owner);
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    InsertionSet insertionSet = InsertionSet(m_proc);
    for (BasicBlock* block : rootOwners) {
        for (unsigned index = 0; index < block->size(); ++index) {
            Value* value = block->at(index);
            if (expressionTreeRoots.contains(value))
                changed |= optimizeRootedTree(value, insertionSet, index, useCounts);
        }
        insertionSet.execute(block);
    }

    return changed;
}

bool optimizeAssociativeExpressionTrees(Procedure& proc)
{
    PhaseScope phaseScope(proc, "optimizeAssociativeExpressionTrees");
    OptimizeAssociativeExpressionTrees optimizeAssociativeExpressionTrees(proc);
    return optimizeAssociativeExpressionTrees.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
