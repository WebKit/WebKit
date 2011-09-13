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
        
        for (unsigned i = 0; i < m_graph.size(); ++i) {
            m_predictions[i] = PredictNone;
            m_uses[i] = PredictNone;
        }
    }
    
    void fixpoint()
    {
#if ENABLE(DFG_DEBUG_VERBOSE)
        m_count = 0;
#endif
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
    }
    
private:
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
    
    void propagateNode(Node& node)
    {
        if (!node.shouldGenerate())
            return;
        
        NodeType op = node.op;

#if ENABLE(DFG_DEBUG_VERBOSE)
        printf("   %s[%u]: ", Graph::opName(op), m_compileIndex);
#endif
        
        bool changed = false;
        
        switch (op) {
        case JSConstant: {
            JSValue value = node.valueOfJSConstant(m_codeBlock);
            if (value.isInt32())
                changed |= setPrediction(makePrediction(PredictInt32, StrongPrediction));
            else if (value.isDouble())
                changed |= setPrediction(makePrediction(PredictDouble, StrongPrediction));
            else if (value.isCell()) {
                JSCell* cell = value.asCell();
                if (isJSArray(&m_globalData, cell))
                    changed |= setPrediction(makePrediction(PredictArray, StrongPrediction));
                else
                    changed |= setPrediction(makePrediction(PredictCell, StrongPrediction));
            } else if (value.isBoolean())
                changed |= setPrediction(makePrediction(PredictBoolean, StrongPrediction));
            else
                changed |= setPrediction(makePrediction(PredictTop, StrongPrediction));
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
        case UInt32ToNumber:
        case ValueToInt32:
        case ArithMod: {
            changed |= setPrediction(makePrediction(PredictInt32, StrongPrediction));
            break;
        }
            
        case ValueToNumber: {
            PredictedType prediction = m_predictions[node.child1()];
            
            if (isStrongPrediction(prediction)) {
                if (isNumberPrediction(prediction))
                    changed |= mergePrediction(prediction);
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
                    if (isInt32Prediction(mergePredictions(left, right)))
                        changed |= mergePrediction(makePrediction(PredictInt32, StrongPrediction));
                    else
                        changed |= mergePrediction(makePrediction(PredictDouble, StrongPrediction));
                } else
                    changed |= mergePrediction(makePrediction(PredictTop, StrongPrediction));
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
                    changed |= mergePrediction(makePrediction(PredictInt32, StrongPrediction));
                else
                    changed |= mergePrediction(makePrediction(PredictDouble, StrongPrediction));
            }
            break;
        }
            
        case ArithDiv: {
            changed |= setPrediction(makePrediction(PredictDouble, StrongPrediction));
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
            changed |= mergeUse(node.child1(), PredictCell | StrongPredictionTag);
            changed |= node.predict(m_uses[m_compileIndex] & ~PredictionTagMask, StrongPrediction);
            if (isStrongPrediction(node.getPrediction()))
                changed |= setPrediction(node.getPrediction());
            break;
        }
            
        case Call:
        case Construct: {
            changed |= mergeUse(m_graph.m_varArgChildren[node.firstChild()], PredictCell | StrongPredictionTag);
            changed |= node.predict(m_uses[m_compileIndex] & ~PredictionTagMask, StrongPrediction);
            if (isStrongPrediction(node.getPrediction()))
                changed |= setPrediction(node.getPrediction());
            break;
        }
            
        case ConvertThis: {
            changed |= setPrediction(makePrediction(PredictCell, StrongPrediction));
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
        case PutByIdDirect:
            changed |= mergeUse(node.child1(), PredictCell | StrongPredictionTag);
            break;

#ifndef NDEBUG
        // These get ignored because they don't return anything.
        case DFG::Jump:
        case Branch:
        case Return:
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

#if ENABLE(DFG_DEBUG_VERBOSE)
        printf("expect(%s) use(%s) %s\n", predictionToString(m_predictions[m_compileIndex]), predictionToString(m_uses[m_compileIndex]), changed ? "CHANGED" : "");
#endif
        
        m_changed |= changed;
    }
    
    void propagateForward()
    {
#if ENABLE(DFG_DEBUG_VERBOSE)
        printf("Propagating forward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex)
            propagateNode(m_graph[m_compileIndex]);
    }
    
    void propagateBackward()
    {
#if ENABLE(DFG_DEBUG_VERBOSE)
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
    Vector<PredictedType, 16> m_uses;
    
    PredictionTracker m_variableUses;

#if ENABLE(DFG_DEBUG_VERBOSE)
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
    
#if ENABLE(DFG_DEBUG_VERBOSE)
    graph.dump(codeBlock);
#endif
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT) && ENABLE(DYNAMIC_OPTIMIZATION)

