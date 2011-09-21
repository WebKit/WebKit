/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DFGPropagator.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGScoreBoard.h"
#include <wtf/FixedArray.h>

namespace JSC { namespace DFG {

class Propagator {
public:
    Propagator(Graph& graph, JSGlobalData& globalData, CodeBlock* codeBlock, CodeBlock* profiledBlock)
        : m_graph(graph)
        , m_globalData(globalData)
        , m_codeBlock(codeBlock)
        , m_profiledBlock(profiledBlock)
    {
        // Predictions is a forward flow property that propagates the values seen at
        // a particular value source to their various uses, ensuring that uses perform
        // speculation that does not contravene the expected values.
        m_predictions.resize(m_graph.size());
        
        // Uses is a backward flow property that propagates the hard expectations at
        // certain uses to their value sources, ensuring that predictions about
        // values do not contravene the code itself. This comes up only in the
        // cases of obvious cell uses, like GetById and friends as well as Call.
        // We're essentially statically speculating that if the value profile indicates
        // that only booleans (for example) flow into a GetById, then the value
        // profile is simply wrong due to insufficient coverage and needs to be
        // adjusted accordingly. The alternatives would be to assume either
        // that the GetById never executes, or always executes on a boolean leading
        // to whatever bizarre behavior that's supposed to cause.
        m_uses.resize(m_graph.size());
        m_variableUses.initializeSimilarTo(m_graph.predictions());
        
        // Replacements are used to implement local common subexpression elimination.
        m_replacements.resize(m_graph.size());
        
        for (unsigned i = 0; i < m_graph.size(); ++i) {
            m_predictions[i] = PredictNone;
            m_uses[i] = PredictNone;
            m_replacements[i] = NoNode;
        }
        
        for (unsigned i = 0; i < LastNodeId; ++i)
            m_lastSeen[i] = NoNode;
    }
    
    void fixpoint()
    {
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
    m_graph.dump(m_codeBlock);
#endif

#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        m_count = 0;
#endif
        do {
            m_changed = false;
            
            // Up here we start with a backward pass because we suspect that to be
            // more profitable.
            propagateArithNodeFlagsBackward();
            if (!m_changed)
                break;
            
            m_changed = false;
            propagateArithNodeFlagsForward();
        } while (m_changed);
        
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        m_count = 0;
#endif
        do {
            m_changed = false;
            
            // Forward propagation is near-optimal for both topologically-sorted and
            // DFS-sorted code.
            propagatePredictionsForward();
            if (!m_changed)
                break;
            
            // Backward propagation reduces the likelihood that pathological code will
            // cause slowness. Loops (especially nested ones) resemble backward flow.
            // This pass captures two cases: (1) it detects if the forward fixpoint
            // found a sound solution and (2) short-circuits backward flow.
            m_changed = false;
            propagatePredictionsBackward();
        } while (m_changed);
        
        fixup();
        
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("Graph after propagation fixup:\n");
        m_graph.dump(m_codeBlock);
#endif

        localCSE();

#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("Graph after CSE:\n");
        m_graph.dump(m_codeBlock);
#endif

        allocateVirtualRegisters();

#if ENABLE(DFG_DEBUG_VERBOSE)
        printf("Graph after propagation:\n");
        m_graph.dump(m_codeBlock);
#endif
    }
    
private:
    bool isNotNegZero(NodeIndex nodeIndex)
    {
        if (!m_graph.isNumberConstant(m_codeBlock, nodeIndex))
            return false;
        double value = m_graph.valueOfNumberConstant(m_codeBlock, nodeIndex);
        return !value && 1.0 / value < 0.0;
    }
    
    bool isNotZero(NodeIndex nodeIndex)
    {
        if (!m_graph.isNumberConstant(m_codeBlock, nodeIndex))
            return false;
        return !!m_graph.valueOfNumberConstant(m_codeBlock, nodeIndex);
    }
    
    void propagateArithNodeFlags(Node& node)
    {
        if (!node.shouldGenerate())
            return;
        
        NodeType op = node.op;
        ArithNodeFlags flags = 0;
        
        if (node.hasArithNodeFlags())
            flags = node.rawArithNodeFlags();
        
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("   %s @%u: %s ", Graph::opName(op), m_compileIndex, arithNodeFlagsAsString(flags));
#endif
        
        flags &= NodeUsedAsMask;
        
        bool changed = false;
        
        switch (op) {
        case ValueToInt32:
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitLShift:
        case BitRShift:
        case BitURShift: {
            // These operations are perfectly happy with truncated integers,
            // so we don't want to propagate anything.
            break;
        }
            
        case ValueToNumber:
        case ValueToDouble:
        case UInt32ToNumber: {
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            break;
        }
            
        case ArithAdd:
        case ValueAdd: {
            if (isNotNegZero(node.child1()) || isNotNegZero(node.child2()))
                flags &= ~NodeNeedsNegZero;
            
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags);
            break;
        }
            
        case ArithSub: {
            if (isNotZero(node.child1()) || isNotZero(node.child2()))
                flags &= ~NodeNeedsNegZero;
            
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags);
            break;
        }
            
        case ArithMul: {
            // As soon as a multiply happens, we can easily end up in the part
            // of the double domain where the point at which you do truncation
            // can change the outcome. So, ArithMul always checks for overflow
            // no matter what, and always forces its inputs to check as well.
            
            flags |= NodeUsedAsNumber | NodeNeedsNegZero;
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags);
            break;
        }
            
        case ArithMin:
        case ArithMax: {
            flags |= NodeUsedAsNumber;
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags);
            break;
        }
            
        case ArithAbs: {
            flags &= ~NodeNeedsNegZero;
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
            break;
        }
            
        case PutByVal: {
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags | NodeUsedAsNumber | NodeNeedsNegZero);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags | NodeUsedAsNumber);
            changed |= m_graph[node.child3()].mergeArithNodeFlags(flags | NodeUsedAsNumber | NodeNeedsNegZero);
            break;
        }
            
        case GetByVal: {
            changed |= m_graph[node.child1()].mergeArithNodeFlags(flags | NodeUsedAsNumber | NodeNeedsNegZero);
            changed |= m_graph[node.child2()].mergeArithNodeFlags(flags | NodeUsedAsNumber);
            break;
        }
            
        default:
            flags |= NodeUsedAsNumber | NodeNeedsNegZero;
            if (op & NodeHasVarArgs) {
                for (unsigned childIdx = node.firstChild(); childIdx < node.firstChild() + node.numChildren(); childIdx++)
                    changed |= m_graph[m_graph.m_varArgChildren[childIdx]].mergeArithNodeFlags(flags);
            } else {
                if (node.child1() == NoNode)
                    break;
                changed |= m_graph[node.child1()].mergeArithNodeFlags(flags);
                if (node.child2() == NoNode)
                    break;
                changed |= m_graph[node.child2()].mergeArithNodeFlags(flags);
                if (node.child3() == NoNode)
                    break;
                changed |= m_graph[node.child3()].mergeArithNodeFlags(flags);
            }
            break;
        }

#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("%s\n", changed ? "CHANGED" : "");
#endif
        
        m_changed |= changed;
    }
    
    void propagateArithNodeFlagsForward()
    {
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("Propagating arithmetic node flags forward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex)
            propagateArithNodeFlags(m_graph[m_compileIndex]);
    }
    
    void propagateArithNodeFlagsBackward()
    {
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("Propagating arithmetic node flags backward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = m_graph.size(); m_compileIndex-- > 0;)
            propagateArithNodeFlags(m_graph[m_compileIndex]);
    }
    
    bool setPrediction(PredictedType prediction)
    {
        ASSERT(m_graph[m_compileIndex].hasResult());
        
        if (m_predictions[m_compileIndex] == prediction)
            return false;
        
        m_predictions[m_compileIndex] = prediction;
        return true;
    }
    
    bool mergeUse(NodeIndex nodeIndex, PredictedType prediction)
    {
        ASSERT(m_graph[nodeIndex].hasResult());
        
        return JSC::mergePrediction(m_uses[nodeIndex], prediction);
    }
    
    bool mergePrediction(PredictedType prediction)
    {
        ASSERT(m_graph[m_compileIndex].hasResult());
        
        return JSC::mergePrediction(m_predictions[m_compileIndex], prediction);
    }
    
    void propagateNodePredictions(Node& node)
    {
        if (!node.shouldGenerate())
            return;
        
        NodeType op = node.op;

#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("   %s @%u: ", Graph::opName(op), m_compileIndex);
#endif
        
        bool changed = false;
        
        switch (op) {
        case JSConstant: {
            changed |= setPrediction(makePrediction(predictionFromValue(m_graph.valueOfJSConstant(m_codeBlock, m_compileIndex)), StrongPrediction));
            break;
        }
            
        case GetLocal: {
            changed |= m_graph.predict(node.local(), m_uses[m_compileIndex] & ~PredictionTagMask, StrongPrediction);
            changed |= m_variableUses.predict(node.local(), m_uses[m_compileIndex] & ~PredictionTagMask, StrongPrediction);

            PredictedType prediction = m_graph.getPrediction(node.local());
            if (isStrongPrediction(prediction))
                changed |= mergePrediction(prediction);
            break;
        }
            
        case SetLocal: {
            changed |= m_graph.predict(node.local(), m_predictions[node.child1()] & ~PredictionTagMask, StrongPrediction);
            changed |= mergeUse(node.child1(), m_variableUses.getPrediction(node.local()));
            break;
        }
            
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitRShift:
        case BitLShift:
        case BitURShift:
        case ValueToInt32: {
            changed |= setPrediction(makePrediction(PredictInt32, StrongPrediction));
            break;
        }

        case ArithMod: {
            PredictedType left = m_predictions[node.child1()];
            PredictedType right = m_predictions[node.child2()];
            
            if (isStrongPrediction(left) && isStrongPrediction(right)) {
                if (isInt32Prediction(mergePredictions(left, right)) && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(makePrediction(PredictInt32, StrongPrediction));
                else
                    changed |= mergePrediction(makePrediction(PredictDouble, StrongPrediction));
            }
            break;
        }
            
        case UInt32ToNumber: {
            if (nodeCanSpeculateInteger(node.arithNodeFlags()))
                changed |= setPrediction(makePrediction(PredictInt32, StrongPrediction));
            else
                changed |= setPrediction(makePrediction(PredictNumber, StrongPrediction));
            break;
        }

        case ValueToNumber: {
            PredictedType prediction = m_predictions[node.child1()];
            
            if (isStrongPrediction(prediction)) {
                if (!(prediction & PredictDouble) && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(makePrediction(PredictInt32, StrongPrediction));
                else
                    changed |= mergePrediction(makePrediction(PredictNumber, StrongPrediction));
            }
            
            break;
        }
            
        case ValueAdd: {
            PredictedType left = m_predictions[node.child1()];
            PredictedType right = m_predictions[node.child2()];
            
            if (isStrongPrediction(left) && isStrongPrediction(right)) {
                if (isNumberPrediction(left) && isNumberPrediction(right)) {
                    if (isInt32Prediction(mergePredictions(left, right)) && nodeCanSpeculateInteger(node.arithNodeFlags()))
                        changed |= mergePrediction(makePrediction(PredictInt32, StrongPrediction));
                    else
                        changed |= mergePrediction(makePrediction(PredictDouble, StrongPrediction));
                } else if (!(left & PredictNumber) || !(right & PredictNumber)) {
                    // left or right is definitely something other than a number.
                    changed |= mergePrediction(makePrediction(PredictString, StrongPrediction));
                } else
                    changed |= mergePrediction(makePrediction(PredictString | PredictInt32 | PredictDouble, StrongPrediction));
            }
            break;
        }
            
        case ArithAdd:
        case ArithSub:
        case ArithMul:
        case ArithMin:
        case ArithMax: {
            PredictedType left = m_predictions[node.child1()];
            PredictedType right = m_predictions[node.child2()];
            
            if (isStrongPrediction(left) && isStrongPrediction(right)) {
                if (isInt32Prediction(mergePredictions(left, right)) && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(makePrediction(PredictInt32, StrongPrediction));
                else
                    changed |= mergePrediction(makePrediction(PredictDouble, StrongPrediction));
            }
            break;
        }
            
        case ArithDiv:
        case ArithSqrt: {
            changed |= setPrediction(makePrediction(PredictDouble, StrongPrediction));
            break;
        }
            
        case ArithAbs: {
            PredictedType child = m_predictions[node.child1()];
            if (isStrongPrediction(child)) {
                if (nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(child);
                else
                    changed |= setPrediction(makePrediction(PredictDouble, StrongPrediction));
            }
            break;
        }
            
        case LogicalNot:
        case CompareLess:
        case CompareLessEq:
        case CompareGreater:
        case CompareGreaterEq:
        case CompareEq:
        case CompareStrictEq:
        case InstanceOf: {
            changed |= setPrediction(makePrediction(PredictBoolean, StrongPrediction));
            break;
        }
            
        case GetById:
        case GetMethod:
        case GetByVal: {
            changed |= mergeUse(node.child1(), PredictObjectUnknown | StrongPredictionTag);
            changed |= node.predict(m_uses[m_compileIndex] & ~PredictionTagMask, StrongPrediction);
            if (isStrongPrediction(node.getPrediction()))
                changed |= setPrediction(node.getPrediction());
            break;
        }
            
        case CheckStructure: {
            // We backward propagate what this CheckStructure tells us. Maybe that's the right way
            // to go; maybe it isn't. I'm not sure. What we'd really want is flow-sensitive
            // forward propagation of what we learn from having executed CheckStructure. But for
            // now we preserve the flow-insensitive nature of this analysis, because it's cheap to
            // run and seems to work well enough.
            changed |= mergeUse(node.child1(), predictionFromStructure(node.structure()) | StrongPredictionTag);
            changed |= setPrediction(PredictOther | StrongPredictionTag);
            break;
        }
            
        case GetByOffset: {
            changed |= node.predict(m_uses[m_compileIndex] & ~PredictionTagMask, StrongPrediction);
            if (isStrongPrediction(node.getPrediction()))
                changed |= setPrediction(node.getPrediction());
            break;
        }
            
        case CheckMethod: {
            changed |= mergeUse(node.child1(), PredictObjectUnknown | StrongPredictionTag);
            changed |= setPrediction(m_graph.getMethodCheckPrediction(node));
            break;
        }

        case Call:
        case Construct: {
            changed |= mergeUse(m_graph.m_varArgChildren[node.firstChild()], PredictObjectUnknown | StrongPredictionTag);
            changed |= node.predict(m_uses[m_compileIndex] & ~PredictionTagMask, StrongPrediction);
            if (isStrongPrediction(node.getPrediction()))
                changed |= setPrediction(node.getPrediction());
            break;
        }
            
        case ConvertThis: {
            changed |= setPrediction(makePrediction(PredictObjectUnknown, StrongPrediction));
            break;
        }
            
        case GetGlobalVar: {
            changed |= m_variableUses.predictGlobalVar(node.varNumber(), m_uses[m_compileIndex] & ~PredictionTagMask, StrongPrediction);
            PredictedType prediction = m_graph.getGlobalVarPrediction(node.varNumber());
            if (isStrongPrediction(prediction))
                changed |= mergePrediction(prediction);
            break;
        }
            
        case PutGlobalVar: {
            changed |= m_graph.predictGlobalVar(node.varNumber(), m_predictions[node.child1()] & ~PredictionTagMask, StrongPrediction);
            changed |= mergeUse(node.child1(), m_variableUses.getGlobalVarPrediction(node.varNumber()));
            break;
        }
            
        case PutByVal:
        case PutByValAlias:
        case PutById:
        case PutByIdDirect: {
            changed |= mergeUse(node.child1(), PredictObjectUnknown | StrongPredictionTag);
            break;
        }
            
        case GetCallee: {
            changed |= setPrediction(makePrediction(PredictObjectOther, StrongPrediction));
            break;
        }
            
        case CreateThis: {
            changed |= mergeUse(node.child1(), PredictObjectUnknown | StrongPredictionTag);
            changed |= setPrediction(makePrediction(PredictFinalObject, StrongPrediction));
            break;
        }

#ifndef NDEBUG
        // These get ignored because they don't return anything.
        case DFG::Jump:
        case Branch:
        case Return:
        case CheckHasInstance:
        case Phi:
        case Throw:
        case ThrowReferenceError:
            break;
            
        // These get ignored because we don't have profiling for them, yet.
        case Resolve:
        case ResolveBase:
        case ResolveBaseStrictPut:
            break;
            
        // This gets ignored because it doesn't do anything.
        case Phantom:
            break;
            
        default:
            ASSERT_NOT_REACHED();
            break;
#else
        default:
            break;
#endif
        }

#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("expect(%s) ", predictionToString(m_predictions[m_compileIndex]));
        printf("use(%s) %s\n", predictionToString(m_uses[m_compileIndex]), changed ? "CHANGED" : "");
#endif
        
        m_changed |= changed;
    }
    
    void propagatePredictionsForward()
    {
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("Propagating predictions forward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex)
            propagateNodePredictions(m_graph[m_compileIndex]);
    }
    
    void propagatePredictionsBackward()
    {
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("Propagating predictions backward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = m_graph.size(); m_compileIndex-- > 0;)
            propagateNodePredictions(m_graph[m_compileIndex]);
    }
    
    void toDouble(NodeIndex nodeIndex)
    {
        if (m_graph[nodeIndex].op == ValueToNumber) {
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
            printf("  @%u -> ValueToDouble", nodeIndex);
#endif
            m_graph[nodeIndex].op = ValueToDouble;
        }
    }
    
    void fixupNode(Node& node)
    {
        if (!node.shouldGenerate())
            return;
        
        NodeType op = node.op;

#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("   %s @%u: ", Graph::opName(op), m_compileIndex);
#endif
        
        switch (op) {
        case ValueAdd: {
            if (!nodeCanSpeculateInteger(node.arithNodeFlags())) {
                toDouble(node.child1());
                toDouble(node.child2());
                break;
            }
            
            PredictedType left = m_predictions[node.child1()];
            PredictedType right = m_predictions[node.child2()];
            
            if (isStrongPrediction(left) && isStrongPrediction(right) && isNumberPrediction(left) && isNumberPrediction(right)) {
                if (left & PredictDouble)
                    toDouble(node.child2());
                if (right & PredictDouble)
                    toDouble(node.child1());
            }
            break;
        }
            
        case ArithAdd:
        case ArithSub:
        case ArithMul:
        case ArithMin:
        case ArithMax:
        case ArithMod: {
            if (!nodeCanSpeculateInteger(node.arithNodeFlags())) {
                toDouble(node.child1());
                toDouble(node.child2());
                break;
            }
            
            PredictedType left = m_predictions[node.child1()];
            PredictedType right = m_predictions[node.child2()];
            
            if (isStrongPrediction(left) && isStrongPrediction(right)) {
                if (left & PredictDouble)
                    toDouble(node.child2());
                if (right & PredictDouble)
                    toDouble(node.child1());
            }
            break;
        }
            
        case ArithDiv: {
            toDouble(node.child1());
            toDouble(node.child2());
            break;
        }
            
        case ArithAbs: {
            if (!nodeCanSpeculateInteger(node.arithNodeFlags())) {
                toDouble(node.child1());
                break;
            }
            
            PredictedType prediction = m_predictions[node.child1()];
            if (isStrongPrediction(prediction) && (prediction & PredictDouble))
                toDouble(node.child1());
            break;
        }
            
        case ArithSqrt: {
            toDouble(node.child1());
            break;
        }
            
        default:
            break;
        }

#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("\n");
#endif
    }
    
    void fixup()
    {
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("Performing Fixup\n");
#endif
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex)
            fixupNode(m_graph[m_compileIndex]);
    }
    
    NodeIndex canonicalize(NodeIndex nodeIndex)
    {
        if (nodeIndex == NoNode)
            return NoNode;
        
        if (m_graph[nodeIndex].op == ValueToNumber)
            nodeIndex = m_graph[nodeIndex].child1();
        
        if (m_graph[nodeIndex].op == ValueToInt32)
            nodeIndex = m_graph[nodeIndex].child1();
        
        return nodeIndex;
    }
    
    // Computes where the search for a candidate for CSE should start. Don't call
    // this directly; call startIndex() instead as it does logging in debug mode.
    NodeIndex computeStartIndexForChildren(NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
    {
        const unsigned limit = 300;
        
        NodeIndex start = m_start;
        if (m_compileIndex - start > limit)
            start = m_compileIndex - limit;
        
        ASSERT(start >= m_start);
        
        NodeIndex child = canonicalize(child1);
        if (child == NoNode)
            return start;
        
        if (start < child)
            start = child;
        
        child = canonicalize(child2);
        if (child == NoNode)
            return start;
        
        if (start < child)
            start = child;
        
        child = canonicalize(child3);
        if (child == NoNode)
            return start;
        
        if (start < child)
            start = child;
        
        return start;
    }
    
    NodeIndex startIndexForChildren(NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
    {
        NodeIndex result = computeStartIndexForChildren(child1, child2, child3);
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("  lookback %u: ", result);
#endif
        return result;
    }
    
    NodeIndex startIndex()
    {
        Node& node = m_graph[m_compileIndex];
        return startIndexForChildren(node.child1(), node.child2(), node.child3());
    }
    
    NodeIndex endIndexForPureCSE()
    {
        NodeIndex result = m_lastSeen[m_graph[m_compileIndex].op & NodeIdMask];
        if (result == NoNode)
            result = 0;
        else
            result++;
        ASSERT(result <= m_compileIndex);
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("  limit %u: ", result);
#endif
        return result;
    }
    
    NodeIndex pureCSE(Node& node)
    {
        NodeIndex child1 = canonicalize(node.child1());
        NodeIndex child2 = canonicalize(node.child2());
        NodeIndex child3 = canonicalize(node.child3());
        
        NodeIndex start = startIndex();
        for (NodeIndex index = endIndexForPureCSE(); index-- > start;) {
            Node& otherNode = m_graph[index];
            if (node.op != otherNode.op)
                continue;
            
            if (node.arithNodeFlagsForCompare() != otherNode.arithNodeFlagsForCompare())
                continue;
            
            NodeIndex otherChild = canonicalize(otherNode.child1());
            if (otherChild == NoNode)
                return index;
            if (otherChild != child1)
                continue;
            
            otherChild = canonicalize(otherNode.child2());
            if (otherChild == NoNode)
                return index;
            if (otherChild != child2)
                continue;
            
            otherChild = canonicalize(otherNode.child3());
            if (otherChild == NoNode)
                return index;
            if (otherChild != child3)
                continue;
            
            return index;
        }
        return NoNode;
    }
    
    bool isPredictedNumerical(Node& node)
    {
        PredictedType left = m_predictions[node.child1()];
        PredictedType right = m_predictions[node.child2()];
        return isStrongPrediction(left) && isStrongPrediction(right)
            && isNumberPrediction(left) && isNumberPrediction(right);
    }
    
    bool logicalNotIsPure(Node& node)
    {
        PredictedType prediction = m_predictions[node.child1()];
        return isBooleanPrediction(prediction) || !isStrongPrediction(prediction);
    }
    
    bool clobbersWorld(NodeIndex nodeIndex)
    {
        Node& node = m_graph[nodeIndex];
        if (node.op & NodeClobbersWorld)
            return true;
        if (!(node.op & NodeMightClobber))
            return false;
        switch (node.op) {
        case ValueAdd:
        case CompareLess:
        case CompareLessEq:
        case CompareGreater:
        case CompareGreaterEq:
        case CompareEq:
            return !isPredictedNumerical(node);
        case LogicalNot:
            return !logicalNotIsPure(node);
        default:
            ASSERT_NOT_REACHED();
            return true; // If by some oddity we hit this case in release build it's safer to have CSE assume the worst.
        }
    }
    
    NodeIndex globalVarLoadElimination(unsigned varNumber)
    {
        NodeIndex start = startIndexForChildren();
        for (NodeIndex index = m_compileIndex; index-- > start;) {
            Node& node = m_graph[index];
            switch (node.op) {
            case GetGlobalVar:
                if (node.varNumber() == varNumber)
                    return index;
                break;
            case PutGlobalVar:
                if (node.varNumber() == varNumber)
                    return node.child1();
                break;
            default:
                break;
            }
            if (clobbersWorld(index))
                break;
        }
        return NoNode;
    }
    
    NodeIndex getByValLoadElimination(NodeIndex child1, NodeIndex child2)
    {
        NodeIndex start = startIndexForChildren(child1, child2);
        for (NodeIndex index = m_compileIndex; index-- > start;) {
            Node& node = m_graph[index];
            switch (node.op) {
            case GetByVal:
                if (node.child1() == child1 && canonicalize(node.child2()) == canonicalize(child2))
                    return index;
                break;
            case PutByVal:
                if (node.child1() == child1 && canonicalize(node.child2()) == canonicalize(child2))
                    return node.child3();
                break;
            default:
                break;
            }
            if (clobbersWorld(index))
                break;
        }
        return NoNode;
    }
    
    NodeIndex getMethodLoadElimination(const MethodCheckData& methodCheckData, unsigned identifierNumber, NodeIndex child1)
    {
        NodeIndex start = startIndexForChildren(child1);
        for (NodeIndex index = m_compileIndex; index-- > start;) {
            Node& node = m_graph[index];
            if (node.op == CheckMethod
                && node.child1() == child1
                && node.identifierNumber() == identifierNumber
                && m_graph.m_methodCheckData[node.methodCheckDataIndex()] == methodCheckData)
                return index;
            if (clobbersWorld(index))
                break;
        }
        return NoNode;
    }
    
    NodeIndex checkStructureLoadElimination(Structure* structure, NodeIndex child1)
    {
        NodeIndex start = startIndexForChildren(child1);
        for (NodeIndex index = m_compileIndex; index-- > start;) {
            Node& node = m_graph[index];
            if (node.op == CheckStructure
                && node.child1() == child1
                && node.structure() == structure)
                return index;
            if (clobbersWorld(index))
                break;
        }
        return NoNode;
    }
    
    NodeIndex getByOffsetLoadElimination(unsigned identifierNumber, NodeIndex child1)
    {
        NodeIndex start = startIndexForChildren(child1);
        for (NodeIndex index = m_compileIndex; index-- > start;) {
            Node& node = m_graph[index];
            if (node.op == GetByOffset
                && node.child1() == child1
                && m_graph.m_storageAccessData[node.storageAccessDataIndex()].identifierNumber == identifierNumber)
                return index;
            if (clobbersWorld(index))
                break;
        }
        return NoNode;
    }
    
    void performSubstitution(NodeIndex& child)
    {
        // Check if this operand is actually unused.
        if (child == NoNode)
            return;
        
        // Check if there is any replacement.
        NodeIndex replacement = m_replacements[child];
        if (replacement == NoNode)
            return;
        
        child = replacement;
        
        // There is definitely a replacement. Assert that the replacement does not
        // have a replacement.
        ASSERT(m_replacements[child] == NoNode);
        
        m_graph[child].ref();
    }
    
    void setReplacement(NodeIndex replacement)
    {
        if (replacement == NoNode)
            return;
        
        // Be safe. Don't try to perform replacements if the predictions don't
        // agree.
        if (m_predictions[m_compileIndex] != m_predictions[replacement])
            return;
        
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("   Replacing @%u -> @%u", m_compileIndex, replacement);
#endif
        
        Node& node = m_graph[m_compileIndex];
        node.op = Phantom;
        node.setRefCount(1);
        
        // At this point we will eliminate all references to this node.
        m_replacements[m_compileIndex] = replacement;
    }
    
    void performNodeCSE(Node& node)
    {
        if (node.op & NodeHasVarArgs) {
            for (unsigned childIdx = node.firstChild(); childIdx < node.firstChild() + node.numChildren(); childIdx++)
                performSubstitution(m_graph.m_varArgChildren[childIdx]);
        } else {
            performSubstitution(node.children.fixed.child1);
            performSubstitution(node.children.fixed.child2);
            performSubstitution(node.children.fixed.child3);
        }
        
        if (!node.shouldGenerate())
            return;
        
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("   %s @%u: ", Graph::opName(m_graph[m_compileIndex].op), m_compileIndex);
#endif

        switch (node.op) {
        
        // Handle the pure nodes. These nodes never have any side-effects.
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitRShift:
        case BitLShift:
        case BitURShift:
        case ArithAdd:
        case ArithSub:
        case ArithMul:
        case ArithMod:
        case ArithDiv:
        case ArithAbs:
        case ArithMin:
        case ArithMax:
        case ArithSqrt:
        case GetCallee:
            setReplacement(pureCSE(node));
            break;
            
        // Handle nodes that are conditionally pure: these are pure, and can
        // be CSE'd, so long as the prediction is the one we want.
        case ValueAdd:
        case CompareLess:
        case CompareLessEq:
        case CompareGreater:
        case CompareGreaterEq:
        case CompareEq: {
            if (isPredictedNumerical(node)) {
                NodeIndex replacementIndex = pureCSE(node);
                if (replacementIndex != NoNode && isPredictedNumerical(m_graph[replacementIndex]))
                    setReplacement(replacementIndex);
            }
            break;
        }
            
        case LogicalNot: {
            if (logicalNotIsPure(node)) {
                NodeIndex replacementIndex = pureCSE(node);
                if (replacementIndex != NoNode && logicalNotIsPure(m_graph[replacementIndex]))
                    setReplacement(replacementIndex);
            }
            break;
        }
            
        // Finally handle heap accesses. These are not quite pure, but we can still
        // optimize them provided that some subtle conditions are met.
        case GetGlobalVar:
            setReplacement(globalVarLoadElimination(node.varNumber()));
            break;
            
        case GetByVal:
            setReplacement(getByValLoadElimination(node.child1(), node.child2()));
            break;
            
        case PutByVal:
            if (getByValLoadElimination(node.child1(), node.child2()) != NoNode)
                node.op = PutByValAlias;
            break;
            
        case CheckMethod:
            setReplacement(getMethodLoadElimination(m_graph.m_methodCheckData[node.methodCheckDataIndex()], node.identifierNumber(), node.child1()));
            break;
            
        case CheckStructure:
            setReplacement(checkStructureLoadElimination(node.structure(), node.child1()));
            break;
            
        case GetByOffset:
            setReplacement(getByOffsetLoadElimination(m_graph.m_storageAccessData[node.storageAccessDataIndex()].identifierNumber, node.child1()));
            break;
            
        default:
            // do nothing.
            break;
        }
        
        m_lastSeen[node.op & NodeIdMask] = m_compileIndex;
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("\n");
#endif
    }
    
    void performBlockCSE(BasicBlock& block)
    {
        m_start = block.begin;
        NodeIndex end = block.end;
        for (m_compileIndex = m_start; m_compileIndex < end; ++m_compileIndex)
            performNodeCSE(m_graph[m_compileIndex]);
    }
    
    void localCSE()
    {
#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
        printf("Performing local CSE:");
#endif
        for (unsigned block = 0; block < m_graph.m_blocks.size(); ++block)
            performBlockCSE(*m_graph.m_blocks[block]);
    }
    
    void allocateVirtualRegisters()
    {
        ScoreBoard scoreBoard(m_graph, m_graph.m_preservedVars);
        unsigned sizeExcludingPhiNodes = m_graph.m_blocks.last()->end;
        for (size_t i = 0; i < sizeExcludingPhiNodes; ++i) {
            Node& node = m_graph[i];
        
            if (!node.shouldGenerate())
                continue;
        
            // GetLocal nodes are effectively phi nodes in the graph, referencing
            // results from prior blocks.
            if (node.op != GetLocal) {
                // First, call use on all of the current node's children, then
                // allocate a VirtualRegister for this node. We do so in this
                // order so that if a child is on its last use, and a
                // VirtualRegister is freed, then it may be reused for node.
                if (node.op & NodeHasVarArgs) {
                    for (unsigned childIdx = node.firstChild(); childIdx < node.firstChild() + node.numChildren(); childIdx++)
                        scoreBoard.use(m_graph.m_varArgChildren[childIdx]);
                } else {
                    scoreBoard.use(node.child1());
                    scoreBoard.use(node.child2());
                    scoreBoard.use(node.child3());
                }
            }

            if (!node.hasResult())
                continue;

            node.setVirtualRegister(scoreBoard.allocate());
            // 'mustGenerate' nodes have their useCount artificially elevated,
            // call use now to account for this.
            if (node.mustGenerate())
                scoreBoard.use(i);
        }

        // 'm_numCalleeRegisters' is the number of locals and temporaries allocated
        // for the function (and checked for on entry). Since we perform a new and
        // different allocation of temporaries, more registers may now be required.
        unsigned calleeRegisters = scoreBoard.allocatedCount() + m_graph.m_preservedVars + m_graph.m_parameterSlots;
        if ((unsigned)m_codeBlock->m_numCalleeRegisters < calleeRegisters)
            m_codeBlock->m_numCalleeRegisters = calleeRegisters;
    }
    
    Graph& m_graph;
    JSGlobalData& m_globalData;
    CodeBlock* m_codeBlock;
    CodeBlock* m_profiledBlock;
    
    NodeIndex m_start;
    NodeIndex m_compileIndex;
    
    Vector<PredictedType, 16> m_predictions;
    Vector<PredictedType, 16> m_uses;
    
    PredictionTracker m_variableUses;

#if ENABLE(DFG_DEBUG_PROPAGATION_VERBOSE)
    unsigned m_count;
#endif
    
    bool m_changed;

    Vector<NodeIndex, 16> m_replacements;
    FixedArray<NodeIndex, LastNodeId> m_lastSeen;
};

void propagate(Graph& graph, JSGlobalData* globalData, CodeBlock* codeBlock)
{
    ASSERT(codeBlock);
    CodeBlock* profiledBlock = codeBlock->alternative();
    ASSERT(profiledBlock);
    
    Propagator propagator(graph, *globalData, codeBlock, profiledBlock);
    propagator.fixpoint();
    
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

