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

namespace JSC { namespace DFG {

class ValueRepReductionPhase : public Phase {
    static constexpr bool verbose = false;
    
public:
    ValueRepReductionPhase(Graph& graph)
        : Phase(graph, "ValueRep reduction")
        , m_insertionSet(graph)
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_form == SSA);
        return convertValueRepsToDouble();
    }

private:
    bool convertValueRepsToDouble()
    {
        HashSet<Node*> candidates;
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block) {
                if (node->op() == ValueRep && node->child1().useKind() == DoubleRepUse)
                    candidates.add(node);
            }
        }

        if (!candidates.size())
            return false;

        HashMap<Node*, Vector<Node*>> usersOf;
        auto getUsersOf = [&] (Node* candidate) {
            auto iter = usersOf.find(candidate);
            RELEASE_ASSERT(iter != usersOf.end());
            return iter->value;
        };

        for (BasicBlock* block : m_graph.blocksInPreOrder()) {
            for (Node* node : *block) {
                if (node->op() == Phi || (node->op() == ValueRep && candidates.contains(node)))
                    usersOf.add(node, Vector<Node*>());

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
            HashSet<Node*> eligiblePhis;
            for (Node* candidate : candidates) {
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
            HashSet<Node*> toRemove;

            auto isEscaped = [&] (Node* node) {
                return !candidates.contains(node) || toRemove.contains(node);
            };

            // Escape rules are as follows:
            // - Any non-well-known use is an escape. Currently, we allow DoubleRep, Hints, Upsilons (described below).
            // - Any Upsilon that forwards the candidate into an escaped phi escapes the candidate.
            // - A Phi remains a candidate as long as all values flowing into it can be made a double.
            //   Currently, this means these are valid things we support to forward into the Phi:
            //    - A JSConstant(some number "x") => DoubleConstant("x")
            //    - ValueRep(DoubleRepUse:@x) => @x
            //    - A Phi so long as that Phi is not escaped.
            for (Node* candidate : candidates) {
                bool ok = true;

                auto dumpEscape = [&] (const char* description, Node* node) {
                    if (!verbose)
                        return;
                    dataLogLn(description);
                    dataLog("   candidate: ");
                    m_graph.dump(WTF::dataFile(), Prefix::noString, candidate);
                    dataLog("   reason: ");
                    m_graph.dump(WTF::dataFile(), Prefix::noString, node);
                    dataLogLn();
                };

                if (candidate->op() == Phi) {
                    phiChildren.forAllIncomingValues(candidate, [&] (Node* node) {
                        if (node->op() == JSConstant) {
                            if (!node->asJSValue().isNumber()) {
                                ok = false;
                                dumpEscape("Phi Incoming JSConstant not a number: ", node);
                            }
                        } else if (node->op() == ValueRep) {
                            if (node->child1().useKind() != DoubleRepUse) {
                                ok = false;
                                dumpEscape("Phi Incoming ValueRep not DoubleRepUse: ", node);
                            }
                        } else if (node->op() == Phi) {
                            if (isEscaped(node)) {
                                ok = false;
                                dumpEscape("An incoming Phi to another Phi is escaped: ", node);
                            }
                        } else {
                            ok = false;
                            dumpEscape("Unsupported incoming value to Phi: ", node);
                        }
                    });

                    if (!ok) {
                        toRemove.add(candidate);
                        continue;
                    }
                }

                for (Node* user : getUsersOf(candidate)) {
                    switch (user->op()) {
                    case DoubleRep:
                        if (user->child1().useKind() != RealNumberUse) {
                            ok = false;
                            dumpEscape("DoubleRep escape: ", user);
                        }
                        break;

                    case PutHint:
                    case MovHint:
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

        if (!candidates.size())
            return false;

        NodeOrigin originForConstant = m_graph.block(0)->at(0)->origin;
        HashSet<Node*> doubleRepRealCheckLocations;

        for (Node* candidate : candidates) {
            if (verbose)
                dataLogLn("Optimized: ", candidate);

            Node* resultNode;
            if (candidate->op() == Phi) {
                resultNode = candidate;

                for (Node* upsilon : phiChildren.upsilonsOf(candidate)) {
                    Node* incomingValue = upsilon->child1().node();
                    Node* newChild;
                    if (incomingValue->op() == JSConstant) {
                        double number = incomingValue->asJSValue().asNumber();
                        newChild = m_insertionSet.insertConstant(0, originForConstant, jsDoubleNumber(number), DoubleConstant);
                    } else if (incomingValue->op() == ValueRep) {
                        // We don't care about the incoming value being an impure NaN because users of
                        // this Phi are either OSR exit or DoubleRep(RealNumberUse:@phi)
                        ASSERT(incomingValue->child1().useKind() == DoubleRepUse);
                        newChild = incomingValue->child1().node();
                    } else if (incomingValue->op() == Phi)
                        newChild = incomingValue;
                    else
                        RELEASE_ASSERT_NOT_REACHED();

                    upsilon->child1() = Edge(newChild, DoubleRepUse);
                }

                candidate->setResult(NodeResultDouble);
            } else if (candidate->op() == ValueRep)
                resultNode = candidate->child1().node();
            else
                RELEASE_ASSERT_NOT_REACHED();

            for (Node* user : getUsersOf(candidate)) {
                switch (user->op()) {
                case DoubleRep: {
                    ASSERT(user->child1().useKind() == RealNumberUse);
                    user->convertToIdentityOn(resultNode);
                    doubleRepRealCheckLocations.add(user);
                    break;
                }
                    
                case PutHint:
                    user->child2() = Edge(resultNode, DoubleRepUse);
                    break;

                case MovHint:
                    user->child1() = Edge(resultNode, DoubleRepUse);
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
        if (doubleRepRealCheckLocations.size()) {
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

