//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLRenderPass.hpp
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

#include "MTLRenderPass.hpp"

namespace MTL
{
_MTL_ENUM(NS::UInteger, LoadAction) {
    LoadActionDontCare = 0,
    LoadActionLoad = 1,
    LoadActionClear = 2,
};

_MTL_ENUM(NS::UInteger, StoreAction) {
    StoreActionDontCare = 0,
    StoreActionStore = 1,
    StoreActionMultisampleResolve = 2,
    StoreActionStoreAndMultisampleResolve = 3,
    StoreActionUnknown = 4,
    StoreActionCustomSampleDepthStore = 5,
};

_MTL_OPTIONS(NS::UInteger, StoreActionOptions) {
    StoreActionOptionNone = 0,
    StoreActionOptionValidMask = 1,
    StoreActionOptionCustomSamplePositions = 1,
};

struct ClearColor
{
    static ClearColor Make(double red, double green, double blue, double alpha);

    ClearColor() = default;

    ClearColor(double red, double green, double blue, double alpha);

    double red;
    double green;
    double blue;
    double alpha;
} _MTL_PACKED;

class RenderPassAttachmentDescriptor : public NS::Copying<RenderPassAttachmentDescriptor>
{
public:
    static class RenderPassAttachmentDescriptor* alloc();

    class RenderPassAttachmentDescriptor*        init();

    class Texture*                               texture() const;
    void                                         setTexture(const class Texture* texture);

    NS::UInteger                                 level() const;
    void                                         setLevel(NS::UInteger level);

    NS::UInteger                                 slice() const;
    void                                         setSlice(NS::UInteger slice);

    NS::UInteger                                 depthPlane() const;
    void                                         setDepthPlane(NS::UInteger depthPlane);

    class Texture*                               resolveTexture() const;
    void                                         setResolveTexture(const class Texture* resolveTexture);

    NS::UInteger                                 resolveLevel() const;
    void                                         setResolveLevel(NS::UInteger resolveLevel);

    NS::UInteger                                 resolveSlice() const;
    void                                         setResolveSlice(NS::UInteger resolveSlice);

    NS::UInteger                                 resolveDepthPlane() const;
    void                                         setResolveDepthPlane(NS::UInteger resolveDepthPlane);

    MTL::LoadAction                              loadAction() const;
    void                                         setLoadAction(MTL::LoadAction loadAction);

    MTL::StoreAction                             storeAction() const;
    void                                         setStoreAction(MTL::StoreAction storeAction);

    MTL::StoreActionOptions                      storeActionOptions() const;
    void                                         setStoreActionOptions(MTL::StoreActionOptions storeActionOptions);
};

class RenderPassColorAttachmentDescriptor : public NS::Copying<RenderPassColorAttachmentDescriptor, MTL::RenderPassAttachmentDescriptor>
{
public:
    static class RenderPassColorAttachmentDescriptor* alloc();

    class RenderPassColorAttachmentDescriptor*        init();

    MTL::ClearColor                                   clearColor() const;
    void                                              setClearColor(MTL::ClearColor clearColor);
};

_MTL_ENUM(NS::UInteger, MultisampleDepthResolveFilter) {
    MultisampleDepthResolveFilterSample0 = 0,
    MultisampleDepthResolveFilterMin = 1,
    MultisampleDepthResolveFilterMax = 2,
};

class RenderPassDepthAttachmentDescriptor : public NS::Copying<RenderPassDepthAttachmentDescriptor, MTL::RenderPassAttachmentDescriptor>
{
public:
    static class RenderPassDepthAttachmentDescriptor* alloc();

    class RenderPassDepthAttachmentDescriptor*        init();

    double                                            clearDepth() const;
    void                                              setClearDepth(double clearDepth);

    MTL::MultisampleDepthResolveFilter                depthResolveFilter() const;
    void                                              setDepthResolveFilter(MTL::MultisampleDepthResolveFilter depthResolveFilter);
};

_MTL_ENUM(NS::UInteger, MultisampleStencilResolveFilter) {
    MultisampleStencilResolveFilterSample0 = 0,
    MultisampleStencilResolveFilterDepthResolvedSample = 1,
};

class RenderPassStencilAttachmentDescriptor : public NS::Copying<RenderPassStencilAttachmentDescriptor, MTL::RenderPassAttachmentDescriptor>
{
public:
    static class RenderPassStencilAttachmentDescriptor* alloc();

    class RenderPassStencilAttachmentDescriptor*        init();

    uint32_t                                            clearStencil() const;
    void                                                setClearStencil(uint32_t clearStencil);

    MTL::MultisampleStencilResolveFilter                stencilResolveFilter() const;
    void                                                setStencilResolveFilter(MTL::MultisampleStencilResolveFilter stencilResolveFilter);
};

class RenderPassColorAttachmentDescriptorArray : public NS::Referencing<RenderPassColorAttachmentDescriptorArray>
{
public:
    static class RenderPassColorAttachmentDescriptorArray* alloc();

    class RenderPassColorAttachmentDescriptorArray*        init();

    class RenderPassColorAttachmentDescriptor*             object(NS::UInteger attachmentIndex);

    void                                                   setObject(const class RenderPassColorAttachmentDescriptor* attachment, NS::UInteger attachmentIndex);
};

class RenderPassSampleBufferAttachmentDescriptor : public NS::Copying<RenderPassSampleBufferAttachmentDescriptor>
{
public:
    static class RenderPassSampleBufferAttachmentDescriptor* alloc();

    class RenderPassSampleBufferAttachmentDescriptor*        init();

    class CounterSampleBuffer*                               sampleBuffer() const;
    void                                                     setSampleBuffer(const class CounterSampleBuffer* sampleBuffer);

    NS::UInteger                                             startOfVertexSampleIndex() const;
    void                                                     setStartOfVertexSampleIndex(NS::UInteger startOfVertexSampleIndex);

    NS::UInteger                                             endOfVertexSampleIndex() const;
    void                                                     setEndOfVertexSampleIndex(NS::UInteger endOfVertexSampleIndex);

    NS::UInteger                                             startOfFragmentSampleIndex() const;
    void                                                     setStartOfFragmentSampleIndex(NS::UInteger startOfFragmentSampleIndex);

    NS::UInteger                                             endOfFragmentSampleIndex() const;
    void                                                     setEndOfFragmentSampleIndex(NS::UInteger endOfFragmentSampleIndex);
};

class RenderPassSampleBufferAttachmentDescriptorArray : public NS::Referencing<RenderPassSampleBufferAttachmentDescriptorArray>
{
public:
    static class RenderPassSampleBufferAttachmentDescriptorArray* alloc();

    class RenderPassSampleBufferAttachmentDescriptorArray*        init();

    class RenderPassSampleBufferAttachmentDescriptor*             object(NS::UInteger attachmentIndex);

    void                                                          setObject(const class RenderPassSampleBufferAttachmentDescriptor* attachment, NS::UInteger attachmentIndex);
};

class RenderPassDescriptor : public NS::Copying<RenderPassDescriptor>
{
public:
    static class RenderPassDescriptor*                     alloc();

    class RenderPassDescriptor*                            init();

    static class RenderPassDescriptor*                     renderPassDescriptor();

    class RenderPassColorAttachmentDescriptorArray*        colorAttachments() const;

    class RenderPassDepthAttachmentDescriptor*             depthAttachment() const;
    void                                                   setDepthAttachment(const class RenderPassDepthAttachmentDescriptor* depthAttachment);

    class RenderPassStencilAttachmentDescriptor*           stencilAttachment() const;
    void                                                   setStencilAttachment(const class RenderPassStencilAttachmentDescriptor* stencilAttachment);

    class Buffer*                                          visibilityResultBuffer() const;
    void                                                   setVisibilityResultBuffer(const class Buffer* visibilityResultBuffer);

    NS::UInteger                                           renderTargetArrayLength() const;
    void                                                   setRenderTargetArrayLength(NS::UInteger renderTargetArrayLength);

    NS::UInteger                                           imageblockSampleLength() const;
    void                                                   setImageblockSampleLength(NS::UInteger imageblockSampleLength);

    NS::UInteger                                           threadgroupMemoryLength() const;
    void                                                   setThreadgroupMemoryLength(NS::UInteger threadgroupMemoryLength);

    NS::UInteger                                           tileWidth() const;
    void                                                   setTileWidth(NS::UInteger tileWidth);

    NS::UInteger                                           tileHeight() const;
    void                                                   setTileHeight(NS::UInteger tileHeight);

    NS::UInteger                                           defaultRasterSampleCount() const;
    void                                                   setDefaultRasterSampleCount(NS::UInteger defaultRasterSampleCount);

    NS::UInteger                                           renderTargetWidth() const;
    void                                                   setRenderTargetWidth(NS::UInteger renderTargetWidth);

    NS::UInteger                                           renderTargetHeight() const;
    void                                                   setRenderTargetHeight(NS::UInteger renderTargetHeight);

    void                                                   setSamplePositions(const MTL::SamplePosition* positions, NS::UInteger count);

    NS::UInteger                                           getSamplePositions(MTL::SamplePosition* positions, NS::UInteger count);

    class RasterizationRateMap*                            rasterizationRateMap() const;
    void                                                   setRasterizationRateMap(const class RasterizationRateMap* rasterizationRateMap);

    class RenderPassSampleBufferAttachmentDescriptorArray* sampleBufferAttachments() const;
};

}

_MTL_INLINE MTL::ClearColor MTL::ClearColor::Make(double red, double green, double blue, double alpha)
{
    return ClearColor(red, green, blue, alpha);
}

_MTL_INLINE MTL::ClearColor::ClearColor(double _red, double _green, double _blue, double _alpha)
    : red(_red)
    , green(_green)
    , blue(_blue)
    , alpha(_alpha)
{
}

// static method: alloc
_MTL_INLINE MTL::RenderPassAttachmentDescriptor* MTL::RenderPassAttachmentDescriptor::alloc()
{
    return NS::Object::alloc<MTL::RenderPassAttachmentDescriptor>(_MTL_PRIVATE_CLS(MTLRenderPassAttachmentDescriptor));
}

// method: init
_MTL_INLINE MTL::RenderPassAttachmentDescriptor* MTL::RenderPassAttachmentDescriptor::init()
{
    return NS::Object::init<MTL::RenderPassAttachmentDescriptor>();
}

// property: texture
_MTL_INLINE MTL::Texture* MTL::RenderPassAttachmentDescriptor::texture() const
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(texture));
}

_MTL_INLINE void MTL::RenderPassAttachmentDescriptor::setTexture(const MTL::Texture* texture)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTexture_), texture);
}

// property: level
_MTL_INLINE NS::UInteger MTL::RenderPassAttachmentDescriptor::level() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(level));
}

_MTL_INLINE void MTL::RenderPassAttachmentDescriptor::setLevel(NS::UInteger level)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLevel_), level);
}

// property: slice
_MTL_INLINE NS::UInteger MTL::RenderPassAttachmentDescriptor::slice() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(slice));
}

_MTL_INLINE void MTL::RenderPassAttachmentDescriptor::setSlice(NS::UInteger slice)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSlice_), slice);
}

// property: depthPlane
_MTL_INLINE NS::UInteger MTL::RenderPassAttachmentDescriptor::depthPlane() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(depthPlane));
}

_MTL_INLINE void MTL::RenderPassAttachmentDescriptor::setDepthPlane(NS::UInteger depthPlane)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthPlane_), depthPlane);
}

// property: resolveTexture
_MTL_INLINE MTL::Texture* MTL::RenderPassAttachmentDescriptor::resolveTexture() const
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(resolveTexture));
}

_MTL_INLINE void MTL::RenderPassAttachmentDescriptor::setResolveTexture(const MTL::Texture* resolveTexture)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setResolveTexture_), resolveTexture);
}

// property: resolveLevel
_MTL_INLINE NS::UInteger MTL::RenderPassAttachmentDescriptor::resolveLevel() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(resolveLevel));
}

_MTL_INLINE void MTL::RenderPassAttachmentDescriptor::setResolveLevel(NS::UInteger resolveLevel)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setResolveLevel_), resolveLevel);
}

// property: resolveSlice
_MTL_INLINE NS::UInteger MTL::RenderPassAttachmentDescriptor::resolveSlice() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(resolveSlice));
}

_MTL_INLINE void MTL::RenderPassAttachmentDescriptor::setResolveSlice(NS::UInteger resolveSlice)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setResolveSlice_), resolveSlice);
}

// property: resolveDepthPlane
_MTL_INLINE NS::UInteger MTL::RenderPassAttachmentDescriptor::resolveDepthPlane() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(resolveDepthPlane));
}

_MTL_INLINE void MTL::RenderPassAttachmentDescriptor::setResolveDepthPlane(NS::UInteger resolveDepthPlane)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setResolveDepthPlane_), resolveDepthPlane);
}

// property: loadAction
_MTL_INLINE MTL::LoadAction MTL::RenderPassAttachmentDescriptor::loadAction() const
{
    return Object::sendMessage<MTL::LoadAction>(this, _MTL_PRIVATE_SEL(loadAction));
}

_MTL_INLINE void MTL::RenderPassAttachmentDescriptor::setLoadAction(MTL::LoadAction loadAction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLoadAction_), loadAction);
}

// property: storeAction
_MTL_INLINE MTL::StoreAction MTL::RenderPassAttachmentDescriptor::storeAction() const
{
    return Object::sendMessage<MTL::StoreAction>(this, _MTL_PRIVATE_SEL(storeAction));
}

_MTL_INLINE void MTL::RenderPassAttachmentDescriptor::setStoreAction(MTL::StoreAction storeAction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStoreAction_), storeAction);
}

// property: storeActionOptions
_MTL_INLINE MTL::StoreActionOptions MTL::RenderPassAttachmentDescriptor::storeActionOptions() const
{
    return Object::sendMessage<MTL::StoreActionOptions>(this, _MTL_PRIVATE_SEL(storeActionOptions));
}

_MTL_INLINE void MTL::RenderPassAttachmentDescriptor::setStoreActionOptions(MTL::StoreActionOptions storeActionOptions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStoreActionOptions_), storeActionOptions);
}

// static method: alloc
_MTL_INLINE MTL::RenderPassColorAttachmentDescriptor* MTL::RenderPassColorAttachmentDescriptor::alloc()
{
    return NS::Object::alloc<MTL::RenderPassColorAttachmentDescriptor>(_MTL_PRIVATE_CLS(MTLRenderPassColorAttachmentDescriptor));
}

// method: init
_MTL_INLINE MTL::RenderPassColorAttachmentDescriptor* MTL::RenderPassColorAttachmentDescriptor::init()
{
    return NS::Object::init<MTL::RenderPassColorAttachmentDescriptor>();
}

// property: clearColor
_MTL_INLINE MTL::ClearColor MTL::RenderPassColorAttachmentDescriptor::clearColor() const
{
    return Object::sendMessage<MTL::ClearColor>(this, _MTL_PRIVATE_SEL(clearColor));
}

_MTL_INLINE void MTL::RenderPassColorAttachmentDescriptor::setClearColor(MTL::ClearColor clearColor)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setClearColor_), clearColor);
}

// static method: alloc
_MTL_INLINE MTL::RenderPassDepthAttachmentDescriptor* MTL::RenderPassDepthAttachmentDescriptor::alloc()
{
    return NS::Object::alloc<MTL::RenderPassDepthAttachmentDescriptor>(_MTL_PRIVATE_CLS(MTLRenderPassDepthAttachmentDescriptor));
}

// method: init
_MTL_INLINE MTL::RenderPassDepthAttachmentDescriptor* MTL::RenderPassDepthAttachmentDescriptor::init()
{
    return NS::Object::init<MTL::RenderPassDepthAttachmentDescriptor>();
}

// property: clearDepth
_MTL_INLINE double MTL::RenderPassDepthAttachmentDescriptor::clearDepth() const
{
    return Object::sendMessage<double>(this, _MTL_PRIVATE_SEL(clearDepth));
}

_MTL_INLINE void MTL::RenderPassDepthAttachmentDescriptor::setClearDepth(double clearDepth)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setClearDepth_), clearDepth);
}

// property: depthResolveFilter
_MTL_INLINE MTL::MultisampleDepthResolveFilter MTL::RenderPassDepthAttachmentDescriptor::depthResolveFilter() const
{
    return Object::sendMessage<MTL::MultisampleDepthResolveFilter>(this, _MTL_PRIVATE_SEL(depthResolveFilter));
}

_MTL_INLINE void MTL::RenderPassDepthAttachmentDescriptor::setDepthResolveFilter(MTL::MultisampleDepthResolveFilter depthResolveFilter)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthResolveFilter_), depthResolveFilter);
}

// static method: alloc
_MTL_INLINE MTL::RenderPassStencilAttachmentDescriptor* MTL::RenderPassStencilAttachmentDescriptor::alloc()
{
    return NS::Object::alloc<MTL::RenderPassStencilAttachmentDescriptor>(_MTL_PRIVATE_CLS(MTLRenderPassStencilAttachmentDescriptor));
}

// method: init
_MTL_INLINE MTL::RenderPassStencilAttachmentDescriptor* MTL::RenderPassStencilAttachmentDescriptor::init()
{
    return NS::Object::init<MTL::RenderPassStencilAttachmentDescriptor>();
}

// property: clearStencil
_MTL_INLINE uint32_t MTL::RenderPassStencilAttachmentDescriptor::clearStencil() const
{
    return Object::sendMessage<uint32_t>(this, _MTL_PRIVATE_SEL(clearStencil));
}

_MTL_INLINE void MTL::RenderPassStencilAttachmentDescriptor::setClearStencil(uint32_t clearStencil)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setClearStencil_), clearStencil);
}

// property: stencilResolveFilter
_MTL_INLINE MTL::MultisampleStencilResolveFilter MTL::RenderPassStencilAttachmentDescriptor::stencilResolveFilter() const
{
    return Object::sendMessage<MTL::MultisampleStencilResolveFilter>(this, _MTL_PRIVATE_SEL(stencilResolveFilter));
}

_MTL_INLINE void MTL::RenderPassStencilAttachmentDescriptor::setStencilResolveFilter(MTL::MultisampleStencilResolveFilter stencilResolveFilter)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStencilResolveFilter_), stencilResolveFilter);
}

// static method: alloc
_MTL_INLINE MTL::RenderPassColorAttachmentDescriptorArray* MTL::RenderPassColorAttachmentDescriptorArray::alloc()
{
    return NS::Object::alloc<MTL::RenderPassColorAttachmentDescriptorArray>(_MTL_PRIVATE_CLS(MTLRenderPassColorAttachmentDescriptorArray));
}

// method: init
_MTL_INLINE MTL::RenderPassColorAttachmentDescriptorArray* MTL::RenderPassColorAttachmentDescriptorArray::init()
{
    return NS::Object::init<MTL::RenderPassColorAttachmentDescriptorArray>();
}

// method: objectAtIndexedSubscript:
_MTL_INLINE MTL::RenderPassColorAttachmentDescriptor* MTL::RenderPassColorAttachmentDescriptorArray::object(NS::UInteger attachmentIndex)
{
    return Object::sendMessage<MTL::RenderPassColorAttachmentDescriptor*>(this, _MTL_PRIVATE_SEL(objectAtIndexedSubscript_), attachmentIndex);
}

// method: setObject:atIndexedSubscript:
_MTL_INLINE void MTL::RenderPassColorAttachmentDescriptorArray::setObject(const MTL::RenderPassColorAttachmentDescriptor* attachment, NS::UInteger attachmentIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setObject_atIndexedSubscript_), attachment, attachmentIndex);
}

// static method: alloc
_MTL_INLINE MTL::RenderPassSampleBufferAttachmentDescriptor* MTL::RenderPassSampleBufferAttachmentDescriptor::alloc()
{
    return NS::Object::alloc<MTL::RenderPassSampleBufferAttachmentDescriptor>(_MTL_PRIVATE_CLS(MTLRenderPassSampleBufferAttachmentDescriptor));
}

// method: init
_MTL_INLINE MTL::RenderPassSampleBufferAttachmentDescriptor* MTL::RenderPassSampleBufferAttachmentDescriptor::init()
{
    return NS::Object::init<MTL::RenderPassSampleBufferAttachmentDescriptor>();
}

// property: sampleBuffer
_MTL_INLINE MTL::CounterSampleBuffer* MTL::RenderPassSampleBufferAttachmentDescriptor::sampleBuffer() const
{
    return Object::sendMessage<MTL::CounterSampleBuffer*>(this, _MTL_PRIVATE_SEL(sampleBuffer));
}

_MTL_INLINE void MTL::RenderPassSampleBufferAttachmentDescriptor::setSampleBuffer(const MTL::CounterSampleBuffer* sampleBuffer)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSampleBuffer_), sampleBuffer);
}

// property: startOfVertexSampleIndex
_MTL_INLINE NS::UInteger MTL::RenderPassSampleBufferAttachmentDescriptor::startOfVertexSampleIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(startOfVertexSampleIndex));
}

_MTL_INLINE void MTL::RenderPassSampleBufferAttachmentDescriptor::setStartOfVertexSampleIndex(NS::UInteger startOfVertexSampleIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStartOfVertexSampleIndex_), startOfVertexSampleIndex);
}

// property: endOfVertexSampleIndex
_MTL_INLINE NS::UInteger MTL::RenderPassSampleBufferAttachmentDescriptor::endOfVertexSampleIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(endOfVertexSampleIndex));
}

_MTL_INLINE void MTL::RenderPassSampleBufferAttachmentDescriptor::setEndOfVertexSampleIndex(NS::UInteger endOfVertexSampleIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setEndOfVertexSampleIndex_), endOfVertexSampleIndex);
}

// property: startOfFragmentSampleIndex
_MTL_INLINE NS::UInteger MTL::RenderPassSampleBufferAttachmentDescriptor::startOfFragmentSampleIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(startOfFragmentSampleIndex));
}

_MTL_INLINE void MTL::RenderPassSampleBufferAttachmentDescriptor::setStartOfFragmentSampleIndex(NS::UInteger startOfFragmentSampleIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStartOfFragmentSampleIndex_), startOfFragmentSampleIndex);
}

// property: endOfFragmentSampleIndex
_MTL_INLINE NS::UInteger MTL::RenderPassSampleBufferAttachmentDescriptor::endOfFragmentSampleIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(endOfFragmentSampleIndex));
}

_MTL_INLINE void MTL::RenderPassSampleBufferAttachmentDescriptor::setEndOfFragmentSampleIndex(NS::UInteger endOfFragmentSampleIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setEndOfFragmentSampleIndex_), endOfFragmentSampleIndex);
}

// static method: alloc
_MTL_INLINE MTL::RenderPassSampleBufferAttachmentDescriptorArray* MTL::RenderPassSampleBufferAttachmentDescriptorArray::alloc()
{
    return NS::Object::alloc<MTL::RenderPassSampleBufferAttachmentDescriptorArray>(_MTL_PRIVATE_CLS(MTLRenderPassSampleBufferAttachmentDescriptorArray));
}

// method: init
_MTL_INLINE MTL::RenderPassSampleBufferAttachmentDescriptorArray* MTL::RenderPassSampleBufferAttachmentDescriptorArray::init()
{
    return NS::Object::init<MTL::RenderPassSampleBufferAttachmentDescriptorArray>();
}

// method: objectAtIndexedSubscript:
_MTL_INLINE MTL::RenderPassSampleBufferAttachmentDescriptor* MTL::RenderPassSampleBufferAttachmentDescriptorArray::object(NS::UInteger attachmentIndex)
{
    return Object::sendMessage<MTL::RenderPassSampleBufferAttachmentDescriptor*>(this, _MTL_PRIVATE_SEL(objectAtIndexedSubscript_), attachmentIndex);
}

// method: setObject:atIndexedSubscript:
_MTL_INLINE void MTL::RenderPassSampleBufferAttachmentDescriptorArray::setObject(const MTL::RenderPassSampleBufferAttachmentDescriptor* attachment, NS::UInteger attachmentIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setObject_atIndexedSubscript_), attachment, attachmentIndex);
}

// static method: alloc
_MTL_INLINE MTL::RenderPassDescriptor* MTL::RenderPassDescriptor::alloc()
{
    return NS::Object::alloc<MTL::RenderPassDescriptor>(_MTL_PRIVATE_CLS(MTLRenderPassDescriptor));
}

// method: init
_MTL_INLINE MTL::RenderPassDescriptor* MTL::RenderPassDescriptor::init()
{
    return NS::Object::init<MTL::RenderPassDescriptor>();
}

// static method: renderPassDescriptor
_MTL_INLINE MTL::RenderPassDescriptor* MTL::RenderPassDescriptor::renderPassDescriptor()
{
    return Object::sendMessage<MTL::RenderPassDescriptor*>(_MTL_PRIVATE_CLS(MTLRenderPassDescriptor), _MTL_PRIVATE_SEL(renderPassDescriptor));
}

// property: colorAttachments
_MTL_INLINE MTL::RenderPassColorAttachmentDescriptorArray* MTL::RenderPassDescriptor::colorAttachments() const
{
    return Object::sendMessage<MTL::RenderPassColorAttachmentDescriptorArray*>(this, _MTL_PRIVATE_SEL(colorAttachments));
}

// property: depthAttachment
_MTL_INLINE MTL::RenderPassDepthAttachmentDescriptor* MTL::RenderPassDescriptor::depthAttachment() const
{
    return Object::sendMessage<MTL::RenderPassDepthAttachmentDescriptor*>(this, _MTL_PRIVATE_SEL(depthAttachment));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setDepthAttachment(const MTL::RenderPassDepthAttachmentDescriptor* depthAttachment)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthAttachment_), depthAttachment);
}

// property: stencilAttachment
_MTL_INLINE MTL::RenderPassStencilAttachmentDescriptor* MTL::RenderPassDescriptor::stencilAttachment() const
{
    return Object::sendMessage<MTL::RenderPassStencilAttachmentDescriptor*>(this, _MTL_PRIVATE_SEL(stencilAttachment));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setStencilAttachment(const MTL::RenderPassStencilAttachmentDescriptor* stencilAttachment)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStencilAttachment_), stencilAttachment);
}

// property: visibilityResultBuffer
_MTL_INLINE MTL::Buffer* MTL::RenderPassDescriptor::visibilityResultBuffer() const
{
    return Object::sendMessage<MTL::Buffer*>(this, _MTL_PRIVATE_SEL(visibilityResultBuffer));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setVisibilityResultBuffer(const MTL::Buffer* visibilityResultBuffer)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVisibilityResultBuffer_), visibilityResultBuffer);
}

// property: renderTargetArrayLength
_MTL_INLINE NS::UInteger MTL::RenderPassDescriptor::renderTargetArrayLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(renderTargetArrayLength));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setRenderTargetArrayLength(NS::UInteger renderTargetArrayLength)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRenderTargetArrayLength_), renderTargetArrayLength);
}

// property: imageblockSampleLength
_MTL_INLINE NS::UInteger MTL::RenderPassDescriptor::imageblockSampleLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(imageblockSampleLength));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setImageblockSampleLength(NS::UInteger imageblockSampleLength)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setImageblockSampleLength_), imageblockSampleLength);
}

// property: threadgroupMemoryLength
_MTL_INLINE NS::UInteger MTL::RenderPassDescriptor::threadgroupMemoryLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(threadgroupMemoryLength));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setThreadgroupMemoryLength(NS::UInteger threadgroupMemoryLength)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setThreadgroupMemoryLength_), threadgroupMemoryLength);
}

// property: tileWidth
_MTL_INLINE NS::UInteger MTL::RenderPassDescriptor::tileWidth() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(tileWidth));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setTileWidth(NS::UInteger tileWidth)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileWidth_), tileWidth);
}

// property: tileHeight
_MTL_INLINE NS::UInteger MTL::RenderPassDescriptor::tileHeight() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(tileHeight));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setTileHeight(NS::UInteger tileHeight)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileHeight_), tileHeight);
}

// property: defaultRasterSampleCount
_MTL_INLINE NS::UInteger MTL::RenderPassDescriptor::defaultRasterSampleCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(defaultRasterSampleCount));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setDefaultRasterSampleCount(NS::UInteger defaultRasterSampleCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDefaultRasterSampleCount_), defaultRasterSampleCount);
}

// property: renderTargetWidth
_MTL_INLINE NS::UInteger MTL::RenderPassDescriptor::renderTargetWidth() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(renderTargetWidth));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setRenderTargetWidth(NS::UInteger renderTargetWidth)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRenderTargetWidth_), renderTargetWidth);
}

// property: renderTargetHeight
_MTL_INLINE NS::UInteger MTL::RenderPassDescriptor::renderTargetHeight() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(renderTargetHeight));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setRenderTargetHeight(NS::UInteger renderTargetHeight)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRenderTargetHeight_), renderTargetHeight);
}

// method: setSamplePositions:count:
_MTL_INLINE void MTL::RenderPassDescriptor::setSamplePositions(const MTL::SamplePosition* positions, NS::UInteger count)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSamplePositions_count_), positions, count);
}

// method: getSamplePositions:count:
_MTL_INLINE NS::UInteger MTL::RenderPassDescriptor::getSamplePositions(MTL::SamplePosition* positions, NS::UInteger count)
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(getSamplePositions_count_), positions, count);
}

// property: rasterizationRateMap
_MTL_INLINE MTL::RasterizationRateMap* MTL::RenderPassDescriptor::rasterizationRateMap() const
{
    return Object::sendMessage<MTL::RasterizationRateMap*>(this, _MTL_PRIVATE_SEL(rasterizationRateMap));
}

_MTL_INLINE void MTL::RenderPassDescriptor::setRasterizationRateMap(const MTL::RasterizationRateMap* rasterizationRateMap)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRasterizationRateMap_), rasterizationRateMap);
}

// property: sampleBufferAttachments
_MTL_INLINE MTL::RenderPassSampleBufferAttachmentDescriptorArray* MTL::RenderPassDescriptor::sampleBufferAttachments() const
{
    return Object::sendMessage<MTL::RenderPassSampleBufferAttachmentDescriptorArray*>(this, _MTL_PRIVATE_SEL(sampleBufferAttachments));
}
