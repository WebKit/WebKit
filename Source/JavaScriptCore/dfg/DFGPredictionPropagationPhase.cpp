/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
#include "DFGPredictionPropagationPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGPhase.h"

namespace JSC { namespace DFG {

class PredictionPropagationPhase : public Phase {
public:
    PredictionPropagationPhase(Graph& graph)
        : Phase(graph, "prediction propagation")
    {
    }
    
    void run()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        m_count = 0;
#endif
        // 1) propagate predictions

        do {
            m_changed = false;
            
            // Forward propagation is near-optimal for both topologically-sorted and
            // DFS-sorted code.
            propagateForward();
            if (!m_changed)
                break;
            
            // Backward propagation reduces the likelihood that pathological code will
            // cause slowness. Loops (especially nested ones) resemble backward flow.
            // This pass captures two cases: (1) it detects if the forward fixpoint
            // found a sound solution and (2) short-circuits backward flow.
            m_changed = false;
            propagateBackward();
        } while (m_changed);
        
        // 2) repropagate predictions while doing double voting.

        do {
            m_changed = false;
            doRoundOfDoubleVoting();
            propagateForward();
            if (!m_changed)
                break;
            
            m_changed = false;
            doRoundOfDoubleVoting();
            propagateBackward();
        } while (m_changed);
    }
    
private:
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
    
    bool isNotNegZero(NodeIndex nodeIndex)
    {
        if (!m_graph.isNumberConstant(nodeIndex))
            return false;
        double value = m_graph.valueOfNumberConstant(nodeIndex);
        return !value && 1.0 / value < 0.0;
    }
    
    bool isNotZero(NodeIndex nodeIndex)
    {
        if (!m_graph.isNumberConstant(nodeIndex))
            return false;
        return !!m_graph.valueOfNumberConstant(nodeIndex);
    }
    
    void propagate(Node& node)
    {
        if (!node.shouldGenerate())
            return;
        
        NodeType op = node.op();
        NodeFlags flags = node.flags() & NodeBackPropMask;

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("   %s @%u: %s ", Graph::opName(op), m_compileIndex, nodeFlagsAsString(flags));
#endif
        
        bool changed = false;
        
        switch (op) {
        case JSConstant:
        case WeakJSConstant: {
            changed |= setPrediction(predictionFromValue(m_graph.valueOfJSConstant(m_compileIndex)));
            break;
        }
            
        case GetLocal: {
            VariableAccessData* variableAccessData = node.variableAccessData();
            PredictedType prediction = variableAccessData->prediction();
            if (prediction)
                changed |= mergePrediction(prediction);
            
            changed |= variableAccessData->mergeFlags(flags);
            break;
        }
            
        case SetLocal: {
            VariableAccessData* variableAccessData = node.variableAccessData();
            changed |= variableAccessData->predict(m_graph[node.child1()].prediction());
            changed |= m_graph[node.child1()].mergeFlags(variableAccessData->flags());
            break;
        }
            
        case Flush: {
            // Make sure that the analysis knows that flushed locals escape.
            VariableAccessData* variableAccessData = node.variableAccessData();
            changed |= variableAccessData->mergeFlags(NodeUsedAsValue);
            break;
        }
            
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitRShift:
        case BitLShift:
        case BitURShift: {
            changed |= setPrediction(PredictInt32);
            flags |= NodeUsedAsInt;
            flags &= ~(NodeUsedAsNumber | NodeNeedsNegZero);
            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ValueToInt32: {
            changed |= setPrediction(PredictInt32);
            flags |= NodeUsedAsInt;
            flags &= ~(NodeUsedAsNumber | NodeNeedsNegZero);
            changed |= m_graph[node.child1()].mergeFlags(flags);
            break;
        }
            
        case ArrayPop: {
            changed |= mergePrediction(node.getHeapPrediction());
            changed |= mergeDefaultFlags(node);
            break;
        }

        case ArrayPush: {
            changed |= mergePrediction(node.getHeapPrediction());
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[node.child2()].mergeFlags(NodeUsedAsValue);
            break;
        }

        case RegExpExec:
        case RegExpTest: {
            changed |= mergePrediction(node.getHeapPrediction());
            changed |= mergeDefaultFlags(node);
            break;
        }

        case StringCharCodeAt: {
            changed |= mergePrediction(PredictInt32);
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[node.child2()].mergeFlags(NodeUsedAsNumber | NodeUsedAsInt);
            break;
        }

        case ArithMod: {
            PredictedType left = m_graph[node.child1()].prediction();
            PredictedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (isInt32Prediction(mergePredictions(left, right))
                    && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(PredictInt32);
                else
                    changed |= mergePrediction(PredictDouble);
            }
            
            flags |= NodeUsedAsValue;
            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case UInt32ToNumber: {
            if (nodeCanSpeculateInteger(node.arithNodeFlags()))
                changed |= mergePrediction(PredictInt32);
            else
                changed |= mergePrediction(PredictNumber);
            
            changed |= m_graph[node.child1()].mergeFlags(flags);
            break;
        }

        case ValueAdd: {
            PredictedType left = m_graph[node.child1()].prediction();
            PredictedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (isNumberPrediction(left) && isNumberPrediction(right)) {
                    if (m_graph.addShouldSpeculateInteger(node))
                        changed |= mergePrediction(PredictInt32);
                    else
                        changed |= mergePrediction(PredictDouble);
                } else if (!(left & PredictNumber) || !(right & PredictNumber)) {
                    // left or right is definitely something other than a number.
                    changed |= mergePrediction(PredictString);
                } else
                    changed |= mergePrediction(PredictString | PredictInt32 | PredictDouble);
            }
            
            if (isNotNegZero(node.child1().index()) || isNotNegZero(node.child2().index()))
                flags &= ~NodeNeedsNegZero;
            
            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ArithAdd: {
            PredictedType left = m_graph[node.child1()].prediction();
            PredictedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (m_graph.addShouldSpeculateInteger(node))
                    changed |= mergePrediction(PredictInt32);
                else
                    changed |= mergePrediction(PredictDouble);
            }
            
            if (isNotNegZero(node.child1().index()) || isNotNegZero(node.child2().index()))
                flags &= ~NodeNeedsNegZero;
            
            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ArithSub: {
            PredictedType left = m_graph[node.child1()].prediction();
            PredictedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (m_graph.addShouldSpeculateInteger(node))
                    changed |= mergePrediction(PredictInt32);
                else
                    changed |= mergePrediction(PredictDouble);
            }

            if (isNotZero(node.child1().index()) || isNotZero(node.child2().index()))
                flags &= ~NodeNeedsNegZero;
            
            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ArithNegate:
            if (m_graph[node.child1()].prediction()) {
                if (m_graph.negateShouldSpeculateInteger(node))
                    changed |= mergePrediction(PredictInt32);
                else
                    changed |= mergePrediction(PredictDouble);
            }

            changed |= m_graph[node.child1()].mergeFlags(flags);
            break;
            
        case ArithMin:
        case ArithMax: {
            PredictedType left = m_graph[node.child1()].prediction();
            PredictedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (isInt32Prediction(mergePredictions(left, right))
                    && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(PredictInt32);
                else
                    changed |= mergePrediction(PredictDouble);
            }

            flags |= NodeUsedAsNumber;
            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ArithMul:
        case ArithDiv: {
            PredictedType left = m_graph[node.child1()].prediction();
            PredictedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (isInt32Prediction(mergePredictions(left, right))
                    && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(PredictInt32);
                else
                    changed |= mergePrediction(PredictDouble);
            }

            // As soon as a multiply happens, we can easily end up in the part
            // of the double domain where the point at which you do truncation
            // can change the outcome. So, ArithMul always checks for overflow
            // no matter what, and always forces its inputs to check as well.
            
            flags |= NodeUsedAsNumber | NodeNeedsNegZero;
            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ArithSqrt: {
            changed |= setPrediction(PredictDouble);
            changed |= m_graph[node.child1()].mergeFlags(flags | NodeUsedAsValue);
            break;
        }
            
        case ArithAbs: {
            PredictedType child = m_graph[node.child1()].prediction();
            if (nodeCanSpeculateInteger(node.arithNodeFlags()))
                changed |= mergePrediction(child);
            else
                changed |= setPrediction(PredictDouble);

            flags &= ~NodeNeedsNegZero;
            changed |= m_graph[node.child1()].mergeFlags(flags);
            break;
        }
            
        case LogicalNot:
        case CompareLess:
        case CompareLessEq:
        case CompareGreater:
        case CompareGreaterEq:
        case CompareEq:
        case CompareStrictEq:
        case InstanceOf:
        case IsUndefined:
        case IsBoolean:
        case IsNumber:
        case IsString:
        case IsObject:
        case IsFunction: {
            changed |= setPrediction(PredictBoolean);
            changed |= mergeDefaultFlags(node);
            break;
        }
            
        case GetById: {
            changed |= mergePrediction(node.getHeapPrediction());
            changed |= mergeDefaultFlags(node);
            break;
        }
            
        case GetByIdFlush:
            changed |= mergePrediction(node.getHeapPrediction());
            changed |= mergeDefaultFlags(node);
            break;
            
        case GetByVal: {
            if (m_graph[node.child1()].shouldSpeculateFloat32Array()
                || m_graph[node.child1()].shouldSpeculateFloat64Array())
                changed |= mergePrediction(PredictDouble);
            else
                changed |= mergePrediction(node.getHeapPrediction());

            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[node.child2()].mergeFlags(NodeUsedAsNumber | NodeUsedAsInt);
            break;
        }
            
        case GetPropertyStorage: 
        case GetIndexedPropertyStorage: {
            changed |= setPrediction(PredictOther);
            changed |= mergeDefaultFlags(node);
            break;
        }

        case GetByOffset: {
            changed |= mergePrediction(node.getHeapPrediction());
            changed |= mergeDefaultFlags(node);
            break;
        }
            
        case Call:
        case Construct: {
            changed |= mergePrediction(node.getHeapPrediction());
            for (unsigned childIdx = node.firstChild();
                 childIdx < node.firstChild() + node.numChildren();
                 ++childIdx) {
                Edge edge = m_graph.m_varArgChildren[childIdx];
                changed |= m_graph[edge].mergeFlags(NodeUsedAsValue);
            }
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
            changed |= mergeDefaultFlags(node);
            break;
        }
            
        case GetGlobalVar: {
            changed |= mergePrediction(node.getHeapPrediction());
            break;
        }
            
        case PutGlobalVar: {
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            break;
        }
            
        case GetScopedVar:
        case Resolve:
        case ResolveBase:
        case ResolveBaseStrictPut:
        case ResolveGlobal: {
            PredictedType prediction = node.getHeapPrediction();
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
            changed |= mergeDefaultFlags(node);
            break;
        }
            
        case NewArray: {
            changed |= setPrediction(PredictArray);
            for (unsigned childIdx = node.firstChild();
                 childIdx < node.firstChild() + node.numChildren();
                 ++childIdx) {
                Edge edge = m_graph.m_varArgChildren[childIdx];
                changed |= m_graph[edge].mergeFlags(NodeUsedAsValue);
            }
            break;
        }
            
        case NewArrayBuffer: {
            changed |= setPrediction(PredictArray);
            break;
        }
            
        case NewRegexp: {
            changed |= setPrediction(PredictObjectOther);
            break;
        }
        
        case StringCharAt: {
            changed |= setPrediction(PredictString);
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[node.child2()].mergeFlags(NodeUsedAsNumber | NodeUsedAsInt);
            break;
        }
            
        case StrCat: {
            changed |= setPrediction(PredictString);
            for (unsigned childIdx = node.firstChild();
                 childIdx < node.firstChild() + node.numChildren();
                 ++childIdx)
                changed |= m_graph[m_graph.m_varArgChildren[childIdx]].mergeFlags(NodeUsedAsNumber);
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
                    changed |= mergePrediction(
                        mergePredictions(child & ~PredictObjectMask, PredictString));
                } else
                    changed |= mergePrediction(child);
            }
            changed |= m_graph[node.child1()].mergeFlags(flags);
            break;
        }
            
        case CreateActivation: {
            changed |= setPrediction(PredictObjectOther);
            break;
        }
            
        case NewFunction:
        case NewFunctionNoCheck:
        case NewFunctionExpression: {
            changed |= setPrediction(PredictFunction);
            break;
        }
            
        case PutByValAlias:
        case GetArrayLength:
        case GetInt8ArrayLength:
        case GetInt16ArrayLength:
        case GetInt32ArrayLength:
        case GetUint8ArrayLength:
        case GetUint8ClampedArrayLength:
        case GetUint16ArrayLength:
        case GetUint32ArrayLength:
        case GetFloat32ArrayLength:
        case GetFloat64ArrayLength:
        case GetStringLength:
        case Int32ToDouble:
        case DoubleAsInt32: {
            // This node should never be visible at this stage of compilation. It is
            // inserted by fixup(), which follows this phase.
            ASSERT_NOT_REACHED();
            break;
        }
        
        case PutByVal:
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[node.child2()].mergeFlags(NodeUsedAsNumber | NodeUsedAsInt);
            changed |= m_graph[node.child3()].mergeFlags(NodeUsedAsValue);
            break;

        case PutScopedVar:
        case Return:
        case Throw:
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            break;

        case PutById:
        case PutByIdDirect:
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[node.child2()].mergeFlags(NodeUsedAsValue);
            break;

        case PutByOffset:
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[node.child3()].mergeFlags(NodeUsedAsValue);
            break;
            
        case Phi:
            break;

#ifndef NDEBUG
        // These get ignored because they don't return anything.
        case DFG::Jump:
        case Branch:
        case Breakpoint:
        case CheckHasInstance:
        case ThrowReferenceError:
        case ForceOSRExit:
        case SetArgument:
        case CheckStructure:
        case CheckFunction:
        case PutStructure:
        case TearOffActivation:
        case CheckNumber:
            changed |= mergeDefaultFlags(node);
            break;
            
        // These gets ignored because it doesn't do anything.
        case Phantom:
        case InlineStart:
        case Nop:
            break;
            
        case LastNodeType:
            ASSERT_NOT_REACHED();
            break;
#else
        default:
            changed |= mergeDefaultFlags(node);
            break;
#endif
        }

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("%s\n", predictionToString(m_graph[m_compileIndex].prediction()));
#endif
        
        m_changed |= changed;
    }
        
    bool mergeDefaultFlags(Node& node)
    {
        bool changed = false;
        if (node.flags() & NodeHasVarArgs) {
            for (unsigned childIdx = node.firstChild();
                 childIdx < node.firstChild() + node.numChildren();
                 childIdx++)
                changed |= m_graph[m_graph.m_varArgChildren[childIdx]].mergeFlags(NodeUsedAsValue);
        } else {
            if (!node.child1())
                return changed;
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            if (!node.child2())
                return changed;
            changed |= m_graph[node.child2()].mergeFlags(NodeUsedAsValue);
            if (!node.child3())
                return changed;
            changed |= m_graph[node.child3()].mergeFlags(NodeUsedAsValue);
        }
        return changed;
    }
    
    void propagateForward()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("Propagating predictions forward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex)
            propagate(m_graph[m_compileIndex]);
    }
    
    void propagateBackward()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("Propagating predictions backward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = m_graph.size(); m_compileIndex-- > 0;)
            propagate(m_graph[m_compileIndex]);
    }
    
    void vote(Edge nodeUse, VariableAccessData::Ballot ballot)
    {
        switch (m_graph[nodeUse].op()) {
        case ValueToInt32:
        case UInt32ToNumber:
            nodeUse = m_graph[nodeUse].child1();
            break;
        default:
            break;
        }
        
        if (m_graph[nodeUse].op() == GetLocal)
            m_graph[nodeUse].variableAccessData()->vote(ballot);
    }
    
    void vote(Node& node, VariableAccessData::Ballot ballot)
    {
        if (node.flags() & NodeHasVarArgs) {
            for (unsigned childIdx = node.firstChild();
                 childIdx < node.firstChild() + node.numChildren();
                 childIdx++)
                vote(m_graph.m_varArgChildren[childIdx], ballot);
            return;
        }
        
        if (!node.child1())
            return;
        vote(node.child1(), ballot);
        if (!node.child2())
            return;
        vote(node.child2(), ballot);
        if (!node.child3())
            return;
        vote(node.child3(), ballot);
    }
    
    void doRoundOfDoubleVoting()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("Voting on double uses of locals [%u]\n", m_count);
#endif
        for (unsigned i = 0; i < m_graph.m_variableAccessData.size(); ++i)
            m_graph.m_variableAccessData[i].find()->clearVotes();
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex) {
            Node& node = m_graph[m_compileIndex];
            switch (node.op()) {
            case ValueAdd:
            case ArithAdd:
            case ArithSub: {
                PredictedType left = m_graph[node.child1()].prediction();
                PredictedType right = m_graph[node.child2()].prediction();
                
                VariableAccessData::Ballot ballot;
                
                if (isNumberPrediction(left) && isNumberPrediction(right)
                    && !m_graph.addShouldSpeculateInteger(node))
                    ballot = VariableAccessData::VoteDouble;
                else
                    ballot = VariableAccessData::VoteValue;
                
                vote(node.child1(), ballot);
                vote(node.child2(), ballot);
                break;
            }
                
            case ArithMul:
            case ArithMin:
            case ArithMax:
            case ArithMod:
            case ArithDiv: {
                PredictedType left = m_graph[node.child1()].prediction();
                PredictedType right = m_graph[node.child2()].prediction();
                
                VariableAccessData::Ballot ballot;
                
                if (isNumberPrediction(left) && isNumberPrediction(right)
                    && !(Node::shouldSpeculateInteger(m_graph[node.child1()], m_graph[node.child1()])
                         && node.canSpeculateInteger()))
                    ballot = VariableAccessData::VoteDouble;
                else
                    ballot = VariableAccessData::VoteValue;
                
                vote(node.child1(), ballot);
                vote(node.child2(), ballot);
                break;
            }
                
            case ArithAbs:
                VariableAccessData::Ballot ballot;
                if (!(m_graph[node.child1()].shouldSpeculateInteger()
                      && node.canSpeculateInteger()))
                    ballot = VariableAccessData::VoteDouble;
                else
                    ballot = VariableAccessData::VoteValue;
                
                vote(node.child1(), ballot);
                break;
                
            case ArithSqrt:
                vote(node.child1(), VariableAccessData::VoteDouble);
                break;
                
            case SetLocal: {
                PredictedType prediction = m_graph[node.child1()].prediction();
                if (isDoublePrediction(prediction))
                    node.variableAccessData()->vote(VariableAccessData::VoteDouble);
                else if (!isNumberPrediction(prediction) || isInt32Prediction(prediction))
                    node.variableAccessData()->vote(VariableAccessData::VoteValue);
                break;
            }
                
            default:
                vote(node, VariableAccessData::VoteValue);
                break;
            }
        }
        for (unsigned i = 0; i < m_graph.m_variableAccessData.size(); ++i) {
            VariableAccessData* variableAccessData = &m_graph.m_variableAccessData[i];
            if (!variableAccessData->isRoot())
                continue;
            if (operandIsArgument(variableAccessData->local())
                || m_graph.isCaptured(variableAccessData->local()))
                continue;
            m_changed |= variableAccessData->tallyVotesForShouldUseDoubleFormat();
        }
        for (unsigned i = 0; i < m_graph.m_argumentPositions.size(); ++i)
            m_changed |= m_graph.m_argumentPositions[i].mergeArgumentAwareness();
        for (unsigned i = 0; i < m_graph.m_variableAccessData.size(); ++i) {
            VariableAccessData* variableAccessData = &m_graph.m_variableAccessData[i];
            if (!variableAccessData->isRoot())
                continue;
            if (operandIsArgument(variableAccessData->local())
                || m_graph.isCaptured(variableAccessData->local()))
                continue;
            m_changed |= variableAccessData->makePredictionForDoubleFormat();
        }
    }
    
    NodeIndex m_compileIndex;
    bool m_changed;

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
    unsigned m_count;
#endif
};
    
void performPredictionPropagation(Graph& graph)
{
    runPhase<PredictionPropagationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

