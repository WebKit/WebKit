//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureGL.h: Defines the class interface for TextureGL.

#ifndef LIBANGLE_RENDERER_GL_TEXTUREGL_H_
#define LIBANGLE_RENDERER_GL_TEXTUREGL_H_

#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/TextureImpl.h"
#include "libANGLE/Texture.h"

namespace rx
{

class BlitGL;
class FunctionsGL;
class StateManagerGL;
struct WorkaroundsGL;

struct LUMAWorkaroundGL
{
    bool enabled;
    GLenum workaroundFormat;

    LUMAWorkaroundGL();
    LUMAWorkaroundGL(bool enabled, GLenum workaroundFormat);
};

// Structure containing information about format and workarounds for each mip level of the
// TextureGL.
struct LevelInfoGL
{
    // Format of the data used in this mip level.
    GLenum sourceFormat;

    // If this mip level requires sampler-state re-writing so that only a red channel is exposed.
    bool depthStencilWorkaround;

    // Information about luminance alpha texture workarounds in the core profile.
    LUMAWorkaroundGL lumaWorkaround;

    LevelInfoGL();
    LevelInfoGL(GLenum sourceFormat,
                bool depthStencilWorkaround,
                const LUMAWorkaroundGL &lumaWorkaround);
};

class TextureGL : public TextureImpl
{
  public:
    TextureGL(const gl::TextureState &state,
              const FunctionsGL *functions,
              const WorkaroundsGL &workarounds,
              StateManagerGL *stateManager,
              BlitGL *blitter);
    ~TextureGL() override;

    gl::Error setImage(ContextImpl *contextImpl,
                       GLenum target,
                       size_t level,
                       GLenum internalFormat,
                       const gl::Extents &size,
                       GLenum format,
                       GLenum type,
                       const gl::PixelUnpackState &unpack,
                       const uint8_t *pixels) override;
    gl::Error setSubImage(ContextImpl *contextImpl,
                          GLenum target,
                          size_t level,
                          const gl::Box &area,
                          GLenum format,
                          GLenum type,
                          const gl::PixelUnpackState &unpack,
                          const uint8_t *pixels) override;

    gl::Error setCompressedImage(ContextImpl *contextImpl,
                                 GLenum target,
                                 size_t level,
                                 GLenum internalFormat,
                                 const gl::Extents &size,
                                 const gl::PixelUnpackState &unpack,
                                 size_t imageSize,
                                 const uint8_t *pixels) override;
    gl::Error setCompressedSubImage(ContextImpl *contextImpl,
                                    GLenum target,
                                    size_t level,
                                    const gl::Box &area,
                                    GLenum format,
                                    const gl::PixelUnpackState &unpack,
                                    size_t imageSize,
                                    const uint8_t *pixels) override;

    gl::Error copyImage(ContextImpl *contextImpl,
                        GLenum target,
                        size_t level,
                        const gl::Rectangle &sourceArea,
                        GLenum internalFormat,
                        const gl::Framebuffer *source) override;
    gl::Error copySubImage(ContextImpl *contextImpl,
                           GLenum target,
                           size_t level,
                           const gl::Offset &destOffset,
                           const gl::Rectangle &sourceArea,
                           const gl::Framebuffer *source) override;

    gl::Error copyTexture(ContextImpl *contextImpl,
                          GLenum target,
                          size_t level,
                          GLenum internalFormat,
                          GLenum type,
                          size_t sourceLevel,
                          bool unpackFlipY,
                          bool unpackPremultiplyAlpha,
                          bool unpackUnmultiplyAlpha,
                          const gl::Texture *source) override;
    gl::Error copySubTexture(ContextImpl *contextImpl,
                             GLenum target,
                             size_t level,
                             const gl::Offset &destOffset,
                             size_t sourceLevel,
                             const gl::Rectangle &sourceArea,
                             bool unpackFlipY,
                             bool unpackPremultiplyAlpha,
                             bool unpackUnmultiplyAlpha,
                             const gl::Texture *source) override;
    gl::Error copySubTextureHelper(GLenum target,
                                   size_t level,
                                   const gl::Offset &destOffset,
                                   size_t sourceLevel,
                                   const gl::Rectangle &sourceArea,
                                   GLenum destFormat,
                                   bool unpackFlipY,
                                   bool unpackPremultiplyAlpha,
                                   bool unpackUnmultiplyAlpha,
                                   const gl::Texture *source);

    gl::Error setStorage(ContextImpl *contextImpl,
                         GLenum target,
                         size_t levels,
                         GLenum internalFormat,
                         const gl::Extents &size) override;

    gl::Error setStorageMultisample(ContextImpl *contextImpl,
                                    GLenum target,
                                    GLsizei samples,
                                    GLint internalFormat,
                                    const gl::Extents &size,
                                    GLboolean fixedSampleLocations) override;

    gl::Error setImageExternal(GLenum target,
                               egl::Stream *stream,
                               const egl::Stream::GLTextureDescription &desc) override;

    gl::Error generateMipmap(ContextImpl *contextImpl) override;

    void bindTexImage(egl::Surface *surface) override;
    void releaseTexImage() override;

    gl::Error setEGLImageTarget(GLenum target, egl::Image *image) override;

    GLuint getTextureID() const;
    GLenum getTarget() const;

    void syncState(const gl::Texture::DirtyBits &dirtyBits) override;
    bool hasAnyDirtyBit() const;

    void setBaseLevel(GLuint baseLevel) override;

    void setMinFilter(GLenum filter);
    void setMagFilter(GLenum filter);

    void setSwizzle(GLint swizzle[4]);

  private:
    void setImageHelper(GLenum target,
                        size_t level,
                        GLenum internalFormat,
                        const gl::Extents &size,
                        GLenum format,
                        GLenum type,
                        const uint8_t *pixels);
    // This changes the current pixel unpack state that will have to be reapplied.
    void reserveTexImageToBeFilled(GLenum target,
                                   size_t level,
                                   GLenum internalFormat,
                                   const gl::Extents &size,
                                   GLenum format,
                                   GLenum type);
    gl::Error setSubImageRowByRowWorkaround(GLenum target,
                                            size_t level,
                                            const gl::Box &area,
                                            GLenum format,
                                            GLenum type,
                                            const gl::PixelUnpackState &unpack,
                                            const uint8_t *pixels);

    gl::Error setSubImagePaddingWorkaround(GLenum target,
                                           size_t level,
                                           const gl::Box &area,
                                           GLenum format,
                                           GLenum type,
                                           const gl::PixelUnpackState &unpack,
                                           const uint8_t *pixels);

    void syncTextureStateSwizzle(const FunctionsGL *functions,
                                 GLenum name,
                                 GLenum value,
                                 GLenum *outValue);

    void setLevelInfo(size_t level, size_t levelCount, const LevelInfoGL &levelInfo);

    const FunctionsGL *mFunctions;
    const WorkaroundsGL &mWorkarounds;
    StateManagerGL *mStateManager;
    BlitGL *mBlitter;

    std::vector<LevelInfoGL> mLevelInfo;
    gl::Texture::DirtyBits mLocalDirtyBits;

    gl::SwizzleState mAppliedSwizzle;
    gl::SamplerState mAppliedSampler;
    GLuint mAppliedBaseLevel;
    GLuint mAppliedMaxLevel;

    GLuint mTextureID;
};

}

#endif // LIBANGLE_RENDERER_GL_TEXTUREGL_H_
