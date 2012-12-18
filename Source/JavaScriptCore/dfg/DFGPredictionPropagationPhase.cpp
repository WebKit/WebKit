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
    
    bool run()
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
        
        return true;
    }
    
private:
    bool setPrediction(SpeculatedType prediction)
    {
        ASSERT(m_graph[m_compileIndex].hasResult());
        
        // setPrediction() is used when we know that there is no way that we can change
        // our minds about what the prediction is going to be. There is no semantic
        // difference between setPrediction() and mergeSpeculation() other than the
        // increased checking to validate this property.
        ASSERT(m_graph[m_compileIndex].prediction() == SpecNone || m_graph[m_compileIndex].prediction() == prediction);
        
        return m_graph[m_compileIndex].predict(prediction);
    }
    
    bool mergePrediction(SpeculatedType prediction)
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
    
    bool isWithinPowerOfTwoForConstant(Node& node, int power)
    {
        JSValue immediateValue = node.valueOfJSConstant(codeBlock());
        if (!immediateValue.isInt32())
            return false;
        int32_t intImmediate = immediateValue.asInt32();
        return intImmediate > -(1 << power) && intImmediate < (1 << power);
    }
    
    bool isWithinPowerOfTwoNonRecursive(NodeIndex nodeIndex, int power)
    {
        Node& node = m_graph[nodeIndex];
        if (node.op() != JSConstant)
            return false;
        return isWithinPowerOfTwoForConstant(node, power);
    }
    
    bool isWithinPowerOfTwo(NodeIndex nodeIndex, int power)
    {
        Node& node = m_graph[nodeIndex];
        switch (node.op()) {
        case JSConstant: {
            return isWithinPowerOfTwoForConstant(node, power);
        }
            
        case BitAnd: {
            return isWithinPowerOfTwoNonRecursive(node.child1().index(), power)
                || isWithinPowerOfTwoNonRecursive(node.child2().index(), power);
        }
            
        case BitRShift:
        case BitURShift: {
            Node& shiftAmount = m_graph[node.child2()];
            if (shiftAmount.op() != JSConstant)
                return false;
            JSValue immediateValue = shiftAmount.valueOfJSConstant(codeBlock());
            if (!immediateValue.isInt32())
                return false;
            return immediateValue > 32 - power;
        }
            
        default:
            return false;
        }
    }

    SpeculatedType speculatedDoubleTypeForPrediction(SpeculatedType value)
    {
        if (!isNumberSpeculation(value))
            return SpecDouble;
        if (value & SpecDoubleNaN)
            return SpecDouble;
        return SpecDoubleReal;
    }

    SpeculatedType speculatedDoubleTypeForPredictions(SpeculatedType left, SpeculatedType right)
    {
        return speculatedDoubleTypeForPrediction(mergeSpeculations(left, right));
    }

    void propagate(Node& node)
    {
        if (!node.shouldGenerate())
            return;
        
        NodeType op = node.op();
        NodeFlags flags = node.flags() & NodeBackPropMask;

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("   %s @%u: %s ", Graph::opName(op), m_compileIndex, nodeFlagsAsString(flags));
#endif
        
        bool changed = false;
        
        switch (op) {
        case JSConstant:
        case WeakJSConstant: {
            changed |= setPrediction(speculationFromValue(m_graph.valueOfJSConstant(m_compileIndex)));
            break;
        }
            
        case GetLocal: {
            VariableAccessData* variableAccessData = node.variableAccessData();
            SpeculatedType prediction = variableAccessData->prediction();
            if (prediction)
                changed |= mergePrediction(prediction);
            
            changed |= variableAccessData->mergeFlags(flags & ~NodeUsedAsIntLocally);
            break;
        }
            
        case SetLocal: {
            VariableAccessData* variableAccessData = node.variableAccessData();
            changed |= variableAccessData->predict(m_graph[node.child1()].prediction());

            // Assume conservatively that a SetLocal implies that the value may flow through a loop,
            // and so we would have overflow leading to the program "observing" numbers even if all
            // users of the value are doing toInt32. It might be worthwhile to revisit this at some
            // point and actually check if the data flow involves loops, but right now I don't think
            // we have evidence that this would be beneficial for benchmarks.
            changed |= m_graph[node.child1()].mergeFlags(variableAccessData->flags() | NodeUsedAsNumber);
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
            changed |= setPrediction(SpecInt32);
            flags |= NodeUsedAsInt | NodeUsedAsIntLocally;
            flags &= ~(NodeUsedAsNumber | NodeNeedsNegZero | NodeUsedAsOther);
            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ValueToInt32: {
            changed |= setPrediction(SpecInt32);
            flags |= NodeUsedAsInt | NodeUsedAsIntLocally;
            flags &= ~(NodeUsedAsNumber | NodeNeedsNegZero | NodeUsedAsOther);
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
            changed |= mergePrediction(SpecInt32);
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[node.child2()].mergeFlags(NodeUsedAsNumber | NodeUsedAsOther | NodeUsedAsInt | NodeUsedAsIntLocally);
            break;
        }

        case UInt32ToNumber: {
            if (nodeCanSpeculateInteger(node.arithNodeFlags()))
                changed |= mergePrediction(SpecInt32);
            else
                changed |= mergePrediction(SpecNumber);
            
            changed |= m_graph[node.child1()].mergeFlags(flags);
            break;
        }

        case ValueAdd: {
            SpeculatedType left = m_graph[node.child1()].prediction();
            SpeculatedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (isNumberSpeculationExpectingDefined(left) && isNumberSpeculationExpectingDefined(right)) {
                    if (m_graph.addShouldSpeculateInteger(node))
                        changed |= mergePrediction(SpecInt32);
                    else
                        changed |= mergePrediction(speculatedDoubleTypeForPredictions(left, right));
                } else if (!(left & SpecNumber) || !(right & SpecNumber)) {
                    // left or right is definitely something other than a number.
                    changed |= mergePrediction(SpecString);
                } else
                    changed |= mergePrediction(SpecString | SpecInt32 | SpecDouble);
            }
            
            if (isNotNegZero(node.child1().index()) || isNotNegZero(node.child2().index()))
                flags &= ~NodeNeedsNegZero;
            if (m_graph[node.child1()].hasNumberResult() || m_graph[node.child2()].hasNumberResult())
                flags &= ~NodeUsedAsOther;
            
            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ArithAdd: {
            SpeculatedType left = m_graph[node.child1()].prediction();
            SpeculatedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (m_graph.addShouldSpeculateInteger(node))
                    changed |= mergePrediction(SpecInt32);
                else
                    changed |= mergePrediction(speculatedDoubleTypeForPredictions(left, right));
            }
            
            if (isNotNegZero(node.child1().index()) || isNotNegZero(node.child2().index()))
                flags &= ~NodeNeedsNegZero;
            flags &= ~NodeUsedAsOther;
            
            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ArithSub: {
            SpeculatedType left = m_graph[node.child1()].prediction();
            SpeculatedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (m_graph.addShouldSpeculateInteger(node))
                    changed |= mergePrediction(SpecInt32);
                else
                    changed |= mergePrediction(speculatedDoubleTypeForPredictions(left, right));
            }

            if (isNotZero(node.child1().index()) || isNotZero(node.child2().index()))
                flags &= ~NodeNeedsNegZero;
            flags &= ~NodeUsedAsOther;
            
            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ArithNegate:
            if (m_graph[node.child1()].prediction()) {
                if (m_graph.negateShouldSpeculateInteger(node))
                    changed |= mergePrediction(SpecInt32);
                else
                    changed |= mergePrediction(speculatedDoubleTypeForPrediction(m_graph[node.child1()].prediction()));
            }

            flags &= ~NodeUsedAsOther;

            changed |= m_graph[node.child1()].mergeFlags(flags);
            break;
            
        case ArithMin:
        case ArithMax: {
            SpeculatedType left = m_graph[node.child1()].prediction();
            SpeculatedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (Node::shouldSpeculateIntegerForArithmetic(m_graph[node.child1()], m_graph[node.child2()])
                    && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(SpecInt32);
                else
                    changed |= mergePrediction(speculatedDoubleTypeForPredictions(left, right));
            }

            flags |= NodeUsedAsNumber;
            flags &= ~NodeUsedAsOther;

            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }

        case ArithMul: {
            SpeculatedType left = m_graph[node.child1()].prediction();
            SpeculatedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (m_graph.mulShouldSpeculateInteger(node))
                    changed |= mergePrediction(SpecInt32);
                else
                    changed |= mergePrediction(speculatedDoubleTypeForPredictions(left, right));
            }

            // As soon as a multiply happens, we can easily end up in the part
            // of the double domain where the point at which you do truncation
            // can change the outcome. So, ArithMul always forces its inputs to
            // check for overflow. Additionally, it will have to check for overflow
            // itself unless we can prove that there is no way for the values
            // produced to cause double rounding.
            
            if (!isWithinPowerOfTwo(node.child1().index(), 22)
                && !isWithinPowerOfTwo(node.child2().index(), 22))
                flags |= NodeUsedAsNumber;
            
            changed |= node.mergeFlags(flags);
            
            flags |= NodeUsedAsNumber | NodeNeedsNegZero;
            flags &= ~NodeUsedAsOther;

            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ArithDiv: {
            SpeculatedType left = m_graph[node.child1()].prediction();
            SpeculatedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (Node::shouldSpeculateIntegerForArithmetic(m_graph[node.child1()], m_graph[node.child2()])
                    && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(SpecInt32);
                else
                    changed |= mergePrediction(SpecDouble);
            }

            // As soon as a multiply happens, we can easily end up in the part
            // of the double domain where the point at which you do truncation
            // can change the outcome. So, ArithDiv always checks for overflow
            // no matter what, and always forces its inputs to check as well.
            
            flags |= NodeUsedAsNumber | NodeNeedsNegZero;
            flags &= ~NodeUsedAsOther;

            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ArithMod: {
            SpeculatedType left = m_graph[node.child1()].prediction();
            SpeculatedType right = m_graph[node.child2()].prediction();
            
            if (left && right) {
                if (Node::shouldSpeculateIntegerForArithmetic(m_graph[node.child1()], m_graph[node.child2()])
                    && nodeCanSpeculateInteger(node.arithNodeFlags()))
                    changed |= mergePrediction(SpecInt32);
                else
                    changed |= mergePrediction(SpecDouble);
            }
            
            flags |= NodeUsedAsNumber | NodeNeedsNegZero;
            flags &= ~NodeUsedAsOther;

            changed |= m_graph[node.child1()].mergeFlags(flags);
            changed |= m_graph[node.child2()].mergeFlags(flags);
            break;
        }
            
        case ArithSqrt: {
            changed |= setPrediction(SpecDouble);
            flags |= NodeUsedAsNumber | NodeNeedsNegZero;
            flags &= ~NodeUsedAsOther;
            changed |= m_graph[node.child1()].mergeFlags(flags);
            break;
        }
            
        case ArithAbs: {
            SpeculatedType child = m_graph[node.child1()].prediction();
            if (isInt32SpeculationForArithmetic(child)
                && nodeCanSpeculateInteger(node.arithNodeFlags()))
                changed |= mergePrediction(SpecInt32);
            else
                changed |= mergePrediction(speculatedDoubleTypeForPrediction(child));

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
            changed |= setPrediction(SpecBoolean);
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
                changed |= mergePrediction(SpecDouble);
            else
                changed |= mergePrediction(node.getHeapPrediction());

            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[node.child2()].mergeFlags(NodeUsedAsNumber | NodeUsedAsOther | NodeUsedAsInt | NodeUsedAsIntLocally);
            break;
        }
            
        case GetMyArgumentByValSafe: {
            changed |= mergePrediction(node.getHeapPrediction());
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsNumber | NodeUsedAsOther | NodeUsedAsInt | NodeUsedAsIntLocally);
            break;
        }
            
        case GetMyArgumentsLengthSafe: {
            changed |= setPrediction(SpecInt32);
            break;
        }

        case GetScopeRegisters:            
        case GetButterfly: 
        case GetIndexedPropertyStorage:
        case AllocatePropertyStorage:
        case ReallocatePropertyStorage: {
            changed |= setPrediction(SpecOther);
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
            SpeculatedType prediction = m_graph[node.child1()].prediction();
            if (prediction) {
                if (prediction & ~SpecObjectMask) {
                    prediction &= SpecObjectMask;
                    prediction = mergeSpeculations(prediction, SpecObjectOther);
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
            
        case PutGlobalVar:
        case PutGlobalVarCheck: {
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            break;
        }
            
        case GetScopedVar:
        case Resolve:
        case ResolveBase:
        case ResolveBaseStrictPut:
        case ResolveGlobal: {
            SpeculatedType prediction = node.getHeapPrediction();
            changed |= mergePrediction(prediction);
            break;
        }
            
        case GetMyScope:
        case SkipTopScope:
        case SkipScope: {
            changed |= setPrediction(SpecCellOther);
            break;
        }
            
        case GetCallee: {
            changed |= setPrediction(SpecFunction);
            break;
        }
            
        case CreateThis:
        case NewObject: {
            changed |= setPrediction(SpecFinalObject);
            changed |= mergeDefaultFlags(node);
            break;
        }
            
        case NewArray: {
            changed |= setPrediction(SpecArray);
            for (unsigned childIdx = node.firstChild();
                 childIdx < node.firstChild() + node.numChildren();
                 ++childIdx) {
                Edge edge = m_graph.m_varArgChildren[childIdx];
                changed |= m_graph[edge].mergeFlags(NodeUsedAsValue);
            }
            break;
        }
            
        case NewArrayWithSize: {
            changed |= setPrediction(SpecArray);
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue | NodeUsedAsInt | NodeUsedAsIntLocally);
            break;
        }
            
        case NewArrayBuffer: {
            changed |= setPrediction(SpecArray);
            break;
        }
            
        case NewRegexp: {
            changed |= setPrediction(SpecObjectOther);
            break;
        }
        
        case StringCharAt: {
            changed |= setPrediction(SpecString);
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[node.child2()].mergeFlags(NodeUsedAsNumber | NodeUsedAsOther | NodeUsedAsInt | NodeUsedAsIntLocally);
            break;
        }
            
        case StrCat: {
            changed |= setPrediction(SpecString);
            for (unsigned childIdx = node.firstChild();
                 childIdx < node.firstChild() + node.numChildren();
                 ++childIdx)
                changed |= m_graph[m_graph.m_varArgChildren[childIdx]].mergeFlags(NodeUsedAsNumber | NodeUsedAsOther);
            break;
        }
            
        case ToPrimitive: {
            SpeculatedType child = m_graph[node.child1()].prediction();
            if (child) {
                if (isObjectSpeculation(child)) {
                    // I'd love to fold this case into the case below, but I can't, because
                    // removing SpecObjectMask from something that only has an object
                    // prediction and nothing else means we have an ill-formed SpeculatedType
                    // (strong predict-none). This should be killed once we remove all traces
                    // of static (aka weak) predictions.
                    changed |= mergePrediction(SpecString);
                } else if (child & SpecObjectMask) {
                    // Objects get turned into strings. So if the input has hints of objectness,
                    // the output will have hinsts of stringiness.
                    changed |= mergePrediction(
                        mergeSpeculations(child & ~SpecObjectMask, SpecString));
                } else
                    changed |= mergePrediction(child);
            }
            changed |= m_graph[node.child1()].mergeFlags(flags);
            break;
        }
            
        case CreateActivation: {
            changed |= setPrediction(SpecObjectOther);
            break;
        }
            
        case CreateArguments: {
            // At this stage we don't try to predict whether the arguments are ours or
            // someone else's. We could, but we don't, yet.
            changed |= setPrediction(SpecArguments);
            break;
        }
            
        case NewFunction:
        case NewFunctionNoCheck:
        case NewFunctionExpression: {
            changed |= setPrediction(SpecFunction);
            break;
        }
            
        case PutByValAlias:
        case GetArrayLength:
        case Int32ToDouble:
        case DoubleAsInt32:
        case GetLocalUnlinked:
        case GetMyArgumentsLength:
        case GetMyArgumentByVal:
        case PhantomPutStructure:
        case PhantomArguments:
        case CheckArray:
        case Arrayify:
        case ArrayifyToStructure:
        case Identity: {
            // This node should never be visible at this stage of compilation. It is
            // inserted by fixup(), which follows this phase.
            CRASH();
            break;
        }
        
        case PutByVal:
            changed |= m_graph[m_graph.varArgChild(node, 0)].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[m_graph.varArgChild(node, 1)].mergeFlags(NodeUsedAsNumber | NodeUsedAsOther | NodeUsedAsInt | NodeUsedAsIntLocally);
            changed |= m_graph[m_graph.varArgChild(node, 2)].mergeFlags(NodeUsedAsValue);
            break;

        case PutScopedVar:
            changed |= m_graph[node.child1()].mergeFlags(NodeUsedAsValue);
            changed |= m_graph[node.child3()].mergeFlags(NodeUsedAsValue);
            break;
            
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
        case ForwardCheckStructure:
        case StructureTransitionWatchpoint:
        case ForwardStructureTransitionWatchpoint:
        case CheckFunction:
        case PutStructure:
        case TearOffActivation:
        case TearOffArguments:
        case CheckNumber:
        case CheckArgumentsNotCreated:
        case GlobalVarWatchpoint:
        case GarbageValue:
        case InheritorIDWatchpoint:
            changed |= mergeDefaultFlags(node);
            break;
            
        // These gets ignored because it doesn't do anything.
        case Phantom:
        case InlineStart:
        case Nop:
        case CountExecution:
            break;
            
        case LastNodeType:
            CRASH();
            break;
#else
        default:
            changed |= mergeDefaultFlags(node);
            break;
#endif
        }

#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog(SpeculationDump(m_graph[m_compileIndex].prediction()), "\n");
#endif
        
        m_changed |= changed;
    }
        
    bool mergeDefaultFlags(Node& node)
    {
        bool changed = false;
        if (node.flags() & NodeHasVarArgs) {
            for (unsigned childIdx = node.firstChild();
                 childIdx < node.firstChild() + node.numChildren();
                 childIdx++) {
                if (!!m_graph.m_varArgChildren[childIdx])
                    changed |= m_graph[m_graph.m_varArgChildren[childIdx]].mergeFlags(NodeUsedAsValue);
            }
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
        dataLogF("Propagating predictions forward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex)
            propagate(m_graph[m_compileIndex]);
    }
    
    void propagateBackward()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("Propagating predictions backward [%u]\n", ++m_count);
#endif
        for (m_compileIndex = m_graph.size(); m_compileIndex-- > 0;)
            propagate(m_graph[m_compileIndex]);
    }
    
    void doRoundOfDoubleVoting()
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLogF("Voting on double uses of locals [%u]\n", m_count);
#endif
        for (unsigned i = 0; i < m_graph.m_variableAccessData.size(); ++i)
            m_graph.m_variableAccessData[i].find()->clearVotes();
        for (m_compileIndex = 0; m_compileIndex < m_graph.size(); ++m_compileIndex) {
            Node& node = m_graph[m_compileIndex];
            switch (node.op()) {
            case ValueAdd:
            case ArithAdd:
            case ArithSub: {
                SpeculatedType left = m_graph[node.child1()].prediction();
                SpeculatedType right = m_graph[node.child2()].prediction();
                
                DoubleBallot ballot;
                
                if (isNumberSpeculationExpectingDefined(left) && isNumberSpeculationExpectingDefined(right)
                    && !m_graph.addShouldSpeculateInteger(node))
                    ballot = VoteDouble;
                else
                    ballot = VoteValue;
                
                m_graph.vote(node.child1(), ballot);
                m_graph.vote(node.child2(), ballot);
                break;
            }
                
            case ArithMul: {
                SpeculatedType left = m_graph[node.child1()].prediction();
                SpeculatedType right = m_graph[node.child2()].prediction();
                
                DoubleBallot ballot;
                
                if (isNumberSpeculation(left) && isNumberSpeculation(right)
                    && !m_graph.mulShouldSpeculateInteger(node))
                    ballot = VoteDouble;
                else
                    ballot = VoteValue;
                
                m_graph.vote(node.child1(), ballot);
                m_graph.vote(node.child2(), ballot);
                break;
            }

            case ArithMin:
            case ArithMax:
            case ArithMod:
            case ArithDiv: {
                SpeculatedType left = m_graph[node.child1()].prediction();
                SpeculatedType right = m_graph[node.child2()].prediction();
                
                DoubleBallot ballot;
                
                if (isNumberSpeculation(left) && isNumberSpeculation(right)
                    && !(Node::shouldSpeculateIntegerForArithmetic(m_graph[node.child1()], m_graph[node.child1()])
                         && node.canSpeculateInteger()))
                    ballot = VoteDouble;
                else
                    ballot = VoteValue;
                
                m_graph.vote(node.child1(), ballot);
                m_graph.vote(node.child2(), ballot);
                break;
            }
                
            case ArithAbs:
                DoubleBallot ballot;
                if (!(m_graph[node.child1()].shouldSpeculateIntegerForArithmetic()
                      && node.canSpeculateInteger()))
                    ballot = VoteDouble;
                else
                    ballot = VoteValue;
                
                m_graph.vote(node.child1(), ballot);
                break;
                
            case ArithSqrt:
                m_graph.vote(node.child1(), VoteDouble);
                break;
                
            case SetLocal: {
                SpeculatedType prediction = m_graph[node.child1()].prediction();
                if (isDoubleSpeculation(prediction))
                    node.variableAccessData()->vote(VoteDouble);
                else if (!isNumberSpeculation(prediction) || isInt32Speculation(prediction))
                    node.variableAccessData()->vote(VoteValue);
                break;
            }
                
            case PutByVal:
            case PutByValAlias: {
                Edge child1 = m_graph.varArgChild(node, 0);
                Edge child2 = m_graph.varArgChild(node, 1);
                Edge child3 = m_graph.varArgChild(node, 2);
                m_graph.vote(child1, VoteValue);
                m_graph.vote(child2, VoteValue);
                switch (node.arrayMode().type()) {
                case Array::Double:
                    m_graph.vote(child3, VoteDouble);
                    break;
                default:
                    m_graph.vote(child3, VoteValue);
                    break;
                }
                break;
            }
                
            default:
                m_graph.vote(node, VoteValue);
                break;
            }
        }
        for (unsigned i = 0; i < m_graph.m_variableAccessData.size(); ++i) {
            VariableAccessData* variableAccessData = &m_graph.m_variableAccessData[i];
            if (!variableAccessData->isRoot())
                continue;
            if (operandIsArgument(variableAccessData->local())
                || variableAccessData->isCaptured())
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
                || variableAccessData->isCaptured())
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
    
bool performPredictionPropagation(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Prediction Propagation Phase");
    return runPhase<PredictionPropagationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

