//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Texture.h: Defines the gl::Texture class [OpenGL ES 2.0.24] section 3.7 page 63.

#ifndef LIBANGLE_TEXTURE_H_
#define LIBANGLE_TEXTURE_H_

#include <vector>
#include <map>

#include "angle_gl.h"
#include "common/debug.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Error.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/TextureImpl.h"

namespace egl
{
class Surface;
}

namespace gl
{
class Framebuffer;
struct Data;

bool IsMipmapFiltered(const gl::SamplerState &samplerState);

class Texture final : public FramebufferAttachmentObject
{
  public:
    Texture(rx::TextureImpl *impl, GLuint id, GLenum target);

    virtual ~Texture();

    GLenum getTarget() const;

    const SamplerState &getSamplerState() const { return mSamplerState; }
    SamplerState &getSamplerState() { return mSamplerState; }

    void setUsage(GLenum usage);
    GLenum getUsage() const;

    size_t getWidth(GLenum target, size_t level) const;
    size_t getHeight(GLenum target, size_t level) const;
    size_t getDepth(GLenum target, size_t level) const;
    GLenum getInternalFormat(GLenum target, size_t level) const;

    bool isSamplerComplete(const SamplerState &samplerState, const Data &data) const;
    bool isCubeComplete() const;

    virtual Error setImage(GLenum target, size_t level, GLenum internalFormat, const Extents &size, GLenum format, GLenum type,
                           const PixelUnpackState &unpack, const uint8_t *pixels);
    virtual Error setSubImage(GLenum target, size_t level, const Box &area, GLenum format, GLenum type,
                              const PixelUnpackState &unpack, const uint8_t *pixels);

    virtual Error setCompressedImage(GLenum target, size_t level, GLenum internalFormat, const Extents &size,
                                     const PixelUnpackState &unpack, const uint8_t *pixels);
    virtual Error setCompressedSubImage(GLenum target, size_t level, const Box &area, GLenum format,
                                        const PixelUnpackState &unpack, const uint8_t *pixels);

    virtual Error copyImage(GLenum target, size_t level, const Rectangle &sourceArea, GLenum internalFormat,
                            const Framebuffer *source);
    virtual Error copySubImage(GLenum target, size_t level, const Offset &destOffset, const Rectangle &sourceArea,
                              const Framebuffer *source);

    virtual Error setStorage(GLenum target, size_t levels, GLenum internalFormat, const Extents &size);

    virtual Error generateMipmaps();

    // Texture serials provide a unique way of identifying a Texture that isn't a raw pointer.
    // "id" is not good enough, as Textures can be deleted, then re-allocated with the same id.
    unsigned int getTextureSerial() const;

    bool isImmutable() const;
    GLsizei immutableLevelCount();

    void bindTexImage(egl::Surface *surface);
    void releaseTexImage();

    rx::TextureImpl *getImplementation() { return mTexture; }
    const rx::TextureImpl *getImplementation() const { return mTexture; }

    static const GLuint INCOMPLETE_TEXTURE_ID = static_cast<GLuint>(-1);   // Every texture takes an id at creation time. The value is arbitrary because it is never registered with the resource manager.

    // FramebufferAttachmentObject implementation
    GLsizei getAttachmentWidth(const FramebufferAttachment::Target &target) const override;
    GLsizei getAttachmentHeight(const FramebufferAttachment::Target &target) const override;
    GLenum getAttachmentInternalFormat(const FramebufferAttachment::Target &target) const override;
    GLsizei getAttachmentSamples(const FramebufferAttachment::Target &target) const override;

  private:
    rx::FramebufferAttachmentObjectImpl *getAttachmentImpl() const override { return mTexture; }
    static unsigned int issueTextureSerial();

    rx::TextureImpl *mTexture;

    SamplerState mSamplerState;
    GLenum mUsage;

    GLsizei mImmutableLevelCount;

    GLenum mTarget;


    struct ImageDesc
    {
        Extents size;
        GLenum internalFormat;

        ImageDesc();
        ImageDesc(const Extents &size, GLenum internalFormat);
    };

    const unsigned int mTextureSerial;
    static unsigned int mCurrentTextureSerial;

    GLenum getBaseImageTarget() const;
    size_t getExpectedMipLevels() const;

    bool computeSamplerCompleteness(const SamplerState &samplerState, const Data &data) const;
    bool computeMipmapCompleteness(const gl::SamplerState &samplerState) const;
    bool computeLevelCompleteness(GLenum target, size_t level, const gl::SamplerState &samplerState) const;

    const ImageDesc &getImageDesc(GLenum target, size_t level) const;
    void setImageDesc(GLenum target, size_t level, const ImageDesc &desc);
    void setImageDescChain(size_t levels, Extents baseSize, GLenum sizedInternalFormat);
    void clearImageDesc(GLenum target, size_t level);
    void clearImageDescs();

    std::vector<ImageDesc> mImageDescs;

    struct SamplerCompletenessCache
    {
        SamplerCompletenessCache();

        bool cacheValid;

        // All values that affect sampler completeness that are not stored within
        // the texture itself
        SamplerState samplerState;
        bool filterable;
        GLint clientVersion;
        bool supportsNPOT;

        // Result of the sampler completeness with the above parameters
        bool samplerComplete;
    };
    mutable SamplerCompletenessCache mCompletenessCache;

    egl::Surface *mBoundSurface;
};

}

#endif   // LIBANGLE_TEXTURE_H_
