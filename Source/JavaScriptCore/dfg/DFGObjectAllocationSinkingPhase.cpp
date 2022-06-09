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
#include "DFGObjectAllocationSinkingPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBlockMapInlines.h"
#include "DFGClobbersExitState.h"
#include "DFGCombinedLiveness.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGLazyNode.h"
#include "DFGLivenessAnalysisPhase.h"
#include "DFGOSRAvailabilityAnalysisPhase.h"
#include "DFGPhase.h"
#include "DFGPromotedHeapLocation.h"
#include "DFGSSACalculator.h"
#include "DFGValidate.h"
#include "JSArrayIterator.h"
#include "JSInternalPromise.h"
#include "JSMapIterator.h"
#include "JSSetIterator.h"
#include "StructureInlines.h"
#include <wtf/StdList.h>

namespace JSC { namespace DFG {

namespace {

namespace DFGObjectAllocationSinkingPhaseInternal {
static constexpr bool verbose = false;
}

// In order to sink object cycles, we use a points-to analysis coupled
// with an escape analysis. This analysis is actually similar to an
// abstract interpreter focused on local allocations and ignoring
// everything else.
//
// We represent the local heap using two mappings:
//
// - A set of the local allocations present in the function, where
//   each of those have a further mapping from
//   PromotedLocationDescriptor to local allocations they must point
//   to.
//
// - A "pointer" mapping from nodes to local allocations, if they must
//   be equal to said local allocation and are currently live. This
//   can be because the node is the actual node that created the
//   allocation, or any other node that must currently point to it -
//   we don't make a difference.
//
// The following graph is a motivation for why we separate allocations
// from pointers:
//
// Block #0
//  0: NewObject({})
//  1: NewObject({})
//  -: PutByOffset(@0, @1, x)
//  -: PutStructure(@0, {x:0})
//  2: GetByOffset(@0, x)
//  -: Jump(#1)
//
// Block #1
//  -: Return(@2)
//
// Here, we need to remember in block #1 that @2 points to a local
// allocation with appropriate fields and structures information
// (because we should be able to place a materialization on top of
// block #1 here), even though @1 is dead. We *could* just keep @1
// artificially alive here, but there is no real reason to do it:
// after all, by the end of block #0, @1 and @2 should be completely
// interchangeable, and there is no reason for us to artificially make
// @1 more important.
//
// An important point to consider to understand this separation is
// that we should think of the local heap as follow: we have a
// bunch of nodes that are pointers to "allocations" that live
// someplace on the heap, and those allocations can have pointers in
// between themselves as well. We shouldn't care about whatever
// names we give to the allocations ; what matters when
// comparing/merging two heaps is the isomorphism/comparison between
// the allocation graphs as seen by the nodes.
//
// For instance, in the following graph:
//
// Block #0
//  0: NewObject({})
//  -: Branch(#1, #2)
//
// Block #1
//  1: NewObject({})
//  -: PutByOffset(@0, @1, x)
//  -: PutStructure(@0, {x:0})
//  -: Jump(#3)
//
// Block #2
//  2: NewObject({})
//  -: PutByOffset(@2, undefined, x)
//  -: PutStructure(@2, {x:0})
//  -: PutByOffset(@0, @2, x)
//  -: PutStructure(@0, {x:0})
//  -: Jump(#3)
//
// Block #3
//  -: Return(@0)
//
// we should think of the heaps at tail of blocks #1 and #2 as being
// exactly the same, even though one has @0.x pointing to @1 and the
// other has @0.x pointing to @2, because in essence this should not
// be different from the graph where we hoisted @1 and @2 into a
// single allocation in block #0. We currently will not handle this
// case, because we merge allocations based on the node they are
// coming from, but this is only a technicality for the sake of
// simplicity that shouldn't hide the deeper idea outlined here.

class Allocation {
public:
    // We use Escaped as a special allocation kind because when we
    // decide to sink an allocation, we still need to keep track of it
    // once it is escaped if it still has pointers to it in order to
    // replace any use of those pointers by the corresponding
    // materialization
    enum class Kind { Escaped, Object, Activation, Function, GeneratorFunction, AsyncFunction, AsyncGeneratorFunction, InternalFieldObject, RegExpObject };

    using Fields = HashMap<PromotedLocationDescriptor, Node*>;

    explicit Allocation(Node* identifier = nullptr, Kind kind = Kind::Escaped)
        : m_identifier(identifier)
        , m_kind(kind)
    {
    }


    const Fields& fields() const
    {
        return m_fields;
    }

    Fields& fields()
    {
        return m_fields;
    }

    Node* get(PromotedLocationDescriptor descriptor)
    {
        return m_fields.get(descriptor);
    }

    Allocation& set(PromotedLocationDescriptor descriptor, Node* value)
    {
        // Pointing to anything else than an unescaped local
        // allocation is represented by simply not having the
        // field
        if (value)
            m_fields.set(descriptor, value);
        else
            m_fields.remove(descriptor);
        return *this;
    }

    void remove(PromotedLocationDescriptor descriptor)
    {
        set(descriptor, nullptr);
    }

    bool hasStructures() const
    {
        switch (kind()) {
        case Kind::Object:
            return true;

        default:
            return false;
        }
    }

    Allocation& setStructures(const RegisteredStructureSet& structures)
    {
        ASSERT(hasStructures() && !structures.isEmpty());
        m_structures = structures;
        m_structuresForMaterialization = structures;
        return *this;
    }

    Allocation& mergeStructures(const Allocation& other)
    {
        ASSERT(hasStructures() || (other.structuresForMaterialization().isEmpty() && other.structures().isEmpty()));
        m_structures.filter(other.structures());
        m_structuresForMaterialization.merge(other.structuresForMaterialization());
        ASSERT(m_structures.isSubsetOf(m_structuresForMaterialization));
        return *this;
    }

    Allocation& filterStructures(const RegisteredStructureSet& structures)
    {
        ASSERT(hasStructures());
        m_structures.filter(structures);
        m_structuresForMaterialization.filter(structures);
        RELEASE_ASSERT(!m_structures.isEmpty());
        return *this;
    }

    const RegisteredStructureSet& structures() const
    {
        return m_structures;
    }

    const RegisteredStructureSet& structuresForMaterialization() const
    {
        return m_structuresForMaterialization;
    }

    Node* identifier() const { return m_identifier; }

    Kind kind() const { return m_kind; }

    bool isEscapedAllocation() const
    {
        return kind() == Kind::Escaped;
    }

    bool isObjectAllocation() const
    {
        return m_kind == Kind::Object;
    }

    bool isActivationAllocation() const
    {
        return m_kind == Kind::Activation;
    }

    bool isFunctionAllocation() const
    {
        return m_kind == Kind::Function || m_kind == Kind::GeneratorFunction || m_kind == Kind::AsyncFunction;
    }

    bool isInternalFieldObjectAllocation() const
    {
        return m_kind == Kind::InternalFieldObject;
    }

    bool isRegExpObjectAllocation() const
    {
        return m_kind == Kind::RegExpObject;
    }

    bool operator==(const Allocation& other) const
    {
        return m_identifier == other.m_identifier
            && m_kind == other.m_kind
            && m_fields == other.m_fields
            && m_structures == other.m_structures
            && m_structuresForMaterialization == other.m_structuresForMaterialization;
    }

    bool operator!=(const Allocation& other) const
    {
        return !(*this == other);
    }

    void dump(PrintStream& out) const
    {
        dumpInContext(out, nullptr);
    }

    void dumpInContext(PrintStream& out, DumpContext* context) const
    {
        switch (m_kind) {
        case Kind::Escaped:
            out.print("Escaped");
            break;

        case Kind::Object:
            out.print("Object");
            break;

        case Kind::Function:
            out.print("Function");
            break;

        case Kind::GeneratorFunction:
            out.print("GeneratorFunction");
            break;

        case Kind::AsyncFunction:
            out.print("AsyncFunction");
            break;

        case Kind::InternalFieldObject:
            out.print("InternalFieldObject");
            break;

        case Kind::AsyncGeneratorFunction:
            out.print("AsyncGeneratorFunction");
            break;

        case Kind::Activation:
            out.print("Activation");
            break;

        case Kind::RegExpObject:
            out.print("RegExpObject");
            break;
        }
        out.print("Allocation(");
        if (!m_structuresForMaterialization.isEmpty())
            out.print(inContext(m_structuresForMaterialization.toStructureSet(), context));
        if (!m_fields.isEmpty()) {
            if (!m_structuresForMaterialization.isEmpty())
                out.print(", ");
            out.print(mapDump(m_fields, " => #", ", "));
        }
        out.print(")");
    }

private:
    Node* m_identifier; // This is the actual node that created the allocation
    Kind m_kind;
    Fields m_fields;

    // This set of structures is the intersection of structures seen at control flow edges. It's used
    // for checks and speculation since it can't be widened.
    RegisteredStructureSet m_structures;

    // The second set of structures is the union of the structures at control flow edges. It's used
    // for materializations, where we need to generate code for all possible incoming structures.
    RegisteredStructureSet m_structuresForMaterialization;
};

class LocalHeap {
public:
    Allocation& newAllocation(Node* node, Allocation::Kind kind)
    {
        ASSERT(!m_pointers.contains(node) && !isAllocation(node));
        m_pointers.add(node, node);
        return m_allocations.set(node, Allocation(node, kind)).iterator->value;
    }

    bool isAllocation(Node* identifier) const
    {
        return m_allocations.contains(identifier);
    }

    // Note that this is fundamentally different from
    // onlyLocalAllocation() below. getAllocation() takes as argument
    // a node-as-identifier, that is, an allocation node. This
    // allocation node doesn't have to be alive; it may only be
    // pointed to by other nodes or allocation fields.
    // For instance, in the following graph:
    //
    // Block #0
    //  0: NewObject({})
    //  1: NewObject({})
    //  -: PutByOffset(@0, @1, x)
    //  -: PutStructure(@0, {x:0})
    //  2: GetByOffset(@0, x)
    //  -: Jump(#1)
    //
    // Block #1
    //  -: Return(@2)
    //
    // At head of block #1, the only reachable allocation is #@1,
    // which can be reached through node @2. Thus, getAllocation(#@1)
    // contains the appropriate metadata for this allocation, but
    // onlyLocalAllocation(@1) is null, as @1 is no longer a pointer
    // to #@1 (since it is dead). Conversely, onlyLocalAllocation(@2)
    // is the same as getAllocation(#@1), while getAllocation(#@2)
    // does not make sense since @2 is not an allocation node.
    //
    // This is meant to be used when the node is already known to be
    // an identifier (i.e. an allocation) - probably because it was
    // found as value of a field or pointer in the current heap, or
    // was the result of a call to follow(). In any other cases (such
    // as when doing anything while traversing the graph), the
    // appropriate function to call is probably onlyLocalAllocation.
    Allocation& getAllocation(Node* identifier)
    {
        auto iter = m_allocations.find(identifier);
        ASSERT(iter != m_allocations.end());
        return iter->value;
    }

    void newPointer(Node* node, Node* identifier)
    {
        ASSERT(!m_allocations.contains(node) && !m_pointers.contains(node));
        ASSERT(isAllocation(identifier));
        m_pointers.add(node, identifier);
    }

    // follow solves the points-to problem. Given a live node, which
    // may be either an allocation itself or a heap read (e.g. a
    // GetByOffset node), it returns the corresponding allocation
    // node, if there is one. If the argument node is neither an
    // allocation or a heap read, or may point to different nodes,
    // nullptr will be returned. Note that a node that points to
    // different nodes can never point to an unescaped local
    // allocation.
    Node* follow(Node* node) const
    {
        auto iter = m_pointers.find(node);
        ASSERT(iter == m_pointers.end() || (!iter->value || m_allocations.contains(iter->value)));
        return iter == m_pointers.end() ? nullptr : iter->value;
    }

    Node* follow(PromotedHeapLocation location) const
    {
        const Allocation& base = m_allocations.find(location.base())->value;
        auto iter = base.fields().find(location.descriptor());
        if (iter == base.fields().end())
            return nullptr;

        return iter->value;
    }

    // onlyLocalAllocation find the corresponding allocation metadata
    // for any live node. onlyLocalAllocation(node) is essentially
    // getAllocation(follow(node)), with appropriate null handling.
    Allocation* onlyLocalAllocation(Node* node)
    {
        Node* identifier = follow(node);
        if (!identifier)
            return nullptr;

        return &getAllocation(identifier);
    }

    Allocation* onlyLocalAllocation(PromotedHeapLocation location)
    {
        Node* identifier = follow(location);
        if (!identifier)
            return nullptr;

        return &getAllocation(identifier);
    }

    bool isUnescapedAllocation(Node* identifier) const
    {
        auto iter = m_allocations.find(identifier);
        return iter != m_allocations.end() && !iter->value.isEscapedAllocation();
    }

    // This allows us to store the escapees only when necessary. If
    // set, the current escapees can be retrieved at any time using
    // takeEscapees(), which will clear the cached set of escapees;
    // otherwise the heap won't remember escaping allocations.
    void setWantEscapees()
    {
        m_wantEscapees = true;
    }

    HashMap<Node*, Allocation> takeEscapees()
    {
        return WTFMove(m_escapees);
    }

    void escape(Node* node)
    {
        Node* identifier = follow(node);
        if (!identifier)
            return;

        escapeAllocation(identifier);
    }

    void merge(const LocalHeap& other)
    {
        assertIsValid();
        other.assertIsValid();
        ASSERT(!m_wantEscapees);

        if (!reached()) {
            ASSERT(other.reached());
            *this = other;
            return;
        }

        NodeSet toEscape;

        for (auto& allocationEntry : other.m_allocations)
            m_allocations.add(allocationEntry.key, allocationEntry.value);
        for (auto& allocationEntry : m_allocations) {
            auto allocationIter = other.m_allocations.find(allocationEntry.key);

            // If we have it and they don't, it died for them but we
            // are keeping it alive from another field somewhere.
            // There is nothing to do - we will be escaped
            // automatically when we handle that other field.
            // This will also happen for allocation that we have and
            // they don't, and all of those will get pruned.
            if (allocationIter == other.m_allocations.end())
                continue;

            if (allocationEntry.value.kind() != allocationIter->value.kind()) {
                toEscape.addVoid(allocationEntry.key);
                for (const auto& fieldEntry : allocationIter->value.fields())
                    toEscape.addVoid(fieldEntry.value);
            } else {
                mergePointerSets(allocationEntry.value.fields(), allocationIter->value.fields(), toEscape);
                allocationEntry.value.mergeStructures(allocationIter->value);
            }
        }

        {
            // This works because we won't collect all pointers until all of our predecessors
            // merge their pointer sets with ours. That allows us to see the full state of the
            // world during our fixpoint analysis. Once we have the full set of pointers, we
            // only mark pointers to TOP, so we will eventually converge.
            for (auto entry : other.m_pointers) {
                auto addResult = m_pointers.add(entry.key, entry.value);
                if (addResult.iterator->value != entry.value) {
                    if (addResult.iterator->value) {
                        toEscape.addVoid(addResult.iterator->value);
                        addResult.iterator->value = nullptr;
                    }
                    if (entry.value)
                        toEscape.addVoid(entry.value);
                }
            }
            // This allows us to rule out pointers for graphs like this:
            // bb#0
            // branch #1, #2
            // #1:
            // x = pointer A
            // jump #3
            // #2:
            // y = pointer B
            // jump #3
            // #3:
            // ...
            //
            // When we merge state at #3, we'll very likely prune away the x and y pointer,
            // since they're not live. But if they do happen to make it to this merge function, when
            // #3 merges with #2 and #1, it'll eventually rule out x and y as not existing
            // in the other, and therefore not existing in #3, which is the desired behavior.
            //
            // This also is necessary for a graph like this:
            // #0
            // o = {}
            // o2 = {}
            // jump #1
            // 
            // #1
            // o.f = o2
            // effects()
            // x = o.f
            // escape(o)
            // branch #2, #1
            // 
            // #2
            // x cannot be o2 here, it has to be TOP
            // ...
            //
            // On the first fixpoint iteration, we might think that x is o2 at the head
            // of #2. However, when we fixpoint our analysis, we determine that o gets
            // escaped. This means that when we fixpoint, x will eventually not be a pointer.
            // When we merge again here, we'll notice and mark o2 as escaped.
            for (auto& entry : m_pointers) {
                if (!other.m_pointers.contains(entry.key)) {
                    if (entry.value) {
                        toEscape.addVoid(entry.value);
                        entry.value = nullptr;
                        ASSERT(!m_pointers.find(entry.key)->value);
                    }
                }
            }
        }

        for (Node* identifier : toEscape)
            escapeAllocation(identifier);

        if (ASSERT_ENABLED) {
            for (const auto& entry : m_allocations)
                ASSERT_UNUSED(entry, entry.value.isEscapedAllocation() || other.m_allocations.contains(entry.key));
        }

        // If there is no remaining pointer to an allocation, we can
        // remove it. This should only happen for escaped allocations,
        // because we only merge liveness-pruned heaps in the first
        // place.
        prune();

        assertIsValid();
    }

    void pruneByLiveness(const NodeSet& live)
    {
        m_pointers.removeIf(
            [&] (const auto& entry) {
                return !live.contains(entry.key);
            });
        prune();
    }

    void assertIsValid() const
    {
        if (!ASSERT_ENABLED)
            return;

        // Pointers should point to an actual allocation
        for (const auto& entry : m_pointers) {
            if (entry.value)
                ASSERT(m_allocations.contains(entry.value));
        }

        for (const auto& allocationEntry : m_allocations) {
            // Fields should point to an actual allocation
            for (const auto& fieldEntry : allocationEntry.value.fields()) {
                ASSERT_UNUSED(fieldEntry, fieldEntry.value);
                ASSERT(m_allocations.contains(fieldEntry.value));
            }
        }
    }

    bool operator==(const LocalHeap& other) const
    {
        assertIsValid();
        other.assertIsValid();
        return m_allocations == other.m_allocations
            && m_pointers == other.m_pointers;
    }

    bool operator!=(const LocalHeap& other) const
    {
        return !(*this == other);
    }

    const HashMap<Node*, Allocation>& allocations() const
    {
        return m_allocations;
    }

    void dump(PrintStream& out) const
    {
        out.print("  Allocations:\n");
        for (const auto& entry : m_allocations)
            out.print("    #", entry.key, ": ", entry.value, "\n");
        out.print("  Pointers:\n");
        for (const auto& entry : m_pointers) {
            out.print("    ", entry.key, " => #");
            if (entry.value)
                out.print(entry.value, "\n");
            else
                out.print("TOP\n");
        }
    }

    bool reached() const
    {
        return m_reached;
    }

    void setReached()
    {
        m_reached = true;
    }

private:
    // When we merge two heaps, we escape all fields of allocations,
    // unless they point to the same thing in both heaps.
    // The reason for this is that it allows us not to do extra work
    // for diamond graphs where we would otherwise have to check
    // whether we have a single definition or not, which would be
    // cumbersome.
    //
    // Note that we should try to unify nodes even when they are not
    // from the same allocation; for instance we should be able to
    // completely eliminate all allocations from the following graph:
    //
    // Block #0
    //  0: NewObject({})
    //  -: Branch(#1, #2)
    //
    // Block #1
    //  1: NewObject({})
    //  -: PutByOffset(@1, "left", val)
    //  -: PutStructure(@1, {val:0})
    //  -: PutByOffset(@0, @1, x)
    //  -: PutStructure(@0, {x:0})
    //  -: Jump(#3)
    //
    // Block #2
    //  2: NewObject({})
    //  -: PutByOffset(@2, "right", val)
    //  -: PutStructure(@2, {val:0})
    //  -: PutByOffset(@0, @2, x)
    //  -: PutStructure(@0, {x:0})
    //  -: Jump(#3)
    //
    // Block #3:
    //  3: GetByOffset(@0, x)
    //  4: GetByOffset(@3, val)
    //  -: Return(@4)
    template<typename Key>
    static void mergePointerSets(HashMap<Key, Node*>& my, const HashMap<Key, Node*>& their, NodeSet& toEscape)
    {
        auto escape = [&] (Node* identifier) {
            toEscape.addVoid(identifier);
        };

        for (const auto& entry : their) {
            if (!my.contains(entry.key))
                escape(entry.value);
        }
        my.removeIf([&] (const auto& entry) {
            auto iter = their.find(entry.key);
            if (iter == their.end()) {
                escape(entry.value);
                return true;
            }
            if (iter->value != entry.value) {
                escape(entry.value);
                escape(iter->value);
                return true;
            }
            return false;
        });
    }

    void escapeAllocation(Node* identifier)
    {
        Allocation& allocation = getAllocation(identifier);
        if (allocation.isEscapedAllocation())
            return;

        Allocation unescaped = WTFMove(allocation);
        allocation = Allocation(unescaped.identifier(), Allocation::Kind::Escaped);

        for (const auto& entry : unescaped.fields())
            escapeAllocation(entry.value);

        if (m_wantEscapees)
            m_escapees.add(unescaped.identifier(), WTFMove(unescaped));
    }

    void prune()
    {
        NodeSet reachable;
        for (const auto& entry : m_pointers) {
            if (entry.value)
                reachable.addVoid(entry.value);
        }

        // Repeatedly mark as reachable allocations in fields of other
        // reachable allocations
        {
            Vector<Node*> worklist;
            worklist.appendRange(reachable.begin(), reachable.end());

            while (!worklist.isEmpty()) {
                Node* identifier = worklist.takeLast();
                Allocation& allocation = m_allocations.find(identifier)->value;
                for (const auto& entry : allocation.fields()) {
                    if (reachable.add(entry.value).isNewEntry)
                        worklist.append(entry.value);
                }
            }
        }

        // Remove unreachable allocations
        m_allocations.removeIf(
            [&] (const auto& entry) {
                return !reachable.contains(entry.key);
            });
    }

    bool m_reached = false;
    HashMap<Node*, Node*> m_pointers;
    HashMap<Node*, Allocation> m_allocations;

    bool m_wantEscapees = false;
    HashMap<Node*, Allocation> m_escapees;
};

class ObjectAllocationSinkingPhase : public Phase {
public:
    ObjectAllocationSinkingPhase(Graph& graph)
        : Phase(graph, "object allocation elimination")
        , m_pointerSSA(graph)
        , m_allocationSSA(graph)
        , m_insertionSet(graph)
    {
    }

    bool run()
    {
        ASSERT(m_graph.m_form == SSA);
        ASSERT(m_graph.m_fixpointState == FixpointNotConverged);

        if (!performSinking())
            return false;

        if (DFGObjectAllocationSinkingPhaseInternal::verbose) {
            dataLog("Graph after elimination:\n");
            m_graph.dump();
        }

        return true;
    }

private:
    bool performSinking()
    {
        m_graph.computeRefCounts();
        m_graph.initializeNodeOwners();
        m_graph.ensureSSADominators();
        performLivenessAnalysis(m_graph);
        performOSRAvailabilityAnalysis(m_graph);
        m_combinedLiveness = CombinedLiveness(m_graph);

        CString graphBeforeSinking;
        if (UNLIKELY(Options::verboseValidationFailure() && Options::validateGraphAtEachPhase())) {
            StringPrintStream out;
            m_graph.dump(out);
            graphBeforeSinking = out.toCString();
        }

        if (DFGObjectAllocationSinkingPhaseInternal::verbose) {
            dataLog("Graph before elimination:\n");
            m_graph.dump();
        }

        performAnalysis();

        if (!determineSinkCandidates())
            return false;

        if (DFGObjectAllocationSinkingPhaseInternal::verbose) {
            for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
                dataLog("Heap at head of ", *block, ": \n", m_heapAtHead[block]);
                dataLog("Heap at tail of ", *block, ": \n", m_heapAtTail[block]);
            }
        }

        promoteLocalHeap();
        removeICStatusFilters();

        if (Options::validateGraphAtEachPhase())
            DFG::validate(m_graph, DumpGraph, graphBeforeSinking);
        return true;
    }

    void performAnalysis()
    {
        m_heapAtHead = BlockMap<LocalHeap>(m_graph);
        m_heapAtTail = BlockMap<LocalHeap>(m_graph);

        bool changed;
        do {
            if (DFGObjectAllocationSinkingPhaseInternal::verbose)
                dataLog("Doing iteration of escape analysis.\n");
            changed = false;

            for (BasicBlock* block : m_graph.blocksInPreOrder()) {
                m_heapAtHead[block].setReached();
                m_heap = m_heapAtHead[block];

                for (Node* node : *block) {
                    handleNode(
                        node,
                        [] (PromotedHeapLocation, LazyNode) { },
                        [&] (PromotedHeapLocation) -> Node* {
                            return nullptr;
                        });
                }

                if (m_heap == m_heapAtTail[block])
                    continue;

                m_heapAtTail[block] = m_heap;
                changed = true;

                m_heap.assertIsValid();

                // We keep only pointers that are live, and only
                // allocations that are either live, pointed to by a
                // live pointer, or (recursively) stored in a field of
                // a live allocation.
                //
                // This means we can accidentally leak non-dominating
                // nodes into the successor. However, due to the
                // non-dominance property, we are guaranteed that the
                // successor has at least one predecessor that is not
                // dominated either: this means any reference to a
                // non-dominating allocation in the successor will
                // trigger an escape and get pruned during the merge.
                m_heap.pruneByLiveness(m_combinedLiveness.liveAtTail[block]);

                for (BasicBlock* successorBlock : block->successors()) {
                    // FIXME: Maybe we should:
                    // 1. Store the liveness pruned heap as part of m_heapAtTail
                    // 2. Move this code above where we make block merge with
                    // its predecessors before walking the block forward.
                    // https://bugs.webkit.org/show_bug.cgi?id=206041
                    LocalHeap heap = m_heapAtHead[successorBlock];
                    m_heapAtHead[successorBlock].merge(m_heap);
                    if (heap != m_heapAtHead[successorBlock])
                        changed = true;
                }
            }
        } while (changed);
    }

    template<typename InternalFieldClass>
    Allocation* handleInternalFieldClass(Node* node, HashMap<PromotedLocationDescriptor, LazyNode>& writes)
    {
        Allocation* result = &m_heap.newAllocation(node, Allocation::Kind::InternalFieldObject);
        writes.add(StructurePLoc, LazyNode(m_graph.freeze(node->structure().get())));
        auto initialValues = InternalFieldClass::initialValues();
        static_assert(initialValues.size() == InternalFieldClass::numberOfInternalFields);        
        for (unsigned index = 0; index < initialValues.size(); ++index)
            writes.add(PromotedLocationDescriptor(InternalFieldObjectPLoc, index), LazyNode(m_graph.freeze(initialValues[index])));

        return result;
    }

    template<typename WriteFunctor, typename ResolveFunctor>
    void handleNode(
        Node* node,
        const WriteFunctor& heapWrite,
        const ResolveFunctor& heapResolve)
    {
        m_heap.assertIsValid();
        ASSERT(m_heap.takeEscapees().isEmpty());

        Allocation* target = nullptr;
        HashMap<PromotedLocationDescriptor, LazyNode> writes;
        PromotedLocationDescriptor exactRead;

        switch (node->op()) {
        case NewObject:
            target = &m_heap.newAllocation(node, Allocation::Kind::Object);
            target->setStructures(node->structure());
            writes.add(
                StructurePLoc, LazyNode(m_graph.freeze(node->structure().get())));
            break;

        case NewFunction:
        case NewGeneratorFunction:
        case NewAsyncGeneratorFunction:
        case NewAsyncFunction: {
            if (isStillValid(node->castOperand<FunctionExecutable*>())) {
                m_heap.escape(node->child1().node());
                break;
            }

            if (node->op() == NewGeneratorFunction)
                target = &m_heap.newAllocation(node, Allocation::Kind::GeneratorFunction);
            else if (node->op() == NewAsyncFunction)
                target = &m_heap.newAllocation(node, Allocation::Kind::AsyncFunction);
            else if (node->op() == NewAsyncGeneratorFunction)
                target = &m_heap.newAllocation(node, Allocation::Kind::AsyncGeneratorFunction);
            else
                target = &m_heap.newAllocation(node, Allocation::Kind::Function);

            writes.add(FunctionExecutablePLoc, LazyNode(node->cellOperand()));
            writes.add(FunctionActivationPLoc, LazyNode(node->child1().node()));
            break;
        }

        case NewInternalFieldObject: {
            switch (node->structure()->typeInfo().type()) {
            case JSArrayIteratorType:
                target = handleInternalFieldClass<JSArrayIterator>(node, writes);
                break;
            case JSMapIteratorType:
                target = handleInternalFieldClass<JSMapIterator>(node, writes);
                break;
            case JSSetIteratorType:
                target = handleInternalFieldClass<JSSetIterator>(node, writes);
                break;
            case JSPromiseType:
                if (node->structure()->classInfo() == JSInternalPromise::info())
                    target = handleInternalFieldClass<JSInternalPromise>(node, writes);
                else {
                    ASSERT(node->structure()->classInfo() == JSPromise::info());
                    target = handleInternalFieldClass<JSPromise>(node, writes);
                }
                break;
            default:
                DFG_CRASH(m_graph, node, "Bad structure");
            }
            break;
        }

        case NewRegexp: {
            target = &m_heap.newAllocation(node, Allocation::Kind::RegExpObject);

            writes.add(RegExpObjectRegExpPLoc, LazyNode(node->cellOperand()));
            writes.add(RegExpObjectLastIndexPLoc, LazyNode(node->child1().node()));
            break;
        }

        case CreateActivation: {
            if (isStillValid(node->castOperand<SymbolTable*>())) {
                m_heap.escape(node->child1().node());
                break;
            }
            target = &m_heap.newAllocation(node, Allocation::Kind::Activation);
            writes.add(ActivationSymbolTablePLoc, LazyNode(node->cellOperand()));
            writes.add(ActivationScopePLoc, LazyNode(node->child1().node()));
            {
                SymbolTable* symbolTable = node->castOperand<SymbolTable*>();
                LazyNode initialValue(m_graph.freeze(node->initializationValueForActivation()));
                for (unsigned offset = 0; offset < symbolTable->scopeSize(); ++offset) {
                    writes.add(
                        PromotedLocationDescriptor(ClosureVarPLoc, offset),
                        initialValue);
                }
            }
            break;
        }

        case PutStructure:
            target = m_heap.onlyLocalAllocation(node->child1().node());
            if (target && target->isObjectAllocation()) {
                writes.add(StructurePLoc, LazyNode(m_graph.freeze(JSValue(node->transition()->next.get()))));
                target->setStructures(node->transition()->next);
            } else
                m_heap.escape(node->child1().node());
            break;

        case CheckStructureOrEmpty:
        case CheckStructure: {
            Allocation* allocation = m_heap.onlyLocalAllocation(node->child1().node());
            if (allocation && allocation->isObjectAllocation()) {
                RegisteredStructureSet filteredStructures = allocation->structures();
                filteredStructures.filter(node->structureSet());
                if (filteredStructures.isEmpty()) {
                    // FIXME: Write a test for this:
                    // https://bugs.webkit.org/show_bug.cgi?id=174322
                    m_heap.escape(node->child1().node());
                    break;
                }
                allocation->setStructures(filteredStructures);
                if (Node* value = heapResolve(PromotedHeapLocation(allocation->identifier(), StructurePLoc)))
                    node->convertToCheckStructureImmediate(value);
            } else
                m_heap.escape(node->child1().node());
            break;
        }

        case GetByOffset:
        case GetGetterSetterByOffset:
            target = m_heap.onlyLocalAllocation(node->child2().node());
            if (target && target->isObjectAllocation()) {
                unsigned identifierNumber = node->storageAccessData().identifierNumber;
                exactRead = PromotedLocationDescriptor(NamedPropertyPLoc, identifierNumber);
            } else {
                m_heap.escape(node->child1().node());
                m_heap.escape(node->child2().node());
            }
            break;

        case MultiGetByOffset: {
            Allocation* allocation = m_heap.onlyLocalAllocation(node->child1().node());
            if (allocation && allocation->isObjectAllocation()) {
                MultiGetByOffsetData& data = node->multiGetByOffsetData();
                RegisteredStructureSet validStructures;
                bool hasInvalidStructures = false;
                for (const auto& multiGetByOffsetCase : data.cases) {
                    if (!allocation->structures().overlaps(multiGetByOffsetCase.set()))
                        continue;

                    switch (multiGetByOffsetCase.method().kind()) {
                    case GetByOffsetMethod::LoadFromPrototype: // We need to escape those
                    case GetByOffsetMethod::Constant: // We don't really have a way of expressing this
                        hasInvalidStructures = true;
                        break;

                    case GetByOffsetMethod::Load: // We're good
                        validStructures.merge(multiGetByOffsetCase.set());
                        break;

                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                }
                if (hasInvalidStructures || validStructures.isEmpty()) {
                    m_heap.escape(node->child1().node());
                    break;
                }
                unsigned identifierNumber = data.identifierNumber;
                PromotedHeapLocation location(NamedPropertyPLoc, allocation->identifier(), identifierNumber);
                if (Node* value = heapResolve(location)) {
                    if (allocation->structures().isSubsetOf(validStructures))
                        node->replaceWithWithoutChecks(value);
                    else {
                        Node* structure = heapResolve(PromotedHeapLocation(allocation->identifier(), StructurePLoc));
                        ASSERT(structure);
                        allocation->filterStructures(validStructures);
                        node->convertToCheckStructure(m_graph.addStructureSet(allocation->structures()));
                        node->convertToCheckStructureImmediate(structure);
                        node->setReplacement(value);
                    }
                } else if (!allocation->structures().isSubsetOf(validStructures)) {
                    // Even though we don't need the result here, we still need
                    // to make the call to tell our caller that we could need
                    // the StructurePLoc.
                    // The reason for this is that when we decide not to sink a
                    // node, we will still lower any read to its fields before
                    // it escapes (which are usually reads across a function
                    // call that DFGClobberize can't handle) - but we only do
                    // this for PromotedHeapLocations that we have seen read
                    // during the analysis!
                    heapResolve(PromotedHeapLocation(allocation->identifier(), StructurePLoc));
                    allocation->filterStructures(validStructures);
                }
                Node* identifier = allocation->get(location.descriptor());
                if (identifier)
                    m_heap.newPointer(node, identifier);
            } else
                m_heap.escape(node->child1().node());
            break;
        }

        case PutByOffset:
            target = m_heap.onlyLocalAllocation(node->child2().node());
            if (target && target->isObjectAllocation()) {
                unsigned identifierNumber = node->storageAccessData().identifierNumber;
                writes.add(
                    PromotedLocationDescriptor(NamedPropertyPLoc, identifierNumber),
                    LazyNode(node->child3().node()));
            } else {
                m_heap.escape(node->child1().node());
                m_heap.escape(node->child2().node());
                m_heap.escape(node->child3().node());
            }
            break;

        case GetClosureVar:
            target = m_heap.onlyLocalAllocation(node->child1().node());
            if (target && target->isActivationAllocation()) {
                exactRead =
                    PromotedLocationDescriptor(ClosureVarPLoc, node->scopeOffset().offset());
            } else
                m_heap.escape(node->child1().node());
            break;

        case PutClosureVar:
            target = m_heap.onlyLocalAllocation(node->child1().node());
            if (target && target->isActivationAllocation()) {
                writes.add(
                    PromotedLocationDescriptor(ClosureVarPLoc, node->scopeOffset().offset()),
                    LazyNode(node->child2().node()));
            } else {
                m_heap.escape(node->child1().node());
                m_heap.escape(node->child2().node());
            }
            break;

        case SkipScope:
            target = m_heap.onlyLocalAllocation(node->child1().node());
            if (target && target->isActivationAllocation())
                exactRead = ActivationScopePLoc;
            else
                m_heap.escape(node->child1().node());
            break;

        case GetExecutable:
            target = m_heap.onlyLocalAllocation(node->child1().node());
            if (target && target->isFunctionAllocation())
                exactRead = FunctionExecutablePLoc;
            else
                m_heap.escape(node->child1().node());
            break;

        case GetScope:
            target = m_heap.onlyLocalAllocation(node->child1().node());
            if (target && target->isFunctionAllocation())
                exactRead = FunctionActivationPLoc;
            else
                m_heap.escape(node->child1().node());
            break;

        case GetRegExpObjectLastIndex:
            target = m_heap.onlyLocalAllocation(node->child1().node());
            if (target && target->isRegExpObjectAllocation())
                exactRead = RegExpObjectLastIndexPLoc;
            else
                m_heap.escape(node->child1().node());
            break;

        case SetRegExpObjectLastIndex:
            target = m_heap.onlyLocalAllocation(node->child1().node());
            if (target && target->isRegExpObjectAllocation()) {
                writes.add(
                    PromotedLocationDescriptor(RegExpObjectLastIndexPLoc),
                    LazyNode(node->child2().node()));
            } else {
                m_heap.escape(node->child1().node());
                m_heap.escape(node->child2().node());
            }
            break;

        case GetInternalField: {
            target = m_heap.onlyLocalAllocation(node->child1().node());
            if (target && target->isInternalFieldObjectAllocation())
                exactRead = PromotedLocationDescriptor(InternalFieldObjectPLoc, node->internalFieldIndex());
            else
                m_heap.escape(node->child1().node());
            break;
        }

        case PutInternalField: {
            target = m_heap.onlyLocalAllocation(node->child1().node());
            if (target && target->isInternalFieldObjectAllocation())
                writes.add(PromotedLocationDescriptor(InternalFieldObjectPLoc, node->internalFieldIndex()), LazyNode(node->child2().node()));
            else {
                m_heap.escape(node->child1().node());
                m_heap.escape(node->child2().node());
            }
            break;
        }

        case Check:
        case CheckVarargs:
            m_graph.doToChildren(
                node,
                [&] (Edge edge) {
                    if (edge.willNotHaveCheck())
                        return;

                    if (alreadyChecked(edge.useKind(), SpecObject))
                        return;

                    m_heap.escape(edge.node());
                });
            break;

        case MovHint:
        case PutHint:
            // Handled by OSR availability analysis
            break;
            
        case FilterCallLinkStatus:
        case FilterGetByStatus:
        case FilterPutByStatus:
        case FilterInByStatus:
        case FilterDeleteByStatus:
        case FilterCheckPrivateBrandStatus:
        case FilterSetPrivateBrandStatus:
            break;

        default:
            m_graph.doToChildren(
                node,
                [&] (Edge edge) {
                    m_heap.escape(edge.node());
                });
            break;
        }

        if (exactRead) {
            ASSERT(target);
            ASSERT(writes.isEmpty());
            if (Node* value = heapResolve(PromotedHeapLocation(target->identifier(), exactRead))) {
                ASSERT(!value->replacement());
                node->replaceWith(m_graph, value);
            }
            Node* identifier = target->get(exactRead);
            if (identifier)
                m_heap.newPointer(node, identifier);
        }

        for (auto entry : writes) {
            ASSERT(target);
            if (entry.value.isNode())
                target->set(entry.key, m_heap.follow(entry.value.asNode()));
            else
                target->remove(entry.key);
            heapWrite(PromotedHeapLocation(target->identifier(), entry.key), entry.value);
        }

        m_heap.assertIsValid();
    }

    bool determineSinkCandidates()
    {
        m_sinkCandidates.clear();
        m_materializationToEscapee.clear();
        m_materializationSiteToMaterializations.clear();
        m_materializationSiteToRecoveries.clear();
        m_materializationSiteToHints.clear();

        // Logically we wish to consider every allocation and sink
        // it. However, it is probably not profitable to sink an
        // allocation that will always escape. So, we only sink an
        // allocation if one of the following is true:
        //
        // 1) There exists a basic block with only backwards outgoing
        //    edges (or no outgoing edges) in which the node wasn't
        //    materialized. This is meant to catch
        //    effectively-infinite loops in which we don't need to
        //    have allocated the object.
        //
        // 2) There exists a basic block at the tail of which the node
        //    is dead and not materialized.
        //
        // 3) The sum of execution counts of the materializations is
        //    less than the sum of execution counts of the original
        //    node.
        //
        // We currently implement only rule #2.
        // FIXME: Implement the two other rules.
        // https://bugs.webkit.org/show_bug.cgi?id=137073 (rule #1)
        // https://bugs.webkit.org/show_bug.cgi?id=137074 (rule #3)
        //
        // However, these rules allow for a sunk object to be put into
        // a non-sunk one, which we don't support. We could solve this
        // by supporting PutHints on local allocations, making these
        // objects only partially correct, and we would need to adapt
        // the OSR availability analysis and OSR exit to handle
        // this. This would be totally doable, but would create a
        // super rare, and thus bug-prone, code path.
        // So, instead, we need to implement one of the following
        // closure rules:
        //
        // 1) If we put a sink candidate into a local allocation that
        //    is not a sink candidate, change our minds and don't
        //    actually sink the sink candidate.
        //
        // 2) If we put a sink candidate into a local allocation, that
        //    allocation becomes a sink candidate as well.
        //
        // We currently choose to implement closure rule #2.
        HashMap<Node*, Vector<Node*>> dependencies;
        bool hasUnescapedReads = false;
        for (BasicBlock* block : m_graph.blocksInPreOrder()) {
            m_heap = m_heapAtHead[block];

            for (Node* node : *block) {
                handleNode(
                    node,
                    [&] (PromotedHeapLocation location, LazyNode value) {
                        if (!value.isNode())
                            return;

                        Allocation* allocation = m_heap.onlyLocalAllocation(value.asNode());
                        if (allocation && !allocation->isEscapedAllocation())
                            dependencies.add(allocation->identifier(), Vector<Node*>()).iterator->value.append(location.base());
                    },
                    [&] (PromotedHeapLocation) -> Node* {
                        hasUnescapedReads = true;
                        return nullptr;
                    });
            }

            // The sink candidates are initially the unescaped
            // allocations dying at tail of blocks
            NodeSet allocations;
            for (const auto& entry : m_heap.allocations()) {
                if (!entry.value.isEscapedAllocation())
                    allocations.addVoid(entry.key);
            }

            m_heap.pruneByLiveness(m_combinedLiveness.liveAtTail[block]);

            for (Node* identifier : allocations) {
                if (!m_heap.isAllocation(identifier))
                    m_sinkCandidates.addVoid(identifier);
            }
        }

        auto forEachEscapee = [&] (auto callback) {
            for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
                m_heap = m_heapAtHead[block];
                m_heap.setWantEscapees();

                for (Node* node : *block) {
                    handleNode(
                        node,
                        [] (PromotedHeapLocation, LazyNode) { },
                        [] (PromotedHeapLocation) -> Node* {
                            return nullptr;
                        });
                    auto escapees = m_heap.takeEscapees();
                    escapees.removeIf([&] (const auto& entry) { return !m_sinkCandidates.contains(entry.key); });
                    callback(escapees, node);
                }

                m_heap.pruneByLiveness(m_combinedLiveness.liveAtTail[block]);

                {
                    HashMap<Node*, Allocation> escapingOnEdge;
                    for (const auto& entry : m_heap.allocations()) {
                        if (entry.value.isEscapedAllocation())
                            continue;

                        bool mustEscape = false;
                        for (BasicBlock* successorBlock : block->successors()) {
                            if (!m_heapAtHead[successorBlock].isAllocation(entry.key)
                                || m_heapAtHead[successorBlock].getAllocation(entry.key).isEscapedAllocation())
                                mustEscape = true;
                        }

                        if (mustEscape && m_sinkCandidates.contains(entry.key))
                            escapingOnEdge.add(entry.key, entry.value);
                    }
                    callback(escapingOnEdge, block->terminal());
                }
            }
        };

        if (m_sinkCandidates.size()) {
            // If we're moving an allocation to `where` in the program, we need to ensure
            // we can still walk the stack at that point in the program for the
            // InlineCallFrame of the original allocation. Certain InlineCallFrames rely on
            // data in the stack when taking a stack trace. All allocation sites can do a
            // stack walk (we do a stack walk when we GC). Conservatively, we say we're
            // still ok to move this allocation if we are moving within the same InlineCallFrame.
            // We could be more precise here and do an analysis of stack writes. However,
            // this scenario is so rare that we just take the conservative-and-straight-forward 
            // approach of checking that we're in the same InlineCallFrame.

            forEachEscapee([&] (HashMap<Node*, Allocation>& escapees, Node* where) {
                for (Node* allocation : escapees.keys()) {
                    InlineCallFrame* inlineCallFrame = allocation->origin.semantic.inlineCallFrame();
                    if (!inlineCallFrame)
                        continue;
                    if ((inlineCallFrame->isClosureCall || inlineCallFrame->isVarargs()) && inlineCallFrame != where->origin.semantic.inlineCallFrame())
                        m_sinkCandidates.remove(allocation);
                }
            });
        }

        // Ensure that the set of sink candidates is closed for put operations
        // This is (2) as described above.
        Vector<Node*> worklist;
        worklist.appendRange(m_sinkCandidates.begin(), m_sinkCandidates.end());

        while (!worklist.isEmpty()) {
            for (Node* identifier : dependencies.get(worklist.takeLast())) {
                if (m_sinkCandidates.add(identifier).isNewEntry)
                    worklist.append(identifier);
            }
        }

        if (m_sinkCandidates.isEmpty())
            return hasUnescapedReads;

        if (DFGObjectAllocationSinkingPhaseInternal::verbose)
            dataLog("Candidates: ", listDump(m_sinkCandidates), "\n");

        // Create the materialization nodes.
        forEachEscapee([&] (HashMap<Node*, Allocation>& escapees, Node* where) {
            placeMaterializations(WTFMove(escapees), where);
        });

        return hasUnescapedReads || !m_sinkCandidates.isEmpty();
    }

    void placeMaterializations(HashMap<Node*, Allocation> escapees, Node* where)
    {
        // First collect the hints that will be needed when the node
        // we materialize is still stored into other unescaped sink candidates.
        // The way to interpret this vector is:
        //
        // PromotedHeapLocation(NotEscapedAllocation, field) = identifierAllocation
        //
        // e.g:
        // PromotedHeapLocation(@PhantomNewFunction, FunctionActivationPLoc) = IdentifierOf(@MaterializeCreateActivation)
        // or:
        // PromotedHeapLocation(@PhantomCreateActivation, ClosureVarPLoc(x)) = IdentifierOf(@NewFunction)
        //
        // When the rhs of the `=` is to be materialized at this `where` point in the program
        // and IdentifierOf(Materialization) is the original sunken allocation of the materialization.
        //
        // The reason we need to collect all the `identifiers` here is that
        // we may materialize multiple versions of the allocation along control
        // flow edges. We will PutHint these values along those edges. However,
        // we also need to PutHint them when we join and have a Phi of the allocations.
        Vector<std::pair<PromotedHeapLocation, Node*>> hints;
        for (const auto& entry : m_heap.allocations()) {
            if (escapees.contains(entry.key))
                continue;

            for (const auto& field : entry.value.fields()) {
                ASSERT(m_sinkCandidates.contains(entry.key) || !escapees.contains(field.value));
                auto iter = escapees.find(field.value);
                if (iter != escapees.end()) {
                    ASSERT(m_sinkCandidates.contains(field.value));
                    hints.append(std::make_pair(PromotedHeapLocation(entry.key, field.key), field.value));
                }
            }
        }

        // Now we need to order the materialization. Any order is
        // valid (as long as we materialize a node first if it is
        // needed for the materialization of another node, e.g. a
        // function's activation must be materialized before the
        // function itself), but we want to try minimizing the number
        // of times we have to place Puts to close cycles after a
        // materialization. In other words, we are trying to find the
        // minimum number of materializations to remove from the
        // materialization graph to make it a DAG, known as the
        // (vertex) feedback set problem. Unfortunately, this is a
        // NP-hard problem, which we don't want to solve exactly.
        //
        // Instead, we use a simple greedy procedure, that procedes as
        // follow:
        //  - While there is at least one node with no outgoing edge
        //    amongst the remaining materializations, materialize it
        //    first
        //
        //  - Similarily, while there is at least one node with no
        //    incoming edge amongst the remaining materializations,
        //    materialize it last.
        //
        //  - When both previous conditions are false, we have an
        //    actual cycle, and we need to pick a node to
        //    materialize. We try greedily to remove the "pressure" on
        //    the remaining nodes by choosing the node with maximum
        //    |incoming edges| * |outgoing edges| as a measure of how
        //    "central" to the graph it is. We materialize it first,
        //    so that all the recoveries will be Puts of things into
        //    it (rather than Puts of the materialization into other
        //    objects), which means we will have a single
        //    StoreBarrier.


        // Compute dependencies between materializations
        HashMap<Node*, NodeSet> dependencies;
        HashMap<Node*, NodeSet> reverseDependencies;
        HashMap<Node*, NodeSet> forMaterialization;
        for (const auto& entry : escapees) {
            auto& myDependencies = dependencies.add(entry.key, NodeSet()).iterator->value;
            auto& myDependenciesForMaterialization = forMaterialization.add(entry.key, NodeSet()).iterator->value;
            reverseDependencies.add(entry.key, NodeSet());
            for (const auto& field : entry.value.fields()) {
                if (escapees.contains(field.value) && field.value != entry.key) {
                    myDependencies.addVoid(field.value);
                    reverseDependencies.add(field.value, NodeSet()).iterator->value.addVoid(entry.key);
                    if (field.key.neededForMaterialization())
                        myDependenciesForMaterialization.addVoid(field.value);
                }
            }
        }

        // Helper function to update the materialized set and the
        // dependencies
        NodeSet materialized;
        auto materialize = [&] (Node* identifier) {
            materialized.addVoid(identifier);
            for (Node* dep : dependencies.get(identifier))
                reverseDependencies.find(dep)->value.remove(identifier);
            for (Node* rdep : reverseDependencies.get(identifier)) {
                dependencies.find(rdep)->value.remove(identifier);
                forMaterialization.find(rdep)->value.remove(identifier);
            }
            dependencies.remove(identifier);
            reverseDependencies.remove(identifier);
            forMaterialization.remove(identifier);
        };

        // Nodes without remaining unmaterialized fields will be
        // materialized first - amongst the remaining unmaterialized
        // nodes
        Vector<Allocation> toMaterialize;
        toMaterialize.resize(escapees.size());
        size_t firstIndex = 0;
        size_t lastIndex = toMaterialize.size();
        auto materializeFirst = [&] (Allocation&& allocation) {
            RELEASE_ASSERT(firstIndex < lastIndex);
            materialize(allocation.identifier());
            toMaterialize[firstIndex] = WTFMove(allocation);
            ++firstIndex;
        };

        // Nodes that no other unmaterialized node points to will be
        // materialized last - amongst the remaining unmaterialized
        // nodes
        auto materializeLast = [&] (Allocation&& allocation) {
            materialize(allocation.identifier());
            RELEASE_ASSERT(firstIndex < lastIndex);
            RELEASE_ASSERT(lastIndex);
            --lastIndex;
            toMaterialize[lastIndex] = WTFMove(allocation);
        };

        // These are the promoted locations that contains some of the
        // allocations we are currently escaping. If they are a location on
        // some other allocation we are currently materializing, we will need
        // to "recover" their value with a real put once the corresponding
        // allocation is materialized; if they are a location on some other
        // not-yet-materialized allocation, we will need a PutHint.
        Vector<PromotedHeapLocation> toRecover;

        // This loop does the actual cycle breaking
        while (!escapees.isEmpty()) {
            materialized.clear();

            // Materialize nodes that won't require recoveries if we can
            for (auto& entry : escapees) {
                if (!forMaterialization.find(entry.key)->value.isEmpty())
                    continue;

                if (dependencies.find(entry.key)->value.isEmpty()) {
                    materializeFirst(WTFMove(entry.value));
                    continue;
                }

                if (reverseDependencies.find(entry.key)->value.isEmpty()) {
                    materializeLast(WTFMove(entry.value));
                    continue;
                }
            }

            // We reach this only if there is an actual cycle that needs
            // breaking. Because we do not want to solve a NP-hard problem
            // here, we just heuristically pick a node and materialize it
            // first.
            if (materialized.isEmpty()) {
                uint64_t maxEvaluation = 0;
                Allocation* bestAllocation = nullptr;
                for (auto& entry : escapees) {
                    if (!forMaterialization.find(entry.key)->value.isEmpty())
                        continue;

                    uint64_t evaluation =
                        static_cast<uint64_t>(dependencies.get(entry.key).size()) * reverseDependencies.get(entry.key).size();
                    if (evaluation > maxEvaluation) {
                        maxEvaluation = evaluation;
                        bestAllocation = &entry.value;
                    }
                }
                RELEASE_ASSERT(maxEvaluation > 0);

                materializeFirst(WTFMove(*bestAllocation));
            }
            RELEASE_ASSERT(!materialized.isEmpty());

            for (Node* identifier : materialized)
                escapees.remove(identifier);
        }

        RELEASE_ASSERT(firstIndex == lastIndex);

        materialized.clear();

        NodeSet escaped;
        for (const Allocation& allocation : toMaterialize)
            escaped.addVoid(allocation.identifier());
        for (const Allocation& allocation : toMaterialize) {
            for (const auto& field : allocation.fields()) {
                if (escaped.contains(field.value) && !materialized.contains(field.value))
                    toRecover.append(PromotedHeapLocation(allocation.identifier(), field.key));
            }
            materialized.addVoid(allocation.identifier());
        }

        Vector<Node*>& materializations = m_materializationSiteToMaterializations.add(
            where, Vector<Node*>()).iterator->value;

        for (const Allocation& allocation : toMaterialize) {
            Node* materialization = createMaterialization(allocation, where);
            materializations.append(materialization);
            m_materializationToEscapee.add(materialization, allocation.identifier());
        }

        if (!toRecover.isEmpty()) {
            m_materializationSiteToRecoveries.add(
                where, Vector<PromotedHeapLocation>()).iterator->value.appendVector(toRecover);
        }

        // The hints need to be after the "real" recoveries so that we
        // don't hint not-yet-complete objects
        m_materializationSiteToHints.add(
            where, Vector<std::pair<PromotedHeapLocation, Node*>>()).iterator->value.appendVector(hints);
    }

    Node* createMaterialization(const Allocation& allocation, Node* where)
    {
        // FIXME: This is the only place where we actually use the
        // fact that an allocation's identifier is indeed the node
        // that created the allocation.
        switch (allocation.kind()) {
        case Allocation::Kind::Object: {
            ObjectMaterializationData* data = m_graph.m_objectMaterializationData.add();

            return m_graph.addNode(
                allocation.identifier()->prediction(), Node::VarArg, MaterializeNewObject,
                where->origin.withSemantic(allocation.identifier()->origin.semantic),
                OpInfo(m_graph.addStructureSet(allocation.structuresForMaterialization())), OpInfo(data), 0, 0);
        }

        case Allocation::Kind::AsyncGeneratorFunction:
        case Allocation::Kind::AsyncFunction:
        case Allocation::Kind::GeneratorFunction:
        case Allocation::Kind::Function: {
            FrozenValue* executable = allocation.identifier()->cellOperand();
            
            NodeType nodeType;
            switch (allocation.kind()) {
            case Allocation::Kind::GeneratorFunction:
                nodeType = NewGeneratorFunction;
                break;
            case Allocation::Kind::AsyncGeneratorFunction:
                nodeType = NewAsyncGeneratorFunction;
                break;
            case Allocation::Kind::AsyncFunction:
                nodeType = NewAsyncFunction;
                break;
            default:
                nodeType = NewFunction;
            }

            return m_graph.addNode(
                allocation.identifier()->prediction(), nodeType,
                where->origin.withSemantic(
                    allocation.identifier()->origin.semantic),
                OpInfo(executable));
        }

        case Allocation::Kind::InternalFieldObject: {
            ObjectMaterializationData* data = m_graph.m_objectMaterializationData.add();
            return m_graph.addNode(
                allocation.identifier()->prediction(), Node::VarArg, MaterializeNewInternalFieldObject,
                where->origin.withSemantic(
                    allocation.identifier()->origin.semantic),
                OpInfo(allocation.identifier()->structure()), OpInfo(data), 0, 0);
        }

        case Allocation::Kind::Activation: {
            ObjectMaterializationData* data = m_graph.m_objectMaterializationData.add();
            FrozenValue* symbolTable = allocation.identifier()->cellOperand();

            return m_graph.addNode(
                allocation.identifier()->prediction(), Node::VarArg, MaterializeCreateActivation,
                where->origin.withSemantic(
                    allocation.identifier()->origin.semantic),
                OpInfo(symbolTable), OpInfo(data), 0, 0);
        }

        case Allocation::Kind::RegExpObject: {
            FrozenValue* regExp = allocation.identifier()->cellOperand();
            return m_graph.addNode(
                allocation.identifier()->prediction(), NewRegexp,
                where->origin.withSemantic(
                    allocation.identifier()->origin.semantic),
                OpInfo(regExp));
        }

        default:
            DFG_CRASH(m_graph, allocation.identifier(), "Bad allocation kind");
        }
    }

    void promoteLocalHeap()
    {
        // Collect the set of heap locations that we will be operating
        // over.
        HashSet<PromotedHeapLocation> locations;
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            m_heap = m_heapAtHead[block];

            for (Node* node : *block) {
                handleNode(
                    node,
                    [&] (PromotedHeapLocation location, LazyNode) {
                        // If the location is not on a sink candidate,
                        // we only sink it if it is read
                        if (m_sinkCandidates.contains(location.base()))
                            locations.addVoid(location);
                    },
                    [&] (PromotedHeapLocation location) -> Node* {
                        locations.addVoid(location);
                        return nullptr;
                    });
            }
        }

        // Figure out which locations belong to which allocations.
        m_locationsForAllocation.clear();
        for (PromotedHeapLocation location : locations) {
            auto result = m_locationsForAllocation.add(
                location.base(),
                Vector<PromotedHeapLocation>());
            ASSERT(!result.iterator->value.contains(location));
            result.iterator->value.append(location);
        }

        m_pointerSSA.reset();
        m_allocationSSA.reset();

        // Collect the set of "variables" that we will be sinking.
        m_locationToVariable.clear();
        m_nodeToVariable.clear();
        Vector<Node*> indexToNode;
        Vector<PromotedHeapLocation> indexToLocation;

        for (Node* index : m_sinkCandidates) {
            SSACalculator::Variable* variable = m_allocationSSA.newVariable();
            m_nodeToVariable.add(index, variable);
            ASSERT(indexToNode.size() == variable->index());
            indexToNode.append(index);
        }

        for (PromotedHeapLocation location : locations) {
            SSACalculator::Variable* variable = m_pointerSSA.newVariable();
            m_locationToVariable.add(location, variable);
            ASSERT(indexToLocation.size() == variable->index());
            indexToLocation.append(location);
        }

        // We insert all required constants at top of block 0 so that
        // they are inserted only once and we don't clutter the graph
        // with useless constants everywhere
        HashMap<FrozenValue*, Node*> lazyMapping;
        if (!m_bottom)
            m_bottom = m_insertionSet.insertConstant(0, m_graph.block(0)->at(0)->origin, jsNumber(1927));

        Vector<HashSet<PromotedHeapLocation>> hintsForPhi(m_sinkCandidates.size());

        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            m_heap = m_heapAtHead[block];

            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);

                // Some named properties can be added conditionally,
                // and that would necessitate bottoms
                for (PromotedHeapLocation location : m_locationsForAllocation.get(node)) {
                    if (location.kind() != NamedPropertyPLoc)
                        continue;

                    SSACalculator::Variable* variable = m_locationToVariable.get(location);
                    m_pointerSSA.newDef(variable, block, m_bottom);
                }

                for (Node* materialization : m_materializationSiteToMaterializations.get(node)) {
                    Node* escapee = m_materializationToEscapee.get(materialization);
                    m_allocationSSA.newDef(m_nodeToVariable.get(escapee), block, materialization);
                }

                for (std::pair<PromotedHeapLocation, Node*> pair : m_materializationSiteToHints.get(node)) {
                    PromotedHeapLocation location = pair.first;
                    Node* identifier = pair.second;
                    // We're materializing `identifier` at this point, and the unmaterialized
                    // version is inside `location`. We track which SSA variable this belongs
                    // to in case we also need a PutHint for the Phi.
                    if (UNLIKELY(validationEnabled())) {
                        RELEASE_ASSERT(m_sinkCandidates.contains(location.base()));
                        RELEASE_ASSERT(m_sinkCandidates.contains(identifier));

                        bool found = false;
                        for (Node* materialization : m_materializationSiteToMaterializations.get(node)) {
                            // We're materializing `identifier` here. This asserts that this is indeed the case.
                            if (m_materializationToEscapee.get(materialization) == identifier) {
                                found = true;
                                break;
                            }
                        }
                        RELEASE_ASSERT(found);
                    }

                    SSACalculator::Variable* variable = m_nodeToVariable.get(identifier);
                    hintsForPhi[variable->index()].addVoid(location);
                }

                if (m_sinkCandidates.contains(node))
                    m_allocationSSA.newDef(m_nodeToVariable.get(node), block, node);

                handleNode(
                    node,
                    [&] (PromotedHeapLocation location, LazyNode value) {
                        if (!locations.contains(location))
                            return;

                        Node* nodeValue;
                        if (value.isNode())
                            nodeValue = value.asNode();
                        else {
                            auto iter = lazyMapping.find(value.asValue());
                            if (iter != lazyMapping.end())
                                nodeValue = iter->value;
                            else {
                                nodeValue = value.ensureIsNode(
                                    m_insertionSet, m_graph.block(0), 0);
                                lazyMapping.add(value.asValue(), nodeValue);
                            }
                        }

                        SSACalculator::Variable* variable = m_locationToVariable.get(location);
                        m_pointerSSA.newDef(variable, block, nodeValue);
                    },
                    [] (PromotedHeapLocation) -> Node* {
                        return nullptr;
                    });
            }
        }
        m_insertionSet.execute(m_graph.block(0));

        // Run the SSA calculators to create Phis
        m_pointerSSA.computePhis(
            [&] (SSACalculator::Variable* variable, BasicBlock* block) -> Node* {
                PromotedHeapLocation location = indexToLocation[variable->index()];

                // Don't create Phi nodes for fields of dead allocations
                if (!m_heapAtHead[block].isAllocation(location.base()))
                    return nullptr;

                // Don't create Phi nodes once we are escaped
                if (m_heapAtHead[block].getAllocation(location.base()).isEscapedAllocation())
                    return nullptr;

                // If we point to a single allocation, we will
                // directly use its materialization
                if (m_heapAtHead[block].follow(location))
                    return nullptr;

                Node* phiNode = m_graph.addNode(SpecHeapTop, Phi, block->at(0)->origin.withInvalidExit());
                phiNode->mergeFlags(NodeResultJS);
                return phiNode;
            });

        m_allocationSSA.computePhis(
            [&] (SSACalculator::Variable* variable, BasicBlock* block) -> Node* {
                Node* identifier = indexToNode[variable->index()];

                // Don't create Phi nodes for dead allocations
                if (!m_heapAtHead[block].isAllocation(identifier))
                    return nullptr;

                // Don't create Phi nodes until we are escaped
                if (!m_heapAtHead[block].getAllocation(identifier).isEscapedAllocation())
                    return nullptr;

                Node* phiNode = m_graph.addNode(SpecHeapTop, Phi, block->at(0)->origin.withInvalidExit());
                phiNode->mergeFlags(NodeResultJS);
                return phiNode;
            });

        // Place Phis in the right places, replace all uses of any load with the appropriate
        // value, and create the materialization nodes.
        LocalOSRAvailabilityCalculator availabilityCalculator(m_graph);
        m_graph.clearReplacements();
        for (BasicBlock* block : m_graph.blocksInPreOrder()) {
            m_heap = m_heapAtHead[block];
            availabilityCalculator.beginBlock(block);

            // These mapping tables are intended to be lazy. If
            // something is omitted from the table, it means that
            // there haven't been any local stores to the promoted
            // heap location (or any local materialization).
            m_localMapping.clear();
            m_escapeeToMaterialization.clear();

            // Insert the Phi functions that we had previously
            // created.
            for (SSACalculator::Def* phiDef : m_pointerSSA.phisForBlock(block)) {
                SSACalculator::Variable* variable = phiDef->variable();
                m_insertionSet.insert(0, phiDef->value());

                PromotedHeapLocation location = indexToLocation[variable->index()];
                m_localMapping.set(location, phiDef->value());

                if (m_sinkCandidates.contains(location.base())) {
                    m_insertionSet.insert(
                        0,
                        location.createHint(
                            m_graph, block->at(0)->origin.withInvalidExit(), phiDef->value()));
                }
            }

            for (SSACalculator::Def* phiDef : m_allocationSSA.phisForBlock(block)) {
                SSACalculator::Variable* variable = phiDef->variable();
                m_insertionSet.insert(0, phiDef->value());

                Node* identifier = indexToNode[variable->index()];
                m_escapeeToMaterialization.add(identifier, phiDef->value());
                bool canExit = false;
                insertOSRHintsForUpdate(
                    0, block->at(0)->origin, canExit,
                    availabilityCalculator.m_availability, identifier, phiDef->value());

                for (PromotedHeapLocation location : hintsForPhi[variable->index()]) {
                    if (m_heap.isUnescapedAllocation(location.base())) {
                        m_insertionSet.insert(0,
                            location.createHint(m_graph, block->at(0)->origin.withInvalidExit(), phiDef->value()));
                        m_localMapping.set(location, phiDef->value());
                    }
                }
            }

            if (DFGObjectAllocationSinkingPhaseInternal::verbose) {
                dataLog("Local mapping at ", pointerDump(block), ": ", mapDump(m_localMapping), "\n");
                dataLog("Local materializations at ", pointerDump(block), ": ", mapDump(m_escapeeToMaterialization), "\n");
            }

            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                bool canExit = true;
                bool nextCanExit = node->origin.exitOK;
                for (PromotedHeapLocation location : m_locationsForAllocation.get(node)) {
                    if (location.kind() != NamedPropertyPLoc)
                        continue;

                    m_localMapping.set(location, m_bottom);

                    if (m_sinkCandidates.contains(node)) {
                        if (DFGObjectAllocationSinkingPhaseInternal::verbose)
                            dataLog("For sink candidate ", node, " found location ", location, "\n");
                        m_insertionSet.insert(
                            nodeIndex + 1,
                            location.createHint(
                                m_graph, node->origin.takeValidExit(nextCanExit), m_bottom));
                    }
                }

                for (Node* materialization : m_materializationSiteToMaterializations.get(node)) {
                    materialization->origin.exitOK &= canExit;
                    Node* escapee = m_materializationToEscapee.get(materialization);
                    populateMaterialization(block, materialization, escapee);
                    m_escapeeToMaterialization.set(escapee, materialization);
                    m_insertionSet.insert(nodeIndex, materialization);
                    if (DFGObjectAllocationSinkingPhaseInternal::verbose)
                        dataLog("Materializing ", escapee, " => ", materialization, " at ", node, "\n");
                }

                for (PromotedHeapLocation location : m_materializationSiteToRecoveries.get(node))
                    m_insertionSet.insert(nodeIndex, createRecovery(block, location, node, canExit));
                for (std::pair<PromotedHeapLocation, Node*> pair : m_materializationSiteToHints.get(node))
                    m_insertionSet.insert(nodeIndex, createRecovery(block, pair.first, node, canExit));

                // We need to put the OSR hints after the recoveries,
                // because we only want the hints once the object is
                // complete
                for (Node* materialization : m_materializationSiteToMaterializations.get(node)) {
                    Node* escapee = m_materializationToEscapee.get(materialization);
                    insertOSRHintsForUpdate(
                        nodeIndex, node->origin, canExit,
                        availabilityCalculator.m_availability, escapee, materialization);
                }

                if (node->origin.exitOK && !canExit) {
                    // We indicate that the exit state is fine now. It is OK because we updated the
                    // state above. We need to indicate this manually because the validation doesn't
                    // have enough information to infer that the exit state is fine.
                    m_insertionSet.insertNode(nodeIndex, SpecNone, ExitOK, node->origin);
                }

                if (m_sinkCandidates.contains(node))
                    m_escapeeToMaterialization.set(node, node);

                availabilityCalculator.executeNode(node);

                bool desiredNextExitOK = node->origin.exitOK && !clobbersExitState(m_graph, node);

                bool doLower = false;
                handleNode(
                    node,
                    [&] (PromotedHeapLocation location, LazyNode value) {
                        if (!locations.contains(location))
                            return;

                        Node* nodeValue;
                        if (value.isNode())
                            nodeValue = value.asNode();
                        else
                            nodeValue = lazyMapping.get(value.asValue());

                        nodeValue = resolve(block, nodeValue);

                        m_localMapping.set(location, nodeValue);

                        if (!m_sinkCandidates.contains(location.base()))
                            return;

                        doLower = true;

                        if (DFGObjectAllocationSinkingPhaseInternal::verbose)
                            dataLog("Creating hint with value ", nodeValue, " before ", node, "\n");
                        m_insertionSet.insert(
                            nodeIndex + 1,
                            location.createHint(
                                m_graph, node->origin.takeValidExit(nextCanExit), nodeValue));
                    },
                    [&] (PromotedHeapLocation location) -> Node* {
                        return resolve(block, location);
                    });

                if (!nextCanExit && desiredNextExitOK) {
                    // We indicate that the exit state is fine now. We need to do this because we
                    // emitted hints that appear to invalidate the exit state.
                    m_insertionSet.insertNode(nodeIndex + 1, SpecNone, ExitOK, node->origin);
                }

                if (m_sinkCandidates.contains(node) || doLower) {
                    switch (node->op()) {
                    case NewObject:
                        node->convertToPhantomNewObject();
                        break;

                    case NewFunction:
                        node->convertToPhantomNewFunction();
                        break;

                    case NewGeneratorFunction:
                        node->convertToPhantomNewGeneratorFunction();
                        break;

                    case NewAsyncGeneratorFunction:
                        node->convertToPhantomNewAsyncGeneratorFunction();
                        break;

                    case NewAsyncFunction:
                        node->convertToPhantomNewAsyncFunction();
                        break;

                    case NewInternalFieldObject:
                        node->convertToPhantomNewInternalFieldObject();
                        break;

                    case CreateActivation:
                        node->convertToPhantomCreateActivation();
                        break;

                    case NewRegexp:
                        node->convertToPhantomNewRegexp();
                        break;

                    default:
                        node->remove(m_graph);
                        break;
                    }
                }

                m_graph.doToChildren(
                    node,
                    [&] (Edge& edge) {
                        edge.setNode(resolve(block, edge.node()));
                    });
            }

            // Gotta drop some Upsilons.
            NodeAndIndex terminal = block->findTerminal();
            size_t upsilonInsertionPoint = terminal.index;
            NodeOrigin upsilonOrigin = terminal.node->origin;
            for (BasicBlock* successorBlock : block->successors()) {
                for (SSACalculator::Def* phiDef : m_pointerSSA.phisForBlock(successorBlock)) {
                    Node* phiNode = phiDef->value();
                    SSACalculator::Variable* variable = phiDef->variable();
                    PromotedHeapLocation location = indexToLocation[variable->index()];
                    Node* incoming = resolve(block, location);

                    m_insertionSet.insertNode(
                        upsilonInsertionPoint, SpecNone, Upsilon, upsilonOrigin,
                        OpInfo(phiNode), incoming->defaultEdge());
                }

                for (SSACalculator::Def* phiDef : m_allocationSSA.phisForBlock(successorBlock)) {
                    Node* phiNode = phiDef->value();
                    SSACalculator::Variable* variable = phiDef->variable();
                    Node* incoming = getMaterialization(block, indexToNode[variable->index()]);

                    m_insertionSet.insertNode(
                        upsilonInsertionPoint, SpecNone, Upsilon, upsilonOrigin,
                        OpInfo(phiNode), incoming->defaultEdge());
                }
            }

            m_insertionSet.execute(block);
        }
    }

    NEVER_INLINE Node* resolve(BasicBlock* block, PromotedHeapLocation location)
    {
        // If we are currently pointing to a single local allocation,
        // simply return the associated materialization.
        if (Node* identifier = m_heap.follow(location))
            return getMaterialization(block, identifier);

        if (Node* result = m_localMapping.get(location))
            return result;

        // This implies that there is no local mapping. Find a non-local mapping.
        SSACalculator::Def* def = m_pointerSSA.nonLocalReachingDef(
            block, m_locationToVariable.get(location));
        ASSERT(def);
        ASSERT(def->value());

        Node* result = def->value();
        if (result->replacement())
            result = result->replacement();
        ASSERT(!result->replacement());

        m_localMapping.add(location, result);
        return result;
    }

    NEVER_INLINE Node* resolve(BasicBlock* block, Node* node)
    {
        // If we are currently pointing to a single local allocation,
        // simply return the associated materialization.
        if (Node* identifier = m_heap.follow(node))
            return getMaterialization(block, identifier);

        if (node->replacement())
            node = node->replacement();
        ASSERT(!node->replacement());

        return node;
    }

    NEVER_INLINE Node* getMaterialization(BasicBlock* block, Node* identifier)
    {
        ASSERT(m_heap.isAllocation(identifier));
        if (!m_sinkCandidates.contains(identifier))
            return identifier;

        if (Node* materialization = m_escapeeToMaterialization.get(identifier))
            return materialization;

        SSACalculator::Def* def = m_allocationSSA.nonLocalReachingDef(
            block, m_nodeToVariable.get(identifier));
        ASSERT(def && def->value());
        m_escapeeToMaterialization.add(identifier, def->value());
        ASSERT(!def->value()->replacement());
        return def->value();
    }

    void insertOSRHintsForUpdate(unsigned nodeIndex, NodeOrigin origin, bool& canExit, AvailabilityMap& availability, Node* escapee, Node* materialization)
    {
        if (DFGObjectAllocationSinkingPhaseInternal::verbose) {
            dataLog("Inserting OSR hints at ", origin, ":\n");
            dataLog("    Escapee: ", escapee, "\n");
            dataLog("    Materialization: ", materialization, "\n");
            dataLog("    Availability: ", availability, "\n");
        }
        
        // We need to follow() the value in the heap.
        // Consider the following graph:
        //
        // Block #0
        //   0: NewObject({})
        //   1: NewObject({})
        //   -: PutByOffset(@0, @1, x:0)
        //   -: PutStructure(@0, {x:0})
        //   2: GetByOffset(@0, x:0)
        //   -: MovHint(@2, loc1)
        //   -: Branch(#1, #2)
        //
        // Block #1
        //   3: Call(f, @1)
        //   4: Return(@0)
        //
        // Block #2
        //   -: Return(undefined)
        //
        // We need to materialize @1 at @3, and when doing so we need
        // to insert a MovHint for the materialization into loc1 as
        // well.
        // In order to do this, we say that we need to insert an
        // update hint for any availability whose node resolve()s to
        // the materialization.
        for (auto entry : availability.m_heap) {
            if (!entry.value.hasNode())
                continue;
            if (m_heap.follow(entry.value.node()) != escapee)
                continue;

            m_insertionSet.insert(
                nodeIndex,
                entry.key.createHint(m_graph, origin.takeValidExit(canExit), materialization));
        }

        for (unsigned i = availability.m_locals.size(); i--;) {
            if (!availability.m_locals[i].hasNode())
                continue;
            if (m_heap.follow(availability.m_locals[i].node()) != escapee)
                continue;

            Operand operand = availability.m_locals.operandForIndex(i);
            m_insertionSet.insertNode(
                nodeIndex, SpecNone, MovHint, origin.takeValidExit(canExit), OpInfo(operand),
                materialization->defaultEdge());
        }
    }

    void populateMaterialization(BasicBlock* block, Node* node, Node* escapee)
    {
        Allocation& allocation = m_heap.getAllocation(escapee);
        switch (node->op()) {
        case MaterializeNewObject: {
            ObjectMaterializationData& data = node->objectMaterializationData();
            unsigned firstChild = m_graph.m_varArgChildren.size();

            Vector<PromotedHeapLocation> locations = m_locationsForAllocation.get(escapee);

            PromotedHeapLocation structure(StructurePLoc, allocation.identifier());
            ASSERT(locations.contains(structure));

            m_graph.m_varArgChildren.append(Edge(resolve(block, structure), KnownCellUse));

            for (PromotedHeapLocation location : locations) {
                switch (location.kind()) {
                case StructurePLoc:
                    ASSERT(location == structure);
                    break;

                case NamedPropertyPLoc: {
                    ASSERT(location.base() == allocation.identifier());
                    data.m_properties.append(location.descriptor());
                    Node* value = resolve(block, location);
                    if (m_sinkCandidates.contains(value))
                        m_graph.m_varArgChildren.append(m_bottom);
                    else
                        m_graph.m_varArgChildren.append(value);
                    break;
                }

                default:
                    DFG_CRASH(m_graph, node, "Bad location kind");
                }
            }

            node->children = AdjacencyList(
                AdjacencyList::Variable,
                firstChild, m_graph.m_varArgChildren.size() - firstChild);
            break;
        }

        case MaterializeCreateActivation: {
            ObjectMaterializationData& data = node->objectMaterializationData();

            unsigned firstChild = m_graph.m_varArgChildren.size();

            Vector<PromotedHeapLocation> locations = m_locationsForAllocation.get(escapee);

            PromotedHeapLocation symbolTable(ActivationSymbolTablePLoc, allocation.identifier());
            ASSERT(locations.contains(symbolTable));
            ASSERT(node->cellOperand() == resolve(block, symbolTable)->constant());
            m_graph.m_varArgChildren.append(Edge(resolve(block, symbolTable), KnownCellUse));

            PromotedHeapLocation scope(ActivationScopePLoc, allocation.identifier());
            ASSERT(locations.contains(scope));
            m_graph.m_varArgChildren.append(Edge(resolve(block, scope), KnownCellUse));

            for (PromotedHeapLocation location : locations) {
                switch (location.kind()) {
                case ActivationScopePLoc: {
                    ASSERT(location == scope);
                    break;
                }

                case ActivationSymbolTablePLoc: {
                    ASSERT(location == symbolTable);
                    break;
                }

                case ClosureVarPLoc: {
                    ASSERT(location.base() == allocation.identifier());
                    data.m_properties.append(location.descriptor());
                    Node* value = resolve(block, location);
                    if (m_sinkCandidates.contains(value))
                        m_graph.m_varArgChildren.append(m_bottom);
                    else
                        m_graph.m_varArgChildren.append(value);
                    break;
                }

                default:
                    DFG_CRASH(m_graph, node, "Bad location kind");
                }
            }

            node->children = AdjacencyList(
                AdjacencyList::Variable,
                firstChild, m_graph.m_varArgChildren.size() - firstChild);
            break;
        }
        
        case NewFunction:
        case NewGeneratorFunction:
        case NewAsyncGeneratorFunction:
        case NewAsyncFunction: {
            Vector<PromotedHeapLocation> locations = m_locationsForAllocation.get(escapee);
            ASSERT(locations.size() == 2);
                
            PromotedHeapLocation executable(FunctionExecutablePLoc, allocation.identifier());
            ASSERT_UNUSED(executable, locations.contains(executable));
                
            PromotedHeapLocation activation(FunctionActivationPLoc, allocation.identifier());
            ASSERT(locations.contains(activation));

            node->child1() = Edge(resolve(block, activation), KnownCellUse);
            break;
        }

        case MaterializeNewInternalFieldObject: {
            ObjectMaterializationData& data = node->objectMaterializationData();

            unsigned firstChild = m_graph.m_varArgChildren.size();

            Vector<PromotedHeapLocation> locations = m_locationsForAllocation.get(escapee);

            PromotedHeapLocation structure(StructurePLoc, allocation.identifier());
            ASSERT(locations.contains(structure));
            m_graph.m_varArgChildren.append(Edge(resolve(block, structure), KnownCellUse));

            for (PromotedHeapLocation location : locations) {
                switch (location.kind()) {
                case StructurePLoc: {
                    ASSERT(location == structure);
                    break;
                }

                case InternalFieldObjectPLoc: {
                    ASSERT(location.base() == allocation.identifier());
                    data.m_properties.append(location.descriptor());
                    Node* value = resolve(block, location);
                    if (m_sinkCandidates.contains(value))
                        m_graph.m_varArgChildren.append(m_bottom);
                    else
                        m_graph.m_varArgChildren.append(value);
                    break;
                }

                default:
                    DFG_CRASH(m_graph, node, "Bad location kind");
                }
            }

            node->children = AdjacencyList(
                AdjacencyList::Variable,
                firstChild, m_graph.m_varArgChildren.size() - firstChild);
            break;
        }

        case NewRegexp: {
            Vector<PromotedHeapLocation> locations = m_locationsForAllocation.get(escapee);
            ASSERT(locations.size() == 2);

            PromotedHeapLocation regExp(RegExpObjectRegExpPLoc, allocation.identifier());
            ASSERT_UNUSED(regExp, locations.contains(regExp));

            PromotedHeapLocation lastIndex(RegExpObjectLastIndexPLoc, allocation.identifier());
            ASSERT(locations.contains(lastIndex));
            Node* value = resolve(block, lastIndex);
            if (m_sinkCandidates.contains(value))
                node->child1() = Edge(m_bottom);
            else
                node->child1() = Edge(value);
            break;
        }

        default:
            DFG_CRASH(m_graph, node, "Bad materialize op");
        }
    }

    Node* createRecovery(BasicBlock* block, PromotedHeapLocation location, Node* where, bool& canExit)
    {
        if (DFGObjectAllocationSinkingPhaseInternal::verbose)
            dataLog("Recovering ", location, " at ", where, "\n");
        ASSERT(location.base()->isPhantomAllocation());
        Node* base = getMaterialization(block, location.base());
        Node* value = resolve(block, location);

        NodeOrigin origin = where->origin.withSemantic(base->origin.semantic);

        if (DFGObjectAllocationSinkingPhaseInternal::verbose)
            dataLog("Base is ", base, " and value is ", value, "\n");

        if (base->isPhantomAllocation()) {
            return PromotedHeapLocation(base, location.descriptor()).createHint(
                m_graph, origin.takeValidExit(canExit), value);
        }

        switch (location.kind()) {
        case NamedPropertyPLoc: {
            Allocation& allocation = m_heap.getAllocation(location.base());

            unsigned identifierNumber = location.info();
            UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];

            Vector<RegisteredStructure> structures;
            for (RegisteredStructure structure : allocation.structuresForMaterialization()) {
                // This structure set is conservative. This set can include Structure which does not have a legit property.
                // We filter out such an apparently inappropriate structures here since MultiPutByOffset assumes all the structures
                // have valid corresponding offset for the given property.
                if (structure->getConcurrently(uid) != invalidOffset)
                    structures.append(structure);
            }

            // Since we filter structures, it is possible that we no longer have any structures here. In this case, we emit ForceOSRExit.
            if (structures.isEmpty())
                return m_graph.addNode(ForceOSRExit, origin.takeValidExit(canExit));

            std::sort(
                structures.begin(),
                structures.end(),
                [uid] (RegisteredStructure a, RegisteredStructure b) -> bool {
                    return a->getConcurrently(uid) < b->getConcurrently(uid);
                });

            RELEASE_ASSERT(structures.size());
            PropertyOffset firstOffset = structures[0]->getConcurrently(uid);

            if (firstOffset == structures.last()->getConcurrently(uid)) {
                Node* storage = base;
                // FIXME: When we decide to sink objects with a
                // property storage, we should handle non-inline offsets.
                RELEASE_ASSERT(isInlineOffset(firstOffset));

                StorageAccessData* data = m_graph.m_storageAccessData.add();
                data->offset = firstOffset;
                data->identifierNumber = identifierNumber;

                return m_graph.addNode(
                    PutByOffset,
                    origin.takeValidExit(canExit),
                    OpInfo(data),
                    Edge(storage, KnownCellUse),
                    Edge(base, KnownCellUse),
                    value->defaultEdge());
            }

            MultiPutByOffsetData* data = m_graph.m_multiPutByOffsetData.add();
            data->identifierNumber = identifierNumber;

            {
                PropertyOffset currentOffset = firstOffset;
                StructureSet currentSet;
                for (RegisteredStructure structure : structures) {
                    PropertyOffset offset = structure->getConcurrently(uid);
                    if (offset != currentOffset) {
                        // Because our analysis treats MultiPutByOffset like an escape, we only have to
                        // deal with storing results that would have been previously stored by PutByOffset
                        // nodes. Those nodes were guarded by the appropriate type checks. This means that
                        // at this point, we can simply trust that the incoming value has the right type
                        // for whatever structure we are using.
                        data->variants.append(PutByVariant::replace(nullptr, currentSet, currentOffset));
                        currentOffset = offset;
                        currentSet.clear();
                    }
                    currentSet.add(structure.get());
                }
                data->variants.append(PutByVariant::replace(nullptr, currentSet, currentOffset));
            }

            return m_graph.addNode(
                MultiPutByOffset,
                origin.takeValidExit(canExit),
                OpInfo(data),
                Edge(base, KnownCellUse),
                value->defaultEdge());
        }

        case ClosureVarPLoc: {
            return m_graph.addNode(
                PutClosureVar,
                origin.takeValidExit(canExit),
                OpInfo(location.info()),
                Edge(base, KnownCellUse),
                value->defaultEdge());
        }

        case InternalFieldObjectPLoc: {
            return m_graph.addNode(
                PutInternalField,
                origin.takeValidExit(canExit),
                OpInfo(location.info()),
                Edge(base, KnownCellUse),
                value->defaultEdge());
        }

        case RegExpObjectLastIndexPLoc: {
            return m_graph.addNode(
                SetRegExpObjectLastIndex,
                origin.takeValidExit(canExit),
                OpInfo(true),
                Edge(base, KnownCellUse),
                value->defaultEdge());
        }

        default:
            DFG_CRASH(m_graph, base, "Bad location kind");
            break;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void removeICStatusFilters()
    {
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block) {
                switch (node->op()) {
                case FilterCallLinkStatus:
                case FilterGetByStatus:
                case FilterPutByStatus:
                case FilterInByStatus:
                case FilterDeleteByStatus:
                case FilterCheckPrivateBrandStatus:
                case FilterSetPrivateBrandStatus:
                    if (node->child1()->isPhantomAllocation())
                        node->removeWithoutChecks();
                    break;
                default:
                    break;
                }
            }
        }
    }

    // This is a great way of asking value->isStillValid() without having to worry about getting
    // different answers. It turns out that this analysis works OK regardless of what this
    // returns but breaks badly if this changes its mind for any particular InferredValue. This
    // method protects us from that.
    bool isStillValid(SymbolTable* value)
    {
        return m_validInferredValues.add(value, value->singleton().isStillValid()).iterator->value;
    }

    bool isStillValid(FunctionExecutable* value)
    {
        return m_validInferredValues.add(value, value->singleton().isStillValid()).iterator->value;
    }


    SSACalculator m_pointerSSA;
    SSACalculator m_allocationSSA;
    NodeSet m_sinkCandidates;
    HashMap<PromotedHeapLocation, SSACalculator::Variable*> m_locationToVariable;
    HashMap<Node*, SSACalculator::Variable*> m_nodeToVariable;
    HashMap<PromotedHeapLocation, Node*> m_localMapping;
    HashMap<Node*, Node*> m_escapeeToMaterialization;
    InsertionSet m_insertionSet;
    CombinedLiveness m_combinedLiveness;

    HashMap<JSCell*, bool> m_validInferredValues;

    HashMap<Node*, Node*> m_materializationToEscapee;
    HashMap<Node*, Vector<Node*>> m_materializationSiteToMaterializations;
    HashMap<Node*, Vector<PromotedHeapLocation>> m_materializationSiteToRecoveries;
    HashMap<Node*, Vector<std::pair<PromotedHeapLocation, Node*>>> m_materializationSiteToHints;

    HashMap<Node*, Vector<PromotedHeapLocation>> m_locationsForAllocation;

    BlockMap<LocalHeap> m_heapAtHead;
    BlockMap<LocalHeap> m_heapAtTail;
    LocalHeap m_heap;

    Node* m_bottom = nullptr;
};

}

bool performObjectAllocationSinking(Graph& graph)
{
    return runPhase<ObjectAllocationSinkingPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
