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

#if ENABLE(DFG_JIT) && ENABLE(DYNAMIC_OPTIMIZATION)

#include "DFGGraph.h"

namespace JSC { namespace DFG {

class Propagator {
public:
    Propagator(Graph& graph, JSGlobalData& globalData, CodeBlock* codeBlock, CodeBlock* profiledBlock)
        : m_graph(graph)
        , m_globalData(globalData)
        , m_codeBlock(codeBlock)
        , m_profiledBlock(profiledBlock)
    {
        m_predictions.resize(m_graph.size());
        for (NodeIndex i = 0; i < m_graph.size(); ++i)
            m_predictions[i] = PredictNone;
    }
    
    void fixpoint()
    {
#if DFG_DEBUG_VERBOSE
        m_count = 0;
#endif
        do {
            // Forward propagation is near-optimal for both topologically-sorted and
            // DFS-sorted code.
            m_changed = false;
            propagateForward();
            if (!m_changed)
                break;
            
            // Backward propagation reduces the likelihood that pathological code will
            // cause slowness. Loops (especially nested ones) resemble backward flow.
            // This pass captures two cases: (1) it detects if the forward fixpoint
            // found a sound solution and (2) short-circuits backward flow.
            m_changed = false;
            propagateBackward();
            if (!m_changed)
                break;
            
            // Do another forward pass because forward passes are on average more
            // profitable.
            m_changed = false;
            propagateForward();
        } while (m_changed);
    }
    
private:
    void setPrediction(PredictedType prediction)
    {
        if (m_predictions[m_compileIndex] == prediction)
            return;
        
        m_predictions[m_compileIndex] = prediction;
        m_changed = true;
    }
    
    void mergePrediction(PredictedType prediction)
    {
        if (DFG::mergePrediction(m_predictions[m_compileIndex], prediction))
            m_changed = true;
    }
    
    void propagateNode(Node& node)
    {
        if (!node.shouldGenerate())
            return;
        
        NodeType op = node.op;
        switch (op) {
        case JSConstant: {
            JSValue value = node.valueOfJSConstant(m_codeBlock);
            if (value.isInt32())
                setPrediction(makePrediction(PredictInt32, StrongPrediction));
            else if (value.isDouble())
                setPrediction(makePrediction(PredictDouble, StrongPrediction));
            else if (value.isCell()) {
                JSCell* cell = value.asCell();
                if (isJSArray(&m_globalData, cell))
                    setPrediction(makePrediction(PredictArray, StrongPrediction));
                else
                    setPrediction(makePrediction(PredictCell, StrongPrediction));
            } else
                setPrediction(makePrediction(PredictTop, StrongPrediction));
            break;
        }
            
        case GetLocal: {
            PredictedType prediction = m_graph.getPrediction(node.local());
            if (isStrongPrediction(prediction))
                mergePrediction(prediction);
            break;
        }
            
        case SetLocal: {
            m_changed |= m_graph.predict(node.local(), m_predictions[node.child1()] & ~PredictionTagMask, StrongPrediction);
            break;
        }
            
        case BitAnd:
        case BitOr:
        case BitXor:
        case BitRShift:
        case BitLShift:
        case BitURShift:
        case UInt32ToNumber:
        case ValueToInt32:
        case ArithMod: {
            setPrediction(makePrediction(PredictInt32, StrongPrediction));
            break;
        }
            
        case ValueToNumber: {
            PredictedType prediction = m_predictions[node.child1()];
            
            if (isStrongPrediction(prediction)) {
                if (isNumberPrediction(prediction))
                    mergePrediction(prediction);
                else
                    mergePrediction(makePrediction(PredictNumber, StrongPrediction));
            }
            break;
        }
            
        case ValueAdd: {
            PredictedType left = m_predictions[node.child1()];
            PredictedType right = m_predictions[node.child2()];
            
            if (isStrongPrediction(left) && isStrongPrediction(right)) {
                if (isNumberPrediction(left) && isNumberPrediction(right)) {
                    if (isInt32Prediction(mergePredictions(left, right)))
                        mergePrediction(makePrediction(PredictInt32, StrongPrediction));
                    else
                        mergePrediction(makePrediction(PredictDouble, StrongPrediction));
                } else
                    mergePrediction(makePrediction(PredictTop, StrongPrediction));
            }
            break;
        }
            
        case ArithAdd:
        case ArithSub:
        case ArithMul: {
            PredictedType left = m_predictions[node.child1()];
            PredictedType right = m_predictions[node.child2()];
            
            if (isStrongPrediction(left) && isStrongPrediction(right)) {
                if (isInt32Prediction(mergePredictions(left, right)))
                    mergePrediction(makePrediction(PredictInt32, StrongPrediction));
                else
                    mergePrediction(makePrediction(PredictDouble, StrongPrediction));
            }
            break;
        }
            
        case ArithDiv: {
            setPrediction(makePrediction(PredictDouble, StrongPrediction));
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
            // This is sad, as we don't have a boolean prediction, even though we easily
            // could.
            setPrediction(makePrediction(PredictTop, StrongPrediction));
            break;
        }
            
        case GetById:
        case GetMethod:
        case GetByVal:
        case Call:
        case Construct: {
            if (isStrongPrediction(node.getPrediction()))
                setPrediction(node.getPrediction());
            break;
        }
            
        case ConvertThis: {
            setPrediction(makePrediction(PredictCell, StrongPrediction));
            break;
        }
            
        case GetGlobalVar: {
            PredictedType prediction = m_graph.getGlobalVarPrediction(node.varNumber());
            if (isStrongPrediction(prediction))
                mergePrediction(prediction);
            break;
        }
            
        case PutGlobalVar: {
            m_changed |= m_graph.predictGlobalVar(node.varNumber(), m_predictions[node.child1()] & ~PredictionTagMask, StrongPrediction);
            break;
        }
            
#ifndef NDEBUG
        // These get ignored because they don't return anything.
        case PutByVal:
        case PutByValAlias:
        case DFG::Jump:
        case Branch:
        case Return:
        case PutById:
        case PutByIdDirect:
        case CheckHasInstance:
        case Phi:
            break;
            
        // These get ignored because we don't have profiling for them, yet.
        case Resolve:
        case ResolveBase:
        case ResolveBaseStrictPut:
            break;
            
        default:
            ASSERT_NOT_REACHED();
            break;
#else
        default:
            break;
#endif
        }
    }
    
    void propagateForward()
    {
#if DFG_DEBUG_VERBOSE
        printf("Propagating forward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex)
            propagateNode(m_graph[m_compileIndex]);
    }
    
    void propagateBackward()
    {
#if DFG_DEBUG_VERBOSE
        printf("Propagating backward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = m_graph.size(); m_compileIndex-- > 0;)
            propagateNode(m_graph[m_compileIndex]);
    }
    
    Graph& m_graph;
    JSGlobalData& m_globalData;
    CodeBlock* m_codeBlock;
    CodeBlock* m_profiledBlock;
    
    NodeIndex m_compileIndex;
    
    Vector<PredictedType, 16> m_predictions;

#if DFG_DEBUG_VERBOSE
    unsigned m_count;
#endif
    
    bool m_changed;
};

void propagate(Graph& graph, JSGlobalData* globalData, CodeBlock* codeBlock)
{
    ASSERT(codeBlock);
    CodeBlock* profiledBlock = codeBlock->alternative();
    ASSERT(profiledBlock);
    
    Propagator propagator(graph, *globalData, codeBlock, profiledBlock);
    propagator.fixpoint();
    
#if DFG_DEBUG_VERBOSE
    graph.dump(codeBlock);
#endif
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT) && ENABLE(DYNAMIC_OPTIMIZATION)

