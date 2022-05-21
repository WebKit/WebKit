//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLComputePipeline.hpp
//
// Copyright 2020-2021 Apple Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

#include "MTLDefines.hpp"
#include "MTLHeaderBridge.hpp"
#include "MTLPrivate.hpp"

#include <Foundation/Foundation.hpp>

#include "MTLTypes.hpp"

namespace MTL
{
class ComputePipelineReflection : public NS::Referencing<ComputePipelineReflection>
{
public:
    static class ComputePipelineReflection* alloc();

    class ComputePipelineReflection*        init();

    NS::Array*                              arguments() const;
};

class ComputePipelineDescriptor : public NS::Copying<ComputePipelineDescriptor>
{
public:
    static class ComputePipelineDescriptor* alloc();

    class ComputePipelineDescriptor*        init();

    NS::String*                             label() const;
    void                                    setLabel(const NS::String* label);

    class Function*                         computeFunction() const;
    void                                    setComputeFunction(const class Function* computeFunction);

    bool                                    threadGroupSizeIsMultipleOfThreadExecutionWidth() const;
    void                                    setThreadGroupSizeIsMultipleOfThreadExecutionWidth(bool threadGroupSizeIsMultipleOfThreadExecutionWidth);

    NS::UInteger                            maxTotalThreadsPerThreadgroup() const;
    void                                    setMaxTotalThreadsPerThreadgroup(NS::UInteger maxTotalThreadsPerThreadgroup);

    class StageInputOutputDescriptor*       stageInputDescriptor() const;
    void                                    setStageInputDescriptor(const class StageInputOutputDescriptor* stageInputDescriptor);

    class PipelineBufferDescriptorArray*    buffers() const;

    bool                                    supportIndirectCommandBuffers() const;
    void                                    setSupportIndirectCommandBuffers(bool supportIndirectCommandBuffers);

    NS::Array*                              insertLibraries() const;
    void                                    setInsertLibraries(const NS::Array* insertLibraries);

    NS::Array*                              preloadedLibraries() const;
    void                                    setPreloadedLibraries(const NS::Array* preloadedLibraries);

    NS::Array*                              binaryArchives() const;
    void                                    setBinaryArchives(const NS::Array* binaryArchives);

    void                                    reset();

    class LinkedFunctions*                  linkedFunctions() const;
    void                                    setLinkedFunctions(const class LinkedFunctions* linkedFunctions);

    bool                                    supportAddingBinaryFunctions() const;
    void                                    setSupportAddingBinaryFunctions(bool supportAddingBinaryFunctions);

    NS::UInteger                            maxCallStackDepth() const;
    void                                    setMaxCallStackDepth(NS::UInteger maxCallStackDepth);
};

class ComputePipelineState : public NS::Referencing<ComputePipelineState>
{
public:
    NS::String*                      label() const;

    class Device*                    device() const;

    NS::UInteger                     maxTotalThreadsPerThreadgroup() const;

    NS::UInteger                     threadExecutionWidth() const;

    NS::UInteger                     staticThreadgroupMemoryLength() const;

    NS::UInteger                     imageblockMemoryLength(MTL::Size imageblockDimensions);

    bool                             supportIndirectCommandBuffers() const;

    class FunctionHandle*            functionHandle(const class Function* function);

    class ComputePipelineState*      newComputePipelineState(const NS::Array* functions, NS::Error** error);

    class VisibleFunctionTable*      newVisibleFunctionTable(const class VisibleFunctionTableDescriptor* descriptor);

    class IntersectionFunctionTable* newIntersectionFunctionTable(const class IntersectionFunctionTableDescriptor* descriptor);
};

}

// static method: alloc
_MTL_INLINE MTL::ComputePipelineReflection* MTL::ComputePipelineReflection::alloc()
{
    return NS::Object::alloc<MTL::ComputePipelineReflection>(_MTL_PRIVATE_CLS(MTLComputePipelineReflection));
}

// method: init
_MTL_INLINE MTL::ComputePipelineReflection* MTL::ComputePipelineReflection::init()
{
    return NS::Object::init<MTL::ComputePipelineReflection>();
}

// property: arguments
_MTL_INLINE NS::Array* MTL::ComputePipelineReflection::arguments() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(arguments));
}

// static method: alloc
_MTL_INLINE MTL::ComputePipelineDescriptor* MTL::ComputePipelineDescriptor::alloc()
{
    return NS::Object::alloc<MTL::ComputePipelineDescriptor>(_MTL_PRIVATE_CLS(MTLComputePipelineDescriptor));
}

// method: init
_MTL_INLINE MTL::ComputePipelineDescriptor* MTL::ComputePipelineDescriptor::init()
{
    return NS::Object::init<MTL::ComputePipelineDescriptor>();
}

// property: label
_MTL_INLINE NS::String* MTL::ComputePipelineDescriptor::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: computeFunction
_MTL_INLINE MTL::Function* MTL::ComputePipelineDescriptor::computeFunction() const
{
    return Object::sendMessage<MTL::Function*>(this, _MTL_PRIVATE_SEL(computeFunction));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setComputeFunction(const MTL::Function* computeFunction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setComputeFunction_), computeFunction);
}

// property: threadGroupSizeIsMultipleOfThreadExecutionWidth
_MTL_INLINE bool MTL::ComputePipelineDescriptor::threadGroupSizeIsMultipleOfThreadExecutionWidth() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(threadGroupSizeIsMultipleOfThreadExecutionWidth));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setThreadGroupSizeIsMultipleOfThreadExecutionWidth(bool threadGroupSizeIsMultipleOfThreadExecutionWidth)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setThreadGroupSizeIsMultipleOfThreadExecutionWidth_), threadGroupSizeIsMultipleOfThreadExecutionWidth);
}

// property: maxTotalThreadsPerThreadgroup
_MTL_INLINE NS::UInteger MTL::ComputePipelineDescriptor::maxTotalThreadsPerThreadgroup() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxTotalThreadsPerThreadgroup));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setMaxTotalThreadsPerThreadgroup(NS::UInteger maxTotalThreadsPerThreadgroup)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxTotalThreadsPerThreadgroup_), maxTotalThreadsPerThreadgroup);
}

// property: stageInputDescriptor
_MTL_INLINE MTL::StageInputOutputDescriptor* MTL::ComputePipelineDescriptor::stageInputDescriptor() const
{
    return Object::sendMessage<MTL::StageInputOutputDescriptor*>(this, _MTL_PRIVATE_SEL(stageInputDescriptor));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setStageInputDescriptor(const MTL::StageInputOutputDescriptor* stageInputDescriptor)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStageInputDescriptor_), stageInputDescriptor);
}

// property: buffers
_MTL_INLINE MTL::PipelineBufferDescriptorArray* MTL::ComputePipelineDescriptor::buffers() const
{
    return Object::sendMessage<MTL::PipelineBufferDescriptorArray*>(this, _MTL_PRIVATE_SEL(buffers));
}

// property: supportIndirectCommandBuffers
_MTL_INLINE bool MTL::ComputePipelineDescriptor::supportIndirectCommandBuffers() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportIndirectCommandBuffers));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setSupportIndirectCommandBuffers(bool supportIndirectCommandBuffers)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSupportIndirectCommandBuffers_), supportIndirectCommandBuffers);
}

// property: insertLibraries
_MTL_INLINE NS::Array* MTL::ComputePipelineDescriptor::insertLibraries() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(insertLibraries));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setInsertLibraries(const NS::Array* insertLibraries)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setInsertLibraries_), insertLibraries);
}

// property: preloadedLibraries
_MTL_INLINE NS::Array* MTL::ComputePipelineDescriptor::preloadedLibraries() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(preloadedLibraries));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setPreloadedLibraries(const NS::Array* preloadedLibraries)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setPreloadedLibraries_), preloadedLibraries);
}

// property: binaryArchives
_MTL_INLINE NS::Array* MTL::ComputePipelineDescriptor::binaryArchives() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(binaryArchives));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setBinaryArchives(const NS::Array* binaryArchives)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBinaryArchives_), binaryArchives);
}

// method: reset
_MTL_INLINE void MTL::ComputePipelineDescriptor::reset()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(reset));
}

// property: linkedFunctions
_MTL_INLINE MTL::LinkedFunctions* MTL::ComputePipelineDescriptor::linkedFunctions() const
{
    return Object::sendMessage<MTL::LinkedFunctions*>(this, _MTL_PRIVATE_SEL(linkedFunctions));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setLinkedFunctions(const MTL::LinkedFunctions* linkedFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLinkedFunctions_), linkedFunctions);
}

// property: supportAddingBinaryFunctions
_MTL_INLINE bool MTL::ComputePipelineDescriptor::supportAddingBinaryFunctions() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportAddingBinaryFunctions));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setSupportAddingBinaryFunctions(bool supportAddingBinaryFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSupportAddingBinaryFunctions_), supportAddingBinaryFunctions);
}

// property: maxCallStackDepth
_MTL_INLINE NS::UInteger MTL::ComputePipelineDescriptor::maxCallStackDepth() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxCallStackDepth));
}

_MTL_INLINE void MTL::ComputePipelineDescriptor::setMaxCallStackDepth(NS::UInteger maxCallStackDepth)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxCallStackDepth_), maxCallStackDepth);
}

// property: label
_MTL_INLINE NS::String* MTL::ComputePipelineState::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

// property: device
_MTL_INLINE MTL::Device* MTL::ComputePipelineState::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: maxTotalThreadsPerThreadgroup
_MTL_INLINE NS::UInteger MTL::ComputePipelineState::maxTotalThreadsPerThreadgroup() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxTotalThreadsPerThreadgroup));
}

// property: threadExecutionWidth
_MTL_INLINE NS::UInteger MTL::ComputePipelineState::threadExecutionWidth() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(threadExecutionWidth));
}

// property: staticThreadgroupMemoryLength
_MTL_INLINE NS::UInteger MTL::ComputePipelineState::staticThreadgroupMemoryLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(staticThreadgroupMemoryLength));
}

// method: imageblockMemoryLengthForDimensions:
_MTL_INLINE NS::UInteger MTL::ComputePipelineState::imageblockMemoryLength(MTL::Size imageblockDimensions)
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(imageblockMemoryLengthForDimensions_), imageblockDimensions);
}

// property: supportIndirectCommandBuffers
_MTL_INLINE bool MTL::ComputePipelineState::supportIndirectCommandBuffers() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportIndirectCommandBuffers));
}

// method: functionHandleWithFunction:
_MTL_INLINE MTL::FunctionHandle* MTL::ComputePipelineState::functionHandle(const MTL::Function* function)
{
    return Object::sendMessage<MTL::FunctionHandle*>(this, _MTL_PRIVATE_SEL(functionHandleWithFunction_), function);
}

// method: newComputePipelineStateWithAdditionalBinaryFunctions:error:
_MTL_INLINE MTL::ComputePipelineState* MTL::ComputePipelineState::newComputePipelineState(const NS::Array* functions, NS::Error** error)
{
    return Object::sendMessage<MTL::ComputePipelineState*>(this, _MTL_PRIVATE_SEL(newComputePipelineStateWithAdditionalBinaryFunctions_error_), functions, error);
}

// method: newVisibleFunctionTableWithDescriptor:
_MTL_INLINE MTL::VisibleFunctionTable* MTL::ComputePipelineState::newVisibleFunctionTable(const MTL::VisibleFunctionTableDescriptor* descriptor)
{
    return Object::sendMessage<MTL::VisibleFunctionTable*>(this, _MTL_PRIVATE_SEL(newVisibleFunctionTableWithDescriptor_), descriptor);
}

// method: newIntersectionFunctionTableWithDescriptor:
_MTL_INLINE MTL::IntersectionFunctionTable* MTL::ComputePipelineState::newIntersectionFunctionTable(const MTL::IntersectionFunctionTableDescriptor* descriptor)
{
    return Object::sendMessage<MTL::IntersectionFunctionTable*>(this, _MTL_PRIVATE_SEL(newIntersectionFunctionTableWithDescriptor_), descriptor);
}
