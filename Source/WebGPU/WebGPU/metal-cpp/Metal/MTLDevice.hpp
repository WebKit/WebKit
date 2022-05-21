//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLDevice.hpp
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

#include "MTLArgument.hpp"
#include "MTLDevice.hpp"
#include "MTLPixelFormat.hpp"
#include "MTLResource.hpp"
#include "MTLTexture.hpp"
#include "MTLTypes.hpp"
#include <IOSurface/IOSurfaceRef.h>
#include <functional>

namespace MTL
{
_MTL_ENUM(NS::UInteger, FeatureSet) {
    FeatureSet_iOS_GPUFamily1_v1 = 0,
    FeatureSet_iOS_GPUFamily2_v1 = 1,
    FeatureSet_iOS_GPUFamily1_v2 = 2,
    FeatureSet_iOS_GPUFamily2_v2 = 3,
    FeatureSet_iOS_GPUFamily3_v1 = 4,
    FeatureSet_iOS_GPUFamily1_v3 = 5,
    FeatureSet_iOS_GPUFamily2_v3 = 6,
    FeatureSet_iOS_GPUFamily3_v2 = 7,
    FeatureSet_iOS_GPUFamily1_v4 = 8,
    FeatureSet_iOS_GPUFamily2_v4 = 9,
    FeatureSet_iOS_GPUFamily3_v3 = 10,
    FeatureSet_iOS_GPUFamily4_v1 = 11,
    FeatureSet_iOS_GPUFamily1_v5 = 12,
    FeatureSet_iOS_GPUFamily2_v5 = 13,
    FeatureSet_iOS_GPUFamily3_v4 = 14,
    FeatureSet_iOS_GPUFamily4_v2 = 15,
    FeatureSet_iOS_GPUFamily5_v1 = 16,
    FeatureSet_macOS_GPUFamily1_v1 = 10000,
    FeatureSet_OSX_GPUFamily1_v1 = 10000,
    FeatureSet_macOS_GPUFamily1_v2 = 10001,
    FeatureSet_OSX_GPUFamily1_v2 = 10001,
    FeatureSet_OSX_ReadWriteTextureTier2 = 10002,
    FeatureSet_macOS_ReadWriteTextureTier2 = 10002,
    FeatureSet_macOS_GPUFamily1_v3 = 10003,
    FeatureSet_macOS_GPUFamily1_v4 = 10004,
    FeatureSet_macOS_GPUFamily2_v1 = 10005,
    FeatureSet_watchOS_GPUFamily1_v1 = 20000,
    FeatureSet_WatchOS_GPUFamily1_v1 = 20000,
    FeatureSet_watchOS_GPUFamily2_v1 = 20001,
    FeatureSet_WatchOS_GPUFamily2_v1 = 20001,
    FeatureSet_tvOS_GPUFamily1_v1 = 30000,
    FeatureSet_TVOS_GPUFamily1_v1 = 30000,
    FeatureSet_tvOS_GPUFamily1_v2 = 30001,
    FeatureSet_tvOS_GPUFamily1_v3 = 30002,
    FeatureSet_tvOS_GPUFamily2_v1 = 30003,
    FeatureSet_tvOS_GPUFamily1_v4 = 30004,
    FeatureSet_tvOS_GPUFamily2_v2 = 30005,
};

_MTL_ENUM(NS::Integer, GPUFamily) {
    GPUFamilyApple1 = 1001,
    GPUFamilyApple2 = 1002,
    GPUFamilyApple3 = 1003,
    GPUFamilyApple4 = 1004,
    GPUFamilyApple5 = 1005,
    GPUFamilyApple6 = 1006,
    GPUFamilyApple7 = 1007,
    GPUFamilyApple8 = 1008,
    GPUFamilyMac1 = 2001,
    GPUFamilyMac2 = 2002,
    GPUFamilyCommon1 = 3001,
    GPUFamilyCommon2 = 3002,
    GPUFamilyCommon3 = 3003,
    GPUFamilyMacCatalyst1 = 4001,
    GPUFamilyMacCatalyst2 = 4002,
};

_MTL_ENUM(NS::UInteger, DeviceLocation) {
    DeviceLocationBuiltIn = 0,
    DeviceLocationSlot = 1,
    DeviceLocationExternal = 2,
    DeviceLocationUnspecified = NS::UIntegerMax,
};

_MTL_OPTIONS(NS::UInteger, PipelineOption) {
    PipelineOptionNone = 0,
    PipelineOptionArgumentInfo = 1,
    PipelineOptionBufferTypeInfo = 2,
    PipelineOptionFailOnBinaryArchiveMiss = 4,
};

_MTL_ENUM(NS::UInteger, ReadWriteTextureTier) {
    ReadWriteTextureTierNone = 0,
    ReadWriteTextureTier1 = 1,
    ReadWriteTextureTier2 = 2,
};

_MTL_ENUM(NS::UInteger, ArgumentBuffersTier) {
    ArgumentBuffersTier1 = 0,
    ArgumentBuffersTier2 = 1,
};

_MTL_ENUM(NS::UInteger, SparseTextureRegionAlignmentMode) {
    SparseTextureRegionAlignmentModeOutward = 0,
    SparseTextureRegionAlignmentModeInward = 1,
};

struct AccelerationStructureSizes
{
    NS::UInteger accelerationStructureSize;
    NS::UInteger buildScratchBufferSize;
    NS::UInteger refitScratchBufferSize;
} _MTL_PACKED;

_MTL_ENUM(NS::UInteger, CounterSamplingPoint) {
    CounterSamplingPointAtStageBoundary = 0,
    CounterSamplingPointAtDrawBoundary = 1,
    CounterSamplingPointAtDispatchBoundary = 2,
    CounterSamplingPointAtTileDispatchBoundary = 3,
    CounterSamplingPointAtBlitBoundary = 4,
};

struct SizeAndAlign
{
    NS::UInteger size;
    NS::UInteger align;
} _MTL_PACKED;

class ArgumentDescriptor : public NS::Copying<ArgumentDescriptor>
{
public:
    static class ArgumentDescriptor* alloc();

    class ArgumentDescriptor*        init();

    static class ArgumentDescriptor* argumentDescriptor();

    MTL::DataType                    dataType() const;
    void                             setDataType(MTL::DataType dataType);

    NS::UInteger                     index() const;
    void                             setIndex(NS::UInteger index);

    NS::UInteger                     arrayLength() const;
    void                             setArrayLength(NS::UInteger arrayLength);

    MTL::ArgumentAccess              access() const;
    void                             setAccess(MTL::ArgumentAccess access);

    MTL::TextureType                 textureType() const;
    void                             setTextureType(MTL::TextureType textureType);

    NS::UInteger                     constantBlockAlignment() const;
    void                             setConstantBlockAlignment(NS::UInteger constantBlockAlignment);
};

using DeviceNotificationName = NS::String*;

_MTL_CONST(DeviceNotificationName, DeviceWasAddedNotification);

_MTL_CONST(DeviceNotificationName, DeviceRemovalRequestedNotification);

_MTL_CONST(DeviceNotificationName, DeviceWasRemovedNotification);

using DeviceNotificationHandlerBlock = void (^)(class Device* pDevice, DeviceNotificationName notifyName);

using DeviceNotificationHandlerFunction = std::function<void(class Device* pDevice, DeviceNotificationName notifyName)>;

using AutoreleasedComputePipelineReflection = class ComputePipelineReflection*;

using AutoreleasedRenderPipelineReflection = class RenderPipelineReflection*;

using NewLibraryCompletionHandler = void (^)(class Library*, NS::Error*);

using NewLibraryCompletionHandlerFunction = std::function<void(class Library*, NS::Error*)>;

using NewRenderPipelineStateCompletionHandler = void (^)(class RenderPipelineState*, NS::Error*);

using NewRenderPipelineStateCompletionHandlerFunction = std::function<void(class RenderPipelineState*, NS::Error*)>;

using NewRenderPipelineStateWithReflectionCompletionHandler = void (^)(class RenderPipelineState*, class RenderPipelineReflection*, NS::Error*);

using NewRenderPipelineStateWithReflectionCompletionHandlerFunction = std::function<void(class RenderPipelineState*, class RenderPipelineReflection*, NS::Error*)>;

using NewComputePipelineStateCompletionHandler = void (^)(class ComputePipelineState*, NS::Error*);

using NewComputePipelineStateCompletionHandlerFunction = std::function<void(class ComputePipelineState*, NS::Error*)>;

using NewComputePipelineStateWithReflectionCompletionHandler = void (^)(class ComputePipelineState*, class ComputePipelineReflection*, NS::Error*);

using NewComputePipelineStateWithReflectionCompletionHandlerFunction = std::function<void(class ComputePipelineState*, class ComputePipelineReflection*, NS::Error*)>;

using Timestamp = std::uint64_t;

MTL::Device* CreateSystemDefaultDevice();

NS::Array*   CopyAllDevices();

NS::Array*   CopyAllDevicesWithObserver(NS::Object** pOutObserver, DeviceNotificationHandlerBlock handler);

NS::Array*   CopyAllDevicesWithObserver(NS::Object** pOutObserver, const DeviceNotificationHandlerFunction& handler);

void         RemoveDeviceObserver(const NS::Object* pObserver);

class Device : public NS::Referencing<Device>
{
public:
    void                            newLibrary(const NS::String* pSource, const class CompileOptions* pOptions, const NewLibraryCompletionHandlerFunction& completionHandler);

    void                            newLibrary(const class StitchedLibraryDescriptor* pDescriptor, const MTL::NewLibraryCompletionHandlerFunction& completionHandler);

    void                            newRenderPipelineState(const class RenderPipelineDescriptor* pDescriptor, const NewRenderPipelineStateCompletionHandlerFunction& completionHandler);

    void                            newRenderPipelineState(const class RenderPipelineDescriptor* pDescriptor, PipelineOption options, const NewRenderPipelineStateWithReflectionCompletionHandlerFunction& completionHandler);

    void                            newRenderPipelineState(const class TileRenderPipelineDescriptor* pDescriptor, PipelineOption options, const NewRenderPipelineStateWithReflectionCompletionHandlerFunction& completionHandler);

    void                            newComputePipelineState(const class Function* pFunction, const NewComputePipelineStateCompletionHandlerFunction& completionHandler);

    void                            newComputePipelineState(const class Function* pFunction, PipelineOption options, const NewComputePipelineStateWithReflectionCompletionHandlerFunction& completionHandler);

    void                            newComputePipelineState(const class ComputePipelineDescriptor* pDescriptor, PipelineOption options, const NewComputePipelineStateWithReflectionCompletionHandlerFunction& completionHandler);

    bool                            isHeadless() const;

    NS::String*                     name() const;

    uint64_t                        registryID() const;

    MTL::Size                       maxThreadsPerThreadgroup() const;

    bool                            lowPower() const;

    bool                            headless() const;

    bool                            removable() const;

    bool                            hasUnifiedMemory() const;

    uint64_t                        recommendedMaxWorkingSetSize() const;

    MTL::DeviceLocation             location() const;

    NS::UInteger                    locationNumber() const;

    uint64_t                        maxTransferRate() const;

    bool                            depth24Stencil8PixelFormatSupported() const;

    MTL::ReadWriteTextureTier       readWriteTextureSupport() const;

    MTL::ArgumentBuffersTier        argumentBuffersSupport() const;

    bool                            rasterOrderGroupsSupported() const;

    bool                            supports32BitFloatFiltering() const;

    bool                            supports32BitMSAA() const;

    bool                            supportsQueryTextureLOD() const;

    bool                            supportsBCTextureCompression() const;

    bool                            supportsPullModelInterpolation() const;

    bool                            barycentricCoordsSupported() const;

    bool                            supportsShaderBarycentricCoordinates() const;

    NS::UInteger                    currentAllocatedSize() const;

    class CommandQueue*             newCommandQueue();

    class CommandQueue*             newCommandQueue(NS::UInteger maxCommandBufferCount);

    MTL::SizeAndAlign               heapTextureSizeAndAlign(const class TextureDescriptor* desc);

    MTL::SizeAndAlign               heapBufferSizeAndAlign(NS::UInteger length, MTL::ResourceOptions options);

    class Heap*                     newHeap(const class HeapDescriptor* descriptor);

    class Buffer*                   newBuffer(NS::UInteger length, MTL::ResourceOptions options);

    class Buffer*                   newBuffer(const void* pointer, NS::UInteger length, MTL::ResourceOptions options);

    class Buffer*                   newBuffer(const void* pointer, NS::UInteger length, MTL::ResourceOptions options, const void (^deallocator)(void*, NS::UInteger));

    class DepthStencilState*        newDepthStencilState(const class DepthStencilDescriptor* descriptor);

    class Texture*                  newTexture(const class TextureDescriptor* descriptor);

    class Texture*                  newTexture(const class TextureDescriptor* descriptor, const IOSurfaceRef iosurface, NS::UInteger plane);

    class Texture*                  newSharedTexture(const class TextureDescriptor* descriptor);

    class Texture*                  newSharedTexture(const class SharedTextureHandle* sharedHandle);

    class SamplerState*             newSamplerState(const class SamplerDescriptor* descriptor);

    class Library*                  newDefaultLibrary();

    class Library*                  newDefaultLibrary(const NS::Bundle* bundle, NS::Error** error);

    class Library*                  newLibrary(const NS::String* filepath, NS::Error** error);

    class Library*                  newLibrary(const NS::URL* url, NS::Error** error);

    class Library*                  newLibrary(const dispatch_data_t data, NS::Error** error);

    class Library*                  newLibrary(const NS::String* source, const class CompileOptions* options, NS::Error** error);

    void                            newLibrary(const NS::String* source, const class CompileOptions* options, const MTL::NewLibraryCompletionHandler completionHandler);

    class Library*                  newLibrary(const class StitchedLibraryDescriptor* descriptor, NS::Error** error);

    void                            newLibrary(const class StitchedLibraryDescriptor* descriptor, const MTL::NewLibraryCompletionHandler completionHandler);

    class RenderPipelineState*      newRenderPipelineState(const class RenderPipelineDescriptor* descriptor, NS::Error** error);

    class RenderPipelineState*      newRenderPipelineState(const class RenderPipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::AutoreleasedRenderPipelineReflection* reflection, NS::Error** error);

    void                            newRenderPipelineState(const class RenderPipelineDescriptor* descriptor, const MTL::NewRenderPipelineStateCompletionHandler completionHandler);

    void                            newRenderPipelineState(const class RenderPipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::NewRenderPipelineStateWithReflectionCompletionHandler completionHandler);

    class ComputePipelineState*     newComputePipelineState(const class Function* computeFunction, NS::Error** error);

    class ComputePipelineState*     newComputePipelineState(const class Function* computeFunction, MTL::PipelineOption options, const MTL::AutoreleasedComputePipelineReflection* reflection, NS::Error** error);

    void                            newComputePipelineState(const class Function* computeFunction, const MTL::NewComputePipelineStateCompletionHandler completionHandler);

    void                            newComputePipelineState(const class Function* computeFunction, MTL::PipelineOption options, const MTL::NewComputePipelineStateWithReflectionCompletionHandler completionHandler);

    class ComputePipelineState*     newComputePipelineState(const class ComputePipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::AutoreleasedComputePipelineReflection* reflection, NS::Error** error);

    void                            newComputePipelineState(const class ComputePipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::NewComputePipelineStateWithReflectionCompletionHandler completionHandler);

    class Fence*                    newFence();

    bool                            supportsFeatureSet(MTL::FeatureSet featureSet);

    bool                            supportsFamily(MTL::GPUFamily gpuFamily);

    bool                            supportsTextureSampleCount(NS::UInteger sampleCount);

    NS::UInteger                    minimumLinearTextureAlignmentForPixelFormat(MTL::PixelFormat format);

    NS::UInteger                    minimumTextureBufferAlignmentForPixelFormat(MTL::PixelFormat format);

    class RenderPipelineState*      newRenderPipelineState(const class TileRenderPipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::AutoreleasedRenderPipelineReflection* reflection, NS::Error** error);

    void                            newRenderPipelineState(const class TileRenderPipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::NewRenderPipelineStateWithReflectionCompletionHandler completionHandler);

    NS::UInteger                    maxThreadgroupMemoryLength() const;

    NS::UInteger                    maxArgumentBufferSamplerCount() const;

    bool                            programmableSamplePositionsSupported() const;

    void                            getDefaultSamplePositions(MTL::SamplePosition* positions, NS::UInteger count);

    class ArgumentEncoder*          newArgumentEncoder(const NS::Array* arguments);

    bool                            supportsRasterizationRateMap(NS::UInteger layerCount);

    class RasterizationRateMap*     newRasterizationRateMap(const class RasterizationRateMapDescriptor* descriptor);

    class IndirectCommandBuffer*    newIndirectCommandBuffer(const class IndirectCommandBufferDescriptor* descriptor, NS::UInteger maxCount, MTL::ResourceOptions options);

    class Event*                    newEvent();

    class SharedEvent*              newSharedEvent();

    class SharedEvent*              newSharedEvent(const class SharedEventHandle* sharedEventHandle);

    uint64_t                        peerGroupID() const;

    uint32_t                        peerIndex() const;

    uint32_t                        peerCount() const;

    MTL::Size                       sparseTileSize(MTL::TextureType textureType, MTL::PixelFormat pixelFormat, NS::UInteger sampleCount);

    NS::UInteger                    sparseTileSizeInBytes() const;

    void                            convertSparsePixelRegions(const MTL::Region* pixelRegions, MTL::Region* tileRegions, MTL::Size tileSize, MTL::SparseTextureRegionAlignmentMode mode, NS::UInteger numRegions);

    void                            convertSparseTileRegions(const MTL::Region* tileRegions, MTL::Region* pixelRegions, MTL::Size tileSize, NS::UInteger numRegions);

    NS::UInteger                    maxBufferLength() const;

    NS::Array*                      counterSets() const;

    class CounterSampleBuffer*      newCounterSampleBuffer(const class CounterSampleBufferDescriptor* descriptor, NS::Error** error);

    void                            sampleTimestamps(MTL::Timestamp* cpuTimestamp, MTL::Timestamp* gpuTimestamp);

    bool                            supportsCounterSampling(MTL::CounterSamplingPoint samplingPoint);

    bool                            supportsVertexAmplificationCount(NS::UInteger count);

    bool                            supportsDynamicLibraries() const;

    bool                            supportsRenderDynamicLibraries() const;

    class DynamicLibrary*           newDynamicLibrary(const class Library* library, NS::Error** error);

    class DynamicLibrary*           newDynamicLibrary(const NS::URL* url, NS::Error** error);

    class BinaryArchive*            newBinaryArchive(const class BinaryArchiveDescriptor* descriptor, NS::Error** error);

    bool                            supportsRaytracing() const;

    MTL::AccelerationStructureSizes accelerationStructureSizes(const class AccelerationStructureDescriptor* descriptor);

    class AccelerationStructure*    newAccelerationStructure(NS::UInteger size);

    class AccelerationStructure*    newAccelerationStructure(const class AccelerationStructureDescriptor* descriptor);

    bool                            supportsFunctionPointers() const;

    bool                            supportsFunctionPointersFromRender() const;

    bool                            supportsRaytracingFromRender() const;

    bool                            supportsPrimitiveMotionBlur() const;
};

}

// static method: alloc
_MTL_INLINE MTL::ArgumentDescriptor* MTL::ArgumentDescriptor::alloc()
{
    return NS::Object::alloc<MTL::ArgumentDescriptor>(_MTL_PRIVATE_CLS(MTLArgumentDescriptor));
}

// method: init
_MTL_INLINE MTL::ArgumentDescriptor* MTL::ArgumentDescriptor::init()
{
    return NS::Object::init<MTL::ArgumentDescriptor>();
}

// static method: argumentDescriptor
_MTL_INLINE MTL::ArgumentDescriptor* MTL::ArgumentDescriptor::argumentDescriptor()
{
    return Object::sendMessage<MTL::ArgumentDescriptor*>(_MTL_PRIVATE_CLS(MTLArgumentDescriptor), _MTL_PRIVATE_SEL(argumentDescriptor));
}

// property: dataType
_MTL_INLINE MTL::DataType MTL::ArgumentDescriptor::dataType() const
{
    return Object::sendMessage<MTL::DataType>(this, _MTL_PRIVATE_SEL(dataType));
}

_MTL_INLINE void MTL::ArgumentDescriptor::setDataType(MTL::DataType dataType)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDataType_), dataType);
}

// property: index
_MTL_INLINE NS::UInteger MTL::ArgumentDescriptor::index() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(index));
}

_MTL_INLINE void MTL::ArgumentDescriptor::setIndex(NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setIndex_), index);
}

// property: arrayLength
_MTL_INLINE NS::UInteger MTL::ArgumentDescriptor::arrayLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(arrayLength));
}

_MTL_INLINE void MTL::ArgumentDescriptor::setArrayLength(NS::UInteger arrayLength)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setArrayLength_), arrayLength);
}

// property: access
_MTL_INLINE MTL::ArgumentAccess MTL::ArgumentDescriptor::access() const
{
    return Object::sendMessage<MTL::ArgumentAccess>(this, _MTL_PRIVATE_SEL(access));
}

_MTL_INLINE void MTL::ArgumentDescriptor::setAccess(MTL::ArgumentAccess access)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setAccess_), access);
}

// property: textureType
_MTL_INLINE MTL::TextureType MTL::ArgumentDescriptor::textureType() const
{
    return Object::sendMessage<MTL::TextureType>(this, _MTL_PRIVATE_SEL(textureType));
}

_MTL_INLINE void MTL::ArgumentDescriptor::setTextureType(MTL::TextureType textureType)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTextureType_), textureType);
}

// property: constantBlockAlignment
_MTL_INLINE NS::UInteger MTL::ArgumentDescriptor::constantBlockAlignment() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(constantBlockAlignment));
}

_MTL_INLINE void MTL::ArgumentDescriptor::setConstantBlockAlignment(NS::UInteger constantBlockAlignment)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setConstantBlockAlignment_), constantBlockAlignment);
}

#if defined(MTL_PRIVATE_IMPLEMENTATION)

extern "C" MTL::Device* MTLCreateSystemDefaultDevice();

extern "C" NS::Array*   MTLCopyAllDevices();

extern "C" NS::Array*   MTLCopyAllDevicesWithObserver(NS::Object**, MTL::DeviceNotificationHandlerBlock);

extern "C" void         MTLRemoveDeviceObserver(const NS::Object*);

#include <TargetConditionals.h>

MTL::Device* MTL::CreateSystemDefaultDevice()
{
    return ::MTLCreateSystemDefaultDevice();
}

NS::Array* MTL::CopyAllDevices()
{
#if TARGET_OS_OSX
    return ::MTLCopyAllDevices();
#else
    return nullptr;
#endif // TARGET_OS_OSX
}

NS::Array* MTL::CopyAllDevicesWithObserver(NS::Object** pOutObserver, DeviceNotificationHandlerBlock handler)
{
#if TARGET_OS_OSX
    return ::MTLCopyAllDevicesWithObserver(pOutObserver, handler);
#else
    (void)pOutObserver;
    (void)handler;

    return nullptr;
#endif // TARGET_OS_OSX
}

NS::Array* MTL::CopyAllDevicesWithObserver(NS::Object** pOutObserver, const DeviceNotificationHandlerFunction& handler)
{
    __block DeviceNotificationHandlerFunction function = handler;

    return CopyAllDevicesWithObserver(pOutObserver, ^(Device* pDevice, DeviceNotificationName pNotificationName) { function(pDevice, pNotificationName); });
}

void MTL::RemoveDeviceObserver(const NS::Object* pObserver)
{
#if TARGET_OS_OSX
    ::MTLRemoveDeviceObserver(pObserver);
#endif // TARGET_OS_OSX
}

#endif // MTL_PRIVATE_IMPLEMENTATION

_MTL_INLINE void MTL::Device::newLibrary(const NS::String* pSource, const CompileOptions* pOptions, const NewLibraryCompletionHandlerFunction& completionHandler)
{
    __block NewLibraryCompletionHandlerFunction blockCompletionHandler = completionHandler;

    newLibrary(pSource, pOptions, ^(Library* pLibrary, NS::Error* pError) { blockCompletionHandler(pLibrary, pError); });
}

_MTL_INLINE void MTL::Device::newLibrary(const class StitchedLibraryDescriptor* pDescriptor, const MTL::NewLibraryCompletionHandlerFunction& completionHandler)
{
    __block NewLibraryCompletionHandlerFunction blockCompletionHandler = completionHandler;

    newLibrary(pDescriptor, ^(Library* pLibrary, NS::Error* pError) { blockCompletionHandler(pLibrary, pError); });
}

_MTL_INLINE void MTL::Device::newRenderPipelineState(const RenderPipelineDescriptor* pDescriptor, const NewRenderPipelineStateCompletionHandlerFunction& completionHandler)
{
    __block NewRenderPipelineStateCompletionHandlerFunction blockCompletionHandler = completionHandler;

    newRenderPipelineState(pDescriptor, ^(RenderPipelineState* pPipelineState, NS::Error* pError) { blockCompletionHandler(pPipelineState, pError); });
}

_MTL_INLINE void MTL::Device::newRenderPipelineState(const RenderPipelineDescriptor* pDescriptor, PipelineOption options, const NewRenderPipelineStateWithReflectionCompletionHandlerFunction& completionHandler)
{
    __block NewRenderPipelineStateWithReflectionCompletionHandlerFunction blockCompletionHandler = completionHandler;

    newRenderPipelineState(pDescriptor, options, ^(RenderPipelineState* pPipelineState, class RenderPipelineReflection* pReflection, NS::Error* pError) { blockCompletionHandler(pPipelineState, pReflection, pError); });
}

_MTL_INLINE void MTL::Device::newRenderPipelineState(const TileRenderPipelineDescriptor* pDescriptor, PipelineOption options, const NewRenderPipelineStateWithReflectionCompletionHandlerFunction& completionHandler)
{
    __block NewRenderPipelineStateWithReflectionCompletionHandlerFunction blockCompletionHandler = completionHandler;

    newRenderPipelineState(pDescriptor, options, ^(RenderPipelineState* pPipelineState, class RenderPipelineReflection* pReflection, NS::Error* pError) { blockCompletionHandler(pPipelineState, pReflection, pError); });
}

_MTL_INLINE void MTL::Device::newComputePipelineState(const class Function* pFunction, const NewComputePipelineStateCompletionHandlerFunction& completionHandler)
{
    __block NewComputePipelineStateCompletionHandlerFunction blockCompletionHandler = completionHandler;

    newComputePipelineState(pFunction, ^(ComputePipelineState* pPipelineState, NS::Error* pError) { blockCompletionHandler(pPipelineState, pError); });
}

_MTL_INLINE void MTL::Device::newComputePipelineState(const Function* pFunction, PipelineOption options, const NewComputePipelineStateWithReflectionCompletionHandlerFunction& completionHandler)
{
    __block NewComputePipelineStateWithReflectionCompletionHandlerFunction blockCompletionHandler = completionHandler;

    newComputePipelineState(pFunction, options, ^(ComputePipelineState* pPipelineState, ComputePipelineReflection* pReflection, NS::Error* pError) { blockCompletionHandler(pPipelineState, pReflection, pError); });
}

_MTL_INLINE void MTL::Device::newComputePipelineState(const ComputePipelineDescriptor* pDescriptor, PipelineOption options, const NewComputePipelineStateWithReflectionCompletionHandlerFunction& completionHandler)
{
    __block NewComputePipelineStateWithReflectionCompletionHandlerFunction blockCompletionHandler = completionHandler;

    newComputePipelineState(pDescriptor, options, ^(ComputePipelineState* pPipelineState, ComputePipelineReflection* pReflection, NS::Error* pError) { blockCompletionHandler(pPipelineState, pReflection, pError); });
}

_MTL_INLINE bool MTL::Device::isHeadless() const
{
    return headless();
}

// property: name
_MTL_INLINE NS::String* MTL::Device::name() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(name));
}

// property: registryID
_MTL_INLINE uint64_t MTL::Device::registryID() const
{
    return Object::sendMessage<uint64_t>(this, _MTL_PRIVATE_SEL(registryID));
}

// property: maxThreadsPerThreadgroup
_MTL_INLINE MTL::Size MTL::Device::maxThreadsPerThreadgroup() const
{
    return Object::sendMessage<MTL::Size>(this, _MTL_PRIVATE_SEL(maxThreadsPerThreadgroup));
}

// property: lowPower
_MTL_INLINE bool MTL::Device::lowPower() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isLowPower));
}

// property: headless
_MTL_INLINE bool MTL::Device::headless() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isHeadless));
}

// property: removable
_MTL_INLINE bool MTL::Device::removable() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isRemovable));
}

// property: hasUnifiedMemory
_MTL_INLINE bool MTL::Device::hasUnifiedMemory() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(hasUnifiedMemory));
}

// property: recommendedMaxWorkingSetSize
_MTL_INLINE uint64_t MTL::Device::recommendedMaxWorkingSetSize() const
{
    return Object::sendMessage<uint64_t>(this, _MTL_PRIVATE_SEL(recommendedMaxWorkingSetSize));
}

// property: location
_MTL_INLINE MTL::DeviceLocation MTL::Device::location() const
{
    return Object::sendMessage<MTL::DeviceLocation>(this, _MTL_PRIVATE_SEL(location));
}

// property: locationNumber
_MTL_INLINE NS::UInteger MTL::Device::locationNumber() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(locationNumber));
}

// property: maxTransferRate
_MTL_INLINE uint64_t MTL::Device::maxTransferRate() const
{
    return Object::sendMessage<uint64_t>(this, _MTL_PRIVATE_SEL(maxTransferRate));
}

// property: depth24Stencil8PixelFormatSupported
_MTL_INLINE bool MTL::Device::depth24Stencil8PixelFormatSupported() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(isDepth24Stencil8PixelFormatSupported));
}

// property: readWriteTextureSupport
_MTL_INLINE MTL::ReadWriteTextureTier MTL::Device::readWriteTextureSupport() const
{
    return Object::sendMessage<MTL::ReadWriteTextureTier>(this, _MTL_PRIVATE_SEL(readWriteTextureSupport));
}

// property: argumentBuffersSupport
_MTL_INLINE MTL::ArgumentBuffersTier MTL::Device::argumentBuffersSupport() const
{
    return Object::sendMessage<MTL::ArgumentBuffersTier>(this, _MTL_PRIVATE_SEL(argumentBuffersSupport));
}

// property: rasterOrderGroupsSupported
_MTL_INLINE bool MTL::Device::rasterOrderGroupsSupported() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(areRasterOrderGroupsSupported));
}

// property: supports32BitFloatFiltering
_MTL_INLINE bool MTL::Device::supports32BitFloatFiltering() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supports32BitFloatFiltering));
}

// property: supports32BitMSAA
_MTL_INLINE bool MTL::Device::supports32BitMSAA() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supports32BitMSAA));
}

// property: supportsQueryTextureLOD
_MTL_INLINE bool MTL::Device::supportsQueryTextureLOD() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsQueryTextureLOD));
}

// property: supportsBCTextureCompression
_MTL_INLINE bool MTL::Device::supportsBCTextureCompression() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsBCTextureCompression));
}

// property: supportsPullModelInterpolation
_MTL_INLINE bool MTL::Device::supportsPullModelInterpolation() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsPullModelInterpolation));
}

// property: barycentricCoordsSupported
_MTL_INLINE bool MTL::Device::barycentricCoordsSupported() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(areBarycentricCoordsSupported));
}

// property: supportsShaderBarycentricCoordinates
_MTL_INLINE bool MTL::Device::supportsShaderBarycentricCoordinates() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsShaderBarycentricCoordinates));
}

// property: currentAllocatedSize
_MTL_INLINE NS::UInteger MTL::Device::currentAllocatedSize() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(currentAllocatedSize));
}

// method: newCommandQueue
_MTL_INLINE MTL::CommandQueue* MTL::Device::newCommandQueue()
{
    return Object::sendMessage<MTL::CommandQueue*>(this, _MTL_PRIVATE_SEL(newCommandQueue));
}

// method: newCommandQueueWithMaxCommandBufferCount:
_MTL_INLINE MTL::CommandQueue* MTL::Device::newCommandQueue(NS::UInteger maxCommandBufferCount)
{
    return Object::sendMessage<MTL::CommandQueue*>(this, _MTL_PRIVATE_SEL(newCommandQueueWithMaxCommandBufferCount_), maxCommandBufferCount);
}

// method: heapTextureSizeAndAlignWithDescriptor:
_MTL_INLINE MTL::SizeAndAlign MTL::Device::heapTextureSizeAndAlign(const MTL::TextureDescriptor* desc)
{
    return Object::sendMessage<MTL::SizeAndAlign>(this, _MTL_PRIVATE_SEL(heapTextureSizeAndAlignWithDescriptor_), desc);
}

// method: heapBufferSizeAndAlignWithLength:options:
_MTL_INLINE MTL::SizeAndAlign MTL::Device::heapBufferSizeAndAlign(NS::UInteger length, MTL::ResourceOptions options)
{
    return Object::sendMessage<MTL::SizeAndAlign>(this, _MTL_PRIVATE_SEL(heapBufferSizeAndAlignWithLength_options_), length, options);
}

// method: newHeapWithDescriptor:
_MTL_INLINE MTL::Heap* MTL::Device::newHeap(const MTL::HeapDescriptor* descriptor)
{
    return Object::sendMessage<MTL::Heap*>(this, _MTL_PRIVATE_SEL(newHeapWithDescriptor_), descriptor);
}

// method: newBufferWithLength:options:
_MTL_INLINE MTL::Buffer* MTL::Device::newBuffer(NS::UInteger length, MTL::ResourceOptions options)
{
    return Object::sendMessage<MTL::Buffer*>(this, _MTL_PRIVATE_SEL(newBufferWithLength_options_), length, options);
}

// method: newBufferWithBytes:length:options:
_MTL_INLINE MTL::Buffer* MTL::Device::newBuffer(const void* pointer, NS::UInteger length, MTL::ResourceOptions options)
{
    return Object::sendMessage<MTL::Buffer*>(this, _MTL_PRIVATE_SEL(newBufferWithBytes_length_options_), pointer, length, options);
}

// method: newBufferWithBytesNoCopy:length:options:deallocator:
_MTL_INLINE MTL::Buffer* MTL::Device::newBuffer(const void* pointer, NS::UInteger length, MTL::ResourceOptions options, const void (^deallocator)(void*, NS::UInteger))
{
    return Object::sendMessage<MTL::Buffer*>(this, _MTL_PRIVATE_SEL(newBufferWithBytesNoCopy_length_options_deallocator_), pointer, length, options, deallocator);
}

// method: newDepthStencilStateWithDescriptor:
_MTL_INLINE MTL::DepthStencilState* MTL::Device::newDepthStencilState(const MTL::DepthStencilDescriptor* descriptor)
{
    return Object::sendMessage<MTL::DepthStencilState*>(this, _MTL_PRIVATE_SEL(newDepthStencilStateWithDescriptor_), descriptor);
}

// method: newTextureWithDescriptor:
_MTL_INLINE MTL::Texture* MTL::Device::newTexture(const MTL::TextureDescriptor* descriptor)
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(newTextureWithDescriptor_), descriptor);
}

// method: newTextureWithDescriptor:iosurface:plane:
_MTL_INLINE MTL::Texture* MTL::Device::newTexture(const MTL::TextureDescriptor* descriptor, const IOSurfaceRef iosurface, NS::UInteger plane)
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(newTextureWithDescriptor_iosurface_plane_), descriptor, iosurface, plane);
}

// method: newSharedTextureWithDescriptor:
_MTL_INLINE MTL::Texture* MTL::Device::newSharedTexture(const MTL::TextureDescriptor* descriptor)
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(newSharedTextureWithDescriptor_), descriptor);
}

// method: newSharedTextureWithHandle:
_MTL_INLINE MTL::Texture* MTL::Device::newSharedTexture(const MTL::SharedTextureHandle* sharedHandle)
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(newSharedTextureWithHandle_), sharedHandle);
}

// method: newSamplerStateWithDescriptor:
_MTL_INLINE MTL::SamplerState* MTL::Device::newSamplerState(const MTL::SamplerDescriptor* descriptor)
{
    return Object::sendMessage<MTL::SamplerState*>(this, _MTL_PRIVATE_SEL(newSamplerStateWithDescriptor_), descriptor);
}

// method: newDefaultLibrary
_MTL_INLINE MTL::Library* MTL::Device::newDefaultLibrary()
{
    return Object::sendMessage<MTL::Library*>(this, _MTL_PRIVATE_SEL(newDefaultLibrary));
}

// method: newDefaultLibraryWithBundle:error:
_MTL_INLINE MTL::Library* MTL::Device::newDefaultLibrary(const NS::Bundle* bundle, NS::Error** error)
{
    return Object::sendMessage<MTL::Library*>(this, _MTL_PRIVATE_SEL(newDefaultLibraryWithBundle_error_), bundle, error);
}

// method: newLibraryWithFile:error:
_MTL_INLINE MTL::Library* MTL::Device::newLibrary(const NS::String* filepath, NS::Error** error)
{
    return Object::sendMessage<MTL::Library*>(this, _MTL_PRIVATE_SEL(newLibraryWithFile_error_), filepath, error);
}

// method: newLibraryWithURL:error:
_MTL_INLINE MTL::Library* MTL::Device::newLibrary(const NS::URL* url, NS::Error** error)
{
    return Object::sendMessage<MTL::Library*>(this, _MTL_PRIVATE_SEL(newLibraryWithURL_error_), url, error);
}

// method: newLibraryWithData:error:
_MTL_INLINE MTL::Library* MTL::Device::newLibrary(const dispatch_data_t data, NS::Error** error)
{
    return Object::sendMessage<MTL::Library*>(this, _MTL_PRIVATE_SEL(newLibraryWithData_error_), data, error);
}

// method: newLibraryWithSource:options:error:
_MTL_INLINE MTL::Library* MTL::Device::newLibrary(const NS::String* source, const MTL::CompileOptions* options, NS::Error** error)
{
    return Object::sendMessage<MTL::Library*>(this, _MTL_PRIVATE_SEL(newLibraryWithSource_options_error_), source, options, error);
}

// method: newLibraryWithSource:options:completionHandler:
_MTL_INLINE void MTL::Device::newLibrary(const NS::String* source, const MTL::CompileOptions* options, const MTL::NewLibraryCompletionHandler completionHandler)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(newLibraryWithSource_options_completionHandler_), source, options, completionHandler);
}

// method: newLibraryWithStitchedDescriptor:error:
_MTL_INLINE MTL::Library* MTL::Device::newLibrary(const MTL::StitchedLibraryDescriptor* descriptor, NS::Error** error)
{
    return Object::sendMessage<MTL::Library*>(this, _MTL_PRIVATE_SEL(newLibraryWithStitchedDescriptor_error_), descriptor, error);
}

// method: newLibraryWithStitchedDescriptor:completionHandler:
_MTL_INLINE void MTL::Device::newLibrary(const MTL::StitchedLibraryDescriptor* descriptor, const MTL::NewLibraryCompletionHandler completionHandler)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(newLibraryWithStitchedDescriptor_completionHandler_), descriptor, completionHandler);
}

// method: newRenderPipelineStateWithDescriptor:error:
_MTL_INLINE MTL::RenderPipelineState* MTL::Device::newRenderPipelineState(const MTL::RenderPipelineDescriptor* descriptor, NS::Error** error)
{
    return Object::sendMessage<MTL::RenderPipelineState*>(this, _MTL_PRIVATE_SEL(newRenderPipelineStateWithDescriptor_error_), descriptor, error);
}

// method: newRenderPipelineStateWithDescriptor:options:reflection:error:
_MTL_INLINE MTL::RenderPipelineState* MTL::Device::newRenderPipelineState(const MTL::RenderPipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::AutoreleasedRenderPipelineReflection* reflection, NS::Error** error)
{
    return Object::sendMessage<MTL::RenderPipelineState*>(this, _MTL_PRIVATE_SEL(newRenderPipelineStateWithDescriptor_options_reflection_error_), descriptor, options, reflection, error);
}

// method: newRenderPipelineStateWithDescriptor:completionHandler:
_MTL_INLINE void MTL::Device::newRenderPipelineState(const MTL::RenderPipelineDescriptor* descriptor, const MTL::NewRenderPipelineStateCompletionHandler completionHandler)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(newRenderPipelineStateWithDescriptor_completionHandler_), descriptor, completionHandler);
}

// method: newRenderPipelineStateWithDescriptor:options:completionHandler:
_MTL_INLINE void MTL::Device::newRenderPipelineState(const MTL::RenderPipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::NewRenderPipelineStateWithReflectionCompletionHandler completionHandler)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(newRenderPipelineStateWithDescriptor_options_completionHandler_), descriptor, options, completionHandler);
}

// method: newComputePipelineStateWithFunction:error:
_MTL_INLINE MTL::ComputePipelineState* MTL::Device::newComputePipelineState(const MTL::Function* computeFunction, NS::Error** error)
{
    return Object::sendMessage<MTL::ComputePipelineState*>(this, _MTL_PRIVATE_SEL(newComputePipelineStateWithFunction_error_), computeFunction, error);
}

// method: newComputePipelineStateWithFunction:options:reflection:error:
_MTL_INLINE MTL::ComputePipelineState* MTL::Device::newComputePipelineState(const MTL::Function* computeFunction, MTL::PipelineOption options, const MTL::AutoreleasedComputePipelineReflection* reflection, NS::Error** error)
{
    return Object::sendMessage<MTL::ComputePipelineState*>(this, _MTL_PRIVATE_SEL(newComputePipelineStateWithFunction_options_reflection_error_), computeFunction, options, reflection, error);
}

// method: newComputePipelineStateWithFunction:completionHandler:
_MTL_INLINE void MTL::Device::newComputePipelineState(const MTL::Function* computeFunction, const MTL::NewComputePipelineStateCompletionHandler completionHandler)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(newComputePipelineStateWithFunction_completionHandler_), computeFunction, completionHandler);
}

// method: newComputePipelineStateWithFunction:options:completionHandler:
_MTL_INLINE void MTL::Device::newComputePipelineState(const MTL::Function* computeFunction, MTL::PipelineOption options, const MTL::NewComputePipelineStateWithReflectionCompletionHandler completionHandler)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(newComputePipelineStateWithFunction_options_completionHandler_), computeFunction, options, completionHandler);
}

// method: newComputePipelineStateWithDescriptor:options:reflection:error:
_MTL_INLINE MTL::ComputePipelineState* MTL::Device::newComputePipelineState(const MTL::ComputePipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::AutoreleasedComputePipelineReflection* reflection, NS::Error** error)
{
    return Object::sendMessage<MTL::ComputePipelineState*>(this, _MTL_PRIVATE_SEL(newComputePipelineStateWithDescriptor_options_reflection_error_), descriptor, options, reflection, error);
}

// method: newComputePipelineStateWithDescriptor:options:completionHandler:
_MTL_INLINE void MTL::Device::newComputePipelineState(const MTL::ComputePipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::NewComputePipelineStateWithReflectionCompletionHandler completionHandler)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(newComputePipelineStateWithDescriptor_options_completionHandler_), descriptor, options, completionHandler);
}

// method: newFence
_MTL_INLINE MTL::Fence* MTL::Device::newFence()
{
    return Object::sendMessage<MTL::Fence*>(this, _MTL_PRIVATE_SEL(newFence));
}

// method: supportsFeatureSet:
_MTL_INLINE bool MTL::Device::supportsFeatureSet(MTL::FeatureSet featureSet)
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsFeatureSet_), featureSet);
}

// method: supportsFamily:
_MTL_INLINE bool MTL::Device::supportsFamily(MTL::GPUFamily gpuFamily)
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsFamily_), gpuFamily);
}

// method: supportsTextureSampleCount:
_MTL_INLINE bool MTL::Device::supportsTextureSampleCount(NS::UInteger sampleCount)
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsTextureSampleCount_), sampleCount);
}

// method: minimumLinearTextureAlignmentForPixelFormat:
_MTL_INLINE NS::UInteger MTL::Device::minimumLinearTextureAlignmentForPixelFormat(MTL::PixelFormat format)
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(minimumLinearTextureAlignmentForPixelFormat_), format);
}

// method: minimumTextureBufferAlignmentForPixelFormat:
_MTL_INLINE NS::UInteger MTL::Device::minimumTextureBufferAlignmentForPixelFormat(MTL::PixelFormat format)
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(minimumTextureBufferAlignmentForPixelFormat_), format);
}

// method: newRenderPipelineStateWithTileDescriptor:options:reflection:error:
_MTL_INLINE MTL::RenderPipelineState* MTL::Device::newRenderPipelineState(const MTL::TileRenderPipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::AutoreleasedRenderPipelineReflection* reflection, NS::Error** error)
{
    return Object::sendMessage<MTL::RenderPipelineState*>(this, _MTL_PRIVATE_SEL(newRenderPipelineStateWithTileDescriptor_options_reflection_error_), descriptor, options, reflection, error);
}

// method: newRenderPipelineStateWithTileDescriptor:options:completionHandler:
_MTL_INLINE void MTL::Device::newRenderPipelineState(const MTL::TileRenderPipelineDescriptor* descriptor, MTL::PipelineOption options, const MTL::NewRenderPipelineStateWithReflectionCompletionHandler completionHandler)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(newRenderPipelineStateWithTileDescriptor_options_completionHandler_), descriptor, options, completionHandler);
}

// property: maxThreadgroupMemoryLength
_MTL_INLINE NS::UInteger MTL::Device::maxThreadgroupMemoryLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxThreadgroupMemoryLength));
}

// property: maxArgumentBufferSamplerCount
_MTL_INLINE NS::UInteger MTL::Device::maxArgumentBufferSamplerCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxArgumentBufferSamplerCount));
}

// property: programmableSamplePositionsSupported
_MTL_INLINE bool MTL::Device::programmableSamplePositionsSupported() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(areProgrammableSamplePositionsSupported));
}

// method: getDefaultSamplePositions:count:
_MTL_INLINE void MTL::Device::getDefaultSamplePositions(MTL::SamplePosition* positions, NS::UInteger count)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(getDefaultSamplePositions_count_), positions, count);
}

// method: newArgumentEncoderWithArguments:
_MTL_INLINE MTL::ArgumentEncoder* MTL::Device::newArgumentEncoder(const NS::Array* arguments)
{
    return Object::sendMessage<MTL::ArgumentEncoder*>(this, _MTL_PRIVATE_SEL(newArgumentEncoderWithArguments_), arguments);
}

// method: supportsRasterizationRateMapWithLayerCount:
_MTL_INLINE bool MTL::Device::supportsRasterizationRateMap(NS::UInteger layerCount)
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsRasterizationRateMapWithLayerCount_), layerCount);
}

// method: newRasterizationRateMapWithDescriptor:
_MTL_INLINE MTL::RasterizationRateMap* MTL::Device::newRasterizationRateMap(const MTL::RasterizationRateMapDescriptor* descriptor)
{
    return Object::sendMessage<MTL::RasterizationRateMap*>(this, _MTL_PRIVATE_SEL(newRasterizationRateMapWithDescriptor_), descriptor);
}

// method: newIndirectCommandBufferWithDescriptor:maxCommandCount:options:
_MTL_INLINE MTL::IndirectCommandBuffer* MTL::Device::newIndirectCommandBuffer(const MTL::IndirectCommandBufferDescriptor* descriptor, NS::UInteger maxCount, MTL::ResourceOptions options)
{
    return Object::sendMessage<MTL::IndirectCommandBuffer*>(this, _MTL_PRIVATE_SEL(newIndirectCommandBufferWithDescriptor_maxCommandCount_options_), descriptor, maxCount, options);
}

// method: newEvent
_MTL_INLINE MTL::Event* MTL::Device::newEvent()
{
    return Object::sendMessage<MTL::Event*>(this, _MTL_PRIVATE_SEL(newEvent));
}

// method: newSharedEvent
_MTL_INLINE MTL::SharedEvent* MTL::Device::newSharedEvent()
{
    return Object::sendMessage<MTL::SharedEvent*>(this, _MTL_PRIVATE_SEL(newSharedEvent));
}

// method: newSharedEventWithHandle:
_MTL_INLINE MTL::SharedEvent* MTL::Device::newSharedEvent(const MTL::SharedEventHandle* sharedEventHandle)
{
    return Object::sendMessage<MTL::SharedEvent*>(this, _MTL_PRIVATE_SEL(newSharedEventWithHandle_), sharedEventHandle);
}

// property: peerGroupID
_MTL_INLINE uint64_t MTL::Device::peerGroupID() const
{
    return Object::sendMessage<uint64_t>(this, _MTL_PRIVATE_SEL(peerGroupID));
}

// property: peerIndex
_MTL_INLINE uint32_t MTL::Device::peerIndex() const
{
    return Object::sendMessage<uint32_t>(this, _MTL_PRIVATE_SEL(peerIndex));
}

// property: peerCount
_MTL_INLINE uint32_t MTL::Device::peerCount() const
{
    return Object::sendMessage<uint32_t>(this, _MTL_PRIVATE_SEL(peerCount));
}

// method: sparseTileSizeWithTextureType:pixelFormat:sampleCount:
_MTL_INLINE MTL::Size MTL::Device::sparseTileSize(MTL::TextureType textureType, MTL::PixelFormat pixelFormat, NS::UInteger sampleCount)
{
    return Object::sendMessage<MTL::Size>(this, _MTL_PRIVATE_SEL(sparseTileSizeWithTextureType_pixelFormat_sampleCount_), textureType, pixelFormat, sampleCount);
}

// property: sparseTileSizeInBytes
_MTL_INLINE NS::UInteger MTL::Device::sparseTileSizeInBytes() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(sparseTileSizeInBytes));
}

// method: convertSparsePixelRegions:toTileRegions:withTileSize:alignmentMode:numRegions:
_MTL_INLINE void MTL::Device::convertSparsePixelRegions(const MTL::Region* pixelRegions, MTL::Region* tileRegions, MTL::Size tileSize, MTL::SparseTextureRegionAlignmentMode mode, NS::UInteger numRegions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(convertSparsePixelRegions_toTileRegions_withTileSize_alignmentMode_numRegions_), pixelRegions, tileRegions, tileSize, mode, numRegions);
}

// method: convertSparseTileRegions:toPixelRegions:withTileSize:numRegions:
_MTL_INLINE void MTL::Device::convertSparseTileRegions(const MTL::Region* tileRegions, MTL::Region* pixelRegions, MTL::Size tileSize, NS::UInteger numRegions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(convertSparseTileRegions_toPixelRegions_withTileSize_numRegions_), tileRegions, pixelRegions, tileSize, numRegions);
}

// property: maxBufferLength
_MTL_INLINE NS::UInteger MTL::Device::maxBufferLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxBufferLength));
}

// property: counterSets
_MTL_INLINE NS::Array* MTL::Device::counterSets() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(counterSets));
}

// method: newCounterSampleBufferWithDescriptor:error:
_MTL_INLINE MTL::CounterSampleBuffer* MTL::Device::newCounterSampleBuffer(const MTL::CounterSampleBufferDescriptor* descriptor, NS::Error** error)
{
    return Object::sendMessage<MTL::CounterSampleBuffer*>(this, _MTL_PRIVATE_SEL(newCounterSampleBufferWithDescriptor_error_), descriptor, error);
}

// method: sampleTimestamps:gpuTimestamp:
_MTL_INLINE void MTL::Device::sampleTimestamps(MTL::Timestamp* cpuTimestamp, MTL::Timestamp* gpuTimestamp)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(sampleTimestamps_gpuTimestamp_), cpuTimestamp, gpuTimestamp);
}

// method: supportsCounterSampling:
_MTL_INLINE bool MTL::Device::supportsCounterSampling(MTL::CounterSamplingPoint samplingPoint)
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsCounterSampling_), samplingPoint);
}

// method: supportsVertexAmplificationCount:
_MTL_INLINE bool MTL::Device::supportsVertexAmplificationCount(NS::UInteger count)
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsVertexAmplificationCount_), count);
}

// property: supportsDynamicLibraries
_MTL_INLINE bool MTL::Device::supportsDynamicLibraries() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsDynamicLibraries));
}

// property: supportsRenderDynamicLibraries
_MTL_INLINE bool MTL::Device::supportsRenderDynamicLibraries() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsRenderDynamicLibraries));
}

// method: newDynamicLibrary:error:
_MTL_INLINE MTL::DynamicLibrary* MTL::Device::newDynamicLibrary(const MTL::Library* library, NS::Error** error)
{
    return Object::sendMessage<MTL::DynamicLibrary*>(this, _MTL_PRIVATE_SEL(newDynamicLibrary_error_), library, error);
}

// method: newDynamicLibraryWithURL:error:
_MTL_INLINE MTL::DynamicLibrary* MTL::Device::newDynamicLibrary(const NS::URL* url, NS::Error** error)
{
    return Object::sendMessage<MTL::DynamicLibrary*>(this, _MTL_PRIVATE_SEL(newDynamicLibraryWithURL_error_), url, error);
}

// method: newBinaryArchiveWithDescriptor:error:
_MTL_INLINE MTL::BinaryArchive* MTL::Device::newBinaryArchive(const MTL::BinaryArchiveDescriptor* descriptor, NS::Error** error)
{
    return Object::sendMessage<MTL::BinaryArchive*>(this, _MTL_PRIVATE_SEL(newBinaryArchiveWithDescriptor_error_), descriptor, error);
}

// property: supportsRaytracing
_MTL_INLINE bool MTL::Device::supportsRaytracing() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsRaytracing));
}

// method: accelerationStructureSizesWithDescriptor:
_MTL_INLINE MTL::AccelerationStructureSizes MTL::Device::accelerationStructureSizes(const MTL::AccelerationStructureDescriptor* descriptor)
{
    return Object::sendMessage<MTL::AccelerationStructureSizes>(this, _MTL_PRIVATE_SEL(accelerationStructureSizesWithDescriptor_), descriptor);
}

// method: newAccelerationStructureWithSize:
_MTL_INLINE MTL::AccelerationStructure* MTL::Device::newAccelerationStructure(NS::UInteger size)
{
    return Object::sendMessage<MTL::AccelerationStructure*>(this, _MTL_PRIVATE_SEL(newAccelerationStructureWithSize_), size);
}

// method: newAccelerationStructureWithDescriptor:
_MTL_INLINE MTL::AccelerationStructure* MTL::Device::newAccelerationStructure(const MTL::AccelerationStructureDescriptor* descriptor)
{
    return Object::sendMessage<MTL::AccelerationStructure*>(this, _MTL_PRIVATE_SEL(newAccelerationStructureWithDescriptor_), descriptor);
}

// property: supportsFunctionPointers
_MTL_INLINE bool MTL::Device::supportsFunctionPointers() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsFunctionPointers));
}

// property: supportsFunctionPointersFromRender
_MTL_INLINE bool MTL::Device::supportsFunctionPointersFromRender() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsFunctionPointersFromRender));
}

// property: supportsRaytracingFromRender
_MTL_INLINE bool MTL::Device::supportsRaytracingFromRender() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsRaytracingFromRender));
}

// property: supportsPrimitiveMotionBlur
_MTL_INLINE bool MTL::Device::supportsPrimitiveMotionBlur() const
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsPrimitiveMotionBlur));
}
