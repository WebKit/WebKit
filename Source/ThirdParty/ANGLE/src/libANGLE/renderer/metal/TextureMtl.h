//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureMtl.h:
//    Defines the class interface for TextureMtl, implementing TextureImpl.
//

#ifndef LIBANGLE_RENDERER_METAL_TEXTUREMTL_H_
#define LIBANGLE_RENDERER_METAL_TEXTUREMTL_H_

#include <map>

#include "common/PackedEnums.h"
#include "libANGLE/renderer/TextureImpl.h"
#include "libANGLE/renderer/metal/RenderTargetMtl.h"
#include "libANGLE/renderer/metal/mtl_command_buffer.h"
#include "libANGLE/renderer/metal/mtl_resources.h"

namespace rx
{

class TextureMtl : public TextureImpl
{
  public:
    TextureMtl(const gl::TextureState &state);
    ~TextureMtl() override;
    void onDestroy(const gl::Context *context) override;

    angle::Result setImage(const gl::Context *context,
                           const gl::ImageIndex &index,
                           GLenum internalFormat,
                           const gl::Extents &size,
                           GLenum format,
                           GLenum type,
                           const gl::PixelUnpackState &unpack,
                           gl::Buffer *unpackBuffer,
                           const uint8_t *pixels) override;
    angle::Result setSubImage(const gl::Context *context,
                              const gl::ImageIndex &index,
                              const gl::Box &area,
                              GLenum format,
                              GLenum type,
                              const gl::PixelUnpackState &unpack,
                              gl::Buffer *unpackBuffer,
                              const uint8_t *pixels) override;

    angle::Result setCompressedImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     GLenum internalFormat,
                                     const gl::Extents &size,
                                     const gl::PixelUnpackState &unpack,
                                     size_t imageSize,
                                     const uint8_t *pixels) override;
    angle::Result setCompressedSubImage(const gl::Context *context,
                                        const gl::ImageIndex &index,
                                        const gl::Box &area,
                                        GLenum format,
                                        const gl::PixelUnpackState &unpack,
                                        size_t imageSize,
                                        const uint8_t *pixels) override;

    angle::Result copyImage(const gl::Context *context,
                            const gl::ImageIndex &index,
                            const gl::Rectangle &sourceArea,
                            GLenum internalFormat,
                            gl::Framebuffer *source) override;
    angle::Result copySubImage(const gl::Context *context,
                               const gl::ImageIndex &index,
                               const gl::Offset &destOffset,
                               const gl::Rectangle &sourceArea,
                               gl::Framebuffer *source) override;

    angle::Result copyTexture(const gl::Context *context,
                              const gl::ImageIndex &index,
                              GLenum internalFormat,
                              GLenum type,
                              size_t sourceLevel,
                              bool unpackFlipY,
                              bool unpackPremultiplyAlpha,
                              bool unpackUnmultiplyAlpha,
                              const gl::Texture *source) override;
    angle::Result copySubTexture(const gl::Context *context,
                                 const gl::ImageIndex &index,
                                 const gl::Offset &destOffset,
                                 size_t sourceLevel,
                                 const gl::Box &sourceBox,
                                 bool unpackFlipY,
                                 bool unpackPremultiplyAlpha,
                                 bool unpackUnmultiplyAlpha,
                                 const gl::Texture *source) override;

    angle::Result copyCompressedTexture(const gl::Context *context,
                                        const gl::Texture *source) override;

    angle::Result setStorage(const gl::Context *context,
                             gl::TextureType type,
                             size_t levels,
                             GLenum internalFormat,
                             const gl::Extents &size) override;

    angle::Result setStorageExternalMemory(const gl::Context *context,
                                           gl::TextureType type,
                                           size_t levels,
                                           GLenum internalFormat,
                                           const gl::Extents &size,
                                           gl::MemoryObject *memoryObject,
                                           GLuint64 offset) override;

    angle::Result setEGLImageTarget(const gl::Context *context,
                                    gl::TextureType type,
                                    egl::Image *image) override;

    angle::Result setImageExternal(const gl::Context *context,
                                   gl::TextureType type,
                                   egl::Stream *stream,
                                   const egl::Stream::GLTextureDescription &desc) override;

    angle::Result generateMipmap(const gl::Context *context) override;

    angle::Result setBaseLevel(const gl::Context *context, GLuint baseLevel) override;

    angle::Result bindTexImage(const gl::Context *context, egl::Surface *surface) override;
    angle::Result releaseTexImage(const gl::Context *context) override;

    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            GLsizei samples,
                                            FramebufferAttachmentRenderTarget **rtOut) override;

    angle::Result syncState(const gl::Context *context,
                            const gl::Texture::DirtyBits &dirtyBits,
                            gl::TextureCommand source) override;

    angle::Result setStorageMultisample(const gl::Context *context,
                                        gl::TextureType type,
                                        GLsizei samples,
                                        GLint internalformat,
                                        const gl::Extents &size,
                                        bool fixedSampleLocations) override;

    angle::Result initializeContents(const gl::Context *context,
                                     const gl::ImageIndex &imageIndex) override;

    // The texture's data is initially initialized and stored in an array
    // of images through glTexImage*/glCopyTex* calls. During draw calls, the caller must make sure
    // the actual texture is created by calling this method to transfer the stored images data
    // to the actual texture.
    angle::Result ensureTextureCreated(const gl::Context *context);

    angle::Result bindToShader(const gl::Context *context,
                               mtl::RenderCommandEncoder *cmdEncoder,
                               gl::ShaderType shaderType,
                               int textureSlotIndex,
                               int samplerSlotIndex);

    const mtl::Format &getFormat() const { return mFormat; }

  private:
    void releaseTexture(bool releaseImages);
    angle::Result ensureSamplerStateCreated(const gl::Context *context);
    // Ensure image at given index is created:
    angle::Result ensureImageCreated(const gl::Context *context, const gl::ImageIndex &index);
    angle::Result checkForEmulatedChannels(const gl::Context *context,
                                           const mtl::Format &mtlFormat,
                                           const mtl::TextureRef &texture);

    // If levels = 0, this function will create full mipmaps texture.
    angle::Result setStorageImpl(const gl::Context *context,
                                 gl::TextureType type,
                                 size_t levels,
                                 const mtl::Format &mtlFormat,
                                 const gl::Extents &size);

    angle::Result redefineImage(const gl::Context *context,
                                const gl::ImageIndex &index,
                                const mtl::Format &mtlFormat,
                                const gl::Extents &size);

    angle::Result setImageImpl(const gl::Context *context,
                               const gl::ImageIndex &index,
                               const gl::InternalFormat &formatInfo,
                               const gl::Extents &size,
                               GLenum type,
                               const gl::PixelUnpackState &unpack,
                               const uint8_t *pixels);
    angle::Result setSubImageImpl(const gl::Context *context,
                                  const gl::ImageIndex &index,
                                  const gl::Box &area,
                                  const gl::InternalFormat &formatInfo,
                                  GLenum type,
                                  const gl::PixelUnpackState &unpack,
                                  const uint8_t *pixels);

    angle::Result copySubImageImpl(const gl::Context *context,
                                   const gl::ImageIndex &index,
                                   const gl::Offset &destOffset,
                                   const gl::Rectangle &sourceArea,
                                   const gl::InternalFormat &internalFormat,
                                   gl::Framebuffer *source);
    angle::Result copySubImageWithDraw(const gl::Context *context,
                                       const gl::ImageIndex &index,
                                       const gl::Offset &destOffset,
                                       const gl::Rectangle &sourceArea,
                                       const gl::InternalFormat &internalFormat,
                                       gl::Framebuffer *source);
    angle::Result copySubImageCPU(const gl::Context *context,
                                  const gl::ImageIndex &index,
                                  const gl::Offset &destOffset,
                                  const gl::Rectangle &sourceArea,
                                  const gl::InternalFormat &internalFormat,
                                  gl::Framebuffer *source);

    // Convert pixels to suported format before uploading to texture
    angle::Result convertAndSetSubImage(const gl::Context *context,
                                        const gl::ImageIndex &index,
                                        const MTLRegion &mtlArea,
                                        const gl::InternalFormat &internalFormat,
                                        const angle::Format &pixelsFormat,
                                        size_t pixelsRowPitch,
                                        const uint8_t *pixels);

    angle::Result generateMipmapCPU(const gl::Context *context);

    mtl::Format mFormat;
    // The real texture used by Metal draw calls.
    mtl::TextureRef mNativeTexture;
    id<MTLSamplerState> mMetalSamplerState = nil;

    std::vector<RenderTargetMtl> mLayeredRenderTargets;
    std::vector<mtl::TextureRef> mLayeredTextureViews;

    // Stored images array defined by glTexImage/glCopy*.
    // Once the images array is complete, they will be transferred to real texture object.
    std::map<int, gl::TexLevelArray<mtl::TextureRef>> mTexImages;

    bool mIsPow2 = false;
};

}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_TEXTUREMTL_H_ */
