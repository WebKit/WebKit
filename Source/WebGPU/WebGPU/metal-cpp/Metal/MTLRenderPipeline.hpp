//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLRenderPipeline.hpp
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

#include "MTLPixelFormat.hpp"
#include "MTLRenderCommandEncoder.hpp"
#include "MTLRenderPipeline.hpp"
#include "MTLTypes.hpp"

namespace MTL
{
_MTL_ENUM(NS::UInteger, BlendFactor) {
    BlendFactorZero = 0,
    BlendFactorOne = 1,
    BlendFactorSourceColor = 2,
    BlendFactorOneMinusSourceColor = 3,
    BlendFactorSourceAlpha = 4,
    BlendFactorOneMinusSourceAlpha = 5,
    BlendFactorDestinationColor = 6,
    BlendFactorOneMinusDestinationColor = 7,
    BlendFactorDestinationAlpha = 8,
    BlendFactorOneMinusDestinationAlpha = 9,
    BlendFactorSourceAlphaSaturated = 10,
    BlendFactorBlendColor = 11,
    BlendFactorOneMinusBlendColor = 12,
    BlendFactorBlendAlpha = 13,
    BlendFactorOneMinusBlendAlpha = 14,
    BlendFactorSource1Color = 15,
    BlendFactorOneMinusSource1Color = 16,
    BlendFactorSource1Alpha = 17,
    BlendFactorOneMinusSource1Alpha = 18,
};

_MTL_ENUM(NS::UInteger, BlendOperation) {
    BlendOperationAdd = 0,
    BlendOperationSubtract = 1,
    BlendOperationReverseSubtract = 2,
    BlendOperationMin = 3,
    BlendOperationMax = 4,
};

_MTL_OPTIONS(NS::UInteger, ColorWriteMask) {
    ColorWriteMaskNone = 0,
    ColorWriteMaskAlpha = 1,
    ColorWriteMaskBlue = 2,
    ColorWriteMaskGreen = 4,
    ColorWriteMaskRed = 8,
    ColorWriteMaskAll = 15,
};

_MTL_ENUM(NS::UInteger, PrimitiveTopologyClass) {
    PrimitiveTopologyClassUnspecified = 0,
    PrimitiveTopologyClassPoint = 1,
    PrimitiveTopologyClassLine = 2,
    PrimitiveTopologyClassTriangle = 3,
};

_MTL_ENUM(NS::UInteger, TessellationPartitionMode) {
    TessellationPartitionModePow2 = 0,
    TessellationPartitionModeInteger = 1,
    TessellationPartitionModeFractionalOdd = 2,
    TessellationPartitionModeFractionalEven = 3,
};

_MTL_ENUM(NS::UInteger, TessellationFactorStepFunction) {
    TessellationFactorStepFunctionConstant = 0,
    TessellationFactorStepFunctionPerPatch = 1,
    TessellationFactorStepFunctionPerInstance = 2,
    TessellationFactorStepFunctionPerPatchAndPerInstance = 3,
};

_MTL_ENUM(NS::UInteger, TessellationFactorFormat) {
    TessellationFactorFormatHalf = 0,
};

_MTL_ENUM(NS::UInteger, TessellationControlPointIndexType) {
    TessellationControlPointIndexTypeNone = 0,
    TessellationControlPointIndexTypeUInt16 = 1,
    TessellationControlPointIndexTypeUInt32 = 2,
};

class RenderPipelineColorAttachmentDescriptor : public NS::Copying<RenderPipelineColorAttachmentDescriptor>
{
public:
    static class RenderPipelineColorAttachmentDescriptor* alloc();

    class RenderPipelineColorAttachmentDescriptor*        init();

    MTL::PixelFormat                                      pixelFormat() const;
    void                                                  setPixelFormat(MTL::PixelFormat pixelFormat);

    bool                                                  blendingEnabled() const;
    void                                                  setBlendingEnabled(bool blendingEnabled);

    MTL::BlendFactor                                      sourceRGBBlendFactor() const;
    void                                                  setSourceRGBBlendFactor(MTL::BlendFactor sourceRGBBlendFactor);

    MTL::BlendFactor                                      destinationRGBBlendFactor() const;
    void                                                  setDestinationRGBBlendFactor(MTL::BlendFactor destinationRGBBlendFactor);

    MTL::BlendOperation                                   rgbBlendOperation() const;
    void                                                  setRgbBlendOperation(MTL::BlendOperation rgbBlendOperation);

    MTL::BlendFactor                                      sourceAlphaBlendFactor() const;
    void                                                  setSourceAlphaBlendFactor(MTL::BlendFactor sourceAlphaBlendFactor);

    MTL::BlendFactor                                      destinationAlphaBlendFactor() const;
    void                                                  setDestinationAlphaBlendFactor(MTL::BlendFactor destinationAlphaBlendFactor);

    MTL::BlendOperation                                   alphaBlendOperation() const;
    void                                                  setAlphaBlendOperation(MTL::BlendOperation alphaBlendOperation);

    MTL::ColorWriteMask                                   writeMask() const;
    void                                                  setWriteMask(MTL::ColorWriteMask writeMask);
};

class RenderPipelineReflection : public NS::Referencing<RenderPipelineReflection>
{
public:
    static class RenderPipelineReflection* alloc();

    class RenderPipelineReflection*        init();

    NS::Array*                             vertexArguments() const;

    NS::Array*                             fragmentArguments() const;

    NS::Array*                             tileArguments() const;
};

class RenderPipelineDescriptor : public NS::Copying<RenderPipelineDescriptor>
{
public:
    static class RenderPipelineDescriptor*              alloc();

    class RenderPipelineDescriptor*                     init();

    NS::String*                                         label() const;
    void                                                setLabel(const NS::String* label);

    class Function*                                     vertexFunction() const;
    void                                                setVertexFunction(const class Function* vertexFunction);

    class Function*                                     fragmentFunction() const;
    void                                                setFragmentFunction(const class Function* fragmentFunction);

    class VertexDescriptor*                             vertexDescriptor() const;
    void                                                setVertexDescriptor(const class VertexDescriptor* vertexDescriptor);

    NS::UInteger                                        sampleCount() const;
    void                                                setSampleCount(NS::UInteger sampleCount);

    NS::UInteger                                        rasterSampleCount() const;
    void                                                setRasterSampleCount(NS::UInteger rasterSampleCount);

    bool                                                alphaToCoverageEnabled() const;
    void                                                setAlphaToCoverageEnabled(bool alphaToCoverageEnabled);

    bool                                                alphaToOneEnabled() const;
    void                                                setAlphaToOneEnabled(bool alphaToOneEnabled);

    bool                                                rasterizationEnabled() const;
    void                                                setRasterizationEnabled(bool rasterizationEnabled);

    NS::UInteger                                        maxVertexAmplificationCount() const;
    void                                                setMaxVertexAmplificationCount(NS::UInteger maxVertexAmplificationCount);

    class RenderPipelineColorAttachmentDescriptorArray* colorAttachments() const;

    MTL::PixelFormat                                    depthAttachmentPixelFormat() const;
    void                                                setDepthAttachmentPixelFormat(MTL::PixelFormat depthAttachmentPixelFormat);

    MTL::PixelFormat                                    stencilAttachmentPixelFormat() const;
    void                                                setStencilAttachmentPixelFormat(MTL::PixelFormat stencilAttachmentPixelFormat);

    MTL::PrimitiveTopologyClass                         inputPrimitiveTopology() const;
    void                                                setInputPrimitiveTopology(MTL::PrimitiveTopologyClass inputPrimitiveTopology);

    MTL::TessellationPartitionMode                      tessellationPartitionMode() const;
    void                                                setTessellationPartitionMode(MTL::TessellationPartitionMode tessellationPartitionMode);

    NS::UInteger                                        maxTessellationFactor() const;
    void                                                setMaxTessellationFactor(NS::UInteger maxTessellationFactor);

    bool                                                tessellationFactorScaleEnabled() const;
    void                                                setTessellationFactorScaleEnabled(bool tessellationFactorScaleEnabled);

    MTL::TessellationFactorFormat                       tessellationFactorFormat() const;
    void                                                setTessellationFactorFormat(MTL::TessellationFactorFormat tessellationFactorFormat);

    MTL::TessellationControlPointIndexType              tessellationControlPointIndexType() const;
    void                                                setTessellationControlPointIndexType(MTL::TessellationControlPointIndexType tessellationControlPointIndexType);

    MTL::TessellationFactorStepFunction                 tessellationFactorStepFunction() const;
    void                                                setTessellationFactorStepFunction(MTL::TessellationFactorStepFunction tessellationFactorStepFunction);

    MTL::Winding                                        tessellationOutputWindingOrder() const;
    void                                                setTessellationOutputWindingOrder(MTL::Winding tessellationOutputWindingOrder);

    class PipelineBufferDescriptorArray*                vertexBuffers() const;

    class PipelineBufferDescriptorArray*                fragmentBuffers() const;

    bool                                                supportIndirectCommandBuffers() const;
    void                                                setSupportIndirectCommandBuffers(bool supportIndirectCommandBuffers);

    NS::Array*                                          binaryArchives() const;
    void                                                setBinaryArchives(const NS::Array* binaryArchives);

    NS::Array*                                          vertexPreloadedLibraries() const;
    void                                                setVertexPreloadedLibraries(const NS::Array* vertexPreloadedLibraries);

    NS::Array*                                          fragmentPreloadedLibraries() const;
    void                                                setFragmentPreloadedLibraries(const NS::Array* fragmentPreloadedLibraries);

    class LinkedFunctions*                              vertexLinkedFunctions() const;
    void                                                setVertexLinkedFunctions(const class LinkedFunctions* vertexLinkedFunctions);

    class LinkedFunctions*                              fragmentLinkedFunctions() const;
    void                                                setFragmentLinkedFunctions(const class LinkedFunctions* fragmentLinkedFunctions);

    bool                                                supportAddingVertexBinaryFunctions() const;
    void                                                setSupportAddingVertexBinaryFunctions(bool supportAddingVertexBinaryFunctions);

    bool                                                supportAddingFragmentBinaryFunctions() const;
    void                                                setSupportAddingFragmentBinaryFunctions(bool supportAddingFragmentBinaryFunctions);

    NS::UInteger                                        maxVertexCallStackDepth() const;
    void                                                setMaxVertexCallStackDepth(NS::UInteger maxVertexCallStackDepth);

    NS::UInteger                                        maxFragmentCallStackDepth() const;
    void                                                setMaxFragmentCallStackDepth(NS::UInteger maxFragmentCallStackDepth);

    void                                                reset();
};

class RenderPipelineFunctionsDescriptor : public NS::Copying<RenderPipelineFunctionsDescriptor>
{
public:
    static class RenderPipelineFunctionsDescriptor* alloc();

    class RenderPipelineFunctionsDescriptor*        init();

    NS::Array*                                      vertexAdditionalBinaryFunctions() const;
    void                                            setVertexAdditionalBinaryFunctions(const NS::Array* vertexAdditionalBinaryFunctions);

    NS::Array*                                      fragmentAdditionalBinaryFunctions() const;
    void                                            setFragmentAdditionalBinaryFunctions(const NS::Array* fragmentAdditionalBinaryFunctions);

    NS::Array*                                      tileAdditionalBinaryFunctions() const;
    void                                            setTileAdditionalBinaryFunctions(const NS::Array* tileAdditionalBinaryFunctions);
};

class RenderPipelineState : public NS::Referencing<RenderPipelineState>
{
public:
    NS::String*                      label() const;

    class Device*                    device() const;

    NS::UInteger                     maxTotalThreadsPerThreadgroup() const;

    bool                             threadgroupSizeMatchesTileSize() const;

    NS::UInteger                     imageblockSampleLength() const;

    NS::UInteger                     imageblockMemoryLength(MTL::Size imageblockDimensions);

    bool                             supportIndirectCommandBuffers() const;

    class FunctionHandle*            functionHandle(const class Function* function, MTL::RenderStages stage);

    class VisibleFunctionTable*      newVisibleFunctionTable(const class VisibleFunctionTableDescriptor* descriptor, MTL::RenderStages stage);

    class IntersectionFunctionTable* newIntersectionFunctionTable(const class IntersectionFunctionTableDescriptor* descriptor, MTL::RenderStages stage);

    class RenderPipelineState*       newRenderPipelineState(const class RenderPipelineFunctionsDescriptor* additionalBinaryFunctions, NS::Error** error);
};

class RenderPipelineColorAttachmentDescriptorArray : public NS::Referencing<RenderPipelineColorAttachmentDescriptorArray>
{
public:
    static class RenderPipelineColorAttachmentDescriptorArray* alloc();

    class RenderPipelineColorAttachmentDescriptorArray*        init();

    class RenderPipelineColorAttachmentDescriptor*             object(NS::UInteger attachmentIndex);

    void                                                       setObject(const class RenderPipelineColorAttachmentDescriptor* attachment, NS::UInteger attachmentIndex);
};

class TileRenderPipelineColorAttachmentDescriptor : public NS::Copying<TileRenderPipelineColorAttachmentDescriptor>
{
public:
    static class TileRenderPipelineColorAttachmentDescriptor* alloc();

    class TileRenderPipelineColorAttachmentDescriptor*        init();

    MTL::PixelFormat                                          pixelFormat() const;
    void                                                      setPixelFormat(MTL::PixelFormat pixelFormat);
};

class TileRenderPipelineColorAttachmentDescriptorArray : public NS::Referencing<TileRenderPipelineColorAttachmentDescriptorArray>
{
public:
    static class TileRenderPipelineColorAttachmentDescriptorArray* alloc();

    class TileRenderPipelineColorAttachmentDescriptorArray*        init();

    class TileRenderPipelineColorAttachmentDescriptor*             object(NS::UInteger attachmentIndex);

    void                                                           setObject(const class TileRenderPipelineColorAttachmentDescriptor* attachment, NS::UInteger attachmentIndex);
};

class TileRenderPipelineDescriptor : public NS::Copying<TileRenderPipelineDescriptor>
{
public:
    static class TileRenderPipelineDescriptor*              alloc();

    class TileRenderPipelineDescriptor*                     init();

    NS::String*                                             label() const;
    void                                                    setLabel(const NS::String* label);

    class Function*                                         tileFunction() const;
    void                                                    setTileFunction(const class Function* tileFunction);

    NS::UInteger                                            rasterSampleCount() const;
    void                                                    setRasterSampleCount(NS::UInteger rasterSampleCount);

    class TileRenderPipelineColorAttachmentDescriptorArray* colorAttachments() const;

    bool                                                    threadgroupSizeMatchesTileSize() const;
    void                                                    setThreadgroupSizeMatchesTileSize(bool threadgroupSizeMatchesTileSize);

    class PipelineBufferDescriptorArray*                    tileBuffers() const;

    NS::UInteger                                            maxTotalThreadsPerThreadgroup() const;
    void                                                    setMaxTotalThreadsPerThreadgroup(NS::UInteger maxTotalThreadsPerThreadgroup);

    NS::Array*                                              binaryArchives() const;
    void                                                    setBinaryArchives(const NS::Array* binaryArchives);

    NS::Array*                                              preloadedLibraries() const;
    void                                                    setPreloadedLibraries(const NS::Array* preloadedLibraries);

    class LinkedFunctions*                                  linkedFunctions() const;
    void                                                    setLinkedFunctions(const class LinkedFunctions* linkedFunctions);

    bool                                                    supportAddingBinaryFunctions() const;
    void                                                    setSupportAddingBinaryFunctions(bool supportAddingBinaryFunctions);

    NS::UInteger                                            maxCallStackDepth() const;
    void                                                    setMaxCallStackDepth(NS::UInteger maxCallStackDepth);

    void                                                    reset();
};

}

// static method: alloc
_MTL_INLINE MTL::RenderPipelineColorAttachmentDescriptor* MTL::RenderPipelineColorAttachmentDescriptor::alloc()
{
    return NS::Object::alloc<MTL::RenderPipelineColorAttachmentDescriptor>(_MTL_PRIVATE_CLS(MTLRenderPipelineColorAttachmentDescriptor));
}

// method: init
_MTL_INLINE MTL::RenderPipelineColorAttachmentDescriptor* MTL::RenderPipelineColorAttachmentDescriptor::init()
{
    return NS::Object::init<MTL::RenderPipelineColorAttachmentDescriptor>();
}

// property: pixelFormat
_MTL_INLINE MTL::PixelFormat MTL::RenderPipelineColorAttachmentDescriptor::pixelFormat() const
{
    return Object::sendMessage<MTL::PixelFormat>(this, _MTL_PRIVATE_SEL(pixelFormat));
}

_MTL_INLINE void MTL::RenderPipelineColorAttachmentDescriptor::setPixelFormat(MTL::PixelFormat pixelFormat)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setPixelFormat_), pixelFormat);
}

// property: blendingEnabled
_MTL_INLINE bool MTL::RenderPipelineColorAttachmentDescriptor::blendingEnabled() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isBlendingEnabled));
}

_MTL_INLINE void MTL::RenderPipelineColorAttachmentDescriptor::setBlendingEnabled(bool blendingEnabled)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBlendingEnabled_), blendingEnabled);
}

// property: sourceRGBBlendFactor
_MTL_INLINE MTL::BlendFactor MTL::RenderPipelineColorAttachmentDescriptor::sourceRGBBlendFactor() const
{
    return Object::sendMessage<MTL::BlendFactor>(this, _MTL_PRIVATE_SEL(sourceRGBBlendFactor));
}

_MTL_INLINE void MTL::RenderPipelineColorAttachmentDescriptor::setSourceRGBBlendFactor(MTL::BlendFactor sourceRGBBlendFactor)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSourceRGBBlendFactor_), sourceRGBBlendFactor);
}

// property: destinationRGBBlendFactor
_MTL_INLINE MTL::BlendFactor MTL::RenderPipelineColorAttachmentDescriptor::destinationRGBBlendFactor() const
{
    return Object::sendMessage<MTL::BlendFactor>(this, _MTL_PRIVATE_SEL(destinationRGBBlendFactor));
}

_MTL_INLINE void MTL::RenderPipelineColorAttachmentDescriptor::setDestinationRGBBlendFactor(MTL::BlendFactor destinationRGBBlendFactor)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDestinationRGBBlendFactor_), destinationRGBBlendFactor);
}

// property: rgbBlendOperation
_MTL_INLINE MTL::BlendOperation MTL::RenderPipelineColorAttachmentDescriptor::rgbBlendOperation() const
{
    return Object::sendMessage<MTL::BlendOperation>(this, _MTL_PRIVATE_SEL(rgbBlendOperation));
}

_MTL_INLINE void MTL::RenderPipelineColorAttachmentDescriptor::setRgbBlendOperation(MTL::BlendOperation rgbBlendOperation)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRgbBlendOperation_), rgbBlendOperation);
}

// property: sourceAlphaBlendFactor
_MTL_INLINE MTL::BlendFactor MTL::RenderPipelineColorAttachmentDescriptor::sourceAlphaBlendFactor() const
{
    return Object::sendMessage<MTL::BlendFactor>(this, _MTL_PRIVATE_SEL(sourceAlphaBlendFactor));
}

_MTL_INLINE void MTL::RenderPipelineColorAttachmentDescriptor::setSourceAlphaBlendFactor(MTL::BlendFactor sourceAlphaBlendFactor)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSourceAlphaBlendFactor_), sourceAlphaBlendFactor);
}

// property: destinationAlphaBlendFactor
_MTL_INLINE MTL::BlendFactor MTL::RenderPipelineColorAttachmentDescriptor::destinationAlphaBlendFactor() const
{
    return Object::sendMessage<MTL::BlendFactor>(this, _MTL_PRIVATE_SEL(destinationAlphaBlendFactor));
}

_MTL_INLINE void MTL::RenderPipelineColorAttachmentDescriptor::setDestinationAlphaBlendFactor(MTL::BlendFactor destinationAlphaBlendFactor)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDestinationAlphaBlendFactor_), destinationAlphaBlendFactor);
}

// property: alphaBlendOperation
_MTL_INLINE MTL::BlendOperation MTL::RenderPipelineColorAttachmentDescriptor::alphaBlendOperation() const
{
    return Object::sendMessage<MTL::BlendOperation>(this, _MTL_PRIVATE_SEL(alphaBlendOperation));
}

_MTL_INLINE void MTL::RenderPipelineColorAttachmentDescriptor::setAlphaBlendOperation(MTL::BlendOperation alphaBlendOperation)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setAlphaBlendOperation_), alphaBlendOperation);
}

// property: writeMask
_MTL_INLINE MTL::ColorWriteMask MTL::RenderPipelineColorAttachmentDescriptor::writeMask() const
{
    return Object::sendMessage<MTL::ColorWriteMask>(this, _MTL_PRIVATE_SEL(writeMask));
}

_MTL_INLINE void MTL::RenderPipelineColorAttachmentDescriptor::setWriteMask(MTL::ColorWriteMask writeMask)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setWriteMask_), writeMask);
}

// static method: alloc
_MTL_INLINE MTL::RenderPipelineReflection* MTL::RenderPipelineReflection::alloc()
{
    return NS::Object::alloc<MTL::RenderPipelineReflection>(_MTL_PRIVATE_CLS(MTLRenderPipelineReflection));
}

// method: init
_MTL_INLINE MTL::RenderPipelineReflection* MTL::RenderPipelineReflection::init()
{
    return NS::Object::init<MTL::RenderPipelineReflection>();
}

// property: vertexArguments
_MTL_INLINE NS::Array* MTL::RenderPipelineReflection::vertexArguments() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(vertexArguments));
}

// property: fragmentArguments
_MTL_INLINE NS::Array* MTL::RenderPipelineReflection::fragmentArguments() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(fragmentArguments));
}

// property: tileArguments
_MTL_INLINE NS::Array* MTL::RenderPipelineReflection::tileArguments() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(tileArguments));
}

// static method: alloc
_MTL_INLINE MTL::RenderPipelineDescriptor* MTL::RenderPipelineDescriptor::alloc()
{
    return NS::Object::alloc<MTL::RenderPipelineDescriptor>(_MTL_PRIVATE_CLS(MTLRenderPipelineDescriptor));
}

// method: init
_MTL_INLINE MTL::RenderPipelineDescriptor* MTL::RenderPipelineDescriptor::init()
{
    return NS::Object::init<MTL::RenderPipelineDescriptor>();
}

// property: label
_MTL_INLINE NS::String* MTL::RenderPipelineDescriptor::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: vertexFunction
_MTL_INLINE MTL::Function* MTL::RenderPipelineDescriptor::vertexFunction() const
{
    return Object::sendMessage<MTL::Function*>(this, _MTL_PRIVATE_SEL(vertexFunction));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setVertexFunction(const MTL::Function* vertexFunction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexFunction_), vertexFunction);
}

// property: fragmentFunction
_MTL_INLINE MTL::Function* MTL::RenderPipelineDescriptor::fragmentFunction() const
{
    return Object::sendMessage<MTL::Function*>(this, _MTL_PRIVATE_SEL(fragmentFunction));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setFragmentFunction(const MTL::Function* fragmentFunction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentFunction_), fragmentFunction);
}

// property: vertexDescriptor
_MTL_INLINE MTL::VertexDescriptor* MTL::RenderPipelineDescriptor::vertexDescriptor() const
{
    return Object::sendMessage<MTL::VertexDescriptor*>(this, _MTL_PRIVATE_SEL(vertexDescriptor));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setVertexDescriptor(const MTL::VertexDescriptor* vertexDescriptor)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexDescriptor_), vertexDescriptor);
}

// property: sampleCount
_MTL_INLINE NS::UInteger MTL::RenderPipelineDescriptor::sampleCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(sampleCount));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setSampleCount(NS::UInteger sampleCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSampleCount_), sampleCount);
}

// property: rasterSampleCount
_MTL_INLINE NS::UInteger MTL::RenderPipelineDescriptor::rasterSampleCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(rasterSampleCount));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setRasterSampleCount(NS::UInteger rasterSampleCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRasterSampleCount_), rasterSampleCount);
}

// property: alphaToCoverageEnabled
_MTL_INLINE bool MTL::RenderPipelineDescriptor::alphaToCoverageEnabled() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isAlphaToCoverageEnabled));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setAlphaToCoverageEnabled(bool alphaToCoverageEnabled)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setAlphaToCoverageEnabled_), alphaToCoverageEnabled);
}

// property: alphaToOneEnabled
_MTL_INLINE bool MTL::RenderPipelineDescriptor::alphaToOneEnabled() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isAlphaToOneEnabled));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setAlphaToOneEnabled(bool alphaToOneEnabled)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setAlphaToOneEnabled_), alphaToOneEnabled);
}

// property: rasterizationEnabled
_MTL_INLINE bool MTL::RenderPipelineDescriptor::rasterizationEnabled() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isRasterizationEnabled));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setRasterizationEnabled(bool rasterizationEnabled)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRasterizationEnabled_), rasterizationEnabled);
}

// property: maxVertexAmplificationCount
_MTL_INLINE NS::UInteger MTL::RenderPipelineDescriptor::maxVertexAmplificationCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxVertexAmplificationCount));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setMaxVertexAmplificationCount(NS::UInteger maxVertexAmplificationCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxVertexAmplificationCount_), maxVertexAmplificationCount);
}

// property: colorAttachments
_MTL_INLINE MTL::RenderPipelineColorAttachmentDescriptorArray* MTL::RenderPipelineDescriptor::colorAttachments() const
{
    return Object::sendMessage<MTL::RenderPipelineColorAttachmentDescriptorArray*>(this, _MTL_PRIVATE_SEL(colorAttachments));
}

// property: depthAttachmentPixelFormat
_MTL_INLINE MTL::PixelFormat MTL::RenderPipelineDescriptor::depthAttachmentPixelFormat() const
{
    return Object::sendMessage<MTL::PixelFormat>(this, _MTL_PRIVATE_SEL(depthAttachmentPixelFormat));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setDepthAttachmentPixelFormat(MTL::PixelFormat depthAttachmentPixelFormat)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthAttachmentPixelFormat_), depthAttachmentPixelFormat);
}

// property: stencilAttachmentPixelFormat
_MTL_INLINE MTL::PixelFormat MTL::RenderPipelineDescriptor::stencilAttachmentPixelFormat() const
{
    return Object::sendMessage<MTL::PixelFormat>(this, _MTL_PRIVATE_SEL(stencilAttachmentPixelFormat));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setStencilAttachmentPixelFormat(MTL::PixelFormat stencilAttachmentPixelFormat)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStencilAttachmentPixelFormat_), stencilAttachmentPixelFormat);
}

// property: inputPrimitiveTopology
_MTL_INLINE MTL::PrimitiveTopologyClass MTL::RenderPipelineDescriptor::inputPrimitiveTopology() const
{
    return Object::sendMessage<MTL::PrimitiveTopologyClass>(this, _MTL_PRIVATE_SEL(inputPrimitiveTopology));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setInputPrimitiveTopology(MTL::PrimitiveTopologyClass inputPrimitiveTopology)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setInputPrimitiveTopology_), inputPrimitiveTopology);
}

// property: tessellationPartitionMode
_MTL_INLINE MTL::TessellationPartitionMode MTL::RenderPipelineDescriptor::tessellationPartitionMode() const
{
    return Object::sendMessage<MTL::TessellationPartitionMode>(this, _MTL_PRIVATE_SEL(tessellationPartitionMode));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setTessellationPartitionMode(MTL::TessellationPartitionMode tessellationPartitionMode)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTessellationPartitionMode_), tessellationPartitionMode);
}

// property: maxTessellationFactor
_MTL_INLINE NS::UInteger MTL::RenderPipelineDescriptor::maxTessellationFactor() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxTessellationFactor));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setMaxTessellationFactor(NS::UInteger maxTessellationFactor)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxTessellationFactor_), maxTessellationFactor);
}

// property: tessellationFactorScaleEnabled
_MTL_INLINE bool MTL::RenderPipelineDescriptor::tessellationFactorScaleEnabled() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isTessellationFactorScaleEnabled));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setTessellationFactorScaleEnabled(bool tessellationFactorScaleEnabled)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTessellationFactorScaleEnabled_), tessellationFactorScaleEnabled);
}

// property: tessellationFactorFormat
_MTL_INLINE MTL::TessellationFactorFormat MTL::RenderPipelineDescriptor::tessellationFactorFormat() const
{
    return Object::sendMessage<MTL::TessellationFactorFormat>(this, _MTL_PRIVATE_SEL(tessellationFactorFormat));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setTessellationFactorFormat(MTL::TessellationFactorFormat tessellationFactorFormat)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTessellationFactorFormat_), tessellationFactorFormat);
}

// property: tessellationControlPointIndexType
_MTL_INLINE MTL::TessellationControlPointIndexType MTL::RenderPipelineDescriptor::tessellationControlPointIndexType() const
{
    return Object::sendMessage<MTL::TessellationControlPointIndexType>(this, _MTL_PRIVATE_SEL(tessellationControlPointIndexType));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setTessellationControlPointIndexType(MTL::TessellationControlPointIndexType tessellationControlPointIndexType)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTessellationControlPointIndexType_), tessellationControlPointIndexType);
}

// property: tessellationFactorStepFunction
_MTL_INLINE MTL::TessellationFactorStepFunction MTL::RenderPipelineDescriptor::tessellationFactorStepFunction() const
{
    return Object::sendMessage<MTL::TessellationFactorStepFunction>(this, _MTL_PRIVATE_SEL(tessellationFactorStepFunction));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setTessellationFactorStepFunction(MTL::TessellationFactorStepFunction tessellationFactorStepFunction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTessellationFactorStepFunction_), tessellationFactorStepFunction);
}

// property: tessellationOutputWindingOrder
_MTL_INLINE MTL::Winding MTL::RenderPipelineDescriptor::tessellationOutputWindingOrder() const
{
    return Object::sendMessage<MTL::Winding>(this, _MTL_PRIVATE_SEL(tessellationOutputWindingOrder));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setTessellationOutputWindingOrder(MTL::Winding tessellationOutputWindingOrder)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTessellationOutputWindingOrder_), tessellationOutputWindingOrder);
}

// property: vertexBuffers
_MTL_INLINE MTL::PipelineBufferDescriptorArray* MTL::RenderPipelineDescriptor::vertexBuffers() const
{
    return Object::sendMessage<MTL::PipelineBufferDescriptorArray*>(this, _MTL_PRIVATE_SEL(vertexBuffers));
}

// property: fragmentBuffers
_MTL_INLINE MTL::PipelineBufferDescriptorArray* MTL::RenderPipelineDescriptor::fragmentBuffers() const
{
    return Object::sendMessage<MTL::PipelineBufferDescriptorArray*>(this, _MTL_PRIVATE_SEL(fragmentBuffers));
}

// property: supportIndirectCommandBuffers
_MTL_INLINE bool MTL::RenderPipelineDescriptor::supportIndirectCommandBuffers() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportIndirectCommandBuffers));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setSupportIndirectCommandBuffers(bool supportIndirectCommandBuffers)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSupportIndirectCommandBuffers_), supportIndirectCommandBuffers);
}

// property: binaryArchives
_MTL_INLINE NS::Array* MTL::RenderPipelineDescriptor::binaryArchives() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(binaryArchives));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setBinaryArchives(const NS::Array* binaryArchives)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBinaryArchives_), binaryArchives);
}

// property: vertexPreloadedLibraries
_MTL_INLINE NS::Array* MTL::RenderPipelineDescriptor::vertexPreloadedLibraries() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(vertexPreloadedLibraries));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setVertexPreloadedLibraries(const NS::Array* vertexPreloadedLibraries)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexPreloadedLibraries_), vertexPreloadedLibraries);
}

// property: fragmentPreloadedLibraries
_MTL_INLINE NS::Array* MTL::RenderPipelineDescriptor::fragmentPreloadedLibraries() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(fragmentPreloadedLibraries));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setFragmentPreloadedLibraries(const NS::Array* fragmentPreloadedLibraries)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentPreloadedLibraries_), fragmentPreloadedLibraries);
}

// property: vertexLinkedFunctions
_MTL_INLINE MTL::LinkedFunctions* MTL::RenderPipelineDescriptor::vertexLinkedFunctions() const
{
    return Object::sendMessage<MTL::LinkedFunctions*>(this, _MTL_PRIVATE_SEL(vertexLinkedFunctions));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setVertexLinkedFunctions(const MTL::LinkedFunctions* vertexLinkedFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexLinkedFunctions_), vertexLinkedFunctions);
}

// property: fragmentLinkedFunctions
_MTL_INLINE MTL::LinkedFunctions* MTL::RenderPipelineDescriptor::fragmentLinkedFunctions() const
{
    return Object::sendMessage<MTL::LinkedFunctions*>(this, _MTL_PRIVATE_SEL(fragmentLinkedFunctions));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setFragmentLinkedFunctions(const MTL::LinkedFunctions* fragmentLinkedFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentLinkedFunctions_), fragmentLinkedFunctions);
}

// property: supportAddingVertexBinaryFunctions
_MTL_INLINE bool MTL::RenderPipelineDescriptor::supportAddingVertexBinaryFunctions() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportAddingVertexBinaryFunctions));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setSupportAddingVertexBinaryFunctions(bool supportAddingVertexBinaryFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSupportAddingVertexBinaryFunctions_), supportAddingVertexBinaryFunctions);
}

// property: supportAddingFragmentBinaryFunctions
_MTL_INLINE bool MTL::RenderPipelineDescriptor::supportAddingFragmentBinaryFunctions() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportAddingFragmentBinaryFunctions));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setSupportAddingFragmentBinaryFunctions(bool supportAddingFragmentBinaryFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSupportAddingFragmentBinaryFunctions_), supportAddingFragmentBinaryFunctions);
}

// property: maxVertexCallStackDepth
_MTL_INLINE NS::UInteger MTL::RenderPipelineDescriptor::maxVertexCallStackDepth() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxVertexCallStackDepth));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setMaxVertexCallStackDepth(NS::UInteger maxVertexCallStackDepth)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxVertexCallStackDepth_), maxVertexCallStackDepth);
}

// property: maxFragmentCallStackDepth
_MTL_INLINE NS::UInteger MTL::RenderPipelineDescriptor::maxFragmentCallStackDepth() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxFragmentCallStackDepth));
}

_MTL_INLINE void MTL::RenderPipelineDescriptor::setMaxFragmentCallStackDepth(NS::UInteger maxFragmentCallStackDepth)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxFragmentCallStackDepth_), maxFragmentCallStackDepth);
}

// method: reset
_MTL_INLINE void MTL::RenderPipelineDescriptor::reset()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(reset));
}

// static method: alloc
_MTL_INLINE MTL::RenderPipelineFunctionsDescriptor* MTL::RenderPipelineFunctionsDescriptor::alloc()
{
    return NS::Object::alloc<MTL::RenderPipelineFunctionsDescriptor>(_MTL_PRIVATE_CLS(MTLRenderPipelineFunctionsDescriptor));
}

// method: init
_MTL_INLINE MTL::RenderPipelineFunctionsDescriptor* MTL::RenderPipelineFunctionsDescriptor::init()
{
    return NS::Object::init<MTL::RenderPipelineFunctionsDescriptor>();
}

// property: vertexAdditionalBinaryFunctions
_MTL_INLINE NS::Array* MTL::RenderPipelineFunctionsDescriptor::vertexAdditionalBinaryFunctions() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(vertexAdditionalBinaryFunctions));
}

_MTL_INLINE void MTL::RenderPipelineFunctionsDescriptor::setVertexAdditionalBinaryFunctions(const NS::Array* vertexAdditionalBinaryFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexAdditionalBinaryFunctions_), vertexAdditionalBinaryFunctions);
}

// property: fragmentAdditionalBinaryFunctions
_MTL_INLINE NS::Array* MTL::RenderPipelineFunctionsDescriptor::fragmentAdditionalBinaryFunctions() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(fragmentAdditionalBinaryFunctions));
}

_MTL_INLINE void MTL::RenderPipelineFunctionsDescriptor::setFragmentAdditionalBinaryFunctions(const NS::Array* fragmentAdditionalBinaryFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentAdditionalBinaryFunctions_), fragmentAdditionalBinaryFunctions);
}

// property: tileAdditionalBinaryFunctions
_MTL_INLINE NS::Array* MTL::RenderPipelineFunctionsDescriptor::tileAdditionalBinaryFunctions() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(tileAdditionalBinaryFunctions));
}

_MTL_INLINE void MTL::RenderPipelineFunctionsDescriptor::setTileAdditionalBinaryFunctions(const NS::Array* tileAdditionalBinaryFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileAdditionalBinaryFunctions_), tileAdditionalBinaryFunctions);
}

// property: label
_MTL_INLINE NS::String* MTL::RenderPipelineState::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

// property: device
_MTL_INLINE MTL::Device* MTL::RenderPipelineState::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: maxTotalThreadsPerThreadgroup
_MTL_INLINE NS::UInteger MTL::RenderPipelineState::maxTotalThreadsPerThreadgroup() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxTotalThreadsPerThreadgroup));
}

// property: threadgroupSizeMatchesTileSize
_MTL_INLINE bool MTL::RenderPipelineState::threadgroupSizeMatchesTileSize() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(threadgroupSizeMatchesTileSize));
}

// property: imageblockSampleLength
_MTL_INLINE NS::UInteger MTL::RenderPipelineState::imageblockSampleLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(imageblockSampleLength));
}

// method: imageblockMemoryLengthForDimensions:
_MTL_INLINE NS::UInteger MTL::RenderPipelineState::imageblockMemoryLength(MTL::Size imageblockDimensions)
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(imageblockMemoryLengthForDimensions_), imageblockDimensions);
}

// property: supportIndirectCommandBuffers
_MTL_INLINE bool MTL::RenderPipelineState::supportIndirectCommandBuffers() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportIndirectCommandBuffers));
}

// method: functionHandleWithFunction:stage:
_MTL_INLINE MTL::FunctionHandle* MTL::RenderPipelineState::functionHandle(const MTL::Function* function, MTL::RenderStages stage)
{
    return Object::sendMessage<MTL::FunctionHandle*>(this, _MTL_PRIVATE_SEL(functionHandleWithFunction_stage_), function, stage);
}

// method: newVisibleFunctionTableWithDescriptor:stage:
_MTL_INLINE MTL::VisibleFunctionTable* MTL::RenderPipelineState::newVisibleFunctionTable(const MTL::VisibleFunctionTableDescriptor* descriptor, MTL::RenderStages stage)
{
    return Object::sendMessage<MTL::VisibleFunctionTable*>(this, _MTL_PRIVATE_SEL(newVisibleFunctionTableWithDescriptor_stage_), descriptor, stage);
}

// method: newIntersectionFunctionTableWithDescriptor:stage:
_MTL_INLINE MTL::IntersectionFunctionTable* MTL::RenderPipelineState::newIntersectionFunctionTable(const MTL::IntersectionFunctionTableDescriptor* descriptor, MTL::RenderStages stage)
{
    return Object::sendMessage<MTL::IntersectionFunctionTable*>(this, _MTL_PRIVATE_SEL(newIntersectionFunctionTableWithDescriptor_stage_), descriptor, stage);
}

// method: newRenderPipelineStateWithAdditionalBinaryFunctions:error:
_MTL_INLINE MTL::RenderPipelineState* MTL::RenderPipelineState::newRenderPipelineState(const MTL::RenderPipelineFunctionsDescriptor* additionalBinaryFunctions, NS::Error** error)
{
    return Object::sendMessage<MTL::RenderPipelineState*>(this, _MTL_PRIVATE_SEL(newRenderPipelineStateWithAdditionalBinaryFunctions_error_), additionalBinaryFunctions, error);
}

// static method: alloc
_MTL_INLINE MTL::RenderPipelineColorAttachmentDescriptorArray* MTL::RenderPipelineColorAttachmentDescriptorArray::alloc()
{
    return NS::Object::alloc<MTL::RenderPipelineColorAttachmentDescriptorArray>(_MTL_PRIVATE_CLS(MTLRenderPipelineColorAttachmentDescriptorArray));
}

// method: init
_MTL_INLINE MTL::RenderPipelineColorAttachmentDescriptorArray* MTL::RenderPipelineColorAttachmentDescriptorArray::init()
{
    return NS::Object::init<MTL::RenderPipelineColorAttachmentDescriptorArray>();
}

// method: objectAtIndexedSubscript:
_MTL_INLINE MTL::RenderPipelineColorAttachmentDescriptor* MTL::RenderPipelineColorAttachmentDescriptorArray::object(NS::UInteger attachmentIndex)
{
    return Object::sendMessage<MTL::RenderPipelineColorAttachmentDescriptor*>(this, _MTL_PRIVATE_SEL(objectAtIndexedSubscript_), attachmentIndex);
}

// method: setObject:atIndexedSubscript:
_MTL_INLINE void MTL::RenderPipelineColorAttachmentDescriptorArray::setObject(const MTL::RenderPipelineColorAttachmentDescriptor* attachment, NS::UInteger attachmentIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setObject_atIndexedSubscript_), attachment, attachmentIndex);
}

// static method: alloc
_MTL_INLINE MTL::TileRenderPipelineColorAttachmentDescriptor* MTL::TileRenderPipelineColorAttachmentDescriptor::alloc()
{
    return NS::Object::alloc<MTL::TileRenderPipelineColorAttachmentDescriptor>(_MTL_PRIVATE_CLS(MTLTileRenderPipelineColorAttachmentDescriptor));
}

// method: init
_MTL_INLINE MTL::TileRenderPipelineColorAttachmentDescriptor* MTL::TileRenderPipelineColorAttachmentDescriptor::init()
{
    return NS::Object::init<MTL::TileRenderPipelineColorAttachmentDescriptor>();
}

// property: pixelFormat
_MTL_INLINE MTL::PixelFormat MTL::TileRenderPipelineColorAttachmentDescriptor::pixelFormat() const
{
    return Object::sendMessage<MTL::PixelFormat>(this, _MTL_PRIVATE_SEL(pixelFormat));
}

_MTL_INLINE void MTL::TileRenderPipelineColorAttachmentDescriptor::setPixelFormat(MTL::PixelFormat pixelFormat)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setPixelFormat_), pixelFormat);
}

// static method: alloc
_MTL_INLINE MTL::TileRenderPipelineColorAttachmentDescriptorArray* MTL::TileRenderPipelineColorAttachmentDescriptorArray::alloc()
{
    return NS::Object::alloc<MTL::TileRenderPipelineColorAttachmentDescriptorArray>(_MTL_PRIVATE_CLS(MTLTileRenderPipelineColorAttachmentDescriptorArray));
}

// method: init
_MTL_INLINE MTL::TileRenderPipelineColorAttachmentDescriptorArray* MTL::TileRenderPipelineColorAttachmentDescriptorArray::init()
{
    return NS::Object::init<MTL::TileRenderPipelineColorAttachmentDescriptorArray>();
}

// method: objectAtIndexedSubscript:
_MTL_INLINE MTL::TileRenderPipelineColorAttachmentDescriptor* MTL::TileRenderPipelineColorAttachmentDescriptorArray::object(NS::UInteger attachmentIndex)
{
    return Object::sendMessage<MTL::TileRenderPipelineColorAttachmentDescriptor*>(this, _MTL_PRIVATE_SEL(objectAtIndexedSubscript_), attachmentIndex);
}

// method: setObject:atIndexedSubscript:
_MTL_INLINE void MTL::TileRenderPipelineColorAttachmentDescriptorArray::setObject(const MTL::TileRenderPipelineColorAttachmentDescriptor* attachment, NS::UInteger attachmentIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setObject_atIndexedSubscript_), attachment, attachmentIndex);
}

// static method: alloc
_MTL_INLINE MTL::TileRenderPipelineDescriptor* MTL::TileRenderPipelineDescriptor::alloc()
{
    return NS::Object::alloc<MTL::TileRenderPipelineDescriptor>(_MTL_PRIVATE_CLS(MTLTileRenderPipelineDescriptor));
}

// method: init
_MTL_INLINE MTL::TileRenderPipelineDescriptor* MTL::TileRenderPipelineDescriptor::init()
{
    return NS::Object::init<MTL::TileRenderPipelineDescriptor>();
}

// property: label
_MTL_INLINE NS::String* MTL::TileRenderPipelineDescriptor::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::TileRenderPipelineDescriptor::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: tileFunction
_MTL_INLINE MTL::Function* MTL::TileRenderPipelineDescriptor::tileFunction() const
{
    return Object::sendMessage<MTL::Function*>(this, _MTL_PRIVATE_SEL(tileFunction));
}

_MTL_INLINE void MTL::TileRenderPipelineDescriptor::setTileFunction(const MTL::Function* tileFunction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileFunction_), tileFunction);
}

// property: rasterSampleCount
_MTL_INLINE NS::UInteger MTL::TileRenderPipelineDescriptor::rasterSampleCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(rasterSampleCount));
}

_MTL_INLINE void MTL::TileRenderPipelineDescriptor::setRasterSampleCount(NS::UInteger rasterSampleCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRasterSampleCount_), rasterSampleCount);
}

// property: colorAttachments
_MTL_INLINE MTL::TileRenderPipelineColorAttachmentDescriptorArray* MTL::TileRenderPipelineDescriptor::colorAttachments() const
{
    return Object::sendMessage<MTL::TileRenderPipelineColorAttachmentDescriptorArray*>(this, _MTL_PRIVATE_SEL(colorAttachments));
}

// property: threadgroupSizeMatchesTileSize
_MTL_INLINE bool MTL::TileRenderPipelineDescriptor::threadgroupSizeMatchesTileSize() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(threadgroupSizeMatchesTileSize));
}

_MTL_INLINE void MTL::TileRenderPipelineDescriptor::setThreadgroupSizeMatchesTileSize(bool threadgroupSizeMatchesTileSize)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setThreadgroupSizeMatchesTileSize_), threadgroupSizeMatchesTileSize);
}

// property: tileBuffers
_MTL_INLINE MTL::PipelineBufferDescriptorArray* MTL::TileRenderPipelineDescriptor::tileBuffers() const
{
    return Object::sendMessage<MTL::PipelineBufferDescriptorArray*>(this, _MTL_PRIVATE_SEL(tileBuffers));
}

// property: maxTotalThreadsPerThreadgroup
_MTL_INLINE NS::UInteger MTL::TileRenderPipelineDescriptor::maxTotalThreadsPerThreadgroup() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxTotalThreadsPerThreadgroup));
}

_MTL_INLINE void MTL::TileRenderPipelineDescriptor::setMaxTotalThreadsPerThreadgroup(NS::UInteger maxTotalThreadsPerThreadgroup)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxTotalThreadsPerThreadgroup_), maxTotalThreadsPerThreadgroup);
}

// property: binaryArchives
_MTL_INLINE NS::Array* MTL::TileRenderPipelineDescriptor::binaryArchives() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(binaryArchives));
}

_MTL_INLINE void MTL::TileRenderPipelineDescriptor::setBinaryArchives(const NS::Array* binaryArchives)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBinaryArchives_), binaryArchives);
}

// property: preloadedLibraries
_MTL_INLINE NS::Array* MTL::TileRenderPipelineDescriptor::preloadedLibraries() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(preloadedLibraries));
}

_MTL_INLINE void MTL::TileRenderPipelineDescriptor::setPreloadedLibraries(const NS::Array* preloadedLibraries)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setPreloadedLibraries_), preloadedLibraries);
}

// property: linkedFunctions
_MTL_INLINE MTL::LinkedFunctions* MTL::TileRenderPipelineDescriptor::linkedFunctions() const
{
    return Object::sendMessage<MTL::LinkedFunctions*>(this, _MTL_PRIVATE_SEL(linkedFunctions));
}

_MTL_INLINE void MTL::TileRenderPipelineDescriptor::setLinkedFunctions(const MTL::LinkedFunctions* linkedFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLinkedFunctions_), linkedFunctions);
}

// property: supportAddingBinaryFunctions
_MTL_INLINE bool MTL::TileRenderPipelineDescriptor::supportAddingBinaryFunctions() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportAddingBinaryFunctions));
}

_MTL_INLINE void MTL::TileRenderPipelineDescriptor::setSupportAddingBinaryFunctions(bool supportAddingBinaryFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSupportAddingBinaryFunctions_), supportAddingBinaryFunctions);
}

// property: maxCallStackDepth
_MTL_INLINE NS::UInteger MTL::TileRenderPipelineDescriptor::maxCallStackDepth() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxCallStackDepth));
}

_MTL_INLINE void MTL::TileRenderPipelineDescriptor::setMaxCallStackDepth(NS::UInteger maxCallStackDepth)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxCallStackDepth_), maxCallStackDepth);
}

// method: reset
_MTL_INLINE void MTL::TileRenderPipelineDescriptor::reset()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(reset));
}
