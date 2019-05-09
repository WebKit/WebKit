//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BlitGL.h: Defines the BlitGL class, a helper for blitting textures

#ifndef LIBANGLE_RENDERER_GL_BLITGL_H_
#define LIBANGLE_RENDERER_GL_BLITGL_H_

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/angletypes.h"

#include <map>

namespace gl
{
class Framebuffer;
class ImageIndex;
}  // namespace gl

namespace rx
{

class FramebufferGL;
class FunctionsGL;
class RenderbufferGL;
class StateManagerGL;
class TextureGL;
struct WorkaroundsGL;

class BlitGL : angle::NonCopyable
{
  public:
    BlitGL(const FunctionsGL *functions,
           const WorkaroundsGL &workarounds,
           StateManagerGL *stateManager);
    ~BlitGL();

    angle::Result copyImageToLUMAWorkaroundTexture(const gl::Context *context,
                                                   GLuint texture,
                                                   gl::TextureType textureType,
                                                   gl::TextureTarget target,
                                                   GLenum lumaFormat,
                                                   size_t level,
                                                   const gl::Rectangle &sourceArea,
                                                   GLenum internalFormat,
                                                   gl::Framebuffer *source);

    angle::Result copySubImageToLUMAWorkaroundTexture(const gl::Context *context,
                                                      GLuint texture,
                                                      gl::TextureType textureType,
                                                      gl::TextureTarget target,
                                                      GLenum lumaFormat,
                                                      size_t level,
                                                      const gl::Offset &destOffset,
                                                      const gl::Rectangle &sourceArea,
                                                      gl::Framebuffer *source);

    angle::Result blitColorBufferWithShader(const gl::Context *context,
                                            const gl::Framebuffer *source,
                                            const gl::Framebuffer *dest,
                                            const gl::Rectangle &sourceArea,
                                            const gl::Rectangle &destArea,
                                            GLenum filter);

    angle::Result copySubTexture(const gl::Context *context,
                                 TextureGL *source,
                                 size_t sourceLevel,
                                 GLenum sourceComponentType,
                                 TextureGL *dest,
                                 gl::TextureTarget destTarget,
                                 size_t destLevel,
                                 GLenum destComponentType,
                                 const gl::Extents &sourceSize,
                                 const gl::Rectangle &sourceArea,
                                 const gl::Offset &destOffset,
                                 bool needsLumaWorkaround,
                                 GLenum lumaFormat,
                                 bool unpackFlipY,
                                 bool unpackPremultiplyAlpha,
                                 bool unpackUnmultiplyAlpha,
                                 bool *copySucceededOut);

    angle::Result copySubTextureCPUReadback(const gl::Context *context,
                                            TextureGL *source,
                                            size_t sourceLevel,
                                            GLenum sourceComponentType,
                                            TextureGL *dest,
                                            gl::TextureTarget destTarget,
                                            size_t destLevel,
                                            GLenum destFormat,
                                            GLenum destType,
                                            const gl::Rectangle &sourceArea,
                                            const gl::Offset &destOffset,
                                            bool unpackFlipY,
                                            bool unpackPremultiplyAlpha,
                                            bool unpackUnmultiplyAlpha);

    angle::Result copyTexSubImage(TextureGL *source,
                                  size_t sourceLevel,
                                  TextureGL *dest,
                                  gl::TextureTarget destTarget,
                                  size_t destLevel,
                                  const gl::Rectangle &sourceArea,
                                  const gl::Offset &destOffset,
                                  bool *copySucceededOut);

    angle::Result clearRenderableTexture(TextureGL *source,
                                         GLenum sizedInternalFormat,
                                         int numTextureLayers,
                                         const gl::ImageIndex &imageIndex,
                                         bool *clearSucceededOut);

    angle::Result clearRenderbuffer(RenderbufferGL *source, GLenum sizedInternalFormat);

    angle::Result clearFramebuffer(FramebufferGL *source);

    angle::Result initializeResources();

  private:
    void orphanScratchTextures();
    void setScratchTextureParameter(GLenum param, GLenum value);

    const FunctionsGL *mFunctions;
    const WorkaroundsGL &mWorkarounds;
    StateManagerGL *mStateManager;

    struct BlitProgram
    {
        GLuint program                = 0;
        GLint sourceTextureLocation   = -1;
        GLint scaleLocation           = -1;
        GLint offsetLocation          = -1;
        GLint multiplyAlphaLocation   = -1;
        GLint unMultiplyAlphaLocation = -1;
    };

    enum class BlitProgramType
    {
        FLOAT_TO_FLOAT,
        FLOAT_TO_UINT,
        UINT_TO_UINT,
    };

    static BlitProgramType getBlitProgramType(GLenum sourceComponentType, GLenum destComponentType);
    angle::Result getBlitProgram(const gl::Context *context,
                                 BlitProgramType type,
                                 BlitProgram **program);

    std::map<BlitProgramType, BlitProgram> mBlitPrograms;

    GLuint mScratchTextures[2];
    GLuint mScratchFBO;

    GLuint mVAO;
    GLuint mVertexBuffer;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_BLITGL_H_
