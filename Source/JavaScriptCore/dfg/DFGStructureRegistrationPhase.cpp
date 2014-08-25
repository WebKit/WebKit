/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "DFGStructureRegistrationPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlockInlines.h"
#include "DFGGraph.h"
#include "DFGPhase.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

class StructureRegistrationPhase : public Phase {
public:
    StructureRegistrationPhase(Graph& graph)
        : Phase(graph, "structure registration")
    {
    }
    
    bool run()
    {
        // These are pretty dumb, but needed to placate subsequent assertions. We don't actually
        // have to watch these because there is no way to transition away from it, but they are
        // watchable and so we will assert if they aren't watched.
        registerStructure(m_graph.m_vm.stringStructure.get());
        registerStructure(m_graph.m_vm.getterSetterStructure.get());
        
        for (FrozenValue* value : m_graph.m_frozenValues)
            registerStructure(value->structure());
        
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
        
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
            
                switch (node->op()) {
                case CheckStructure:
                    registerStructures(node->structureSet());
                    break;
                
                case NewObject:
                case ArrayifyToStructure:
                case NewStringObject:
                    registerStructure(node->structure());
                    break;
                
                case PutStructure:
                case AllocatePropertyStorage:
                case ReallocatePropertyStorage:
                    registerStructure(node->transition()->previous);
                    registerStructure(node->transition()->next);
                    break;
                    
                case MultiGetByOffset:
                    for (unsigned i = node->multiGetByOffsetData().variants.size(); i--;) {
                        GetByIdVariant& variant = node->multiGetByOffsetData().variants[i];
                        registerStructures(variant.structureSet());
                        // Don't need to watch anything in the structure chain because that would
                        // have been decomposed into CheckStructure's. Don't need to watch the
                        // callLinkStatus because we wouldn't use MultiGetByOffset if any of the
                        // variants did that.
                        ASSERT(!variant.callLinkStatus());
                    }
                    break;
                    
                case MultiPutByOffset:
                    for (unsigned i = node->multiPutByOffsetData().variants.size(); i--;) {
                        PutByIdVariant& variant = node->multiPutByOffsetData().variants[i];
                        registerStructures(variant.oldStructure());
                        if (variant.kind() == PutByIdVariant::Transition)
                            registerStructure(variant.newStructure());
                    }
                    break;
                    
                case NewArray:
                case NewArrayBuffer:
                    registerStructure(m_graph.globalObjectFor(node->origin.semantic)->arrayStructureForIndexingTypeDuringAllocation(node->indexingType()));
                    break;
                    
                case NewTypedArray:
                    registerStructure(m_graph.globalObjectFor(node->origin.semantic)->typedArrayStructure(node->typedArrayType()));
                    break;
                    
                case ToString:
                    registerStructure(m_graph.globalObjectFor(node->origin.semantic)->stringObjectStructure());
                    break;
                    
                case CreateActivation:
                    registerStructure(m_graph.globalObjectFor(node->origin.semantic)->activationStructure());
                    break;
                    
                case NewRegexp:
                    registerStructure(m_graph.globalObjectFor(node->origin.semantic)->regExpStructure());
                    break;
                    
                case NewFunctionExpression:
                case NewFunctionNoCheck:
                    registerStructure(m_graph.globalObjectFor(node->origin.semantic)->functionStructure());
                    break;
                    
                default:
                    break;
                }
            }
        }
        
        m_graph.m_structureRegistrationState = AllStructuresAreRegistered;
        
        return true;
    }

private:
    void registerStructures(const StructureSet& set)
    {
        for (unsigned i = set.size(); i--;)
            registerStructure(set[i]);
    }
    
    void registerStructure(Structure* structure)
    {
        if (structure)
            m_graph.registerStructure(structure);
    }
};

bool performStructureRegistration(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Structure Registration Phase");
    return runPhase<StructureRegistrationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

