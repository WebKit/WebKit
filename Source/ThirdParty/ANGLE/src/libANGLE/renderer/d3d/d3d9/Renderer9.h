//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderer9.h: Defines a back-end specific class for the D3D9 renderer.

#ifndef LIBANGLE_RENDERER_D3D_D3D9_RENDERER9_H_
#define LIBANGLE_RENDERER_D3D_D3D9_RENDERER9_H_

#include "common/angleutils.h"
#include "common/mathutil.h"
#include "libANGLE/renderer/d3d/HLSLCompiler.h"
#include "libANGLE/renderer/d3d/RenderTargetD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/d3d9/DebugAnnotator9.h"
#include "libANGLE/renderer/d3d/d3d9/ShaderCache.h"
#include "libANGLE/renderer/d3d/d3d9/StateManager9.h"
#include "libANGLE/renderer/d3d/d3d9/VertexDeclarationCache.h"
#include "libANGLE/renderer/driver_utils.h"

namespace gl
{
class FramebufferAttachment;
}

namespace egl
{
class AttributeMap;
}

namespace rx
{
class Blit9;
class Context9;
class IndexDataManager;
class ProgramD3D;
class StreamingIndexBufferInterface;
class StaticIndexBufferInterface;
class VertexDataManager;
struct ClearParameters;
struct D3DUniform;
struct TranslatedAttribute;

enum D3D9InitError
{
    D3D9_INIT_SUCCESS = 0,
    // Failed to load the D3D or ANGLE compiler
    D3D9_INIT_COMPILER_ERROR,
    // Failed to load a necessary DLL
    D3D9_INIT_MISSING_DEP,
    // Device creation error
    D3D9_INIT_CREATE_DEVICE_ERROR,
    // System does not meet minimum shader spec
    D3D9_INIT_UNSUPPORTED_VERSION,
    // System does not support stretchrect from textures
    D3D9_INIT_UNSUPPORTED_STRETCHRECT,
    // A call returned out of memory or device lost
    D3D9_INIT_OUT_OF_MEMORY,
    // Other unspecified error
    D3D9_INIT_OTHER_ERROR,
    NUM_D3D9_INIT_ERRORS
};

class Renderer9 : public RendererD3D
{
  public:
    explicit Renderer9(egl::Display *display);
    ~Renderer9() override;

    egl::Error initialize() override;
    bool resetDevice() override;

    egl::ConfigSet generateConfigs() override;
    void generateDisplayExtensions(egl::DisplayExtensions *outExtensions) const override;

    void startScene();
    void endScene();

    gl::Error flush();
    gl::Error finish();

    bool isValidNativeWindow(EGLNativeWindowType window) const override;
    NativeWindowD3D *createNativeWindow(EGLNativeWindowType window,
                                        const egl::Config *config,
                                        const egl::AttributeMap &attribs) const override;

    SwapChainD3D *createSwapChain(NativeWindowD3D *nativeWindow,
                                  HANDLE shareHandle,
                                  IUnknown *d3dTexture,
                                  GLenum backBufferFormat,
                                  GLenum depthBufferFormat,
                                  EGLint orientation,
                                  EGLint samples) override;
    egl::Error getD3DTextureInfo(const egl::Config *configuration,
                                 IUnknown *d3dTexture,
                                 EGLint *width,
                                 EGLint *height,
                                 GLenum *fboFormat) const override;
    egl::Error validateShareHandle(const egl::Config *config,
                                   HANDLE shareHandle,
                                   const egl::AttributeMap &attribs) const override;

    ContextImpl *createContext(const gl::ContextState &state) override;

    gl::Error allocateEventQuery(IDirect3DQuery9 **outQuery);
    void freeEventQuery(IDirect3DQuery9 *query);

    // resource creation
    gl::Error createVertexShader(const DWORD *function,
                                 size_t length,
                                 IDirect3DVertexShader9 **outShader);
    gl::Error createPixelShader(const DWORD *function,
                                size_t length,
                                IDirect3DPixelShader9 **outShader);
    HRESULT createVertexBuffer(UINT Length, DWORD Usage, IDirect3DVertexBuffer9 **ppVertexBuffer);
    HRESULT createIndexBuffer(UINT Length,
                              DWORD Usage,
                              D3DFORMAT Format,
                              IDirect3DIndexBuffer9 **ppIndexBuffer);
    gl::Error setSamplerState(const gl::Context *context,
                              gl::SamplerType type,
                              int index,
                              gl::Texture *texture,
                              const gl::SamplerState &sampler);
    gl::Error setTexture(const gl::Context *context,
                         gl::SamplerType type,
                         int index,
                         gl::Texture *texture);

    gl::Error updateState(const gl::Context *context, GLenum drawMode);

    void setScissorRectangle(const gl::Rectangle &scissor, bool enabled);
    void setViewport(const gl::Rectangle &viewport,
                     float zNear,
                     float zFar,
                     GLenum drawMode,
                     GLenum frontFace,
                     bool ignoreViewport);

    gl::Error applyRenderTarget(const gl::Context *context, const gl::Framebuffer *frameBuffer);
    gl::Error applyRenderTarget(const gl::Context *context,
                                const gl::FramebufferAttachment *colorAttachment,
                                const gl::FramebufferAttachment *depthStencilAttachment);
    gl::Error applyUniforms(ProgramD3D *programD3D);
    bool applyPrimitiveType(GLenum primitiveType, GLsizei elementCount, bool usesPointSize);
    gl::Error applyVertexBuffer(const gl::Context *context,
                                GLenum mode,
                                GLint first,
                                GLsizei count,
                                GLsizei instances,
                                TranslatedIndexData *indexInfo);
    gl::Error applyIndexBuffer(const gl::Context *context,
                               const void *indices,
                               GLsizei count,
                               GLenum mode,
                               GLenum type,
                               TranslatedIndexData *indexInfo);

    gl::Error clear(const gl::Context *context,
                    const ClearParameters &clearParams,
                    const gl::FramebufferAttachment *colorBuffer,
                    const gl::FramebufferAttachment *depthStencilBuffer);

    void markAllStateDirty();

    // lost device
    bool testDeviceLost() override;
    bool testDeviceResettable() override;

    VendorID getVendorId() const;
    std::string getRendererDescription() const;
    DeviceIdentifier getAdapterIdentifier() const override;

    IDirect3DDevice9 *getDevice() { return mDevice; }
    void *getD3DDevice() override;

    unsigned int getReservedVertexUniformVectors() const;
    unsigned int getReservedFragmentUniformVectors() const;

    bool getShareHandleSupport() const;

    int getMajorShaderModel() const override;
    int getMinorShaderModel() const override;
    std::string getShaderModelSuffix() const override;

    DWORD getCapsDeclTypes() const;

    // Pixel operations
    gl::Error copyImage2D(const gl::Context *context,
                          const gl::Framebuffer *framebuffer,
                          const gl::Rectangle &sourceRect,
                          GLenum destFormat,
                          const gl::Offset &destOffset,
                          TextureStorage *storage,
                          GLint level) override;
    gl::Error copyImageCube(const gl::Context *context,
                            const gl::Framebuffer *framebuffer,
                            const gl::Rectangle &sourceRect,
                            GLenum destFormat,
                            const gl::Offset &destOffset,
                            TextureStorage *storage,
                            GLenum target,
                            GLint level) override;
    gl::Error copyImage3D(const gl::Context *context,
                          const gl::Framebuffer *framebuffer,
                          const gl::Rectangle &sourceRect,
                          GLenum destFormat,
                          const gl::Offset &destOffset,
                          TextureStorage *storage,
                          GLint level) override;
    gl::Error copyImage2DArray(const gl::Context *context,
                               const gl::Framebuffer *framebuffer,
                               const gl::Rectangle &sourceRect,
                               GLenum destFormat,
                               const gl::Offset &destOffset,
                               TextureStorage *storage,
                               GLint level) override;

    gl::Error copyTexture(const gl::Context *context,
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
                          bool unpackUnmultiplyAlpha) override;
    gl::Error copyCompressedTexture(const gl::Context *context,
                                    const gl::Texture *source,
                                    GLint sourceLevel,
                                    TextureStorage *storage,
                                    GLint destLevel) override;

    // RenderTarget creation
    gl::Error createRenderTarget(int width,
                                 int height,
                                 GLenum format,
                                 GLsizei samples,
                                 RenderTargetD3D **outRT) override;
    gl::Error createRenderTargetCopy(RenderTargetD3D *source, RenderTargetD3D **outRT) override;

    // Shader operations
    gl::Error loadExecutable(const uint8_t *function,
                             size_t length,
                             gl::ShaderType type,
                             const std::vector<D3DVarying> &streamOutVaryings,
                             bool separatedOutputBuffers,
                             ShaderExecutableD3D **outExecutable) override;
    gl::Error compileToExecutable(gl::InfoLog &infoLog,
                                  const std::string &shaderHLSL,
                                  gl::ShaderType type,
                                  const std::vector<D3DVarying> &streamOutVaryings,
                                  bool separatedOutputBuffers,
                                  const angle::CompilerWorkaroundsD3D &workarounds,
                                  ShaderExecutableD3D **outExectuable) override;
    gl::Error ensureHLSLCompilerInitialized() override;

    UniformStorageD3D *createUniformStorage(size_t storageSize) override;

    // Image operations
    ImageD3D *createImage() override;
    gl::Error generateMipmap(const gl::Context *context, ImageD3D *dest, ImageD3D *source) override;
    gl::Error generateMipmapUsingD3D(const gl::Context *context,
                                     TextureStorage *storage,
                                     const gl::TextureState &textureState) override;
    gl::Error copyImage(const gl::Context *context,
                        ImageD3D *dest,
                        ImageD3D *source,
                        const gl::Rectangle &sourceRect,
                        const gl::Offset &destOffset,
                        bool unpackFlipY,
                        bool unpackPremultiplyAlpha,
                        bool unpackUnmultiplyAlpha) override;
    TextureStorage *createTextureStorage2D(SwapChainD3D *swapChain) override;
    TextureStorage *createTextureStorageEGLImage(EGLImageD3D *eglImage,
                                                 RenderTargetD3D *renderTargetD3D) override;
    TextureStorage *createTextureStorageExternal(
        egl::Stream *stream,
        const egl::Stream::GLTextureDescription &desc) override;
    TextureStorage *createTextureStorage2D(GLenum internalformat,
                                           bool renderTarget,
                                           GLsizei width,
                                           GLsizei height,
                                           int levels,
                                           bool hintLevelZeroOnly) override;
    TextureStorage *createTextureStorageCube(GLenum internalformat,
                                             bool renderTarget,
                                             int size,
                                             int levels,
                                             bool hintLevelZeroOnly) override;
    TextureStorage *createTextureStorage3D(GLenum internalformat,
                                           bool renderTarget,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           int levels) override;
    TextureStorage *createTextureStorage2DArray(GLenum internalformat,
                                                bool renderTarget,
                                                GLsizei width,
                                                GLsizei height,
                                                GLsizei depth,
                                                int levels) override;

    TextureStorage *createTextureStorage2DMultisample(GLenum internalformat,
                                                      GLsizei width,
                                                      GLsizei height,
                                                      int levels,
                                                      int samples,
                                                      bool fixedSampleLocations) override;

    // Buffer creation
    VertexBuffer *createVertexBuffer() override;
    IndexBuffer *createIndexBuffer() override;

    // Stream Creation
    StreamProducerImpl *createStreamProducerD3DTextureNV12(
        egl::Stream::ConsumerType consumerType,
        const egl::AttributeMap &attribs) override;

    // Buffer-to-texture and Texture-to-buffer copies
    bool supportsFastCopyBufferToTexture(GLenum internalFormat) const override;
    gl::Error fastCopyBufferToTexture(const gl::Context *context,
                                      const gl::PixelUnpackState &unpack,
                                      unsigned int offset,
                                      RenderTargetD3D *destRenderTarget,
                                      GLenum destinationFormat,
                                      GLenum sourcePixelsType,
                                      const gl::Box &destArea) override;

    // D3D9-renderer specific methods
    gl::Error boxFilter(IDirect3DSurface9 *source, IDirect3DSurface9 *dest);

    D3DPOOL getTexturePool(DWORD usage) const;

    bool getLUID(LUID *adapterLuid) const override;
    VertexConversionType getVertexConversionType(
        gl::VertexFormatType vertexFormatType) const override;
    GLenum getVertexComponentType(gl::VertexFormatType vertexFormatType) const override;

    // Warning: you should ensure binding really matches attrib.bindingIndex before using this
    // function.
    gl::ErrorOrResult<unsigned int> getVertexSpaceRequired(const gl::VertexAttribute &attrib,
                                                           const gl::VertexBinding &binding,
                                                           GLsizei count,
                                                           GLsizei instances) const override;

    gl::Error copyToRenderTarget(IDirect3DSurface9 *dest,
                                 IDirect3DSurface9 *source,
                                 bool fromManaged);

    RendererClass getRendererClass() const override;

    D3DDEVTYPE getD3D9DeviceType() const { return mDeviceType; }

    egl::Error getEGLDevice(DeviceImpl **device) override;

    StateManager9 *getStateManager() { return &mStateManager; }

    gl::Error genericDrawArrays(const gl::Context *context,
                                GLenum mode,
                                GLint first,
                                GLsizei count,
                                GLsizei instances);

    gl::Error genericDrawElements(const gl::Context *context,
                                  GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const void *indices,
                                  GLsizei instances);

    // Necessary hack for default framebuffers in D3D.
    FramebufferImpl *createDefaultFramebuffer(const gl::FramebufferState &state) override;

    DebugAnnotator9 *getAnnotator() { return &mAnnotator; }

    gl::Version getMaxSupportedESVersion() const override;

    gl::Error clearRenderTarget(RenderTargetD3D *renderTarget,
                                const gl::ColorF &clearColorValue,
                                const float clearDepthValue,
                                const unsigned int clearStencilValue) override;

    bool canSelectViewInVertexShader() const override;

  private:
    gl::Error drawArraysImpl(const gl::Context *context,
                             GLenum mode,
                             GLint startVertex,
                             GLsizei count,
                             GLsizei instances);
    gl::Error drawElementsImpl(const gl::Context *context,
                               GLenum mode,
                               GLsizei count,
                               GLenum type,
                               const void *indices,
                               GLsizei instances);

    gl::Error applyShaders(const gl::Context *context, GLenum drawMode);

    gl::Error applyTextures(const gl::Context *context);
    gl::Error applyTextures(const gl::Context *context, gl::SamplerType shaderType);

    void generateCaps(gl::Caps *outCaps,
                      gl::TextureCapsMap *outTextureCaps,
                      gl::Extensions *outExtensions,
                      gl::Limitations *outLimitations) const override;

    angle::WorkaroundsD3D generateWorkarounds() const override;

    gl::Error setBlendDepthRasterStates(const gl::Context *context, GLenum drawMode);

    void release();

    void applyUniformnfv(const D3DUniform *targetUniform, const GLfloat *v);
    void applyUniformniv(const D3DUniform *targetUniform, const GLint *v);
    void applyUniformnbv(const D3DUniform *targetUniform, const GLint *v);

    gl::Error drawLineLoop(const gl::Context *context,
                           GLsizei count,
                           GLenum type,
                           const void *indices,
                           int minIndex,
                           gl::Buffer *elementArrayBuffer);
    gl::Error drawIndexedPoints(const gl::Context *context,
                                GLsizei count,
                                GLenum type,
                                const void *indices,
                                int minIndex,
                                gl::Buffer *elementArrayBuffer);

    gl::Error getCountingIB(size_t count, StaticIndexBufferInterface **outIB);

    gl::Error getNullColorbuffer(const gl::Context *context,
                                 const gl::FramebufferAttachment *depthbuffer,
                                 const gl::FramebufferAttachment **outColorBuffer);

    D3DPOOL getBufferPool(DWORD usage) const;

    HMODULE mD3d9Module;

    egl::Error initializeDevice();
    D3DPRESENT_PARAMETERS getDefaultPresentParameters();
    void releaseDeviceResources();

    HRESULT getDeviceStatusCode();
    bool isRemovedDeviceResettable() const;
    bool resetRemovedDevice();

    UINT mAdapter;
    D3DDEVTYPE mDeviceType;
    IDirect3D9 *mD3d9;      // Always valid after successful initialization.
    IDirect3D9Ex *mD3d9Ex;  // Might be null if D3D9Ex is not supported.
    IDirect3DDevice9 *mDevice;
    IDirect3DDevice9Ex *mDeviceEx;  // Might be null if D3D9Ex is not supported.

    HLSLCompiler mCompiler;

    Blit9 *mBlit;

    HWND mDeviceWindow;

    D3DCAPS9 mDeviceCaps;
    D3DADAPTER_IDENTIFIER9 mAdapterIdentifier;

    D3DPRIMITIVETYPE mPrimitiveType;
    int mPrimitiveCount;
    GLsizei mRepeatDraw;

    bool mSceneStarted;

    bool mVertexTextureSupport;

    // current render target states
    unsigned int mAppliedRenderTargetSerial;
    unsigned int mAppliedDepthStencilSerial;
    bool mDepthStencilInitialized;
    bool mRenderTargetDescInitialized;

    IDirect3DStateBlock9 *mMaskedClearSavedState;

    StateManager9 mStateManager;

    // Currently applied sampler states
    struct CurSamplerState
    {
        CurSamplerState();

        bool forceSet;
        size_t baseLevel;
        gl::SamplerState samplerState;
    };
    std::vector<CurSamplerState> mCurVertexSamplerStates;
    std::vector<CurSamplerState> mCurPixelSamplerStates;

    // Currently applied textures
    std::vector<uintptr_t> mCurVertexTextures;
    std::vector<uintptr_t> mCurPixelTextures;

    unsigned int mAppliedIBSerial;
    IDirect3DVertexShader9 *mAppliedVertexShader;
    IDirect3DPixelShader9 *mAppliedPixelShader;
    unsigned int mAppliedProgramSerial;

    // A pool of event queries that are currently unused.
    std::vector<IDirect3DQuery9 *> mEventQueryPool;
    VertexShaderCache mVertexShaderCache;
    PixelShaderCache mPixelShaderCache;

    VertexDataManager *mVertexDataManager;
    VertexDeclarationCache mVertexDeclarationCache;

    IndexDataManager *mIndexDataManager;
    StreamingIndexBufferInterface *mLineLoopIB;
    StaticIndexBufferInterface *mCountingIB;

    enum
    {
        NUM_NULL_COLORBUFFER_CACHE_ENTRIES = 12
    };
    struct NullColorbufferCacheEntry
    {
        UINT lruCount;
        int width;
        int height;
        gl::FramebufferAttachment *buffer;
    } mNullColorbufferCache[NUM_NULL_COLORBUFFER_CACHE_ENTRIES];
    UINT mMaxNullColorbufferLRU;

    DeviceD3D *mEGLDevice;
    std::vector<TranslatedAttribute> mTranslatedAttribCache;

    DebugAnnotator9 mAnnotator;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D9_RENDERER9_H_
