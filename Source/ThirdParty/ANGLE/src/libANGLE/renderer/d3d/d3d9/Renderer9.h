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
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/RenderTargetD3D.h"
#include "libANGLE/renderer/d3d/d3d9/DebugAnnotator9.h"
#include "libANGLE/renderer/d3d/d3d9/ShaderCache.h"
#include "libANGLE/renderer/d3d/d3d9/VertexDeclarationCache.h"

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
class IndexDataManager;
class StreamingIndexBufferInterface;
class StaticIndexBufferInterface;
class VertexDataManager;
struct ClearParameters;
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
    virtual ~Renderer9();

    egl::Error initialize() override;
    virtual bool resetDevice();

    egl::ConfigSet generateConfigs() const override;

    void startScene();
    void endScene();

    gl::Error flush() override;
    gl::Error finish() override;

    bool shouldCreateChildWindowForSurface(EGLNativeWindowType window) const override;
    virtual SwapChainD3D *createSwapChain(NativeWindow nativeWindow, HANDLE shareHandle, GLenum backBufferFormat, GLenum depthBufferFormat);

    gl::Error allocateEventQuery(IDirect3DQuery9 **outQuery);
    void freeEventQuery(IDirect3DQuery9* query);

    // resource creation
    gl::Error createVertexShader(const DWORD *function, size_t length, IDirect3DVertexShader9 **outShader);
    gl::Error createPixelShader(const DWORD *function, size_t length, IDirect3DPixelShader9 **outShader);
    HRESULT createVertexBuffer(UINT Length, DWORD Usage, IDirect3DVertexBuffer9 **ppVertexBuffer);
    HRESULT createIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, IDirect3DIndexBuffer9 **ppIndexBuffer);
    virtual gl::Error generateSwizzle(gl::Texture *texture);
    virtual gl::Error setSamplerState(gl::SamplerType type, int index, gl::Texture *texture, const gl::SamplerState &sampler);
    virtual gl::Error setTexture(gl::SamplerType type, int index, gl::Texture *texture);

    gl::Error setUniformBuffers(const gl::Data &data,
                                const GLint vertexUniformBuffers[],
                                const GLint fragmentUniformBuffers[]) override;

    virtual gl::Error setRasterizerState(const gl::RasterizerState &rasterState);
    gl::Error setBlendState(const gl::Framebuffer *framebuffer, const gl::BlendState &blendState, const gl::ColorF &blendColor,
                            unsigned int sampleMask) override;
    virtual gl::Error setDepthStencilState(const gl::DepthStencilState &depthStencilState, int stencilRef,
                                           int stencilBackRef, bool frontFaceCCW);

    virtual void setScissorRectangle(const gl::Rectangle &scissor, bool enabled);
    virtual void setViewport(const gl::Rectangle &viewport, float zNear, float zFar, GLenum drawMode, GLenum frontFace,
                             bool ignoreViewport);

    gl::Error applyRenderTarget(const gl::Framebuffer *frameBuffer) override;
    gl::Error applyRenderTarget(const gl::FramebufferAttachment *colorAttachment,
                                const gl::FramebufferAttachment *depthStencilAttachment);
    virtual gl::Error applyShaders(gl::Program *program, const gl::VertexFormat inputLayout[], const gl::Framebuffer *framebuffer,
                                   bool rasterizerDiscard, bool transformFeedbackActive);
    virtual gl::Error applyUniforms(const ProgramImpl &program, const std::vector<gl::LinkedUniform*> &uniformArray);
    virtual bool applyPrimitiveType(GLenum primitiveType, GLsizei elementCount, bool usesPointSize);
    virtual gl::Error applyVertexBuffer(const gl::State &state, GLenum mode, GLint first, GLsizei count, GLsizei instances);
    virtual gl::Error applyIndexBuffer(const GLvoid *indices, gl::Buffer *elementArrayBuffer, GLsizei count, GLenum mode, GLenum type, TranslatedIndexData *indexInfo);

    void applyTransformFeedbackBuffers(const gl::State &state) override;

    gl::Error drawArrays(const gl::Data &data, GLenum mode, GLsizei count, GLsizei instances, bool usesPointSize) override;
    virtual gl::Error drawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices,
                                   gl::Buffer *elementArrayBuffer, const TranslatedIndexData &indexInfo, GLsizei instances);

    gl::Error clear(const ClearParameters &clearParams,
                    const gl::FramebufferAttachment *colorBuffer,
                    const gl::FramebufferAttachment *depthStencilBuffer);

    virtual void markAllStateDirty();

    // lost device
    bool testDeviceLost() override;
    bool testDeviceResettable() override;

    VendorID getVendorId() const override;
    std::string getRendererDescription() const override;
    GUID getAdapterIdentifier() const override;

    IDirect3DDevice9 *getDevice() { return mDevice; }
    void *getD3DDevice() override;

    virtual unsigned int getReservedVertexUniformVectors() const;
    virtual unsigned int getReservedFragmentUniformVectors() const;
    virtual unsigned int getReservedVertexUniformBuffers() const;
    virtual unsigned int getReservedFragmentUniformBuffers() const;
    virtual bool getShareHandleSupport() const;
    virtual bool getPostSubBufferSupport() const;

    virtual int getMajorShaderModel() const;
    int getMinorShaderModel() const override;
    std::string getShaderModelSuffix() const override;

    DWORD getCapsDeclTypes() const;

    // Pixel operations
    virtual gl::Error copyImage2D(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                  const gl::Offset &destOffset, TextureStorage *storage, GLint level);
    virtual gl::Error copyImageCube(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                    const gl::Offset &destOffset, TextureStorage *storage, GLenum target, GLint level);
    virtual gl::Error copyImage3D(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                  const gl::Offset &destOffset, TextureStorage *storage, GLint level);
    virtual gl::Error copyImage2DArray(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                       const gl::Offset &destOffset, TextureStorage *storage, GLint level);

    // RenderTarget creation
    virtual gl::Error createRenderTarget(int width, int height, GLenum format, GLsizei samples, RenderTargetD3D **outRT);

    // Framebuffer creation
    FramebufferImpl *createDefaultFramebuffer(const gl::Framebuffer::Data &data) override;
    FramebufferImpl *createFramebuffer(const gl::Framebuffer::Data &data) override;

    // Shader creation
    virtual CompilerImpl *createCompiler(const gl::Data &data);
    virtual ShaderImpl *createShader(GLenum type);
    virtual ProgramImpl *createProgram();

    // Shader operations
    virtual gl::Error loadExecutable(const void *function, size_t length, ShaderType type,
                                     const std::vector<gl::LinkedVarying> &transformFeedbackVaryings,
                                     bool separatedOutputBuffers, ShaderExecutableD3D **outExecutable);
    virtual gl::Error compileToExecutable(gl::InfoLog &infoLog, const std::string &shaderHLSL, ShaderType type,
                                          const std::vector<gl::LinkedVarying> &transformFeedbackVaryings,
                                          bool separatedOutputBuffers, const D3DCompilerWorkarounds &workarounds,
                                          ShaderExecutableD3D **outExectuable);
    virtual UniformStorageD3D *createUniformStorage(size_t storageSize);

    // Image operations
    virtual ImageD3D *createImage();
    gl::Error generateMipmap(ImageD3D *dest, ImageD3D *source) override;
    gl::Error generateMipmapsUsingD3D(TextureStorage *storage, const gl::SamplerState &samplerState) override;
    virtual TextureStorage *createTextureStorage2D(SwapChainD3D *swapChain);
    virtual TextureStorage *createTextureStorage2D(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, int levels, bool hintLevelZeroOnly);
    virtual TextureStorage *createTextureStorageCube(GLenum internalformat, bool renderTarget, int size, int levels, bool hintLevelZeroOnly);
    virtual TextureStorage *createTextureStorage3D(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, GLsizei depth, int levels);
    virtual TextureStorage *createTextureStorage2DArray(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, GLsizei depth, int levels);

    // Texture creation
    virtual TextureImpl *createTexture(GLenum target);

    // Renderbuffer creation
    virtual RenderbufferImpl *createRenderbuffer();

    // Buffer creation
    virtual BufferImpl *createBuffer();
    virtual VertexBuffer *createVertexBuffer();
    virtual IndexBuffer *createIndexBuffer();

    // Vertex Array creation
    virtual VertexArrayImpl *createVertexArray();

    // Query and Fence creation
    virtual QueryImpl *createQuery(GLenum type);
    virtual FenceNVImpl *createFenceNV();
    virtual FenceSyncImpl *createFenceSync();

    // Transform Feedback creation
    virtual TransformFeedbackImpl* createTransformFeedback();

    // Buffer-to-texture and Texture-to-buffer copies
    virtual bool supportsFastCopyBufferToTexture(GLenum internalFormat) const;
    virtual gl::Error fastCopyBufferToTexture(const gl::PixelUnpackState &unpack, unsigned int offset, RenderTargetD3D *destRenderTarget,
                                              GLenum destinationFormat, GLenum sourcePixelsType, const gl::Box &destArea);

    // D3D9-renderer specific methods
    gl::Error boxFilter(IDirect3DSurface9 *source, IDirect3DSurface9 *dest);

    D3DPOOL getTexturePool(DWORD usage) const;

    bool getLUID(LUID *adapterLuid) const override;
    virtual VertexConversionType getVertexConversionType(const gl::VertexFormat &vertexFormat) const;
    virtual GLenum getVertexComponentType(const gl::VertexFormat &vertexFormat) const;

    gl::Error copyToRenderTarget(IDirect3DSurface9 *dest, IDirect3DSurface9 *source, bool fromManaged);

    RendererClass getRendererClass() const override { return RENDERER_D3D9; }

    D3DDEVTYPE getD3D9DeviceType() const { return mDeviceType; }

  private:
    void generateCaps(gl::Caps *outCaps, gl::TextureCapsMap *outTextureCaps, gl::Extensions *outExtensions) const override;
    Workarounds generateWorkarounds() const override;

    void release();

    void applyUniformnfv(gl::LinkedUniform *targetUniform, const GLfloat *v);
    void applyUniformniv(gl::LinkedUniform *targetUniform, const GLint *v);
    void applyUniformnbv(gl::LinkedUniform *targetUniform, const GLint *v);

    gl::Error drawLineLoop(GLsizei count, GLenum type, const GLvoid *indices, int minIndex, gl::Buffer *elementArrayBuffer);
    gl::Error drawIndexedPoints(GLsizei count, GLenum type, const GLvoid *indices, int minIndex, gl::Buffer *elementArrayBuffer);

    gl::Error getCountingIB(size_t count, StaticIndexBufferInterface **outIB);

    gl::Error getNullColorbuffer(const gl::FramebufferAttachment *depthbuffer, const gl::FramebufferAttachment **outColorBuffer);

    D3DPOOL getBufferPool(DWORD usage) const;

    HMODULE mD3d9Module;

    void initializeDevice();
    D3DPRESENT_PARAMETERS getDefaultPresentParameters();
    void releaseDeviceResources();

    HRESULT getDeviceStatusCode();
    bool isRemovedDeviceResettable() const;
    bool resetRemovedDevice();

    UINT mAdapter;
    D3DDEVTYPE mDeviceType;
    IDirect3D9 *mD3d9;  // Always valid after successful initialization.
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
    unsigned int mCurStencilSize;
    unsigned int mCurDepthSize;

    struct RenderTargetDesc
    {
        size_t width;
        size_t height;
        D3DFORMAT format;
    };
    RenderTargetDesc mRenderTargetDesc;

    IDirect3DStateBlock9 *mMaskedClearSavedState;

    // previously set render states
    bool mForceSetDepthStencilState;
    gl::DepthStencilState mCurDepthStencilState;
    int mCurStencilRef;
    int mCurStencilBackRef;
    bool mCurFrontFaceCCW;

    bool mForceSetRasterState;
    gl::RasterizerState mCurRasterState;

    bool mForceSetScissor;
    gl::Rectangle mCurScissor;
    bool mScissorEnabled;

    bool mForceSetViewport;
    gl::Rectangle mCurViewport;
    float mCurNear;
    float mCurFar;
    float mCurDepthFront;

    bool mForceSetBlendState;
    gl::BlendState mCurBlendState;
    gl::ColorF mCurBlendColor;
    GLuint mCurSampleMask;

    // Currently applied sampler states
    std::vector<bool> mForceSetVertexSamplerStates;
    std::vector<gl::SamplerState> mCurVertexSamplerStates;

    std::vector<bool> mForceSetPixelSamplerStates;
    std::vector<gl::SamplerState> mCurPixelSamplerStates;

    // Currently applied textures
    std::vector<unsigned int> mCurVertexTextureSerials;
    std::vector<unsigned int> mCurPixelTextureSerials;

    unsigned int mAppliedIBSerial;
    IDirect3DVertexShader9 *mAppliedVertexShader;
    IDirect3DPixelShader9 *mAppliedPixelShader;
    unsigned int mAppliedProgramSerial;

    dx_VertexConstants mVertexConstants;
    dx_PixelConstants mPixelConstants;
    bool mDxUniformsDirty;

    // A pool of event queries that are currently unused.
    std::vector<IDirect3DQuery9*> mEventQueryPool;
    VertexShaderCache mVertexShaderCache;
    PixelShaderCache mPixelShaderCache;

    VertexDataManager *mVertexDataManager;
    VertexDeclarationCache mVertexDeclarationCache;

    IndexDataManager *mIndexDataManager;
    StreamingIndexBufferInterface *mLineLoopIB;
    StaticIndexBufferInterface *mCountingIB;

    enum { NUM_NULL_COLORBUFFER_CACHE_ENTRIES = 12 };
    struct NullColorbufferCacheEntry
    {
        UINT lruCount;
        int width;
        int height;
        gl::FramebufferAttachment *buffer;
    } mNullColorbufferCache[NUM_NULL_COLORBUFFER_CACHE_ENTRIES];
    UINT mMaxNullColorbufferLRU;

    DebugAnnotator9 mAnnotator;
};

}
#endif // LIBANGLE_RENDERER_D3D_D3D9_RENDERER9_H_
