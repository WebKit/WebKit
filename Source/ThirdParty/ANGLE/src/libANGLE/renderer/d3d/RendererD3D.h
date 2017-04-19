
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererD3D.h: Defines a back-end specific class for the DirectX renderer.

#ifndef LIBANGLE_RENDERER_D3D_RENDERERD3D_H_
#define LIBANGLE_RENDERER_D3D_RENDERERD3D_H_

#include <array>

#include "common/debug.h"
#include "common/MemoryBuffer.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/Device.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/VertexDataManager.h"
#include "libANGLE/renderer/d3d/formatutilsD3D.h"
#include "libANGLE/Version.h"
#include "libANGLE/WorkerThread.h"
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

enum ShaderType
{
    SHADER_VERTEX,
    SHADER_PIXEL,
    SHADER_GEOMETRY,
    SHADER_COMPUTE,
    SHADER_TYPE_MAX
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

class RendererD3D : public BufferFactoryD3D
{
  public:
    explicit RendererD3D(egl::Display *display);
    virtual ~RendererD3D();

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
                                          EGLint orientation) = 0;
    virtual egl::Error getD3DTextureInfo(IUnknown *d3dTexture,
                                         EGLint *width,
                                         EGLint *height,
                                         GLenum *fboFormat) const = 0;
    virtual egl::Error validateShareHandle(const egl::Config *config,
                                           HANDLE shareHandle,
                                           const egl::AttributeMap &attribs) const = 0;

    virtual gl::Error setSamplerState(gl::SamplerType type, int index, gl::Texture *texture, const gl::SamplerState &sampler) = 0;
    virtual gl::Error setTexture(gl::SamplerType type, int index, gl::Texture *texture) = 0;

    virtual gl::Error setUniformBuffers(const gl::ContextState &data,
                                        const std::vector<GLint> &vertexUniformBuffers,
                                        const std::vector<GLint> &fragmentUniformBuffers) = 0;

    virtual gl::Error applyUniforms(const ProgramD3D &programD3D,
                                    GLenum drawMode,
                                    const std::vector<D3DUniform *> &uniformArray) = 0;

    virtual unsigned int getReservedVertexUniformBuffers() const = 0;
    virtual unsigned int getReservedFragmentUniformBuffers() const = 0;

    virtual int getMajorShaderModel() const = 0;

    const angle::WorkaroundsD3D &getWorkarounds() const;

    // Pixel operations
    virtual gl::Error copyImage2D(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                 const gl::Offset &destOffset, TextureStorage *storage, GLint level) = 0;
    virtual gl::Error copyImageCube(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                    const gl::Offset &destOffset, TextureStorage *storage, GLenum target, GLint level) = 0;
    virtual gl::Error copyImage3D(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                  const gl::Offset &destOffset, TextureStorage *storage, GLint level) = 0;
    virtual gl::Error copyImage2DArray(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                       const gl::Offset &destOffset, TextureStorage *storage, GLint level) = 0;

    virtual gl::Error copyTexture(const gl::Texture *source,
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
    virtual gl::Error copyCompressedTexture(const gl::Texture *source,
                                            GLint sourceLevel,
                                            TextureStorage *storage,
                                            GLint destLevel) = 0;

    // RenderTarget creation
    virtual gl::Error createRenderTarget(int width, int height, GLenum format, GLsizei samples, RenderTargetD3D **outRT) = 0;
    virtual gl::Error createRenderTargetCopy(RenderTargetD3D *source, RenderTargetD3D **outRT) = 0;

    // Shader operations
    virtual gl::Error loadExecutable(const void *function,
                                     size_t length,
                                     ShaderType type,
                                     const std::vector<D3DVarying> &streamOutVaryings,
                                     bool separatedOutputBuffers,
                                     ShaderExecutableD3D **outExecutable) = 0;
    virtual gl::Error compileToExecutable(gl::InfoLog &infoLog,
                                          const std::string &shaderHLSL,
                                          ShaderType type,
                                          const std::vector<D3DVarying> &streamOutVaryings,
                                          bool separatedOutputBuffers,
                                          const angle::CompilerWorkaroundsD3D &workarounds,
                                          ShaderExecutableD3D **outExectuable) = 0;
    virtual gl::Error ensureHLSLCompilerInitialized()                          = 0;

    virtual UniformStorageD3D *createUniformStorage(size_t storageSize) = 0;

    // Image operations
    virtual ImageD3D *createImage() = 0;
    virtual gl::Error generateMipmap(ImageD3D *dest, ImageD3D *source) = 0;
    virtual gl::Error generateMipmapUsingD3D(TextureStorage *storage,
                                             const gl::TextureState &textureState) = 0;
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

    // Buffer-to-texture and Texture-to-buffer copies
    virtual bool supportsFastCopyBufferToTexture(GLenum internalFormat) const = 0;
    virtual gl::Error fastCopyBufferToTexture(const gl::PixelUnpackState &unpack, unsigned int offset, RenderTargetD3D *destRenderTarget,
                                              GLenum destinationFormat, GLenum sourcePixelsType, const gl::Box &destArea) = 0;

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

    // In D3D11, faster than calling setTexture a jillion times
    virtual gl::Error clearTextures(gl::SamplerType samplerType, size_t rangeStart, size_t rangeEnd) = 0;

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

    angle::WorkerThreadPool *getWorkerThreadPool();

  protected:
    virtual bool getLUID(LUID *adapterLuid) const = 0;
    virtual void generateCaps(gl::Caps *outCaps,
                              gl::TextureCapsMap *outTextureCaps,
                              gl::Extensions *outExtensions,
                              gl::Limitations *outLimitations) const = 0;

    void cleanup();

    static unsigned int GetBlendSampleMask(const gl::ContextState &data, int samples);
    // dirtyPointer is a special value that will make the comparison with any valid pointer fail and force the renderer to re-apply the state.

    gl::Error applyTextures(GLImplFactory *implFactory, const gl::ContextState &data);
    bool skipDraw(const gl::ContextState &data, GLenum drawMode);
    gl::Error markTransformFeedbackUsage(const gl::ContextState &data);

    egl::Display *mDisplay;

    bool mPresentPathFastEnabled;

  private:
    void ensureCapsInitialized() const;

    typedef std::array<gl::Texture*, gl::IMPLEMENTATION_MAX_FRAMEBUFFER_ATTACHMENTS> FramebufferTextureArray;

    gl::Error applyTextures(GLImplFactory *implFactory,
                            const gl::ContextState &data,
                            gl::SamplerType shaderType,
                            const FramebufferTextureArray &framebufferTextures,
                            size_t framebufferTextureCount);

    size_t getBoundFramebufferTextures(const gl::ContextState &data,
                                       FramebufferTextureArray *outTextureArray);
    gl::Texture *getIncompleteTexture(GLImplFactory *implFactory, GLenum type);

    virtual angle::WorkaroundsD3D generateWorkarounds() const = 0;

    mutable bool mCapsInitialized;
    mutable gl::Caps mNativeCaps;
    mutable gl::TextureCapsMap mNativeTextureCaps;
    mutable gl::Extensions mNativeExtensions;
    mutable gl::Limitations mNativeLimitations;

    gl::TextureMap mIncompleteTextures;

    mutable bool mWorkaroundsInitialized;
    mutable angle::WorkaroundsD3D mWorkarounds;

    bool mDisjoint;
    bool mDeviceLost;

    angle::WorkerThreadPool mWorkerThreadPool;
};

}  // namespace rx

#endif // LIBANGLE_RENDERER_D3D_RENDERERD3D_H_
