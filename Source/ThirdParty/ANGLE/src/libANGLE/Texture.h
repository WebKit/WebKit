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
#include "common/Optional.h"
#include "common/debug.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Image.h"
#include "libANGLE/Stream.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"

namespace egl
{
class Surface;
class Stream;
}

namespace rx
{
class GLImplFactory;
class TextureImpl;
class TextureGL;
}

namespace gl
{
class ContextState;
class Framebuffer;
class Sampler;
class Texture;

bool IsMipmapFiltered(const SamplerState &samplerState);

struct ImageDesc final
{
    ImageDesc();
    ImageDesc(const Extents &size, const Format &format, const InitState initState);
    ImageDesc(const Extents &size,
              const Format &format,
              const GLsizei samples,
              const bool fixedSampleLocations,
              const InitState initState);

    ImageDesc(const ImageDesc &other) = default;
    ImageDesc &operator=(const ImageDesc &other) = default;

    Extents size;
    Format format;
    GLsizei samples;
    bool fixedSampleLocations;

    // Needed for robust resource initialization.
    InitState initState;
};

struct SwizzleState final
{
    SwizzleState();
    SwizzleState(GLenum red, GLenum green, GLenum blue, GLenum alpha);
    SwizzleState(const SwizzleState &other) = default;
    SwizzleState &operator=(const SwizzleState &other) = default;

    bool swizzleRequired() const;

    bool operator==(const SwizzleState &other) const;
    bool operator!=(const SwizzleState &other) const;

    GLenum swizzleRed;
    GLenum swizzleGreen;
    GLenum swizzleBlue;
    GLenum swizzleAlpha;
};

// State from Table 6.9 (state per texture object) in the OpenGL ES 3.0.2 spec.
struct TextureState final : private angle::NonCopyable
{
    TextureState(GLenum target);
    ~TextureState();

    bool swizzleRequired() const;
    GLuint getEffectiveBaseLevel() const;
    GLuint getEffectiveMaxLevel() const;

    // Returns the value called "q" in the GLES 3.0.4 spec section 3.8.10.
    GLuint getMipmapMaxLevel() const;

    // Returns true if base level changed.
    bool setBaseLevel(GLuint baseLevel);
    bool setMaxLevel(GLuint maxLevel);

    bool isCubeComplete() const;

    const ImageDesc &getImageDesc(GLenum target, size_t level) const;
    const ImageDesc &getImageDesc(const ImageIndex &imageIndex) const;

    GLenum getTarget() const { return mTarget; }
    const SwizzleState &getSwizzleState() const { return mSwizzleState; }
    const SamplerState &getSamplerState() const { return mSamplerState; }
    GLenum getUsage() const { return mUsage; }

  private:
    // Texture needs access to the ImageDesc functions.
    friend class Texture;
    // TODO(jmadill): Remove TextureGL from friends.
    friend class rx::TextureGL;
    friend bool operator==(const TextureState &a, const TextureState &b);

    bool computeSamplerCompleteness(const SamplerState &samplerState,
                                    const ContextState &data) const;
    bool computeMipmapCompleteness() const;
    bool computeLevelCompleteness(GLenum target, size_t level) const;

    GLenum getBaseImageTarget() const;

    void setImageDesc(GLenum target, size_t level, const ImageDesc &desc);
    void setImageDescChain(GLuint baselevel,
                           GLuint maxLevel,
                           Extents baseSize,
                           const Format &format,
                           InitState initState);
    void setImageDescChainMultisample(Extents baseSize,
                                      const Format &format,
                                      GLsizei samples,
                                      bool fixedSampleLocations,
                                      InitState initState);

    void clearImageDesc(GLenum target, size_t level);
    void clearImageDescs();

    const GLenum mTarget;

    SwizzleState mSwizzleState;

    SamplerState mSamplerState;

    GLuint mBaseLevel;
    GLuint mMaxLevel;

    GLenum mDepthStencilTextureMode;

    bool mImmutableFormat;
    GLuint mImmutableLevels;

    // From GL_ANGLE_texture_usage
    GLenum mUsage;

    std::vector<ImageDesc> mImageDescs;
    InitState mInitState;
};

bool operator==(const TextureState &a, const TextureState &b);
bool operator!=(const TextureState &a, const TextureState &b);

class Texture final : public egl::ImageSibling,
                      public LabeledObject
{
  public:
    Texture(rx::GLImplFactory *factory, GLuint id, GLenum target);
    ~Texture() override;

    Error onDestroy(const Context *context) override;

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    GLenum getTarget() const;

    void setSwizzleRed(GLenum swizzleRed);
    GLenum getSwizzleRed() const;

    void setSwizzleGreen(GLenum swizzleGreen);
    GLenum getSwizzleGreen() const;

    void setSwizzleBlue(GLenum swizzleBlue);
    GLenum getSwizzleBlue() const;

    void setSwizzleAlpha(GLenum swizzleAlpha);
    GLenum getSwizzleAlpha() const;

    void setMinFilter(GLenum minFilter);
    GLenum getMinFilter() const;

    void setMagFilter(GLenum magFilter);
    GLenum getMagFilter() const;

    void setWrapS(GLenum wrapS);
    GLenum getWrapS() const;

    void setWrapT(GLenum wrapT);
    GLenum getWrapT() const;

    void setWrapR(GLenum wrapR);
    GLenum getWrapR() const;

    void setMaxAnisotropy(float maxAnisotropy);
    float getMaxAnisotropy() const;

    void setMinLod(GLfloat minLod);
    GLfloat getMinLod() const;

    void setMaxLod(GLfloat maxLod);
    GLfloat getMaxLod() const;

    void setCompareMode(GLenum compareMode);
    GLenum getCompareMode() const;

    void setCompareFunc(GLenum compareFunc);
    GLenum getCompareFunc() const;

    void setSRGBDecode(GLenum sRGBDecode);
    GLenum getSRGBDecode() const;

    const SamplerState &getSamplerState() const;

    gl::Error setBaseLevel(const Context *context, GLuint baseLevel);
    GLuint getBaseLevel() const;

    void setMaxLevel(GLuint maxLevel);
    GLuint getMaxLevel() const;

    void setDepthStencilTextureMode(GLenum mode);
    GLenum getDepthStencilTextureMode() const;

    bool getImmutableFormat() const;

    GLuint getImmutableLevels() const;

    void setUsage(GLenum usage);
    GLenum getUsage() const;

    const TextureState &getTextureState() const;

    size_t getWidth(GLenum target, size_t level) const;
    size_t getHeight(GLenum target, size_t level) const;
    size_t getDepth(GLenum target, size_t level) const;
    GLsizei getSamples(GLenum target, size_t level) const;
    bool getFixedSampleLocations(GLenum target, size_t level) const;
    const Format &getFormat(GLenum target, size_t level) const;

    // Returns the value called "q" in the GLES 3.0.4 spec section 3.8.10.
    GLuint getMipmapMaxLevel() const;

    bool isMipmapComplete() const;

    Error setImage(const Context *context,
                   const PixelUnpackState &unpackState,
                   GLenum target,
                   size_t level,
                   GLenum internalFormat,
                   const Extents &size,
                   GLenum format,
                   GLenum type,
                   const uint8_t *pixels);
    Error setSubImage(const Context *context,
                      const PixelUnpackState &unpackState,
                      GLenum target,
                      size_t level,
                      const Box &area,
                      GLenum format,
                      GLenum type,
                      const uint8_t *pixels);

    Error setCompressedImage(const Context *context,
                             const PixelUnpackState &unpackState,
                             GLenum target,
                             size_t level,
                             GLenum internalFormat,
                             const Extents &size,
                             size_t imageSize,
                             const uint8_t *pixels);
    Error setCompressedSubImage(const Context *context,
                                const PixelUnpackState &unpackState,
                                GLenum target,
                                size_t level,
                                const Box &area,
                                GLenum format,
                                size_t imageSize,
                                const uint8_t *pixels);

    Error copyImage(const Context *context,
                    GLenum target,
                    size_t level,
                    const Rectangle &sourceArea,
                    GLenum internalFormat,
                    Framebuffer *source);
    Error copySubImage(const Context *context,
                       GLenum target,
                       size_t level,
                       const Offset &destOffset,
                       const Rectangle &sourceArea,
                       Framebuffer *source);

    Error copyTexture(const Context *context,
                      GLenum target,
                      size_t level,
                      GLenum internalFormat,
                      GLenum type,
                      size_t sourceLevel,
                      bool unpackFlipY,
                      bool unpackPremultiplyAlpha,
                      bool unpackUnmultiplyAlpha,
                      Texture *source);
    Error copySubTexture(const Context *context,
                         GLenum target,
                         size_t level,
                         const Offset &destOffset,
                         size_t sourceLevel,
                         const Rectangle &sourceArea,
                         bool unpackFlipY,
                         bool unpackPremultiplyAlpha,
                         bool unpackUnmultiplyAlpha,
                         Texture *source);
    Error copyCompressedTexture(const Context *context, const Texture *source);

    Error setStorage(const Context *context,
                     GLenum target,
                     GLsizei levels,
                     GLenum internalFormat,
                     const Extents &size);

    Error setStorageMultisample(const Context *context,
                                GLenum target,
                                GLsizei samples,
                                GLint internalformat,
                                const Extents &size,
                                bool fixedSampleLocations);

    Error setEGLImageTarget(const Context *context, GLenum target, egl::Image *imageTarget);

    Error generateMipmap(const Context *context);

    egl::Surface *getBoundSurface() const;
    egl::Stream *getBoundStream() const;

    void signalDirty(InitState initState) const;

    bool isSamplerComplete(const Context *context, const Sampler *optionalSampler);

    rx::TextureImpl *getImplementation() const { return mTexture; }

    // FramebufferAttachmentObject implementation
    Extents getAttachmentSize(const ImageIndex &imageIndex) const override;
    const Format &getAttachmentFormat(GLenum binding, const ImageIndex &imageIndex) const override;
    GLsizei getAttachmentSamples(const ImageIndex &imageIndex) const override;

    void onAttach(const Context *context) override;
    void onDetach(const Context *context) override;
    GLuint getId() const override;

    // Needed for robust resource init.
    Error ensureInitialized(const Context *context);
    InitState initState(const ImageIndex &imageIndex) const override;
    InitState initState() const;
    void setInitState(const ImageIndex &imageIndex, InitState initState) override;

    enum DirtyBitType
    {
        // Sampler state
        DIRTY_BIT_MIN_FILTER,
        DIRTY_BIT_MAG_FILTER,
        DIRTY_BIT_WRAP_S,
        DIRTY_BIT_WRAP_T,
        DIRTY_BIT_WRAP_R,
        DIRTY_BIT_MAX_ANISOTROPY,
        DIRTY_BIT_MIN_LOD,
        DIRTY_BIT_MAX_LOD,
        DIRTY_BIT_COMPARE_MODE,
        DIRTY_BIT_COMPARE_FUNC,
        DIRTY_BIT_SRGB_DECODE,

        // Texture state
        DIRTY_BIT_SWIZZLE_RED,
        DIRTY_BIT_SWIZZLE_GREEN,
        DIRTY_BIT_SWIZZLE_BLUE,
        DIRTY_BIT_SWIZZLE_ALPHA,
        DIRTY_BIT_BASE_LEVEL,
        DIRTY_BIT_MAX_LEVEL,

        // Misc
        DIRTY_BIT_LABEL,
        DIRTY_BIT_USAGE,

        DIRTY_BIT_COUNT,
    };
    using DirtyBits = angle::BitSet<DIRTY_BIT_COUNT>;

    void syncState();
    bool hasAnyDirtyBit() const { return mDirtyBits.any(); }

  private:
    rx::FramebufferAttachmentObjectImpl *getAttachmentImpl() const override;

    // ANGLE-only method, used internally
    friend class egl::Surface;
    Error bindTexImageFromSurface(const Context *context, egl::Surface *surface);
    Error releaseTexImageFromSurface(const Context *context);

    // ANGLE-only methods, used internally
    friend class egl::Stream;
    void bindStream(egl::Stream *stream);
    void releaseStream();
    Error acquireImageFromStream(const Context *context,
                                 const egl::Stream::GLTextureDescription &desc);
    Error releaseImageFromStream(const Context *context);

    void invalidateCompletenessCache() const;
    Error releaseTexImageInternal(const Context *context);

    Error ensureSubImageInitialized(const Context *context,
                                    GLenum target,
                                    size_t level,
                                    const gl::Box &area);

    TextureState mState;
    DirtyBits mDirtyBits;
    rx::TextureImpl *mTexture;

    std::string mLabel;

    egl::Surface *mBoundSurface;
    egl::Stream *mBoundStream;

    struct SamplerCompletenessCache
    {
        SamplerCompletenessCache();

        // Context used to generate this cache entry
        ContextID context;

        // All values that affect sampler completeness that are not stored within
        // the texture itself
        SamplerState samplerState;

        // Result of the sampler completeness with the above parameters
        bool samplerComplete;
    };

    mutable SamplerCompletenessCache mCompletenessCache;
};

inline bool operator==(const TextureState &a, const TextureState &b)
{
    return a.mSwizzleState == b.mSwizzleState && a.mSamplerState == b.mSamplerState &&
           a.mBaseLevel == b.mBaseLevel && a.mMaxLevel == b.mMaxLevel &&
           a.mImmutableFormat == b.mImmutableFormat && a.mImmutableLevels == b.mImmutableLevels &&
           a.mUsage == b.mUsage;
}

inline bool operator!=(const TextureState &a, const TextureState &b)
{
    return !(a == b);
}
}  // namespace gl

#endif  // LIBANGLE_TEXTURE_H_
