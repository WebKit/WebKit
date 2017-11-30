
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererD3D.h: Defines a back-end specific class for the DirectX renderer.

#ifndef LIBANGLE_RENDERER_D3D_RENDERERD3D_H_
#define LIBANGLE_RENDERER_D3D_RENDERERD3D_H_

#include <array>

#include "common/Color.h"
#include "common/MemoryBuffer.h"
#include "common/debug.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/Device.h"
#include "libANGLE/Version.h"
#include "libANGLE/WorkerThread.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/VertexDataManager.h"
#include "libANGLE/renderer/d3d/formatutilsD3D.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "platform/WorkaroundsD3D.h"

namespace egl
{
class ConfigSet;
}

namespace gl
{
class FramebufferState;
class InfoLog;
class Texture;
struct LinkedVarying;
}

namespace rx
{
class ContextImpl;
struct D3DUniform;
struct D3DVarying;
class DeviceD3D;
class EGLImageD3D;
class FramebufferImpl;
class ImageD3D;
class IndexBuffer;
class NativeWindowD3D;
class ProgramD3D;
class RenderTargetD3D;
class ShaderExecutableD3D;
class SwapChainD3D;
class TextureStorage;
struct TranslatedIndexData;
class UniformStorageD3D;
class VertexBuffer;

struct DeviceIdentifier
{
    UINT VendorId;
    UINT DeviceId;
    UINT SubSysId;
    UINT Revision;
    UINT FeatureLevel;
};

enum RendererClass
{
    RENDERER_D3D11,
    RENDERER_D3D9
};

// Useful for unit testing
class BufferFactoryD3D : angle::NonCopyable
{
  public:
    BufferFactoryD3D() {}
    virtual ~BufferFactoryD3D() {}

    virtual VertexBuffer *createVertexBuffer() = 0;
    virtual IndexBuffer *createIndexBuffer() = 0;

    // TODO(jmadill): add VertexFormatCaps
    virtual VertexConversionType getVertexConversionType(gl::VertexFormatType vertexFormatType) const = 0;
    virtual GLenum getVertexComponentType(gl::VertexFormatType vertexFormatType) const = 0;

    // Warning: you should ensure binding really matches attrib.bindingIndex before using this
    // function.
    virtual gl::ErrorOrResult<unsigned int> getVertexSpaceRequired(
        const gl::VertexAttribute &attrib,
        const gl::VertexBinding &binding,
        GLsizei count,
        GLsizei instances) const = 0;
};

using AttribIndexArray = std::array<int, gl::MAX_VERTEX_ATTRIBS>;

class RendererD3D : public BufferFactoryD3D, public MultisampleTextureInitializer
{
  public:
    explicit RendererD3D(egl::Display *display);
    ~RendererD3D() override;

    virtual egl::Error initialize() = 0;

    virtual egl::ConfigSet generateConfigs() = 0;
    virtual void generateDisplayExtensions(egl::DisplayExtensions *outExtensions) const = 0;

    virtual ContextImpl *createContext(const gl::ContextState &state) = 0;

    std::string getVendorString() const;

    virtual int getMinorShaderModel() const = 0;
    virtual std::string getShaderModelSuffix() const = 0;

    // Direct3D Specific methods
    virtual DeviceIdentifier getAdapterIdentifier() const = 0;

    virtual bool isValidNativeWindow(EGLNativeWindowType window) const = 0;
    virtual NativeWindowD3D *createNativeWindow(EGLNativeWindowType window,
                                                const egl::Config *config,
                                                const egl::AttributeMap &attribs) const = 0;

    virtual SwapChainD3D *createSwapChain(NativeWindowD3D *nativeWindow,
                                          HANDLE shareHandle,
                                          IUnknown *d3dTexture,
                                          GLenum backBufferFormat,
                                          GLenum depthBufferFormat,
                                          EGLint orientation,
                                          EGLint samples) = 0;
    virtual egl::Error getD3DTextureInfo(const egl::Config *configuration,
                                         IUnknown *d3dTexture,
                                         EGLint *width,
                                         EGLint *height,
                                         GLenum *fboFormat) const = 0;
    virtual egl::Error validateShareHandle(const egl::Config *config,
                                           HANDLE shareHandle,
                                           const egl::AttributeMap &attribs) const = 0;

    virtual int getMajorShaderModel() const = 0;

    const angle::WorkaroundsD3D &getWorkarounds() const;

    // Pixel operations
    virtual gl::Error copyImage2D(const gl::Context *context,
                                  const gl::Framebuffer *framebuffer,
                                  const gl::Rectangle &sourceRect,
                                  GLenum destFormat,
                                  const gl::Offset &destOffset,
                                  TextureStorage *storage,
                                  GLint level) = 0;
    virtual gl::Error copyImageCube(const gl::Context *context,
                                    const gl::Framebuffer *framebuffer,
                                    const gl::Rectangle &sourceRect,
                                    GLenum destFormat,
                                    const gl::Offset &destOffset,
                                    TextureStorage *storage,
                                    GLenum target,
                                    GLint level) = 0;
    virtual gl::Error copyImage3D(const gl::Context *context,
                                  const gl::Framebuffer *framebuffer,
                                  const gl::Rectangle &sourceRect,
                                  GLenum destFormat,
                                  const gl::Offset &destOffset,
                                  TextureStorage *storage,
                                  GLint level) = 0;
    virtual gl::Error copyImage2DArray(const gl::Context *context,
                                       const gl::Framebuffer *framebuffer,
                                       const gl::Rectangle &sourceRect,
                                       GLenum destFormat,
                                       const gl::Offset &destOffset,
                                       TextureStorage *storage,
                                       GLint level) = 0;

    virtual gl::Error copyTexture(const gl::Context *context,
                                  const gl::Texture *source,
                                  GLint sourceLevel,
                                  const gl::Rectangle &sourceRect,
                                  GLenum destFormat,
                                  const gl::Offset &destOffset,
                                  TextureStorage *storage,
                                  GLenum destTarget,
                                  GLint destLevel,
                                  bool unpackFlipY,
                                  bool unpackPremultiplyAlpha,
                                  bool unpackUnmultiplyAlpha) = 0;
    virtual gl::Error copyCompressedTexture(const gl::Context *context,
                                            const gl::Texture *source,
                                            GLint sourceLevel,
                                            TextureStorage *storage,
                                            GLint destLevel) = 0;

    // RenderTarget creation
    virtual gl::Error createRenderTarget(int width, int height, GLenum format, GLsizei samples, RenderTargetD3D **outRT) = 0;
    virtual gl::Error createRenderTargetCopy(RenderTargetD3D *source, RenderTargetD3D **outRT) = 0;

    // Shader operations
    virtual gl::Error loadExecutable(const uint8_t *function,
                                     size_t length,
                                     gl::ShaderType type,
                                     const std::vector<D3DVarying> &streamOutVaryings,
                                     bool separatedOutputBuffers,
                                     ShaderExecutableD3D **outExecutable)      = 0;
    virtual gl::Error compileToExecutable(gl::InfoLog &infoLog,
                                          const std::string &shaderHLSL,
                                          gl::ShaderType type,
                                          const std::vector<D3DVarying> &streamOutVaryings,
                                          bool separatedOutputBuffers,
                                          const angle::CompilerWorkaroundsD3D &workarounds,
                                          ShaderExecutableD3D **outExectuable) = 0;
    virtual gl::Error ensureHLSLCompilerInitialized()                          = 0;

    virtual UniformStorageD3D *createUniformStorage(size_t storageSize) = 0;

    // Image operations
    virtual ImageD3D *createImage() = 0;
    virtual gl::Error generateMipmap(const gl::Context *context,
                                     ImageD3D *dest,
                                     ImageD3D *source) = 0;
    virtual gl::Error generateMipmapUsingD3D(const gl::Context *context,
                                             TextureStorage *storage,
                                             const gl::TextureState &textureState) = 0;
    virtual gl::Error copyImage(const gl::Context *context,
                                ImageD3D *dest,
                                ImageD3D *source,
                                const gl::Rectangle &sourceRect,
                                const gl::Offset &destOffset,
                                bool unpackFlipY,
                                bool unpackPremultiplyAlpha,
                                bool unpackUnmultiplyAlpha)                 = 0;
    virtual TextureStorage *createTextureStorage2D(SwapChainD3D *swapChain) = 0;
    virtual TextureStorage *createTextureStorageEGLImage(EGLImageD3D *eglImage,
                                                         RenderTargetD3D *renderTargetD3D) = 0;
    virtual TextureStorage *createTextureStorageExternal(
        egl::Stream *stream,
        const egl::Stream::GLTextureDescription &desc) = 0;
    virtual TextureStorage *createTextureStorage2D(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, int levels, bool hintLevelZeroOnly) = 0;
    virtual TextureStorage *createTextureStorageCube(GLenum internalformat, bool renderTarget, int size, int levels, bool hintLevelZeroOnly) = 0;
    virtual TextureStorage *createTextureStorage3D(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, GLsizei depth, int levels) = 0;
    virtual TextureStorage *createTextureStorage2DArray(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, GLsizei depth, int levels) = 0;
    virtual TextureStorage *createTextureStorage2DMultisample(GLenum internalformat,
                                                              GLsizei width,
                                                              GLsizei height,
                                                              int levels,
                                                              int samples,
                                                              bool fixedSampleLocations) = 0;

    // Buffer-to-texture and Texture-to-buffer copies
    virtual bool supportsFastCopyBufferToTexture(GLenum internalFormat) const = 0;
    virtual gl::Error fastCopyBufferToTexture(const gl::Context *context,
                                              const gl::PixelUnpackState &unpack,
                                              unsigned int offset,
                                              RenderTargetD3D *destRenderTarget,
                                              GLenum destinationFormat,
                                              GLenum sourcePixelsType,
                                              const gl::Box &destArea) = 0;

    // Device lost
    GLenum getResetStatus();
    void notifyDeviceLost();
    virtual bool resetDevice() = 0;
    virtual bool testDeviceLost()       = 0;
    virtual bool testDeviceResettable() = 0;

    virtual RendererClass getRendererClass() const = 0;
    virtual void *getD3DDevice() = 0;

    void setGPUDisjoint();

    GLint getGPUDisjoint();
    GLint64 getTimestamp();

    virtual gl::Error clearRenderTarget(RenderTargetD3D *renderTarget,
                                        const gl::ColorF &clearColorValue,
                                        const float clearDepthValue,
                                        const unsigned int clearStencilValue) = 0;

    virtual egl::Error getEGLDevice(DeviceImpl **device) = 0;

    bool presentPathFastEnabled() const { return mPresentPathFastEnabled; }

    // Stream creation
    virtual StreamProducerImpl *createStreamProducerD3DTextureNV12(
        egl::Stream::ConsumerType consumerType,
        const egl::AttributeMap &attribs) = 0;

    const gl::Caps &getNativeCaps() const;
    const gl::TextureCapsMap &getNativeTextureCaps() const;
    const gl::Extensions &getNativeExtensions() const;
    const gl::Limitations &getNativeLimitations() const;

    // Necessary hack for default framebuffers in D3D.
    virtual FramebufferImpl *createDefaultFramebuffer(const gl::FramebufferState &state) = 0;

    virtual gl::Version getMaxSupportedESVersion() const = 0;

    gl::Error initRenderTarget(RenderTargetD3D *renderTarget);

    angle::WorkerThreadPool *getWorkerThreadPool();

    gl::Error getIncompleteTexture(const gl::Context *context,
                                   GLenum type,
                                   gl::Texture **textureOut);

    Serial generateSerial();

    virtual bool canSelectViewInVertexShader() const = 0;

    gl::Error initializeMultisampleTextureToBlack(const gl::Context *context,
                                                  gl::Texture *glTexture) override;

  protected:
    virtual bool getLUID(LUID *adapterLuid) const = 0;
    virtual void generateCaps(gl::Caps *outCaps,
                              gl::TextureCapsMap *outTextureCaps,
                              gl::Extensions *outExtensions,
                              gl::Limitations *outLimitations) const = 0;

    void cleanup();

    bool skipDraw(const gl::State &glState, GLenum drawMode);

    egl::Display *mDisplay;

    bool mPresentPathFastEnabled;

  private:
    void ensureCapsInitialized() const;

    virtual angle::WorkaroundsD3D generateWorkarounds() const = 0;

    mutable bool mCapsInitialized;
    mutable gl::Caps mNativeCaps;
    mutable gl::TextureCapsMap mNativeTextureCaps;
    mutable gl::Extensions mNativeExtensions;
    mutable gl::Limitations mNativeLimitations;

    IncompleteTextureSet mIncompleteTextures;

    mutable bool mWorkaroundsInitialized;
    mutable angle::WorkaroundsD3D mWorkarounds;

    bool mDisjoint;
    bool mDeviceLost;

    angle::WorkerThreadPool mWorkerThreadPool;

    SerialFactory mSerialFactory;
};

unsigned int GetBlendSampleMask(const gl::State &glState, int samples);
bool InstancedPointSpritesActive(ProgramD3D *programD3D, GLenum mode);

}  // namespace rx

#endif // LIBANGLE_RENDERER_D3D_RENDERERD3D_H_
