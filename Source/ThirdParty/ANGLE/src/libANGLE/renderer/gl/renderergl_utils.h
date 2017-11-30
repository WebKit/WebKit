//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// renderergl_utils.h: Conversion functions and other utility routines
// specific to the OpenGL renderer.

#ifndef LIBANGLE_RENDERER_GL_RENDERERGLUTILS_H_
#define LIBANGLE_RENDERER_GL_RENDERERGLUTILS_H_

#include "libANGLE/angletypes.h"
#include "libANGLE/Error.h"
#include "libANGLE/Version.h"
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
}

namespace rx
{
class FunctionsGL;
struct WorkaroundsGL;
enum class MultiviewImplementationTypeGL
{
    NV_VIEWPORT_ARRAY2,
    UNSPECIFIED
};

VendorID GetVendorID(const FunctionsGL *functions);
std::string GetDriverVersion(const FunctionsGL *functions);

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
}

namespace nativegl
{
bool SupportsFenceSync(const FunctionsGL *functions);
bool SupportsOcclusionQueries(const FunctionsGL *functions);
bool SupportsNativeRendering(const FunctionsGL *functions, GLenum target, GLenum internalFormat);
}

bool CanMapBufferForRead(const FunctionsGL *functions);
uint8_t *MapBufferRangeWithFallback(const FunctionsGL *functions,
                                    GLenum target,
                                    size_t offset,
                                    size_t length,
                                    GLbitfield access);

gl::ErrorOrResult<bool> ShouldApplyLastRowPaddingWorkaround(const gl::Extents &size,
                                                            const gl::PixelStoreStateBase &state,
                                                            const gl::Buffer *pixelBuffer,
                                                            GLenum format,
                                                            GLenum type,
                                                            bool is3D,
                                                            const void *pixels);

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
    {
    }

    EGLint displayType;
    Type type;
    gl::Version version;
};

std::vector<ContextCreationTry> GenerateContextCreationToTry(EGLint requestedType, bool isMesaGLX);
}

#endif // LIBANGLE_RENDERER_GL_RENDERERGLUTILS_H_
