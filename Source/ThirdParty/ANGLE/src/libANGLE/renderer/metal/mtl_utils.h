//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_utils.h:
//    Declares utilities functions that create Metal shaders, convert from angle enums
//    to Metal enums and so on.
//

#ifndef LIBANGLE_RENDERER_METAL_MTL_UTILS_H_
#define LIBANGLE_RENDERER_METAL_MTL_UTILS_H_

#import <Metal/Metal.h>

#include "angle_gl.h"
#include "common/PackedEnums.h"
#include "libANGLE/Context.h"
#include "libANGLE/Texture.h"
#include "libANGLE/renderer/metal/mtl_format_utils.h"
#include "libANGLE/renderer/metal/mtl_resources.h"
#include "libANGLE/renderer/metal/mtl_state_cache.h"

namespace rx
{

class ContextMtl;

namespace mtl
{

NS_ASSUME_NONNULL_BEGIN

// Initialize texture content to black.
angle::Result InitializeTextureContents(const gl::Context *context,
                                        const TextureRef &texture,
                                        const Format &textureObjFormat,
                                        const ImageNativeIndex &index);
// Same as above but using GPU clear operation instead of CPU.
// - channelsToInit parameter controls which channels will get their content initialized.
angle::Result InitializeTextureContentsGPU(const gl::Context *context,
                                           const TextureRef &texture,
                                           const Format &textureObjFormat,
                                           const ImageNativeIndex &index,
                                           MTLColorWriteMask channelsToInit);

// Same as above but for a depth/stencil texture.
angle::Result InitializeDepthStencilTextureContentsGPU(const gl::Context *context,
                                                       const TextureRef &texture,
                                                       const Format &textureObjFormat,
                                                       const ImageNativeIndex &index);

// Unified texture's per slice/depth texel reading function
angle::Result ReadTexturePerSliceBytes(const gl::Context *context,
                                       const TextureRef &texture,
                                       size_t bytesPerRow,
                                       const gl::Rectangle &fromRegion,
                                       const MipmapNativeLevel &mipLevel,
                                       uint32_t sliceOrDepth,
                                       uint8_t *dataOut);

angle::Result ReadTexturePerSliceBytesToBuffer(const gl::Context *context,
                                               const TextureRef &texture,
                                               size_t bytesPerRow,
                                               const gl::Rectangle &fromRegion,
                                               const MipmapNativeLevel &mipLevel,
                                               uint32_t sliceOrDepth,
                                               uint32_t dstOffset,
                                               const BufferRef &dstBuffer);

MTLViewport GetViewport(const gl::Rectangle &rect, double znear = 0, double zfar = 1);
MTLViewport GetViewportFlipY(const gl::Rectangle &rect,
                             NSUInteger screenHeight,
                             double znear = 0,
                             double zfar  = 1);
MTLViewport GetViewport(const gl::Rectangle &rect,
                        NSUInteger screenHeight,
                        bool flipY,
                        double znear = 0,
                        double zfar  = 1);
MTLScissorRect GetScissorRect(const gl::Rectangle &rect,
                              NSUInteger screenHeight = 0,
                              bool flipY              = false);

uint32_t GetDeviceVendorId(id<MTLDevice> metalDevice);


AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(id<MTLDevice> metalDevice,
                                                const std::string &source,
                                                NSDictionary<NSString *, NSObject *> * substitutionDictionary,
                                                bool enableFastMath,
                                                AutoObjCPtr<NSError *> *error);

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(id<MTLDevice> metalDevice,
                                                const std::string &source,
                                                AutoObjCPtr<NSError *> *error);

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(id<MTLDevice> metalDevice,
                                                const char *source,
                                                size_t sourceLen,
                                                NSDictionary<NSString *, NSObject *> * substitutionDictionary,
                                                bool enableFastMath,
                                                AutoObjCPtr<NSError *> *error);

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibraryFromBinary(id<MTLDevice> metalDevice,
                                                          const uint8_t *binarySource,
                                                          size_t binarySourceLen,
                                                          AutoObjCPtr<NSError *> *error);

bool SupportsIOSGPUFamily(id<MTLDevice> device, uint8_t iOSFamily);

bool SupportsMacGPUFamily(id<MTLDevice> device, uint8_t macFamily);

// Need to define invalid enum value since Metal doesn't define it
constexpr MTLTextureType MTLTextureTypeInvalid = static_cast<MTLTextureType>(NSUIntegerMax);
static_assert(sizeof(MTLTextureType) == sizeof(NSUInteger),
              "MTLTextureType is supposed to be based on NSUInteger");

constexpr MTLPrimitiveType MTLPrimitiveTypeInvalid = static_cast<MTLPrimitiveType>(NSUIntegerMax);
static_assert(sizeof(MTLPrimitiveType) == sizeof(NSUInteger),
              "MTLPrimitiveType is supposed to be based on NSUInteger");

constexpr MTLIndexType MTLIndexTypeInvalid = static_cast<MTLIndexType>(NSUIntegerMax);
static_assert(sizeof(MTLIndexType) == sizeof(NSUInteger),
              "MTLIndexType is supposed to be based on NSUInteger");

MTLTextureType GetTextureType(gl::TextureType glType);

MTLSamplerMinMagFilter GetFilter(GLenum filter);
MTLSamplerMipFilter GetMipmapFilter(GLenum filter);
MTLSamplerAddressMode GetSamplerAddressMode(GLenum wrap);

MTLBlendFactor GetBlendFactor(GLenum factor);
MTLBlendOperation GetBlendOp(GLenum op);

MTLCompareFunction GetCompareFunc(GLenum func);
MTLStencilOperation GetStencilOp(GLenum op);

MTLWinding GetFontfaceWinding(GLenum frontFaceMode, bool invert);

PrimitiveTopologyClass GetPrimitiveTopologyClass(gl::PrimitiveMode mode);
MTLPrimitiveType GetPrimitiveType(gl::PrimitiveMode mode);
MTLIndexType GetIndexType(gl::DrawElementsType type);

#if ANGLE_MTL_SWIZZLE_AVAILABLE
MTLTextureSwizzle GetTextureSwizzle(GLenum swizzle);
#endif

// Get color write mask for a specified format. Some formats such as RGB565 doesn't have alpha
// channel but is emulated by a RGBA8 format, we need to disable alpha write for this format.
// - emulatedChannelsOut: if the format is emulated, this pointer will store a true value.
MTLColorWriteMask GetEmulatedColorWriteMask(const mtl::Format &mtlFormat,
                                            bool *emulatedChannelsOut);
MTLColorWriteMask GetEmulatedColorWriteMask(const mtl::Format &mtlFormat);
bool IsFormatEmulated(const mtl::Format &mtlFormat);

NSUInteger GetMaxRenderTargetSizeForDeviceInBytes(id<MTLDevice> device);
NSUInteger GetMaxNumberOfRenderTargetsForDevice(id<MTLDevice> device);
bool DeviceHasMaximumRenderTargetSize(id<MTLDevice> device);

// Useful to set clear color for texture originally having no alpha in GL, but backend's format
// has alpha channel.
MTLClearColor EmulatedAlphaClearColor(MTLClearColor color, MTLColorWriteMask colorMask);


NSUInteger ComputeTotalSizeUsedForMTLRenderPassDescriptor(const MTLRenderPassDescriptor *descriptor,
                                                          const Context *context,
                                                          id<MTLDevice> device);

NSUInteger ComputeTotalSizeUsedForMTLRenderPipelineDescriptor(
    const MTLRenderPipelineDescriptor *descriptor,
    const Context *context,
    id<MTLDevice> device);

gl::Box MTLRegionToGLBox(const MTLRegion &mtlRegion);

MipmapNativeLevel GetNativeMipLevel(GLuint level, GLuint base);
GLuint GetGLMipLevel(const MipmapNativeLevel &nativeLevel, GLuint base);

angle::Result TriangleFanBoundCheck(ContextMtl *context, size_t numTris);

angle::Result GetTriangleFanIndicesCount(ContextMtl *context,
                                         GLsizei vetexCount,
                                         uint32_t *numElemsOut);

angle::Result CreateMslShader(Context *context,
                              id<MTLLibrary> shaderLib,
                              NSString * shaderName,
                              MTLFunctionConstantValues *funcConstants,
                              AutoObjCPtr<id<MTLFunction>> *shaderOut);

angle::Result CreateMslShader(Context * _Nonnull context,
                              id<MTLLibrary> shaderLib,
                              NSString * _Nonnull shaderName,
                              MTLFunctionConstantValues * _Nullable funcConstants,
                              id<MTLFunction> _Nullable  * _Nonnull  shaderOut);

NS_ASSUME_NONNULL_END
}  // namespace mtl
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_MTL_UTILS_H_ */
