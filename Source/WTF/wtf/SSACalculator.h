/*
 * Copyright (C) 2014-2026 Apple Inc. All rights reserved.
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

#include <climits>
#include <wtf/Bag.h>
#include <wtf/CommaPrinter.h>
#include <wtf/Dominators.h>
#include <wtf/HashMap.h>
#include <wtf/IndexMap.h>
#include <wtf/PrintStream.h>
#include <wtf/SegmentedVector.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WTF {

// SSACalculator provides a reusable tool for using the Cytron, Ferrante, Rosen, Wegman, and
// Zadeck "Efficiently Computing Static Single Assignment Form and the Control Dependence Graph"
// (TOPLAS'91) algorithm for computing SSA. SSACalculator doesn't magically do everything for you
// but it maintains the major data structures and handles most of the non-local reasoning. Here's
// the workflow of using SSACalculator to execute this algorithm:
//
// 0) Create a fresh SSACalculator instance. You will need this instance only for as long as
//    you're not yet done computing SSA.
//
// 1) Create an SSACalculator::Variable for every variable that you want to do Phi insertion
//    on. SSACalculator::Variable::index() is a dense indexing of the Variables that you
//    created, so you can easily use a Vector to map the SSACalculator::Variables to your
//    variables.
//
// 2) Create a SSACalculator::Def for every assignment to those variables. A Def knows about the
//    variable, the block, and the Value that has the value being put into the variable.
//    Note that creating a Def in block B for variable V if block B already has a def for variable
//    V will overwrite the previous Def's Value. This enables you to create Defs by
//    processing basic blocks in forward order. If a block has multiple Defs of a variable, this
//    "just works" because each block will then remember the last Def of each variable.
//
// 3) Call SSACalculator::computePhis(). This takes a functor that will create the Phi nodes. The
//    functor returns either the Phi node it created, or nullptr, if it chooses to prune. (As an
//    aside, it's always sound not to prune, and the safest reason for pruning is liveness.) The
//    computePhis() code will record the created Phi nodes as Defs, and it will separately record
//    the list of Phis inserted at each block. It's OK for the functor you pass here to modify the
//    Graph on the fly, but the easiest way to write this is to just create the Phi nodes by
//    doing Graph::addNode() and return them. It's then best to insert all Phi nodes for a block
//    in bulk as part of the pass you do below, in step (4).
//
// 4) Modify the graph to create the SSA data flow. For each block, this should:
//
//    4.0) Compute the set of reaching defs (aka available values) for each variable by calling
//         SSACalculator::reachingDefAtHead() for each variable. Record this in a local table that
//         will be incrementally updated as you proceed through the block in forward order in the
//         next steps:
//
//         FIXME: It might be better to compute reaching defs for all live variables in one go, to
//         avoid doing repeated dom tree traversals.
//         https://bugs.webkit.org/show_bug.cgi?id=136610
//
//    4.1) Insert all of the Phi nodes for the block by using SSACalculator::phisForBlock(), and
//         record those Phi nodes as being available values.
//
//    4.2) Process the block in forward order. For each load from a variable, replace it with the
//         available SSA value for that variable. For each store, delete it and record the stored
//         value as being available.
//
//         Note that you have two options of how to replace loads with SSA values. You can replace
//         the load with an Identity node; this will end up working fairly naturally so long as
//         you run GCSE after your phase. Or, you can replace all uses of the load with the SSA
//         value yourself (using the Graph::performSubstitution() idiom), but that requires that
//         your loop over basic blocks proceeds in the appropriate graph order, for example
//         preorder.
//
//         FIXME: Make it easier to do this, that doesn't involve rerunning GCSE.
//         https://bugs.webkit.org/show_bug.cgi?id=136639
//
//    4.3) Insert Upsilons at the end of the current block for the corresponding Phis in each successor block.
//         Use the available values table to decide the source value for each Phi's variable. Note that
//         you could also use SSACalculator::reachingDefAtTail() instead of the available values table,
//         though your local available values table is likely to be more efficient.
//
// The most obvious use of SSACalculator is for the CPS->SSA conversion itself, but it's meant to
// also be used for SSA update and for things like the promotion of heap fields to local SSA
// variables.

template<typename Adapter>
class SSACalculator : public Adapter {
    WTF_MAKE_TZONE_ALLOCATED_TEMPLATE(SSACalculator);
public:
    using CFG = typename Adapter::CFG;
    using Node = typename CFG::Node; // The basic block type.
    using Value = typename Adapter::Value;

    template<typename... Arguments>
    SSACalculator(Arguments&&... arguments)
        : Adapter(std::forward<Arguments>(arguments)...)
        , m_data(this->cfg().template newMap<BlockData>())
    {
    }

    ~SSACalculator() = default;

    void reset()
    {
        m_variables.clear();
        m_defs.clear();
        m_phis.clear();
        for (unsigned blockIndex = m_data.size(); blockIndex--;) {
            m_data[blockIndex].m_defs.clear();
            m_data[blockIndex].m_phis.clear();
        }
        m_dominanceFrontiersComputed = false;
    }

    class Variable {
    public:
        unsigned index() const { return m_index; }

        void dump(PrintStream& out) const
        {
            out.print("var", m_index);
        }

        void dumpVerbose(PrintStream& out) const
        {
            dump(out);
            if (!m_blocksWithDefs.isEmpty()) {
                out.print("(defs: "_s);
                CommaPrinter comma;
                for (Node block : m_blocksWithDefs)
                    out.print(comma, *block);
                out.print(")"_s);
            }
        }

    private:
        friend class SSACalculator;

        Variable() = default;

        explicit Variable(unsigned index)
            : m_index(index)
        {
        }

        Vector<Node, 4> m_blocksWithDefs;
        unsigned m_index { UINT_MAX };
    };

    class Def {
    public:
        Variable* variable() const { return m_variable; }
        Node block() const { return m_block; }
        Value value() const { return m_value; }

        void dump(PrintStream& out) const
        {
            out.print("def(", *m_variable, ", ", *m_block, ", ");
            Adapter::dumpValue(out, m_value);
            out.print(")");
        }

    private:
        friend class SSACalculator;

        Def() = default;

        Def(Variable* variable, Node block, Value value)
            : m_variable(variable)
            , m_block(block)
            , m_value(value)
        {
        }

        Variable* m_variable { nullptr };
        Node m_block { };
        Value m_value { };
    };

    Variable* newVariable()
    {
        return &m_variables.alloc(Variable(m_variables.size()));
    }

    Def* newDef(Variable* variable, Node block, Value value)
    {
        Def* def = m_defs.add(Def(variable, block, value));
        auto result = m_data[block].m_defs.add(variable, def);
        if (result.isNewEntry)
            variable->m_blocksWithDefs.append(block);
        else
            result.iterator->value = def;
        return def;
    }

    Variable* variable(unsigned index) { return &m_variables[index]; }

    // The functor takes a Variable and a Node (basic block) and either inserts a Phi and returns
    // the Value for that Phi, or it decides that it's not worth it to insert a Phi at that block
    // because of some additional pruning condition (typically liveness) and returns a null Value.
    // If a non-null Value is returned, a new Def is created, so that nonLocalReachingDef() will
    // find it later. Note that it is generally always sound to not prune any Phis (that is, to
    // always have the functor insert a Phi and never return a null Value).
    template<typename Functor>
    void computePhis(const Functor& functor)
    {
        ensureDominanceFrontiers();

        CFG& cfg = this->cfg();

        // Iterated dominance frontier per variable, computed over the shared per-block
        // dominance-frontier cache. The per-variable "visited" set is tracked with
        // an epoch stamp so we don't allocate a fresh set for every variable.
        Vector<Node, 16> worklist;
        IndexMap<Node, unsigned> visited(cfg.numNodes(), 0);
        for (Variable& variable : m_variables) {
            unsigned epoch = variable.index() + 1;
            worklist.shrink(0);
            worklist.appendVector(variable.m_blocksWithDefs);
            while (!worklist.isEmpty()) {
                Node block = worklist.takeLast();
                for (Node to : m_dominanceFrontiers[block]) {
                    if (visited[to] == epoch)
                        continue;
                    visited[to] = epoch;

                    Value phi = functor(&variable, to);
                    if (!phi)
                        continue;

                    BlockData& data = m_data[to];
                    Def* phiDef = m_phis.add(Def(&variable, to, phi));
                    data.m_phis.append(phiDef);

                    // Note that it's possible to have a block that looks like this before SSA
                    // conversion:
                    //
                    // label:
                    //     print(x);
                    //     ...
                    //     x = 42;
                    //     goto label;
                    //
                    // And it may look like this after SSA conversion:
                    //
                    // label:
                    //     x1: Phi()
                    //     ...
                    //     Upsilon(42, ^x1)
                    //     goto label;
                    //
                    // In this case, we will want to insert a Phi in this block, and the block
                    // will already have a Def for the variable. When this happens, we don't want
                    // the Phi to override the original Def, since the Phi is at the top, the
                    // original Def in the m_defs table would have been at the bottom, and we want
                    // m_defs to tell us about defs at tail.
                    //
                    // So, we rely on the fact that UncheckedKeyHashMap::add() does nothing if the
                    // key was already present.
                    data.m_defs.add(&variable, phiDef);
                    worklist.append(to);
                }
            }
        }
    }

    const Vector<Def*>& phisForBlock(Node block)
    {
        return m_data[block].m_phis;
    }

    // Ignores defs within the given block; it assumes that you've taken care of those yourself.
    Def* nonLocalReachingDef(Node block, Variable* variable)
    {
        return reachingDefAtTail(this->dominators().idom(block), variable);
    }

    Def* reachingDefAtHead(Node block, Variable* variable)
    {
        return nonLocalReachingDef(block, variable);
    }

    // Considers the def within the given block, but only works at the tail of the block.
    Def* reachingDefAtTail(Node startingBlock, Variable* variable)
    {
        auto& dominators = this->dominators();
        for (Node block = startingBlock; block; block = dominators.idom(block)) {
            if (Def* def = m_data[block].m_defs.get(variable)) {
                for (Node otherBlock = startingBlock; otherBlock != block; otherBlock = dominators.idom(otherBlock))
                    m_data[otherBlock].m_defs.add(variable, def);
                return def;
            }
        }
        return nullptr;
    }

    void dump(PrintStream& out) const
    {
        CFG& cfg = this->cfg();

        out.print("<Variables: ["_s);
        CommaPrinter comma;
        for (unsigned i = 0; i < m_variables.size(); ++i) {
            out.print(comma);
            m_variables[i].dumpVerbose(out);
        }
        out.print("], Defs: ["_s);
        comma = CommaPrinter();
        for (Def* def : const_cast<SSACalculator*>(this)->m_defs)
            out.print(comma, *def);
        out.print("], Phis: ["_s);
        comma = CommaPrinter();
        for (Def* def : const_cast<SSACalculator*>(this)->m_phis)
            out.print(comma, *def);
        out.print("], Block data: ["_s);
        comma = CommaPrinter();
        for (unsigned blockIndex = 0; blockIndex < cfg.numNodes(); ++blockIndex) {
            Node block = cfg.node(blockIndex);
            if (!block)
                continue;

            out.print(comma, *block, "=>("_s);
            out.print("Defs: {"_s);
            CommaPrinter innerComma;
            for (auto entry : m_data[block].m_defs)
                out.print(innerComma, *entry.key, "->"_s, *entry.value);
            out.print("}, Phis: {"_s);
            innerComma = CommaPrinter();
            for (Def* def : m_data[block].m_phis)
                out.print(innerComma, *def);
            out.print("})"_s);
        }
        out.print("]>"_s);
    }

private:
    struct BlockData {
        UncheckedKeyHashMap<Variable*, Def*> m_defs;
        Vector<Def*> m_phis;
    };

    void ensureDominanceFrontiers()
    {
        if (m_dominanceFrontiersComputed)
            return;
        m_dominanceFrontiersComputed = true;

        CFG& cfg = this->cfg();
        auto& dominators = this->dominators();

        // "A Simple, Fast Dominance Algorithm", Cooper, Harvey, and Kennedy, 2001.
        // https://www.cs.tufts.edu/~nr/cs257/archive/keith-cooper/dom14.pdf
        m_dominanceFrontiers = cfg.template newMap<Vector<Node>>();
        for (unsigned blockIndex = 0; blockIndex < cfg.numNodes(); ++blockIndex) {
            Node block = cfg.node(blockIndex);
            if (!block || cfg.predecessors(block).size() < 2)
                continue;
            Node idomBlock = dominators.idom(block);
            for (Node predecessor : cfg.predecessors(block)) {
                Node runner = predecessor;
                while (runner && runner != idomBlock) {
                    Vector<Node>& frontier = m_dominanceFrontiers[runner];
                    if (frontier.isEmpty() || frontier.last() != block)
                        frontier.append(block);
                    runner = dominators.idom(runner);
                }
            }
        }
    }

    SegmentedVector<Variable> m_variables;
    Bag<Def> m_defs;
    Bag<Def> m_phis;
    IndexMap<Node, BlockData> m_data;
    IndexMap<Node, Vector<Node>> m_dominanceFrontiers;
    bool m_dominanceFrontiersComputed { false };
};

WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL(template<typename Adapter>, SSACalculator<Adapter>);

} // namespace WTF
