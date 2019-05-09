//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// renderergl_utils.h: Conversion functions and other utility routines
// specific to the OpenGL renderer.

#ifndef LIBANGLE_RENDERER_GL_RENDERERGLUTILS_H_
#define LIBANGLE_RENDERER_GL_RENDERERGLUTILS_H_

#include "libANGLE/Error.h"
#include "libANGLE/Version.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/driver_utils.h"
#include "libANGLE/renderer/gl/functionsgl_typedefs.h"

#include <string>
#include <vector>

namespace gl
{
struct Caps;
class TextureCapsMap;
struct Extensions;
struct Version;
struct Workarounds;
}  // namespace gl

namespace rx
{
class BlitGL;
class ClearMultiviewGL;
class ContextGL;
class FunctionsGL;
class StateManagerGL;
struct WorkaroundsGL;
enum class MultiviewImplementationTypeGL
{
    NV_VIEWPORT_ARRAY2,
    UNSPECIFIED
};

VendorID GetVendorID(const FunctionsGL *functions);

// Helpers for extracting the GL helper objects out of a context
const FunctionsGL *GetFunctionsGL(const gl::Context *context);
StateManagerGL *GetStateManagerGL(const gl::Context *context);
BlitGL *GetBlitGL(const gl::Context *context);
ClearMultiviewGL *GetMultiviewClearer(const gl::Context *context);
const WorkaroundsGL &GetWorkaroundsGL(const gl::Context *context);

namespace nativegl_gl
{

void GenerateCaps(const FunctionsGL *functions,
                  const WorkaroundsGL &workarounds,
                  gl::Caps *caps,
                  gl::TextureCapsMap *textureCapsMap,
                  gl::Extensions *extensions,
                  gl::Version *maxSupportedESVersion,
                  MultiviewImplementationTypeGL *multiviewImplementationType);

void GenerateWorkarounds(const FunctionsGL *functions, WorkaroundsGL *workarounds);
void ApplyWorkarounds(const FunctionsGL *functions, gl::Workarounds *workarounds);
}  // namespace nativegl_gl

namespace nativegl
{
bool SupportsFenceSync(const FunctionsGL *functions);
bool SupportsOcclusionQueries(const FunctionsGL *functions);
bool SupportsNativeRendering(const FunctionsGL *functions,
                             gl::TextureType type,
                             GLenum internalFormat);
bool UseTexImage2D(gl::TextureType textureType);
bool UseTexImage3D(gl::TextureType textureType);
}  // namespace nativegl

bool CanMapBufferForRead(const FunctionsGL *functions);
uint8_t *MapBufferRangeWithFallback(const FunctionsGL *functions,
                                    GLenum target,
                                    size_t offset,
                                    size_t length,
                                    GLbitfield access);

angle::Result ShouldApplyLastRowPaddingWorkaround(ContextGL *contextGL,
                                                  const gl::Extents &size,
                                                  const gl::PixelStoreStateBase &state,
                                                  const gl::Buffer *pixelBuffer,
                                                  GLenum format,
                                                  GLenum type,
                                                  bool is3D,
                                                  const void *pixels,
                                                  bool *shouldApplyOut);

struct ContextCreationTry
{
    enum class Type
    {
        DESKTOP_CORE,
        DESKTOP_LEGACY,
        ES,
    };

    ContextCreationTry(EGLint displayType, Type type, gl::Version version)
        : displayType(displayType), type(type), version(version)
    {}

    EGLint displayType;
    Type type;
    gl::Version version;
};

std::vector<ContextCreationTry> GenerateContextCreationToTry(EGLint requestedType, bool isMesaGLX);
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_RENDERERGLUTILS_H_
