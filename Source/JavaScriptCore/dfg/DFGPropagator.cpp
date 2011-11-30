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

#include "DFGAbstractState.h"
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
        // Replacements are used to implement local common subexpression elimination.
        m_replacements.resize(m_graph.size());
        
        for (unsigned i = 0; i < m_graph.size(); ++i)
            m_replacements[i] = NoNode;
        
        for (unsigned i = 0; i < LastNodeId; ++i)
            m_lastSeen[i] = NoNode;
    }
    
    void fixpoint()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        m_graph.dump(m_codeBlock);
#endif

        propagateArithNodeFlags();
        propagatePredictions();
        fixup();
        
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("Graph after propagation fixup:\n");
        m_graph.dump(m_codeBlock);
#endif

        localCSE();

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("Graph after CSE:\n");
        m_graph.dump(m_codeBlock);
#endif

        allocateVirtualRegisters();

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("Graph after virtual register allocation:\n");
        m_graph.dump(m_codeBlock);
#endif

        globalCFA();

#if DFG_ENABLE(DEBUG_VERBOSE)
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
        
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
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
            
        case ArithMul:
        case ArithDiv: {
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

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("%s\n", changed ? "CHANGED" : "");
#endif
        
        m_changed |= changed;
    }
    
    void propagateArithNodeFlagsForward()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("Propagating arithmetic node flags forward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex)
            propagateArithNodeFlags(m_graph[m_compileIndex]);
    }
    
    void propagateArithNodeFlagsBackward()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("Propagating arithmetic node flags backward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = m_graph.size(); m_compileIndex-- > 0;)
            propagateArithNodeFlags(m_graph[m_compileIndex]);
    }
    
    void propagateArithNodeFlags()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
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
    }
    
    bool setPrediction(PredictedType prediction)
    {
        ASSERT(m_graph[m_compileIndex].hasResult());
        
        // setPrediction() is used when we know that there is no way that we can change
        // our minds about what the prediction is going to be. There is no semantic
        // difference between setPrediction() and mergePrediction() other than the
        // increased checking to validate this property.
        ASSERT(m_graph[m_compileIndex].prediction() == PredictNone || m_graph[m_compileIndex].prediction() == prediction);
        
        return m_graph[m_compileIndex].predict(prediction);
    }
    
    bool mergePrediction(PredictedType prediction)
    {
        ASSERT(m_graph[m_compileIndex].hasResult());
        
        return m_graph[m_compileIndex].predict(prediction);
    }
    
    void propagateNodePredictions(Node& node)
    {
        if (!node.shouldGenerate())
            return;
        
        NodeType op = node.op;

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("   %s @%u: ", Graph::opName(op), m_compileIndex);
#endif
        
        bool changed = false;
        
        switch (op) {
        case JSConstant:
        case WeakJSConstant: {
            changed |= setPrediction(predictionFromValue(m_graph.valueOfJSConstant(m_codeBlock, m_compileIndex)));
            break;
        }
            
        case GetLocal: {
            PredictedType prediction = node.variableAccessData()->prediction();
            if (prediction)
                changed |= mergePrediction(prediction);
            break;
        }
            
        case SetLocal: {
            changed |= node.variableAccessData()->predict(m_graph[node.child1()].prediction());
            break;
        }
            
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitRShift:
        case BitLShift:
        case BitURShift:
        case ValueToInt32: {
            changed |= setPrediction(PredictInt32);
            break;
        }
            
        case ArrayPop:
        case ArrayPush: {
            if (node.getHeapPrediction())
                changed |= mergePrediction(node.getHeapPrediction());
            break;
        }

        case StringCharCodeAt: {
            changed |= mergePrediction(PredictInt32);
            break;
        }

        case ArithMod: {
            PredictedType left = m_graph[node.child1()].prediction();
            PredictedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (isInt32Prediction(mergePredictions(left, right)) && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(PredictInt32);
                else
                    changed |= mergePrediction(PredictDouble);
            }
            break;
        }
            
        case UInt32ToNumber: {
            if (nodeCanSpeculateInteger(node.arithNodeFlags()))
                changed |= setPrediction(PredictInt32);
            else
                changed |= setPrediction(PredictNumber);
            break;
        }

        case ValueToNumber: {
            PredictedType prediction = m_graph[node.child1()].prediction();
            
            if (prediction) {
                if (!(prediction & PredictDouble) && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(PredictInt32);
                else
                    changed |= mergePrediction(PredictNumber);
            }
            
            break;
        }

        case ValueAdd: {
            PredictedType left = m_graph[node.child1()].prediction();
            PredictedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (isNumberPrediction(left) && isNumberPrediction(right)) {
                    if (isInt32Prediction(mergePredictions(left, right)) && nodeCanSpeculateInteger(node.arithNodeFlags()))
                        changed |= mergePrediction(PredictInt32);
                    else
                        changed |= mergePrediction(PredictDouble);
                } else if (!(left & PredictNumber) || !(right & PredictNumber)) {
                    // left or right is definitely something other than a number.
                    changed |= mergePrediction(PredictString);
                } else
                    changed |= mergePrediction(PredictString | PredictInt32 | PredictDouble);
            }
            break;
        }
            
        case ArithAdd:
        case ArithSub:
        case ArithMul:
        case ArithMin:
        case ArithMax:
        case ArithDiv: {
            PredictedType left = m_graph[node.child1()].prediction();
            PredictedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (isInt32Prediction(mergePredictions(left, right)) && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(PredictInt32);
                else
                    changed |= mergePrediction(PredictDouble);
            }
            break;
        }
            
        case ArithSqrt: {
            changed |= setPrediction(PredictDouble);
            break;
        }
            
        case ArithAbs: {
            PredictedType child = m_graph[node.child1()].prediction();
            if (child) {
                if (nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(child);
                else
                    changed |= setPrediction(PredictDouble);
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
            changed |= setPrediction(PredictBoolean);
            break;
        }
            
        case GetById: {
            if (node.getHeapPrediction())
                changed |= mergePrediction(node.getHeapPrediction());
            else if (m_codeBlock->identifier(node.identifierNumber()) == m_globalData.propertyNames->length) {
                // If there is no prediction from value profiles, check if we might be
                // able to infer the type ourselves.
                bool isArray = isArrayPrediction(m_graph[node.child1()].prediction());
                bool isString = isStringPrediction(m_graph[node.child1()].prediction());
                bool isByteArray = m_graph[node.child1()].shouldSpeculateByteArray();
                if (isArray || isString || isByteArray)
                    changed |= mergePrediction(PredictInt32);
            }
            break;
        }
            
        case GetByVal: {
            if (node.getHeapPrediction())
                changed |= mergePrediction(node.getHeapPrediction());
            break;
        }
            
        case GetPropertyStorage: {
            changed |= setPrediction(PredictOther);
            break;
        }
            
        case GetByOffset: {
            if (node.getHeapPrediction())
                changed |= mergePrediction(node.getHeapPrediction());
            break;
        }
            
        case Call:
        case Construct: {
            if (node.getHeapPrediction())
                changed |= mergePrediction(node.getHeapPrediction());
            break;
        }
            
        case ConvertThis: {
            PredictedType prediction = m_graph[node.child1()].prediction();
            if (prediction) {
                if (prediction & ~PredictObjectMask) {
                    prediction &= PredictObjectMask;
                    prediction = mergePredictions(prediction, PredictObjectOther);
                }
                changed |= mergePrediction(prediction);
            }
            break;
        }
            
        case GetGlobalVar: {
            PredictedType prediction = m_graph.getGlobalVarPrediction(node.varNumber());
            if (prediction)
                changed |= mergePrediction(prediction);
            break;
        }
            
        case PutGlobalVar: {
            changed |= m_graph.predictGlobalVar(node.varNumber(), m_graph[node.child1()].prediction());
            break;
        }
            
        case GetScopedVar:
        case Resolve:
        case ResolveBase:
        case ResolveBaseStrictPut:
        case ResolveGlobal: {
            PredictedType prediction = node.getHeapPrediction();
            if (prediction)
                changed |= mergePrediction(prediction);
            break;
        }
            
        case GetScopeChain: {
            changed |= setPrediction(PredictCellOther);
            break;
        }
            
        case GetCallee: {
            changed |= setPrediction(PredictFunction);
            break;
        }
            
        case CreateThis:
        case NewObject: {
            changed |= setPrediction(PredictFinalObject);
            break;
        }
            
        case NewArray:
        case NewArrayBuffer: {
            changed |= setPrediction(PredictArray);
            break;
        }
            
        case NewRegexp: {
            changed |= setPrediction(PredictObjectOther);
            break;
        }
        
        case StringCharAt:
        case StrCat: {
            changed |= setPrediction(PredictString);
            break;
        }
            
        case ToPrimitive: {
            PredictedType child = m_graph[node.child1()].prediction();
            if (child) {
                if (isObjectPrediction(child)) {
                    // I'd love to fold this case into the case below, but I can't, because
                    // removing PredictObjectMask from something that only has an object
                    // prediction and nothing else means we have an ill-formed PredictedType
                    // (strong predict-none). This should be killed once we remove all traces
                    // of static (aka weak) predictions.
                    changed |= mergePrediction(PredictString);
                } else if (child & PredictObjectMask) {
                    // Objects get turned into strings. So if the input has hints of objectness,
                    // the output will have hinsts of stringiness.
                    changed |= mergePrediction(mergePredictions(child & ~PredictObjectMask, PredictString));
                } else
                    changed |= mergePrediction(child);
            }
            break;
        }
            
        case ValueToDouble:
        case GetArrayLength:
        case GetByteArrayLength:
        case GetStringLength: {
            // This node should never be visible at this stage of compilation. It is
            // inserted by fixup(), which follows this phase.
            ASSERT_NOT_REACHED();
            break;
        }
        
#ifndef NDEBUG
        // These get ignored because they don't return anything.
        case PutScopedVar:
        case DFG::Jump:
        case Branch:
        case Breakpoint:
        case Return:
        case CheckHasInstance:
        case Phi:
        case Flush:
        case Throw:
        case ThrowReferenceError:
        case ForceOSRExit:
        case SetArgument:
        case PutByVal:
        case PutByValAlias:
        case PutById:
        case PutByIdDirect:
        case CheckStructure:
        case CheckFunction:
        case PutStructure:
        case PutByOffset:
            break;
            
        // These gets ignored because it doesn't do anything.
        case Phantom:
        case InlineStart:
            break;
#else
        default:
            break;
#endif
        }

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("%s\n", predictionToString(m_graph[m_compileIndex].prediction()));
#endif
        
        m_changed |= changed;
    }
    
    void propagatePredictionsForward()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("Propagating predictions forward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex)
            propagateNodePredictions(m_graph[m_compileIndex]);
    }
    
    void propagatePredictionsBackward()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("Propagating predictions backward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = m_graph.size(); m_compileIndex-- > 0;)
            propagateNodePredictions(m_graph[m_compileIndex]);
    }
    
    void propagatePredictions()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
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
    }
    
    void toDouble(NodeIndex nodeIndex)
    {
        if (m_graph[nodeIndex].op == ValueToNumber) {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
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

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("   %s @%u: ", Graph::opName(op), m_compileIndex);
#endif
        
        switch (op) {
        case ValueAdd: {
            if (!nodeCanSpeculateInteger(node.arithNodeFlags())) {
                toDouble(node.child1());
                toDouble(node.child2());
                break;
            }
            
            PredictedType left = m_graph[node.child1()].prediction();
            PredictedType right = m_graph[node.child2()].prediction();
            
            if (left && right && isNumberPrediction(left) && isNumberPrediction(right)) {
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
        case ArithMod:
        case ArithDiv: {
            if (!nodeCanSpeculateInteger(node.arithNodeFlags())) {
                toDouble(node.child1());
                toDouble(node.child2());
                break;
            }
            
            PredictedType left = m_graph[node.child1()].prediction();
            PredictedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (left & PredictDouble)
                    toDouble(node.child2());
                if (right & PredictDouble)
                    toDouble(node.child1());
            }
            break;
        }
            
        case ArithAbs: {
            if (!nodeCanSpeculateInteger(node.arithNodeFlags())) {
                toDouble(node.child1());
                break;
            }
            
            PredictedType prediction = m_graph[node.child1()].prediction();
            if (prediction & PredictDouble)
                toDouble(node.child1());
            break;
        }
            
        case ArithSqrt: {
            toDouble(node.child1());
            break;
        }
            
        case GetById: {
            bool isArray = isArrayPrediction(m_graph[node.child1()].prediction());
            bool isString = isStringPrediction(m_graph[node.child1()].prediction());
            bool isByteArray = m_graph[node.child1()].shouldSpeculateByteArray();
            if (!isInt32Prediction(m_graph[m_compileIndex].prediction()))
                break;
            if (!isArray && !isString && !isByteArray)
                break;
            if (m_codeBlock->identifier(node.identifierNumber()) != m_globalData.propertyNames->length)
                break;
            
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            printf("  @%u -> %s", m_compileIndex, isArray ? "GetArrayLength" : "GetStringLength");
#endif
            if (isArray)
                node.op = GetArrayLength;
            else if (isString)
                node.op = GetStringLength;
            else if (isByteArray)
                node.op = GetByteArrayLength;
            else
                ASSERT_NOT_REACHED();
            break;
        }
            
        default:
            break;
        }

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("\n");
#endif
    }
    
    void fixup()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
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
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
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
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
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
        PredictedType left = m_graph[node.child1()].prediction();
        PredictedType right = m_graph[node.child2()].prediction();
        return isNumberPrediction(left) && isNumberPrediction(right);
    }
    
    bool logicalNotIsPure(Node& node)
    {
        PredictedType prediction = m_graph[node.child1()].prediction();
        return isBooleanPrediction(prediction) || !prediction;
    }
    
    bool byValHasIntBase(Node& node)
    {
        PredictedType prediction = m_graph[node.child2()].prediction();
        return (prediction & PredictInt32) || !prediction;
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
        case GetByVal:
            return !byValHasIntBase(node);
        default:
            ASSERT_NOT_REACHED();
            return true; // If by some oddity we hit this case in release build it's safer to have CSE assume the worst.
        }
    }
    
    NodeIndex impureCSE(Node& node)
    {
        NodeIndex child1 = canonicalize(node.child1());
        NodeIndex child2 = canonicalize(node.child2());
        NodeIndex child3 = canonicalize(node.child3());
        
        NodeIndex start = startIndex();
        for (NodeIndex index = m_compileIndex; index-- > start;) {
            Node& otherNode = m_graph[index];
            if (node.op == otherNode.op
                && node.arithNodeFlagsForCompare() == otherNode.arithNodeFlagsForCompare()) {
                NodeIndex otherChild = canonicalize(otherNode.child1());
                if (otherChild == NoNode)
                    return index;
                if (otherChild == child1) {
                    otherChild = canonicalize(otherNode.child2());
                    if (otherChild == NoNode)
                        return index;
                    if (otherChild == child2) {
                        otherChild = canonicalize(otherNode.child3());
                        if (otherChild == NoNode)
                            return index;
                        if (otherChild == child3)
                            return index;
                    }
                }
            }
            if (clobbersWorld(index))
                break;
        }
        return NoNode;
    }
    
    NodeIndex globalVarLoadElimination(unsigned varNumber, JSGlobalObject* globalObject)
    {
        NodeIndex start = startIndexForChildren();
        for (NodeIndex index = m_compileIndex; index-- > start;) {
            Node& node = m_graph[index];
            switch (node.op) {
            case GetGlobalVar:
                if (node.varNumber() == varNumber && m_codeBlock->globalObjectFor(node.codeOrigin) == globalObject)
                    return index;
                break;
            case PutGlobalVar:
                if (node.varNumber() == varNumber && m_codeBlock->globalObjectFor(node.codeOrigin) == globalObject)
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
            case PutStructure:
            case PutByOffset:
                // GetByVal currently always speculates that it's accessing an
                // array with an integer index, which means that it's impossible
                // for a structure change or a put to property storage to affect
                // the GetByVal.
                break;
            case ArrayPush:
                // A push cannot affect previously existing elements in the array.
                break;
            default:
                if (clobbersWorld(index))
                    return NoNode;
                break;
            }
        }
        return NoNode;
    }

    bool checkFunctionElimination(JSFunction* function, NodeIndex child1)
    {
        NodeIndex start = startIndexForChildren(child1);
        for (NodeIndex index = m_compileIndex; index-- > start;) {
            Node& node = m_graph[index];
            switch (node.op) {
            case CheckFunction:
                if (node.child1() == child1 && node.function() == function)
                    return true;
                break;
                
            default:
                if (clobbersWorld(index))
                    return false;
                break;
            }
        }
        return false;
    }

    bool checkStructureLoadElimination(const StructureSet& structureSet, NodeIndex child1)
    {
        NodeIndex start = startIndexForChildren(child1);
        for (NodeIndex index = m_compileIndex; index-- > start;) {
            Node& node = m_graph[index];
            switch (node.op) {
            case CheckStructure:
                if (node.child1() == child1
                    && structureSet.isSupersetOf(node.structureSet()))
                    return true;
                break;
                
            case PutStructure:
                if (node.child1() == child1
                    && structureSet.contains(node.structureTransitionData().newStructure))
                    return true;
                if (structureSet.contains(node.structureTransitionData().previousStructure))
                    return false;
                break;
                
            case PutByOffset:
                // Setting a property cannot change the structure.
                break;
                
            case PutByVal:
            case PutByValAlias:
                if (byValHasIntBase(node)) {
                    // If PutByVal speculates that it's accessing an array with an
                    // integer index, then it's impossible for it to cause a structure
                    // change.
                    break;
                }
                return false;
                
            default:
                if (clobbersWorld(index))
                    return false;
                break;
            }
        }
        return false;
    }
    
    NodeIndex getByOffsetLoadElimination(unsigned identifierNumber, NodeIndex child1)
    {
        NodeIndex start = startIndexForChildren(child1);
        for (NodeIndex index = m_compileIndex; index-- > start;) {
            Node& node = m_graph[index];
            switch (node.op) {
            case GetByOffset:
                if (node.child1() == child1
                    && m_graph.m_storageAccessData[node.storageAccessDataIndex()].identifierNumber == identifierNumber)
                    return index;
                break;
                
            case PutByOffset:
                if (m_graph.m_storageAccessData[node.storageAccessDataIndex()].identifierNumber == identifierNumber) {
                    if (node.child2() == child1)
                        return node.child3();
                    return NoNode;
                }
                break;
                
            case PutStructure:
                // Changing the structure cannot change the outcome of a property get.
                break;
                
            case PutByVal:
            case PutByValAlias:
                if (byValHasIntBase(node)) {
                    // If PutByVal speculates that it's accessing an array with an
                    // integer index, then it's impossible for it to cause a structure
                    // change.
                    break;
                }
                return NoNode;
                
            default:
                if (clobbersWorld(index))
                    return NoNode;
                break;
            }
        }
        return NoNode;
    }
    
    NodeIndex getPropertyStorageLoadElimination(NodeIndex child1)
    {
        NodeIndex start = startIndexForChildren(child1);
        for (NodeIndex index = m_compileIndex; index-- > start;) {
            Node& node = m_graph[index];
            switch (node.op) {
            case GetPropertyStorage:
                if (node.child1() == child1)
                    return index;
                break;
                
            case PutByOffset:
            case PutStructure:
                // Changing the structure or putting to the storage cannot
                // change the property storage pointer.
                break;
                
            case PutByVal:
            case PutByValAlias:
                if (byValHasIntBase(node)) {
                    // If PutByVal speculates that it's accessing an array with an
                    // integer index, then it's impossible for it to cause a structure
                    // change.
                    break;
                }
                return NoNode;
                
            default:
                if (clobbersWorld(index))
                    return NoNode;
                break;
            }
        }
        return NoNode;
    }
    
    NodeIndex getScopeChainLoadElimination(unsigned depth)
    {
        NodeIndex start = startIndexForChildren();
        for (NodeIndex index = endIndexForPureCSE(); index-- > start;) {
            Node& node = m_graph[index];
            if (node.op == GetScopeChain
                && node.scopeChainDepth() == depth)
                return index;
        }
        return NoNode;
    }
    
    void performSubstitution(NodeIndex& child, bool addRef = true)
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
        
        if (addRef)
            m_graph[child].ref();
    }
    
    void setReplacement(NodeIndex replacement)
    {
        if (replacement == NoNode)
            return;
        
        // Be safe. Don't try to perform replacements if the predictions don't
        // agree.
        if (m_graph[m_compileIndex].prediction() != m_graph[replacement].prediction())
            return;
        
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("   Replacing @%u -> @%u", m_compileIndex, replacement);
#endif
        
        Node& node = m_graph[m_compileIndex];
        node.op = Phantom;
        node.setRefCount(1);
        
        // At this point we will eliminate all references to this node.
        m_replacements[m_compileIndex] = replacement;
    }
    
    void eliminate()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("   Eliminating @%u", m_compileIndex);
#endif
        
        Node& node = m_graph[m_compileIndex];
        ASSERT(node.refCount() == 1);
        ASSERT(node.mustGenerate());
        node.op = Phantom;
    }
    
    void performNodeCSE(Node& node)
    {
        bool shouldGenerate = node.shouldGenerate();

        if (node.op & NodeHasVarArgs) {
            for (unsigned childIdx = node.firstChild(); childIdx < node.firstChild() + node.numChildren(); childIdx++)
                performSubstitution(m_graph.m_varArgChildren[childIdx], shouldGenerate);
        } else {
            performSubstitution(node.children.fixed.child1, shouldGenerate);
            performSubstitution(node.children.fixed.child2, shouldGenerate);
            performSubstitution(node.children.fixed.child3, shouldGenerate);
        }
        
        if (!shouldGenerate)
            return;
        
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("   %s @%u: ", Graph::opName(m_graph[m_compileIndex].op), m_compileIndex);
#endif
        
        // NOTE: there are some nodes that we deliberately don't CSE even though we
        // probably could, like StrCat and ToPrimitive. That's because there is no
        // evidence that doing CSE on these nodes would result in a performance
        // progression. Hence considering these nodes in CSE would just mean that this
        // code does more work with no win. Of course, we may want to reconsider this,
        // since StrCat is trivially CSE-able. It's not trivially doable for
        // ToPrimitive, but we could change that with some speculations if we really
        // needed to.
        
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
        case GetByteArrayLength:
        case GetCallee:
        case GetStringLength:
        case StringCharAt:
        case StringCharCodeAt:
            setReplacement(pureCSE(node));
            break;
            
        case GetArrayLength:
            setReplacement(impureCSE(node));
            break;
            
        case GetScopeChain:
            setReplacement(getScopeChainLoadElimination(node.scopeChainDepth()));
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
            setReplacement(globalVarLoadElimination(node.varNumber(), m_codeBlock->globalObjectFor(node.codeOrigin)));
            break;
            
        case GetByVal:
            if (byValHasIntBase(node))
                setReplacement(getByValLoadElimination(node.child1(), node.child2()));
            break;
            
        case PutByVal:
            if (byValHasIntBase(node) && getByValLoadElimination(node.child1(), node.child2()) != NoNode)
                node.op = PutByValAlias;
            break;
            
        case CheckStructure:
            if (checkStructureLoadElimination(node.structureSet(), node.child1()))
                eliminate();
            break;

        case CheckFunction:
            if (checkFunctionElimination(node.function(), node.child1()))
                eliminate();
            break;

        case GetPropertyStorage:
            setReplacement(getPropertyStorageLoadElimination(node.child1()));
            break;
            
        case GetByOffset:
            setReplacement(getByOffsetLoadElimination(m_graph.m_storageAccessData[node.storageAccessDataIndex()].identifierNumber, node.child1()));
            break;
            
        default:
            // do nothing.
            break;
        }
        
        m_lastSeen[node.op & NodeIdMask] = m_compileIndex;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
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
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("Performing local CSE:");
#endif
        for (unsigned block = 0; block < m_graph.m_blocks.size(); ++block)
            performBlockCSE(*m_graph.m_blocks[block]);
    }
    
    void allocateVirtualRegisters()
    {
#if DFG_ENABLE(DEBUG_VERBOSE)
        printf("Preserved vars: ");
        m_graph.m_preservedVars.dump(stdout);
        printf("\n");
#endif
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
        unsigned calleeRegisters = scoreBoard.highWatermark() + m_graph.m_parameterSlots;
        if ((unsigned)m_codeBlock->m_numCalleeRegisters < calleeRegisters)
            m_codeBlock->m_numCalleeRegisters = calleeRegisters;
#if DFG_ENABLE(DEBUG_VERBOSE)
        printf("Num callee registers: %u\n", calleeRegisters);
#endif
    }
    
    void performBlockCFA(AbstractState& state, BlockIndex blockIndex)
    {
        BasicBlock* block = m_graph.m_blocks[blockIndex].get();
        if (!block->cfaShouldRevisit)
            return;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("   Block #%u (bc#%u):\n", blockIndex, block->bytecodeBegin);
#endif
        state.beginBasicBlock(block);
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("      head vars: ");
        dumpOperands(block->valuesAtHead, stdout);
        printf("\n");
#endif
        for (NodeIndex nodeIndex = block->begin; nodeIndex < block->end; ++nodeIndex) {
            if (!m_graph[nodeIndex].shouldGenerate())
                continue;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            printf("      %s @%u: ", Graph::opName(m_graph[nodeIndex].op), nodeIndex);
            state.dump(stdout);
            printf("\n");
#endif
            if (!state.execute(nodeIndex))
                break;
        }
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("      tail regs: ");
        state.dump(stdout);
        printf("\n");
#endif
        m_changed |= state.endBasicBlock(AbstractState::MergeToSuccessors);
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("      tail vars: ");
        dumpOperands(block->valuesAtTail, stdout);
        printf("\n");
#endif
    }
    
    void performForwardCFA(AbstractState& state)
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        printf("CFA [%u]\n", ++m_count);
#endif
        
        for (BlockIndex block = 0; block < m_graph.m_blocks.size(); ++block)
            performBlockCFA(state, block);
    }
    
    void globalCFA()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        m_count = 0;
#endif
        
        // This implements a pseudo-worklist-based forward CFA, except that the visit order
        // of blocks is the bytecode program order (which is nearly topological), and
        // instead of a worklist we just walk all basic blocks checking if cfaShouldRevisit
        // is set to true. This is likely to balance the efficiency properties of both
        // worklist-based and forward fixpoint-based approaches. Like a worklist-based
        // approach, it won't visit code if it's meaningless to do so (nothing changed at
        // the head of the block or the predecessors have not been visited). Like a forward
        // fixpoint-based approach, it has a high probability of only visiting a block
        // after all predecessors have been visited. Only loops will cause this analysis to
        // revisit blocks, and the amount of revisiting is proportional to loop depth.
        
        AbstractState::initialize(m_graph);
        
        AbstractState state(m_codeBlock, m_graph);
        
        do {
            m_changed = false;
            performForwardCFA(state);
        } while (m_changed);
    }
    
    Graph& m_graph;
    JSGlobalData& m_globalData;
    CodeBlock* m_codeBlock;
    CodeBlock* m_profiledBlock;
    
    NodeIndex m_start;
    NodeIndex m_compileIndex;
    
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
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

