//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLTexture.hpp
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
#include "MTLResource.hpp"
#include "MTLTexture.hpp"
#include "MTLTypes.hpp"
#include <IOSurface/IOSurfaceRef.h>

namespace MTL
{
_MTL_ENUM(NS::UInteger, TextureType) {
    TextureType1D = 0,
    TextureType1DArray = 1,
    TextureType2D = 2,
    TextureType2DArray = 3,
    TextureType2DMultisample = 4,
    TextureTypeCube = 5,
    TextureTypeCubeArray = 6,
    TextureType3D = 7,
    TextureType2DMultisampleArray = 8,
    TextureTypeTextureBuffer = 9,
};

_MTL_ENUM(uint8_t, TextureSwizzle) {
    TextureSwizzleZero = 0,
    TextureSwizzleOne = 1,
    TextureSwizzleRed = 2,
    TextureSwizzleGreen = 3,
    TextureSwizzleBlue = 4,
    TextureSwizzleAlpha = 5,
};

struct TextureSwizzleChannels
{
    MTL::TextureSwizzle red;
    MTL::TextureSwizzle green;
    MTL::TextureSwizzle blue;
    MTL::TextureSwizzle alpha;
} _MTL_PACKED;

class SharedTextureHandle : public NS::Referencing<SharedTextureHandle>
{
public:
    static class SharedTextureHandle* alloc();

    class SharedTextureHandle*        init();

    class Device*                     device() const;

    NS::String*                       label() const;
};

struct SharedTextureHandlePrivate
{
} _MTL_PACKED;

_MTL_OPTIONS(NS::UInteger, TextureUsage) {
    TextureUsageUnknown = 0,
    TextureUsageShaderRead = 1,
    TextureUsageShaderWrite = 2,
    TextureUsageRenderTarget = 4,
    TextureUsagePixelFormatView = 16,
};

_MTL_ENUM(NS::Integer, TextureCompressionType) {
    TextureCompressionTypeLossless = 0,
    TextureCompressionTypeLossy = 1,
};

class TextureDescriptor : public NS::Copying<TextureDescriptor>
{
public:
    static class TextureDescriptor* alloc();

    class TextureDescriptor*        init();

    static class TextureDescriptor* texture2DDescriptor(MTL::PixelFormat pixelFormat, NS::UInteger width, NS::UInteger height, bool mipmapped);

    static class TextureDescriptor* textureCubeDescriptor(MTL::PixelFormat pixelFormat, NS::UInteger size, bool mipmapped);

    static class TextureDescriptor* textureBufferDescriptor(MTL::PixelFormat pixelFormat, NS::UInteger width, MTL::ResourceOptions resourceOptions, MTL::TextureUsage usage);

    MTL::TextureType                textureType() const;
    void                            setTextureType(MTL::TextureType textureType);

    MTL::PixelFormat                pixelFormat() const;
    void                            setPixelFormat(MTL::PixelFormat pixelFormat);

    NS::UInteger                    width() const;
    void                            setWidth(NS::UInteger width);

    NS::UInteger                    height() const;
    void                            setHeight(NS::UInteger height);

    NS::UInteger                    depth() const;
    void                            setDepth(NS::UInteger depth);

    NS::UInteger                    mipmapLevelCount() const;
    void                            setMipmapLevelCount(NS::UInteger mipmapLevelCount);

    NS::UInteger                    sampleCount() const;
    void                            setSampleCount(NS::UInteger sampleCount);

    NS::UInteger                    arrayLength() const;
    void                            setArrayLength(NS::UInteger arrayLength);

    MTL::ResourceOptions            resourceOptions() const;
    void                            setResourceOptions(MTL::ResourceOptions resourceOptions);

    MTL::CPUCacheMode               cpuCacheMode() const;
    void                            setCpuCacheMode(MTL::CPUCacheMode cpuCacheMode);

    MTL::StorageMode                storageMode() const;
    void                            setStorageMode(MTL::StorageMode storageMode);

    MTL::HazardTrackingMode         hazardTrackingMode() const;
    void                            setHazardTrackingMode(MTL::HazardTrackingMode hazardTrackingMode);

    MTL::TextureUsage               usage() const;
    void                            setUsage(MTL::TextureUsage usage);

    bool                            allowGPUOptimizedContents() const;
    void                            setAllowGPUOptimizedContents(bool allowGPUOptimizedContents);

    MTL::TextureSwizzleChannels     swizzle() const;
    void                            setSwizzle(MTL::TextureSwizzleChannels swizzle);
};

class Texture : public NS::Referencing<Texture, Resource>
{
public:
    class Resource*             rootResource() const;

    class Texture*              parentTexture() const;

    NS::UInteger                parentRelativeLevel() const;

    NS::UInteger                parentRelativeSlice() const;

    class Buffer*               buffer() const;

    NS::UInteger                bufferOffset() const;

    NS::UInteger                bufferBytesPerRow() const;

    IOSurfaceRef                iosurface() const;

    NS::UInteger                iosurfacePlane() const;

    MTL::TextureType            textureType() const;

    MTL::PixelFormat            pixelFormat() const;

    NS::UInteger                width() const;

    NS::UInteger                height() const;

    NS::UInteger                depth() const;

    NS::UInteger                mipmapLevelCount() const;

    NS::UInteger                sampleCount() const;

    NS::UInteger                arrayLength() const;

    MTL::TextureUsage           usage() const;

    bool                        shareable() const;

    bool                        framebufferOnly() const;

    NS::UInteger                firstMipmapInTail() const;

    NS::UInteger                tailSizeInBytes() const;

    bool                        isSparse() const;

    bool                        allowGPUOptimizedContents() const;

    void                        getBytes(const void* pixelBytes, NS::UInteger bytesPerRow, NS::UInteger bytesPerImage, MTL::Region region, NS::UInteger level, NS::UInteger slice);

    void                        replaceRegion(MTL::Region region, NS::UInteger level, NS::UInteger slice, const void* pixelBytes, NS::UInteger bytesPerRow, NS::UInteger bytesPerImage);

    void                        getBytes(const void* pixelBytes, NS::UInteger bytesPerRow, MTL::Region region, NS::UInteger level);

    void                        replaceRegion(MTL::Region region, NS::UInteger level, const void* pixelBytes, NS::UInteger bytesPerRow);

    class Texture*              newTextureView(MTL::PixelFormat pixelFormat);

    class Texture*              newTextureView(MTL::PixelFormat pixelFormat, MTL::TextureType textureType, NS::Range levelRange, NS::Range sliceRange);

    class SharedTextureHandle*  newSharedTextureHandle();

    class Texture*              remoteStorageTexture() const;

    class Texture*              newRemoteTextureViewForDevice(const class Device* device);

    MTL::TextureSwizzleChannels swizzle() const;

    class Texture*              newTextureView(MTL::PixelFormat pixelFormat, MTL::TextureType textureType, NS::Range levelRange, NS::Range sliceRange, MTL::TextureSwizzleChannels swizzle);
};

}

// static method: alloc
_MTL_INLINE MTL::SharedTextureHandle* MTL::SharedTextureHandle::alloc()
{
    return NS::Object::alloc<MTL::SharedTextureHandle>(_MTL_PRIVATE_CLS(MTLSharedTextureHandle));
}

// method: init
_MTL_INLINE MTL::SharedTextureHandle* MTL::SharedTextureHandle::init()
{
    return NS::Object::init<MTL::SharedTextureHandle>();
}

// property: device
_MTL_INLINE MTL::Device* MTL::SharedTextureHandle::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: label
_MTL_INLINE NS::String* MTL::SharedTextureHandle::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

// static method: alloc
_MTL_INLINE MTL::TextureDescriptor* MTL::TextureDescriptor::alloc()
{
    return NS::Object::alloc<MTL::TextureDescriptor>(_MTL_PRIVATE_CLS(MTLTextureDescriptor));
}

// method: init
_MTL_INLINE MTL::TextureDescriptor* MTL::TextureDescriptor::init()
{
    return NS::Object::init<MTL::TextureDescriptor>();
}

// static method: texture2DDescriptorWithPixelFormat:width:height:mipmapped:
_MTL_INLINE MTL::TextureDescriptor* MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormat pixelFormat, NS::UInteger width, NS::UInteger height, bool mipmapped)
{
    return Object::sendMessage<MTL::TextureDescriptor*>(_MTL_PRIVATE_CLS(MTLTextureDescriptor), _MTL_PRIVATE_SEL(texture2DDescriptorWithPixelFormat_width_height_mipmapped_), pixelFormat, width, height, mipmapped);
}

// static method: textureCubeDescriptorWithPixelFormat:size:mipmapped:
_MTL_INLINE MTL::TextureDescriptor* MTL::TextureDescriptor::textureCubeDescriptor(MTL::PixelFormat pixelFormat, NS::UInteger size, bool mipmapped)
{
    return Object::sendMessage<MTL::TextureDescriptor*>(_MTL_PRIVATE_CLS(MTLTextureDescriptor), _MTL_PRIVATE_SEL(textureCubeDescriptorWithPixelFormat_size_mipmapped_), pixelFormat, size, mipmapped);
}

// static method: textureBufferDescriptorWithPixelFormat:width:resourceOptions:usage:
_MTL_INLINE MTL::TextureDescriptor* MTL::TextureDescriptor::textureBufferDescriptor(MTL::PixelFormat pixelFormat, NS::UInteger width, MTL::ResourceOptions resourceOptions, MTL::TextureUsage usage)
{
    return Object::sendMessage<MTL::TextureDescriptor*>(_MTL_PRIVATE_CLS(MTLTextureDescriptor), _MTL_PRIVATE_SEL(textureBufferDescriptorWithPixelFormat_width_resourceOptions_usage_), pixelFormat, width, resourceOptions, usage);
}

// property: textureType
_MTL_INLINE MTL::TextureType MTL::TextureDescriptor::textureType() const
{
    return Object::sendMessage<MTL::TextureType>(this, _MTL_PRIVATE_SEL(textureType));
}

_MTL_INLINE void MTL::TextureDescriptor::setTextureType(MTL::TextureType textureType)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTextureType_), textureType);
}

// property: pixelFormat
_MTL_INLINE MTL::PixelFormat MTL::TextureDescriptor::pixelFormat() const
{
    return Object::sendMessage<MTL::PixelFormat>(this, _MTL_PRIVATE_SEL(pixelFormat));
}

_MTL_INLINE void MTL::TextureDescriptor::setPixelFormat(MTL::PixelFormat pixelFormat)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setPixelFormat_), pixelFormat);
}

// property: width
_MTL_INLINE NS::UInteger MTL::TextureDescriptor::width() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(width));
}

_MTL_INLINE void MTL::TextureDescriptor::setWidth(NS::UInteger width)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setWidth_), width);
}

// property: height
_MTL_INLINE NS::UInteger MTL::TextureDescriptor::height() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(height));
}

_MTL_INLINE void MTL::TextureDescriptor::setHeight(NS::UInteger height)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setHeight_), height);
}

// property: depth
_MTL_INLINE NS::UInteger MTL::TextureDescriptor::depth() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(depth));
}

_MTL_INLINE void MTL::TextureDescriptor::setDepth(NS::UInteger depth)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepth_), depth);
}

// property: mipmapLevelCount
_MTL_INLINE NS::UInteger MTL::TextureDescriptor::mipmapLevelCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(mipmapLevelCount));
}

_MTL_INLINE void MTL::TextureDescriptor::setMipmapLevelCount(NS::UInteger mipmapLevelCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMipmapLevelCount_), mipmapLevelCount);
}

// property: sampleCount
_MTL_INLINE NS::UInteger MTL::TextureDescriptor::sampleCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(sampleCount));
}

_MTL_INLINE void MTL::TextureDescriptor::setSampleCount(NS::UInteger sampleCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSampleCount_), sampleCount);
}

// property: arrayLength
_MTL_INLINE NS::UInteger MTL::TextureDescriptor::arrayLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(arrayLength));
}

_MTL_INLINE void MTL::TextureDescriptor::setArrayLength(NS::UInteger arrayLength)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setArrayLength_), arrayLength);
}

// property: resourceOptions
_MTL_INLINE MTL::ResourceOptions MTL::TextureDescriptor::resourceOptions() const
{
    return Object::sendMessage<MTL::ResourceOptions>(this, _MTL_PRIVATE_SEL(resourceOptions));
}

_MTL_INLINE void MTL::TextureDescriptor::setResourceOptions(MTL::ResourceOptions resourceOptions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setResourceOptions_), resourceOptions);
}

// property: cpuCacheMode
_MTL_INLINE MTL::CPUCacheMode MTL::TextureDescriptor::cpuCacheMode() const
{
    return Object::sendMessage<MTL::CPUCacheMode>(this, _MTL_PRIVATE_SEL(cpuCacheMode));
}

_MTL_INLINE void MTL::TextureDescriptor::setCpuCacheMode(MTL::CPUCacheMode cpuCacheMode)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setCpuCacheMode_), cpuCacheMode);
}

// property: storageMode
_MTL_INLINE MTL::StorageMode MTL::TextureDescriptor::storageMode() const
{
    return Object::sendMessage<MTL::StorageMode>(this, _MTL_PRIVATE_SEL(storageMode));
}

_MTL_INLINE void MTL::TextureDescriptor::setStorageMode(MTL::StorageMode storageMode)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStorageMode_), storageMode);
}

// property: hazardTrackingMode
_MTL_INLINE MTL::HazardTrackingMode MTL::TextureDescriptor::hazardTrackingMode() const
{
    return Object::sendMessage<MTL::HazardTrackingMode>(this, _MTL_PRIVATE_SEL(hazardTrackingMode));
}

_MTL_INLINE void MTL::TextureDescriptor::setHazardTrackingMode(MTL::HazardTrackingMode hazardTrackingMode)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setHazardTrackingMode_), hazardTrackingMode);
}

// property: usage
_MTL_INLINE MTL::TextureUsage MTL::TextureDescriptor::usage() const
{
    return Object::sendMessage<MTL::TextureUsage>(this, _MTL_PRIVATE_SEL(usage));
}

_MTL_INLINE void MTL::TextureDescriptor::setUsage(MTL::TextureUsage usage)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setUsage_), usage);
}

// property: allowGPUOptimizedContents
_MTL_INLINE bool MTL::TextureDescriptor::allowGPUOptimizedContents() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(allowGPUOptimizedContents));
}

_MTL_INLINE void MTL::TextureDescriptor::setAllowGPUOptimizedContents(bool allowGPUOptimizedContents)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setAllowGPUOptimizedContents_), allowGPUOptimizedContents);
}

// property: swizzle
_MTL_INLINE MTL::TextureSwizzleChannels MTL::TextureDescriptor::swizzle() const
{
    return Object::sendMessage<MTL::TextureSwizzleChannels>(this, _MTL_PRIVATE_SEL(swizzle));
}

_MTL_INLINE void MTL::TextureDescriptor::setSwizzle(MTL::TextureSwizzleChannels swizzle)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSwizzle_), swizzle);
}

// property: rootResource
_MTL_INLINE MTL::Resource* MTL::Texture::rootResource() const
{
    return Object::sendMessage<MTL::Resource*>(this, _MTL_PRIVATE_SEL(rootResource));
}

// property: parentTexture
_MTL_INLINE MTL::Texture* MTL::Texture::parentTexture() const
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(parentTexture));
}

// property: parentRelativeLevel
_MTL_INLINE NS::UInteger MTL::Texture::parentRelativeLevel() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(parentRelativeLevel));
}

// property: parentRelativeSlice
_MTL_INLINE NS::UInteger MTL::Texture::parentRelativeSlice() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(parentRelativeSlice));
}

// property: buffer
_MTL_INLINE MTL::Buffer* MTL::Texture::buffer() const
{
    return Object::sendMessage<MTL::Buffer*>(this, _MTL_PRIVATE_SEL(buffer));
}

// property: bufferOffset
_MTL_INLINE NS::UInteger MTL::Texture::bufferOffset() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(bufferOffset));
}

// property: bufferBytesPerRow
_MTL_INLINE NS::UInteger MTL::Texture::bufferBytesPerRow() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(bufferBytesPerRow));
}

// property: iosurface
_MTL_INLINE IOSurfaceRef MTL::Texture::iosurface() const
{
    return Object::sendMessage<IOSurfaceRef>(this, _MTL_PRIVATE_SEL(iosurface));
}

// property: iosurfacePlane
_MTL_INLINE NS::UInteger MTL::Texture::iosurfacePlane() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(iosurfacePlane));
}

// property: textureType
_MTL_INLINE MTL::TextureType MTL::Texture::textureType() const
{
    return Object::sendMessage<MTL::TextureType>(this, _MTL_PRIVATE_SEL(textureType));
}

// property: pixelFormat
_MTL_INLINE MTL::PixelFormat MTL::Texture::pixelFormat() const
{
    return Object::sendMessage<MTL::PixelFormat>(this, _MTL_PRIVATE_SEL(pixelFormat));
}

// property: width
_MTL_INLINE NS::UInteger MTL::Texture::width() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(width));
}

// property: height
_MTL_INLINE NS::UInteger MTL::Texture::height() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(height));
}

// property: depth
_MTL_INLINE NS::UInteger MTL::Texture::depth() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(depth));
}

// property: mipmapLevelCount
_MTL_INLINE NS::UInteger MTL::Texture::mipmapLevelCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(mipmapLevelCount));
}

// property: sampleCount
_MTL_INLINE NS::UInteger MTL::Texture::sampleCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(sampleCount));
}

// property: arrayLength
_MTL_INLINE NS::UInteger MTL::Texture::arrayLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(arrayLength));
}

// property: usage
_MTL_INLINE MTL::TextureUsage MTL::Texture::usage() const
{
    return Object::sendMessage<MTL::TextureUsage>(this, _MTL_PRIVATE_SEL(usage));
}

// property: shareable
_MTL_INLINE bool MTL::Texture::shareable() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isShareable));
}

// property: framebufferOnly
_MTL_INLINE bool MTL::Texture::framebufferOnly() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isFramebufferOnly));
}

// property: firstMipmapInTail
_MTL_INLINE NS::UInteger MTL::Texture::firstMipmapInTail() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(firstMipmapInTail));
}

// property: tailSizeInBytes
_MTL_INLINE NS::UInteger MTL::Texture::tailSizeInBytes() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(tailSizeInBytes));
}

// property: isSparse
_MTL_INLINE bool MTL::Texture::isSparse() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isSparse));
}

// property: allowGPUOptimizedContents
_MTL_INLINE bool MTL::Texture::allowGPUOptimizedContents() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(allowGPUOptimizedContents));
}

// method: getBytes:bytesPerRow:bytesPerImage:fromRegion:mipmapLevel:slice:
_MTL_INLINE void MTL::Texture::getBytes(const void* pixelBytes, NS::UInteger bytesPerRow, NS::UInteger bytesPerImage, MTL::Region region, NS::UInteger level, NS::UInteger slice)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(getBytes_bytesPerRow_bytesPerImage_fromRegion_mipmapLevel_slice_), pixelBytes, bytesPerRow, bytesPerImage, region, level, slice);
}

// method: replaceRegion:mipmapLevel:slice:withBytes:bytesPerRow:bytesPerImage:
_MTL_INLINE void MTL::Texture::replaceRegion(MTL::Region region, NS::UInteger level, NS::UInteger slice, const void* pixelBytes, NS::UInteger bytesPerRow, NS::UInteger bytesPerImage)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(replaceRegion_mipmapLevel_slice_withBytes_bytesPerRow_bytesPerImage_), region, level, slice, pixelBytes, bytesPerRow, bytesPerImage);
}

// method: getBytes:bytesPerRow:fromRegion:mipmapLevel:
_MTL_INLINE void MTL::Texture::getBytes(const void* pixelBytes, NS::UInteger bytesPerRow, MTL::Region region, NS::UInteger level)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(getBytes_bytesPerRow_fromRegion_mipmapLevel_), pixelBytes, bytesPerRow, region, level);
}

// method: replaceRegion:mipmapLevel:withBytes:bytesPerRow:
_MTL_INLINE void MTL::Texture::replaceRegion(MTL::Region region, NS::UInteger level, const void* pixelBytes, NS::UInteger bytesPerRow)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(replaceRegion_mipmapLevel_withBytes_bytesPerRow_), region, level, pixelBytes, bytesPerRow);
}

// method: newTextureViewWithPixelFormat:
_MTL_INLINE MTL::Texture* MTL::Texture::newTextureView(MTL::PixelFormat pixelFormat)
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(newTextureViewWithPixelFormat_), pixelFormat);
}

// method: newTextureViewWithPixelFormat:textureType:levels:slices:
_MTL_INLINE MTL::Texture* MTL::Texture::newTextureView(MTL::PixelFormat pixelFormat, MTL::TextureType textureType, NS::Range levelRange, NS::Range sliceRange)
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(newTextureViewWithPixelFormat_textureType_levels_slices_), pixelFormat, textureType, levelRange, sliceRange);
}

// method: newSharedTextureHandle
_MTL_INLINE MTL::SharedTextureHandle* MTL::Texture::newSharedTextureHandle()
{
    return Object::sendMessage<MTL::SharedTextureHandle*>(this, _MTL_PRIVATE_SEL(newSharedTextureHandle));
}

// property: remoteStorageTexture
_MTL_INLINE MTL::Texture* MTL::Texture::remoteStorageTexture() const
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(remoteStorageTexture));
}

// method: newRemoteTextureViewForDevice:
_MTL_INLINE MTL::Texture* MTL::Texture::newRemoteTextureViewForDevice(const MTL::Device* device)
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(newRemoteTextureViewForDevice_), device);
}

// property: swizzle
_MTL_INLINE MTL::TextureSwizzleChannels MTL::Texture::swizzle() const
{
    return Object::sendMessage<MTL::TextureSwizzleChannels>(this, _MTL_PRIVATE_SEL(swizzle));
}

// method: newTextureViewWithPixelFormat:textureType:levels:slices:swizzle:
_MTL_INLINE MTL::Texture* MTL::Texture::newTextureView(MTL::PixelFormat pixelFormat, MTL::TextureType textureType, NS::Range levelRange, NS::Range sliceRange, MTL::TextureSwizzleChannels swizzle)
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(newTextureViewWithPixelFormat_textureType_levels_slices_swizzle_), pixelFormat, textureType, levelRange, sliceRange, swizzle);
}
