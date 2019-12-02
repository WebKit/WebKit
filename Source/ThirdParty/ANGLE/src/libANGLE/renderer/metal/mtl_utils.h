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
namespace mtl
{

NS_ASSUME_NONNULL_BEGIN

angle::Result InitializeTextureContents(const gl::Context *context,
                                        const TextureRef &texture,
                                        const Format &textureObjFormat,
                                        const gl::ImageIndex &index);

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

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(id<MTLDevice> metalDevice,
                                                const std::string &source,
                                                AutoObjCPtr<NSError *> *error);

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(id<MTLDevice> metalDevice,
                                                const char *source,
                                                size_t sourceLen,
                                                AutoObjCPtr<NSError *> *error);

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibraryFromBinary(id<MTLDevice> metalDevice,
                                                          const uint8_t *binarySource,
                                                          size_t binarySourceLen,
                                                          AutoObjCPtr<NSError *> *error);

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

// Useful to set clear color for texture originally having no alpha in GL, but backend's format
// has alpha channel.
MTLClearColor EmulatedAlphaClearColor(MTLClearColor color, MTLColorWriteMask colorMask);

NS_ASSUME_NONNULL_END
}  // namespace mtl
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_MTL_UTILS_H_ */
