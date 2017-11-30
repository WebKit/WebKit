//
// Copyright (c) 2012-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureStorage11.h: Defines the abstract rx::TextureStorage11 class and its concrete derived
// classes TextureStorage11_2D and TextureStorage11_Cube, which act as the interface to the D3D11 texture.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_TEXTURESTORAGE11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_TEXTURESTORAGE11_H_

#include "libANGLE/Error.h"
#include "libANGLE/Texture.h"
#include "libANGLE/renderer/d3d/TextureStorage.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/texture_format_table.h"

#include <array>
#include <map>

namespace gl
{
struct ImageIndex;
}

namespace rx
{
class EGLImageD3D;
class RenderTargetD3D;
class RenderTarget11;
class Renderer11;
class SwapChain11;
class Image11;
struct Renderer11DeviceCaps;

template <typename T>
using TexLevelArray = std::array<T, gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS>;

template <typename T>
using CubeFaceArray = std::array<T, gl::CUBE_FACE_COUNT>;

class TextureStorage11 : public TextureStorage
{
  public:
    ~TextureStorage11() override;

    static DWORD GetTextureBindFlags(GLenum internalFormat, const Renderer11DeviceCaps &renderer11DeviceCaps, bool renderTarget);
    static DWORD GetTextureMiscFlags(GLenum internalFormat, const Renderer11DeviceCaps &renderer11DeviceCaps, bool renderTarget, int levels);

    UINT getBindFlags() const;
    UINT getMiscFlags() const;
    const d3d11::Format &getFormatSet() const;
    gl::Error getSRVLevels(const gl::Context *context,
                           GLint baseLevel,
                           GLint maxLevel,
                           const d3d11::SharedSRV **outSRV);
    gl::Error generateSwizzles(const gl::Context *context, const gl::SwizzleState &swizzleTarget);
    void markLevelDirty(int mipLevel);
    void markDirty();

    gl::Error updateSubresourceLevel(const gl::Context *context,
                                     const TextureHelper11 &texture,
                                     unsigned int sourceSubresource,
                                     const gl::ImageIndex &index,
                                     const gl::Box &copyArea);

    gl::Error copySubresourceLevel(const gl::Context *context,
                                   const TextureHelper11 &dstTexture,
                                   unsigned int dstSubresource,
                                   const gl::ImageIndex &index,
                                   const gl::Box &region);

    // TextureStorage virtual functions
    int getTopLevel() const override;
    bool isRenderTarget() const override;
    bool isManaged() const override;
    bool supportsNativeMipmapFunction() const override;
    int getLevelCount() const override;
    gl::Error generateMipmap(const gl::Context *context,
                             const gl::ImageIndex &sourceIndex,
                             const gl::ImageIndex &destIndex) override;
    gl::Error copyToStorage(const gl::Context *context, TextureStorage *destStorage) override;
    gl::Error setData(const gl::Context *context,
                      const gl::ImageIndex &index,
                      ImageD3D *image,
                      const gl::Box *destBox,
                      GLenum type,
                      const gl::PixelUnpackState &unpack,
                      const uint8_t *pixelData) override;

    virtual gl::Error getSRV(const gl::Context *context,
                             const gl::TextureState &textureState,
                             const d3d11::SharedSRV **outSRV);
    virtual UINT getSubresourceIndex(const gl::ImageIndex &index) const;
    virtual gl::Error getResource(const gl::Context *context,
                                  const TextureHelper11 **outResource) = 0;
    virtual void associateImage(Image11* image, const gl::ImageIndex &index) = 0;
    virtual void disassociateImage(const gl::ImageIndex &index, Image11* expectedImage) = 0;
    virtual void verifyAssociatedImageValid(const gl::ImageIndex &index,
                                            Image11 *expectedImage) = 0;
    virtual gl::Error releaseAssociatedImage(const gl::Context *context,
                                             const gl::ImageIndex &index,
                                             Image11 *incomingImage) = 0;

  protected:
    TextureStorage11(Renderer11 *renderer, UINT bindFlags, UINT miscFlags, GLenum internalFormat);
    int getLevelWidth(int mipLevel) const;
    int getLevelHeight(int mipLevel) const;
    int getLevelDepth(int mipLevel) const;

    // Some classes (e.g. TextureStorage11_2D) will override getMippedResource.
    virtual gl::Error getMippedResource(const gl::Context *context,
                                        const TextureHelper11 **outResource);

    virtual gl::Error getSwizzleTexture(const TextureHelper11 **outTexture) = 0;
    virtual gl::Error getSwizzleRenderTarget(int mipLevel,
                                             const d3d11::RenderTargetView **outRTV) = 0;
    gl::Error getSRVLevel(const gl::Context *context,
                          int mipLevel,
                          bool blitSRV,
                          const d3d11::SharedSRV **outSRV);

    // Get a version of a depth texture with only depth information, not stencil.
    enum DropStencil
    {
        CREATED,
        ALREADY_EXISTS
    };
    virtual gl::ErrorOrResult<DropStencil> ensureDropStencilTexture(const gl::Context *context);
    gl::Error initDropStencilTexture(const gl::Context *context, const gl::ImageIndexIterator &it);

    // The baseLevel parameter should *not* have mTopLevel applied.
    virtual gl::Error createSRV(const gl::Context *context,
                                int baseLevel,
                                int mipLevels,
                                DXGI_FORMAT format,
                                const TextureHelper11 &texture,
                                d3d11::SharedSRV *outSRV) = 0;

    void verifySwizzleExists(const gl::SwizzleState &swizzleState);

    // Clear all cached non-swizzle SRVs and invalidate the swizzle cache.
    void clearSRVCache();

    Renderer11 *mRenderer;
    int mTopLevel;
    unsigned int mMipLevels;

    const d3d11::Format &mFormatInfo;
    unsigned int mTextureWidth;
    unsigned int mTextureHeight;
    unsigned int mTextureDepth;

    TexLevelArray<gl::SwizzleState> mSwizzleCache;
    TextureHelper11 mDropStencilTexture;

  private:
    const UINT mBindFlags;
    const UINT mMiscFlags;

    struct SRVKey
    {
        SRVKey(int baseLevel, int mipLevels, bool swizzle, bool dropStencil);

        bool operator<(const SRVKey &rhs) const;

        int baseLevel    = 0;  // Without mTopLevel applied.
        int mipLevels    = 0;
        bool swizzle     = false;
        bool dropStencil = false;
    };
    typedef std::map<SRVKey, d3d11::SharedSRV> SRVCache;

    gl::Error getCachedOrCreateSRV(const gl::Context *context,
                                   const SRVKey &key,
                                   const d3d11::SharedSRV **outSRV);

    SRVCache mSrvCache;
    TexLevelArray<d3d11::SharedSRV> mLevelSRVs;
    TexLevelArray<d3d11::SharedSRV> mLevelBlitSRVs;
};

class TextureStorage11_2D : public TextureStorage11
{
  public:
    TextureStorage11_2D(Renderer11 *renderer, SwapChain11 *swapchain);
    TextureStorage11_2D(Renderer11 *renderer, GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, int levels, bool hintLevelZeroOnly = false);
    ~TextureStorage11_2D() override;

    gl::Error onDestroy(const gl::Context *context) override;

    gl::Error getResource(const gl::Context *context, const TextureHelper11 **outResource) override;
    gl::Error getMippedResource(const gl::Context *context,
                                const TextureHelper11 **outResource) override;
    gl::Error getRenderTarget(const gl::Context *context,
                              const gl::ImageIndex &index,
                              RenderTargetD3D **outRT) override;

    gl::Error copyToStorage(const gl::Context *context, TextureStorage *destStorage) override;

    void associateImage(Image11 *image, const gl::ImageIndex &index) override;
    void disassociateImage(const gl::ImageIndex &index, Image11 *expectedImage) override;
    void verifyAssociatedImageValid(const gl::ImageIndex &index, Image11 *expectedImage) override;
    gl::Error releaseAssociatedImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     Image11 *incomingImage) override;

    gl::Error useLevelZeroWorkaroundTexture(const gl::Context *context,
                                            bool useLevelZeroTexture) override;

  protected:
    gl::Error getSwizzleTexture(const TextureHelper11 **outTexture) override;
    gl::Error getSwizzleRenderTarget(int mipLevel, const d3d11::RenderTargetView **outRTV) override;

    gl::ErrorOrResult<DropStencil> ensureDropStencilTexture(const gl::Context *context) override;

    gl::Error ensureTextureExists(int mipLevels);

  private:
    gl::Error createSRV(const gl::Context *context,
                        int baseLevel,
                        int mipLevels,
                        DXGI_FORMAT format,
                        const TextureHelper11 &texture,
                        d3d11::SharedSRV *outSRV) override;

    TextureHelper11 mTexture;
    TexLevelArray<std::unique_ptr<RenderTarget11>> mRenderTarget;
    bool mHasKeyedMutex;

    // These are members related to the zero max-LOD workaround.
    // D3D11 Feature Level 9_3 can't disable mipmaps on a mipmapped texture (i.e. solely sample from level zero).
    // These members are used to work around this limitation.
    // Usually only mTexture XOR mLevelZeroTexture will exist.
    // For example, if an app creates a texture with only one level, then 9_3 will only create mLevelZeroTexture.
    // However, in some scenarios, both textures have to be created. This incurs additional memory overhead.
    // One example of this is an application that creates a texture, calls glGenerateMipmap, and then disables mipmaps on the texture.
    // A more likely example is an app that creates an empty texture, renders to it, and then calls glGenerateMipmap
    // TODO: In this rendering scenario, release the mLevelZeroTexture after mTexture has been created to save memory.
    TextureHelper11 mLevelZeroTexture;
    std::unique_ptr<RenderTarget11> mLevelZeroRenderTarget;
    bool mUseLevelZeroTexture;

    // Swizzle-related variables
    TextureHelper11 mSwizzleTexture;
    TexLevelArray<d3d11::RenderTargetView> mSwizzleRenderTargets;

    TexLevelArray<Image11 *> mAssociatedImages;
};

class TextureStorage11_External : public TextureStorage11
{
  public:
    TextureStorage11_External(Renderer11 *renderer,
                              egl::Stream *stream,
                              const egl::Stream::GLTextureDescription &glDesc);
    ~TextureStorage11_External() override;

    gl::Error onDestroy(const gl::Context *context) override;

    gl::Error getResource(const gl::Context *context, const TextureHelper11 **outResource) override;
    gl::Error getMippedResource(const gl::Context *context,
                                const TextureHelper11 **outResource) override;
    gl::Error getRenderTarget(const gl::Context *context,
                              const gl::ImageIndex &index,
                              RenderTargetD3D **outRT) override;

    gl::Error copyToStorage(const gl::Context *context, TextureStorage *destStorage) override;

    void associateImage(Image11 *image, const gl::ImageIndex &index) override;
    void disassociateImage(const gl::ImageIndex &index, Image11 *expectedImage) override;
    void verifyAssociatedImageValid(const gl::ImageIndex &index, Image11 *expectedImage) override;
    gl::Error releaseAssociatedImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     Image11 *incomingImage) override;

  protected:
    gl::Error getSwizzleTexture(const TextureHelper11 **outTexture) override;
    gl::Error getSwizzleRenderTarget(int mipLevel, const d3d11::RenderTargetView **outRTV) override;

  private:
    gl::Error createSRV(const gl::Context *context,
                        int baseLevel,
                        int mipLevels,
                        DXGI_FORMAT format,
                        const TextureHelper11 &texture,
                        d3d11::SharedSRV *outSRV) override;

    TextureHelper11 mTexture;
    int mSubresourceIndex;
    bool mHasKeyedMutex;

    Image11 *mAssociatedImage;
};

class TextureStorage11_EGLImage final : public TextureStorage11
{
  public:
    TextureStorage11_EGLImage(Renderer11 *renderer,
                              EGLImageD3D *eglImage,
                              RenderTarget11 *renderTarget11);
    ~TextureStorage11_EGLImage() override;

    gl::Error getResource(const gl::Context *context, const TextureHelper11 **outResource) override;
    gl::Error getSRV(const gl::Context *context,
                     const gl::TextureState &textureState,
                     const d3d11::SharedSRV **outSRV) override;
    gl::Error getMippedResource(const gl::Context *context,
                                const TextureHelper11 **outResource) override;
    gl::Error getRenderTarget(const gl::Context *context,
                              const gl::ImageIndex &index,
                              RenderTargetD3D **outRT) override;

    gl::Error copyToStorage(const gl::Context *context, TextureStorage *destStorage) override;

    void associateImage(Image11 *image, const gl::ImageIndex &index) override;
    void disassociateImage(const gl::ImageIndex &index, Image11 *expectedImage) override;
    void verifyAssociatedImageValid(const gl::ImageIndex &index, Image11 *expectedImage) override;
    gl::Error releaseAssociatedImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     Image11 *incomingImage) override;

    gl::Error useLevelZeroWorkaroundTexture(const gl::Context *context,
                                            bool useLevelZeroTexture) override;

  protected:
    gl::Error getSwizzleTexture(const TextureHelper11 **outTexture) override;
    gl::Error getSwizzleRenderTarget(int mipLevel, const d3d11::RenderTargetView **outRTV) override;

  private:
    // Check if the EGL image's render target has been updated due to orphaning and delete
    // any SRVs and other resources based on the image's old render target.
    gl::Error checkForUpdatedRenderTarget(const gl::Context *context);

    gl::Error createSRV(const gl::Context *context,
                        int baseLevel,
                        int mipLevels,
                        DXGI_FORMAT format,
                        const TextureHelper11 &texture,
                        d3d11::SharedSRV *outSRV) override;

    gl::Error getImageRenderTarget(const gl::Context *context, RenderTarget11 **outRT) const;

    EGLImageD3D *mImage;
    uintptr_t mCurrentRenderTarget;

    // Swizzle-related variables
    TextureHelper11 mSwizzleTexture;
    std::vector<d3d11::RenderTargetView> mSwizzleRenderTargets;
};

class TextureStorage11_Cube : public TextureStorage11
{
  public:
    TextureStorage11_Cube(Renderer11 *renderer, GLenum internalformat, bool renderTarget, int size, int levels, bool hintLevelZeroOnly);
    ~TextureStorage11_Cube() override;

    gl::Error onDestroy(const gl::Context *context) override;

    UINT getSubresourceIndex(const gl::ImageIndex &index) const override;

    gl::Error getResource(const gl::Context *context, const TextureHelper11 **outResource) override;
    gl::Error getMippedResource(const gl::Context *context,
                                const TextureHelper11 **outResource) override;
    gl::Error getRenderTarget(const gl::Context *context,
                              const gl::ImageIndex &index,
                              RenderTargetD3D **outRT) override;

    gl::Error copyToStorage(const gl::Context *context, TextureStorage *destStorage) override;

    void associateImage(Image11 *image, const gl::ImageIndex &index) override;
    void disassociateImage(const gl::ImageIndex &index, Image11 *expectedImage) override;
    void verifyAssociatedImageValid(const gl::ImageIndex &index, Image11 *expectedImage) override;
    gl::Error releaseAssociatedImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     Image11 *incomingImage) override;

    gl::Error useLevelZeroWorkaroundTexture(const gl::Context *context,
                                            bool useLevelZeroTexture) override;

  protected:
    gl::Error getSwizzleTexture(const TextureHelper11 **outTexture) override;
    gl::Error getSwizzleRenderTarget(int mipLevel, const d3d11::RenderTargetView **outRTV) override;

    gl::ErrorOrResult<DropStencil> ensureDropStencilTexture(const gl::Context *context) override;

    gl::Error ensureTextureExists(int mipLevels);

  private:
    gl::Error createSRV(const gl::Context *context,
                        int baseLevel,
                        int mipLevels,
                        DXGI_FORMAT format,
                        const TextureHelper11 &texture,
                        d3d11::SharedSRV *outSRV) override;
    gl::Error createRenderTargetSRV(const TextureHelper11 &texture,
                                    const gl::ImageIndex &index,
                                    DXGI_FORMAT resourceFormat,
                                    d3d11::SharedSRV *srv) const;

    TextureHelper11 mTexture;
    CubeFaceArray<TexLevelArray<std::unique_ptr<RenderTarget11>>> mRenderTarget;

    // Level-zero workaround members. See TextureStorage11_2D's workaround members for a description.
    TextureHelper11 mLevelZeroTexture;
    CubeFaceArray<std::unique_ptr<RenderTarget11>> mLevelZeroRenderTarget;
    bool mUseLevelZeroTexture;

    TextureHelper11 mSwizzleTexture;
    TexLevelArray<d3d11::RenderTargetView> mSwizzleRenderTargets;

    CubeFaceArray<TexLevelArray<Image11 *>> mAssociatedImages;
};

class TextureStorage11_3D : public TextureStorage11
{
  public:
    TextureStorage11_3D(Renderer11 *renderer, GLenum internalformat, bool renderTarget,
                        GLsizei width, GLsizei height, GLsizei depth, int levels);
    ~TextureStorage11_3D() override;

    gl::Error onDestroy(const gl::Context *context) override;

    gl::Error getResource(const gl::Context *context, const TextureHelper11 **outResource) override;

    // Handles both layer and non-layer RTs
    gl::Error getRenderTarget(const gl::Context *context,
                              const gl::ImageIndex &index,
                              RenderTargetD3D **outRT) override;

    void associateImage(Image11 *image, const gl::ImageIndex &index) override;
    void disassociateImage(const gl::ImageIndex &index, Image11 *expectedImage) override;
    void verifyAssociatedImageValid(const gl::ImageIndex &index, Image11 *expectedImage) override;
    gl::Error releaseAssociatedImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     Image11 *incomingImage) override;

  protected:
    gl::Error getSwizzleTexture(const TextureHelper11 **outTexture) override;
    gl::Error getSwizzleRenderTarget(int mipLevel, const d3d11::RenderTargetView **outRTV) override;

  private:
    gl::Error createSRV(const gl::Context *context,
                        int baseLevel,
                        int mipLevels,
                        DXGI_FORMAT format,
                        const TextureHelper11 &texture,
                        d3d11::SharedSRV *outSRV) override;

    typedef std::pair<int, int> LevelLayerKey;
    std::map<LevelLayerKey, std::unique_ptr<RenderTarget11>> mLevelLayerRenderTargets;

    TexLevelArray<std::unique_ptr<RenderTarget11>> mLevelRenderTargets;

    TextureHelper11 mTexture;
    TextureHelper11 mSwizzleTexture;
    TexLevelArray<d3d11::RenderTargetView> mSwizzleRenderTargets;

    TexLevelArray<Image11 *> mAssociatedImages;
};

class TextureStorage11_2DArray : public TextureStorage11
{
  public:
    TextureStorage11_2DArray(Renderer11 *renderer, GLenum internalformat, bool renderTarget,
                             GLsizei width, GLsizei height, GLsizei depth, int levels);
    ~TextureStorage11_2DArray() override;

    gl::Error onDestroy(const gl::Context *context) override;

    gl::Error getResource(const gl::Context *context, const TextureHelper11 **outResource) override;
    gl::Error getRenderTarget(const gl::Context *context,
                              const gl::ImageIndex &index,
                              RenderTargetD3D **outRT) override;

    void associateImage(Image11 *image, const gl::ImageIndex &index) override;
    void disassociateImage(const gl::ImageIndex &index, Image11 *expectedImage) override;
    void verifyAssociatedImageValid(const gl::ImageIndex &index, Image11 *expectedImage) override;
    gl::Error releaseAssociatedImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     Image11 *incomingImage) override;

  protected:
    gl::Error getSwizzleTexture(const TextureHelper11 **outTexture) override;
    gl::Error getSwizzleRenderTarget(int mipLevel, const d3d11::RenderTargetView **outRTV) override;

    gl::ErrorOrResult<DropStencil> ensureDropStencilTexture(const gl::Context *context) override;

  private:
    struct LevelLayerRangeKey
    {
        LevelLayerRangeKey(int mipLevelIn, int layerIn, int numLayersIn)
            : mipLevel(mipLevelIn), layer(layerIn), numLayers(numLayersIn)
        {
        }
        bool operator<(const LevelLayerRangeKey &other) const
        {
            if (mipLevel != other.mipLevel)
            {
                return mipLevel < other.mipLevel;
            }
            if (layer != other.layer)
            {
                return layer < other.layer;
            }
            return numLayers < other.numLayers;
        }
        int mipLevel;
        int layer;
        int numLayers;
    };

  private:
    gl::Error createSRV(const gl::Context *context,
                        int baseLevel,
                        int mipLevels,
                        DXGI_FORMAT format,
                        const TextureHelper11 &texture,
                        d3d11::SharedSRV *outSRV) override;
    gl::Error createRenderTargetSRV(const TextureHelper11 &texture,
                                    const gl::ImageIndex &index,
                                    DXGI_FORMAT resourceFormat,
                                    d3d11::SharedSRV *srv) const;

    std::map<LevelLayerRangeKey, std::unique_ptr<RenderTarget11>> mRenderTargets;

    TextureHelper11 mTexture;

    TextureHelper11 mSwizzleTexture;
    TexLevelArray<d3d11::RenderTargetView> mSwizzleRenderTargets;

    typedef std::map<LevelLayerRangeKey, Image11 *> ImageMap;
    ImageMap mAssociatedImages;
};

class TextureStorage11_2DMultisample : public TextureStorage11
{
  public:
    TextureStorage11_2DMultisample(Renderer11 *renderer,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   int levels,
                                   int samples,
                                   bool fixedSampleLocations);
    ~TextureStorage11_2DMultisample() override;

    gl::Error onDestroy(const gl::Context *context) override;

    gl::Error getResource(const gl::Context *context, const TextureHelper11 **outResource) override;
    gl::Error getRenderTarget(const gl::Context *context,
                              const gl::ImageIndex &index,
                              RenderTargetD3D **outRT) override;

    gl::Error copyToStorage(const gl::Context *context, TextureStorage *destStorage) override;

    void associateImage(Image11 *image, const gl::ImageIndex &index) override;
    void disassociateImage(const gl::ImageIndex &index, Image11 *expectedImage) override;
    void verifyAssociatedImageValid(const gl::ImageIndex &index, Image11 *expectedImage) override;
    gl::Error releaseAssociatedImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     Image11 *incomingImage) override;

  protected:
    gl::Error getSwizzleTexture(const TextureHelper11 **outTexture) override;
    gl::Error getSwizzleRenderTarget(int mipLevel, const d3d11::RenderTargetView **outRTV) override;

    gl::ErrorOrResult<DropStencil> ensureDropStencilTexture(const gl::Context *context) override;

    gl::Error ensureTextureExists(int mipLevels);

  private:
    gl::Error createSRV(const gl::Context *context,
                        int baseLevel,
                        int mipLevels,
                        DXGI_FORMAT format,
                        const TextureHelper11 &texture,
                        d3d11::SharedSRV *outSRV) override;

    TextureHelper11 mTexture;
    std::unique_ptr<RenderTarget11> mRenderTarget;

    unsigned int mSamples;
    GLboolean mFixedSampleLocations;
};
}

#endif // LIBANGLE_RENDERER_D3D_D3D11_TEXTURESTORAGE11_H_
