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
#include "DFGValueRepReductionPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGPhiChildren.h"
#include "JSCJSValueInlines.h"

namespace JSC { namespace DFG {

class ValueRepReductionPhase : public Phase {
    static constexpr bool verbose = false;
    
public:
    ValueRepReductionPhase(Graph& graph)
        : Phase(graph, "ValueRep reduction"_s)
        , m_insertionSet(graph)
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == SSA);
        bool changed = false;
        changed |= convertValueRepsToUnboxed<DoubleRepUse>();
        changed |= convertValueRepsToUnboxed<Int52RepUse>();
        changed |= convertValueRepsToUnboxed<Int32Use>();
        return changed;
    }

private:
    template<UseKind useKind>
    bool convertValueRepsToUnboxed()
    {
        UncheckedKeyHashSet<Node*> candidates;
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block) {
                switch (node->op()) {
                case ValueRep: {
                    if constexpr (useKind != DoubleRepUse && useKind != Int52RepUse)
                        break;

                    if (node->child1().useKind() == useKind)
                        candidates.add(node);
                    break;
                }

                case DoubleRep: {
                    if constexpr (useKind != DoubleRepUse)
                        break;

                    switch (node->child1()->op()) {
                    case GetClosureVar:
                    case GetGlobalVar:
                    case GetGlobalLexicalVariable:
                    case MultiGetByOffset:
                    case GetByOffset: {
                        if (node->child1().useKind() == RealNumberUse || node->child1().useKind() == NumberUse) {
                            if (node->child1()->origin.exitOK)
                                candidates.add(node->child1().node());
                            break;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                    break;
                }

                case Int52Rep: {
                    if constexpr (useKind != Int52RepUse)
                        break;

                    Edge& child1 = node->child1();
                    switch (child1->op()) {
                    case MultiGetByVal: {
                        if (child1->arrayMode().isOutOfBounds())
                            break;

                        if (child1->arrayMode().isInBoundsSaneChain())
                            break;

                        if (m_graph.hasExitSite(child1->origin.semantic, Int52Overflow))
                            break;

                        constexpr ArrayModes supportedArrays = 0
                            | asArrayModesIgnoringTypedArrays(ArrayWithInt32)
                            | asArrayModesIgnoringTypedArrays(ArrayWithDouble)
                            | Int8ArrayMode
                            | Int16ArrayMode
                            | Int32ArrayMode
                            | Uint8ArrayMode
                            | Uint8ClampedArrayMode
                            | Uint16ArrayMode
                            | Uint32ArrayMode
                            | 0;

                        if (child1->arrayModes() & ~supportedArrays)
                            break;

                        if (child1.useKind() == AnyIntUse || child1.useKind() == RealNumberUse) {
                            if (child1->origin.exitOK)
                                candidates.add(child1.node());
                            break;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                    break;
                }

                case ValueToInt32: {
                    if constexpr (useKind != Int32Use)
                        break;

                    Edge& child1 = node->child1();
                    switch (child1->op()) {
                    case MultiGetByVal: {
                        if (child1->arrayMode().isOutOfBounds())
                            break;

                        if (child1->arrayMode().isInBoundsSaneChain())
                            break;

                        if (!bytecodeCanTruncateInteger(child1->arithNodeFlags()))
                            break;

                        if (!bytecodeCanIgnoreNaNAndInfinity(child1->arithNodeFlags()))
                            break;

                        if (!bytecodeCanIgnoreNegativeZero(child1->arithNodeFlags()))
                            break;

                        constexpr ArrayModes supportedArrays = 0
                            | asArrayModesIgnoringTypedArrays(ArrayWithInt32)
                            | asArrayModesIgnoringTypedArrays(ArrayWithDouble)
                            | Int8ArrayMode
                            | Int16ArrayMode
                            | Int32ArrayMode
                            | Uint8ArrayMode
                            | Uint8ClampedArrayMode
                            | Uint16ArrayMode
                            | Uint32ArrayMode
                            | 0;

                        if (child1->arrayModes() & ~supportedArrays)
                            break;

                        if (child1->origin.exitOK)
                            candidates.add(child1.node());
                        break;
                    }
                    default:
                        break;
                    }
                    break;
                }

                default:
                    break;
                }
            }
        }

        if (candidates.isEmpty())
            return false;

        UncheckedKeyHashMap<Node*, Vector<Node*>> usersOf;
        auto getUsersOf = [&] (Node* candidate) {
            auto iter = usersOf.find(candidate);
            RELEASE_ASSERT(iter != usersOf.end());
            return iter->value;
        };

        for (BasicBlock* block : m_graph.blocksInPreOrder()) {
            for (Node* node : *block) {
                switch (node->op()) {
                case Phi: {
                    usersOf.add(node, Vector<Node*>());
                    break;
                }

                case GetClosureVar:
                case GetGlobalVar:
                case GetGlobalLexicalVariable:
                case MultiGetByOffset:
                case GetByOffset: {
                    if constexpr (useKind != DoubleRepUse)
                        break;

                    if (candidates.contains(node))
                        usersOf.add(node, Vector<Node*>());
                    break;
                }

                case MultiGetByVal: {
                    if constexpr (useKind != Int52RepUse && useKind != Int32Use)
                        break;

                    if (candidates.contains(node))
                        usersOf.add(node, Vector<Node*>());
                    break;
                }

                case ValueRep: {
                    if constexpr (useKind != DoubleRepUse && useKind != Int52RepUse)
                        break;

                    if (candidates.contains(node))
                        usersOf.add(node, Vector<Node*>());
                    break;
                }

                default:
                    break;
                }

                Vector<Node*, 3> alreadyAdded;
                m_graph.doToChildren(node, [&] (Edge edge) {
                    Node* candidate = edge.node();
                    if (alreadyAdded.contains(candidate))
                        return;
                    auto iter = usersOf.find(candidate);
                    if (iter == usersOf.end())
                        return;
                    iter->value.append(node);
                    alreadyAdded.append(candidate);
                });
            }
        }

        PhiChildren phiChildren(m_graph);

        // - Any candidate that forwards into a Phi turns that Phi into a candidate.
        // - Any Phi-1 that forwards into another Phi-2, where Phi-2 is a candidate,
        //   makes Phi-1 a candidate too.
        do {
            UncheckedKeyHashSet<Node*> eligiblePhis;
            for (auto* candidate : candidates) {
                if (candidate->op() == Phi) {
                    phiChildren.forAllIncomingValues(candidate, [&] (Node* incoming) {
                        if (incoming->op() == Phi)
                            eligiblePhis.add(incoming);
                    });
                }

                for (Node* user : getUsersOf(candidate)) {
                    if (user->op() == Upsilon)
                        eligiblePhis.add(user->phi());
                }
            }

            bool sawNewCandidate = false;
            for (Node* phi : eligiblePhis)
                sawNewCandidate |= candidates.add(phi).isNewEntry;

            if (!sawNewCandidate)
                break;
        } while (true);

        do {
            UncheckedKeyHashSet<Node*> toRemove;

            auto isEscaped = [&] (Node* node) {
                return !candidates.contains(node) || toRemove.contains(node);
            };

            // Escape rules are as follows:
            // - Any non-well-known use is an escape. Currently, we allow DoubleRep, Hints,
            //   Upsilons, PutByOffset, MultiPutByOffset, PutClosureVar, PutGlobalVariable (described below).
            // - Any Upsilon that forwards the candidate into an escaped phi escapes the candidate.
            // - A Phi remains a candidate as long as all values flowing into it can be made a double.
            //   Currently, this means these are valid things we support to forward into the Phi:
            //    - A JSConstant(some number "x") => DoubleConstant("x")
            //    - ValueRep(DoubleRepUse:@x) => @x
            //    - A Phi so long as that Phi is not escaped.
            for (auto* candidate : candidates) {
                bool ok = true;

                auto dumpEscape = [&](const char* description, Node* node) {
                    if constexpr (!verbose)
                        return;
                    WTF::dataFile().atomically([&](auto&) {
                        dataLogLn(description);
                        dataLog("   candidate: ");
                        m_graph.dump(WTF::dataFile(), Prefix::noString, candidate);
                        dataLog("   reason: ");
                        m_graph.dump(WTF::dataFile(), Prefix::noString, node);
                        dataLogLn();
                    });
                };

                if (candidate->op() == Phi) {
                    phiChildren.forAllIncomingValues(candidate, [&] (Node* node) {
                        switch (node->op()) {
                        case JSConstant: {
                            if constexpr (useKind == DoubleRepUse) {
                                if (!node->asJSValue().isNumber()) {
                                    ok = false;
                                    dumpEscape("Phi Incoming JSConstant not a number: ", node);
                                }
                            }
                            if constexpr (useKind == Int52RepUse) {
                                if (!node->asJSValue().isAnyInt()) {
                                    ok = false;
                                    dumpEscape("Phi Incoming JSConstant not a anyint: ", node);
                                }
                            }
                            if constexpr (useKind == Int32Use) {
                                if (!node->asJSValue().isInt32AsAnyInt()) {
                                    ok = false;
                                    dumpEscape("Phi Incoming JSConstant not a int32: ", node);
                                }
                            }
                            break;
                        }

                        case GetClosureVar:
                        case GetGlobalVar:
                        case GetGlobalLexicalVariable:
                        case MultiGetByOffset:
                        case GetByOffset: {
                            if constexpr (useKind != DoubleRepUse) {
                                ok = false;
                                dumpEscape("Phi Incoming Get is escaped: ", node);
                                break;
                            }

                            if (isEscaped(node)) {
                                ok = false;
                                dumpEscape("Phi Incoming Get is escaped: ", node);
                                break;
                            }
                            break;
                        }

                        case MultiGetByVal: {
                            if constexpr (useKind != Int52RepUse && useKind != Int32Use) {
                                ok = false;
                                dumpEscape("Phi Incoming Get is escaped: ", node);
                                break;
                            }

                            if (isEscaped(node)) {
                                ok = false;
                                dumpEscape("Phi Incoming Get is escaped: ", node);
                                break;
                            }
                            break;
                        }

                        case ValueRep: {
                            if constexpr (useKind != DoubleRepUse && useKind != Int52RepUse) {
                                ok = false;
                                dumpEscape("Phi Incoming ValueRep not DoubleRepUse / Int52RepUse: ", node);
                                break;
                            }

                            if (node->child1().useKind() != useKind) {
                                ok = false;
                                dumpEscape("Phi Incoming ValueRep not DoubleRepUse / Int52RepUse: ", node);
                            }
                            break;
                        }

                        case Phi: {
                            if (isEscaped(node)) {
                                ok = false;
                                dumpEscape("An incoming Phi to another Phi is escaped: ", node);
                            }
                            break;
                        }

                        default: {
                            ok = false;
                            dumpEscape("Unsupported incoming value to Phi: ", node);
                            break;
                        }
                        }
                    });

                    if (!ok) {
                        toRemove.add(candidate);
                        continue;
                    }
                }

                for (Node* user : getUsersOf(candidate)) {
                    switch (user->op()) {
                    case DoubleRep: {
                        if constexpr (useKind != DoubleRepUse) {
                            ok = false;
                            dumpEscape("DoubleRep escape: ", user);
                            break;
                        }
                        switch (user->child1().useKind()) {
                        case RealNumberUse:
                        case NumberUse:
                            break;
                        default: {
                            ok = false;
                            dumpEscape("DoubleRep escape: ", user);
                            break;
                        }
                        }
                        break;
                    }

                    case Int52Rep: {
                        if constexpr (useKind != Int52RepUse) {
                            ok = false;
                            dumpEscape("Int52RepUse escape: ", user);
                            break;
                        }
                        switch (user->child1().useKind()) {
                        case AnyIntUse:
                        case RealNumberUse:
                            break;
                        default: {
                            ok = false;
                            dumpEscape("Int52Rep escape: ", user);
                            break;
                        }
                        }
                        break;
                    }

                    case ValueToInt32: {
                        if constexpr (useKind != Int32Use) {
                            ok = false;
                            dumpEscape("Int32Use escape: ", user);
                            break;
                        }
                        break;
                    }

                    case PutHint:
                    case MovHint:
                        break;

                    // We can handle these nodes only when we have FPRReg addition in integer form.
                    case PutByOffset:
                    case MultiPutByOffset:
                    case PutClosureVar:
                    case PutGlobalVariable:
                        if constexpr (useKind != DoubleRepUse) {
                            ok = false;
                            dumpEscape("Normal escape: ", user);
                            break;
                        }
                        break;

                    case Upsilon: {
                        Node* phi = user->phi();
                        if (isEscaped(phi)) {
                            dumpEscape("Upsilon of escaped phi escapes candidate: ", phi);
                            ok = false;
                        }
                        break;
                    }

                    default:
                        dumpEscape("Normal escape: ", user);
                        ok = false;
                        break;
                    }

                    if (!ok)
                        break;
                }

                if (!ok)
                    toRemove.add(candidate);
            }

            if (toRemove.isEmpty())
                break;

            for (Node* node : toRemove)
                candidates.remove(node);
        } while (true);

        if (candidates.isEmpty())
            return false;

        NodeOrigin originForConstant = m_graph.block(0)->at(0)->origin;
        UncheckedKeyHashSet<Node*> doubleRepRealCheckLocations;

        for (auto* candidate : candidates) {
            dataLogLnIf(verbose, "Optimized: ", candidate, " with ", useKind);

            Node* resultNode = nullptr;
            switch (candidate->op()) {
            case Phi: {
                resultNode = candidate;

                for (Node* upsilon : phiChildren.upsilonsOf(candidate)) {
                    Node* incomingValue = upsilon->child1().node();
                    Node* newChild = nullptr;
                    switch (incomingValue->op()) {
                    case JSConstant: {
                        if constexpr (useKind == DoubleRepUse) {
                            double number = incomingValue->asJSValue().asNumber();
                            newChild = m_insertionSet.insertConstant(0, originForConstant, jsDoubleNumber(number), DoubleConstant);
                            break;
                        }
                        if constexpr (useKind == Int52RepUse) {
                            int64_t number = incomingValue->asJSValue().asAnyInt();
                            newChild = m_insertionSet.insertConstant(0, originForConstant, jsNumber(number), Int52Constant);
                            break;
                        }
                        if constexpr (useKind == Int32Use) {
                            int32_t number = incomingValue->asJSValue().asInt32AsAnyInt();
                            newChild = m_insertionSet.insertConstant(0, originForConstant, jsNumber(number), JSConstant);
                            break;
                        }
                        break;
                    }
                    case ValueRep: {
                        // We don't care about the incoming value being an impure NaN because users of
                        // this Phi are either OSR exit, DoubleRep(RealNumberUse:@phi), or PurifyNaN(@phi).
                        ASSERT(incomingValue->child1().useKind() == useKind);
                        newChild = incomingValue->child1().node();
                        break;
                    }
                    case Phi: {
                        newChild = incomingValue;
                        break;
                    }

                    case GetClosureVar:
                    case GetGlobalVar:
                    case GetGlobalLexicalVariable:
                    case MultiGetByOffset:
                    case GetByOffset: {
                        newChild = incomingValue;
                        break;
                    }

                    case MultiGetByVal: {
                        newChild = incomingValue;
                        break;
                    }

                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                        break;
                    }

                    upsilon->child1() = Edge(newChild, useKind);
                }

                if constexpr (useKind == DoubleRepUse)
                    candidate->setResult(NodeResultDouble);
                if constexpr (useKind == Int52RepUse)
                    candidate->setResult(NodeResultInt52);
                if constexpr (useKind == Int32Use)
                    candidate->setResult(NodeResultInt32);
                break;
            }

            case ValueRep: {
                resultNode = candidate->child1().node();
                break;
            }

            case GetClosureVar:
            case GetGlobalVar:
            case GetGlobalLexicalVariable:
            case MultiGetByOffset:
            case GetByOffset: {
                candidate->setResult(NodeResultDouble);
                candidate->mergeFlags(NodeMustGenerate); // Absorbs speculation check from the using edge
                resultNode = candidate;
                break;
            }

            case MultiGetByVal: {
                if constexpr (useKind == Int52RepUse) {
                    candidate->setResult(NodeResultInt52);
                    candidate->mergeFlags(NodeMustGenerate); // Absorbs speculation check from using edge
                }
                if constexpr (useKind == Int32Use) {
                    candidate->setResult(NodeResultInt32);
                    candidate->mergeFlags(NodeMustGenerate); // Absorbs speculation check from using edge
                }
                resultNode = candidate;
                break;
            }

            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            for (Node* user : getUsersOf(candidate)) {
                switch (user->op()) {
                case DoubleRep: {
                    if (user->child1().useKind() == RealNumberUse) {
                        user->convertToIdentityOn(resultNode);
                        doubleRepRealCheckLocations.add(user);
                    } else
                        user->convertToPurifyNaN(resultNode);
                    break;
                }

                case Int52Rep: {
                    user->convertToIdentityOn(resultNode);
                    break;
                }

                case ValueToInt32: {
                    user->convertToIdentityOn(resultNode);
                    break;
                }

                case PutHint:
                    if constexpr (useKind != DoubleRepUse && useKind != Int52RepUse)
                        user->child2() = Edge(resultNode, UntypedUse);
                    else
                        user->child2() = Edge(resultNode, useKind);
                    break;

                case MovHint:
                    if constexpr (useKind != DoubleRepUse && useKind != Int52RepUse)
                        user->child1() = Edge(resultNode, UntypedUse);
                    else
                        user->child1() = Edge(resultNode, useKind);
                    break;

                case PutByOffset:
                    user->child3() = Edge(resultNode, useKind);
                    break;

                case MultiPutByOffset:
                    user->child2() = Edge(resultNode, useKind);
                    break;

                case PutClosureVar:
                    user->child2() = Edge(resultNode, useKind);
                    break;

                case PutGlobalVariable:
                    user->child2() = Edge(resultNode, useKind);
                    break;

                case Upsilon: {
                    Node* phi = user->phi();
                    ASSERT_UNUSED(phi, candidates.contains(phi));
                    break;
                }

                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
            }
        }

        // Insert constants.
        m_insertionSet.execute(m_graph.block(0));

        // Insert checks that are needed when removing DoubleRep(RealNumber:@x)
        if (!doubleRepRealCheckLocations.isEmpty()) {
            for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
                for (unsigned i = 0; i < block->size(); ++i) {
                    Node* node = block->at(i);
                    if (node->op() != Identity) {
                        ASSERT(!doubleRepRealCheckLocations.contains(node));
                        continue;
                    }
                    if (!doubleRepRealCheckLocations.contains(node))
                        continue;
                    m_insertionSet.insertNode(
                        i, SpecNone, Check, node->origin,
                        Edge(node->child1().node(), DoubleRepRealUse));
                }

                m_insertionSet.execute(block);
            }
        }

        return true;
    }

    InsertionSet m_insertionSet;
};
    
bool performValueRepReduction(Graph& graph)
{
    return runPhase<ValueRepReductionPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

