//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderer11.h: Defines a back-end specific class for the D3D11 renderer.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_RENDERER11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_RENDERER11_H_

#include "common/angleutils.h"
#include "common/mathutil.h"
#include "libANGLE/AttributeMap.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/d3d/HLSLCompiler.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/RenderTargetD3D.h"
#include "libANGLE/renderer/d3d/d3d11/DebugAnnotator11.h"
#include "libANGLE/renderer/d3d/d3d11/InputLayoutCache.h"
#include "libANGLE/renderer/d3d/d3d11/RenderStateCache.h"

namespace gl
{
class FramebufferAttachment;
struct ImageIndex;
}

namespace rx
{

class VertexDataManager;
class IndexDataManager;
class StreamingIndexBufferInterface;
class Blit11;
class Clear11;
class PixelTransfer11;
class RenderTarget11;
class Trim11;
struct PackPixelsParams;

enum
{
    MAX_VERTEX_UNIFORM_VECTORS_D3D11 = 1024,
    MAX_FRAGMENT_UNIFORM_VECTORS_D3D11 = 1024
};

// Possible reasons RendererD3D initialize can fail
enum D3D11InitError
{
    // The renderer loaded successfully
    D3D11_INIT_SUCCESS = 0,
    // Failed to load the ANGLE & D3D compiler libraries
    D3D11_INIT_COMPILER_ERROR,
    // Failed to load a necessary DLL (non-compiler)
    D3D11_INIT_MISSING_DEP,
    // CreateDevice returned E_INVALIDARG
    D3D11_INIT_CREATEDEVICE_INVALIDARG,
    // CreateDevice failed with an error other than invalid arg
    D3D11_INIT_CREATEDEVICE_ERROR,
    // DXGI 1.2 required but not found
    D3D11_INIT_INCOMPATIBLE_DXGI,
    // Other initialization error
    D3D11_INIT_OTHER_ERROR,
    // CreateDevice returned E_FAIL
    D3D11_INIT_CREATEDEVICE_FAIL,
    // CreateDevice returned E_NOTIMPL
    D3D11_INIT_CREATEDEVICE_NOTIMPL,
    // CreateDevice returned E_OUTOFMEMORY
    D3D11_INIT_CREATEDEVICE_OUTOFMEMORY,
    // CreateDevice returned DXGI_ERROR_INVALID_CALL
    D3D11_INIT_CREATEDEVICE_INVALIDCALL,
    // CreateDevice returned DXGI_ERROR_SDK_COMPONENT_MISSING
    D3D11_INIT_CREATEDEVICE_COMPONENTMISSING,
    // CreateDevice returned DXGI_ERROR_WAS_STILL_DRAWING
    D3D11_INIT_CREATEDEVICE_WASSTILLDRAWING,
    // CreateDevice returned DXGI_ERROR_NOT_CURRENTLY_AVAILABLE
    D3D11_INIT_CREATEDEVICE_NOTAVAILABLE,
    // CreateDevice returned DXGI_ERROR_DEVICE_HUNG
    D3D11_INIT_CREATEDEVICE_DEVICEHUNG,
    // CreateDevice returned NULL
    D3D11_INIT_CREATEDEVICE_NULL,
    NUM_D3D11_INIT_ERRORS
};

class Renderer11 : public RendererD3D
{
  public:
    explicit Renderer11(egl::Display *display);
    virtual ~Renderer11();

    egl::Error initialize() override;
    virtual bool resetDevice();

    egl::ConfigSet generateConfigs() const override;

    gl::Error flush() override;
    gl::Error finish() override;

    bool shouldCreateChildWindowForSurface(EGLNativeWindowType window) const override;
    virtual SwapChainD3D *createSwapChain(NativeWindow nativeWindow, HANDLE shareHandle, GLenum backBufferFormat, GLenum depthBufferFormat);

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

    virtual bool applyPrimitiveType(GLenum mode, GLsizei count, bool usesPointSize);
    gl::Error applyRenderTarget(const gl::Framebuffer *frameBuffer) override;
    virtual gl::Error applyShaders(gl::Program *program, const gl::VertexFormat inputLayout[], const gl::Framebuffer *framebuffer,
                                   bool rasterizerDiscard, bool transformFeedbackActive);

    virtual gl::Error applyUniforms(const ProgramImpl &program, const std::vector<gl::LinkedUniform*> &uniformArray);
    virtual gl::Error applyVertexBuffer(const gl::State &state, GLenum mode, GLint first, GLsizei count, GLsizei instances);
    virtual gl::Error applyIndexBuffer(const GLvoid *indices, gl::Buffer *elementArrayBuffer, GLsizei count, GLenum mode, GLenum type, TranslatedIndexData *indexInfo);
    void applyTransformFeedbackBuffers(const gl::State &state) override;

    gl::Error drawArrays(const gl::Data &data, GLenum mode, GLsizei count, GLsizei instances, bool usesPointSize) override;
    virtual gl::Error drawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices,
                                   gl::Buffer *elementArrayBuffer, const TranslatedIndexData &indexInfo, GLsizei instances);

    virtual void markAllStateDirty();

    // lost device
    bool testDeviceLost() override;
    bool testDeviceResettable() override;

    VendorID getVendorId() const override;
    std::string getRendererDescription() const override;
    GUID getAdapterIdentifier() const override;

    virtual unsigned int getReservedVertexUniformVectors() const;
    virtual unsigned int getReservedFragmentUniformVectors() const;
    virtual unsigned int getReservedVertexUniformBuffers() const;
    virtual unsigned int getReservedFragmentUniformBuffers() const;
    virtual bool getShareHandleSupport() const;
    virtual bool getPostSubBufferSupport() const;

    virtual int getMajorShaderModel() const;
    int getMinorShaderModel() const override;
    std::string getShaderModelSuffix() const override;

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

    // D3D11-renderer specific methods
    ID3D11Device *getDevice() { return mDevice; }
    void *getD3DDevice() override;
    ID3D11DeviceContext *getDeviceContext() { return mDeviceContext; };
    ID3D11DeviceContext1 *getDeviceContext1IfSupported() { return mDeviceContext1; };
    DXGIFactory *getDxgiFactory() { return mDxgiFactory; };

    Blit11 *getBlitter() { return mBlit; }
    Clear11 *getClearer() { return mClear; }

    // Buffer-to-texture and Texture-to-buffer copies
    virtual bool supportsFastCopyBufferToTexture(GLenum internalFormat) const;
    virtual gl::Error fastCopyBufferToTexture(const gl::PixelUnpackState &unpack, unsigned int offset, RenderTargetD3D *destRenderTarget,
                                              GLenum destinationFormat, GLenum sourcePixelsType, const gl::Box &destArea);

    void unapplyRenderTargets();
    void setOneTimeRenderTarget(ID3D11RenderTargetView *renderTargetView);
    gl::Error packPixels(ID3D11Texture2D *readTexture, const PackPixelsParams &params, uint8_t *pixelsOut);

    bool getLUID(LUID *adapterLuid) const override;
    virtual VertexConversionType getVertexConversionType(const gl::VertexFormat &vertexFormat) const;
    virtual GLenum getVertexComponentType(const gl::VertexFormat &vertexFormat) const;

    gl::Error readTextureData(ID3D11Texture2D *texture, unsigned int subResource, const gl::Rectangle &area, GLenum format,
                              GLenum type, GLuint outputPitch, const gl::PixelPackState &pack, uint8_t *pixels);

    void setShaderResource(gl::SamplerType shaderType, UINT resourceSlot, ID3D11ShaderResourceView *srv);

    gl::Error blitRenderbufferRect(const gl::Rectangle &readRect, const gl::Rectangle &drawRect, RenderTargetD3D *readRenderTarget,
                                   RenderTargetD3D *drawRenderTarget, GLenum filter, const gl::Rectangle *scissor,
                                   bool colorBlit, bool depthBlit, bool stencilBlit);

    bool isES3Capable() const;
    D3D_FEATURE_LEVEL getFeatureLevel() const { return mFeatureLevel; };

    RendererClass getRendererClass() const override { return RENDERER_D3D11; }

  private:
    void generateCaps(gl::Caps *outCaps, gl::TextureCapsMap *outTextureCaps, gl::Extensions *outExtensions) const override;
    Workarounds generateWorkarounds() const override;

    gl::Error drawLineLoop(GLsizei count, GLenum type, const GLvoid *indices, int minIndex, gl::Buffer *elementArrayBuffer);
    gl::Error drawTriangleFan(GLsizei count, GLenum type, const GLvoid *indices, int minIndex, gl::Buffer *elementArrayBuffer, int instances);

    ID3D11Texture2D *resolveMultisampledTexture(ID3D11Texture2D *source, unsigned int subresource);
    void unsetConflictingSRVs(gl::SamplerType shaderType, uintptr_t resource, const gl::ImageIndex &index);

    HMODULE mD3d11Module;
    HMODULE mDxgiModule;
    std::vector<D3D_FEATURE_LEVEL> mAvailableFeatureLevels;
    D3D_DRIVER_TYPE mDriverType;

    HLSLCompiler mCompiler;

    void initializeDevice();
    void releaseDeviceResources();
    void release();

    RenderStateCache mStateCache;

    // current render target states
    uintptr_t mAppliedRTVs[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS];
    uintptr_t mAppliedDSV;
    bool mDepthStencilInitialized;
    bool mRenderTargetDescInitialized;

    struct RenderTargetDesc
    {
        size_t width;
        size_t height;
        DXGI_FORMAT format;
    };
    RenderTargetDesc mRenderTargetDesc;

    // Currently applied sampler states
    std::vector<bool> mForceSetVertexSamplerStates;
    std::vector<gl::SamplerState> mCurVertexSamplerStates;

    std::vector<bool> mForceSetPixelSamplerStates;
    std::vector<gl::SamplerState> mCurPixelSamplerStates;

    // Currently applied textures
    struct SRVRecord
    {
        uintptr_t srv;
        uintptr_t resource;
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    };
    std::vector<SRVRecord> mCurVertexSRVs;
    std::vector<SRVRecord> mCurPixelSRVs;

    // Currently applied blend state
    bool mForceSetBlendState;
    gl::BlendState mCurBlendState;
    gl::ColorF mCurBlendColor;
    unsigned int mCurSampleMask;

    // Currently applied rasterizer state
    bool mForceSetRasterState;
    gl::RasterizerState mCurRasterState;

    // Currently applied depth stencil state
    bool mForceSetDepthStencilState;
    gl::DepthStencilState mCurDepthStencilState;
    int mCurStencilRef;
    int mCurStencilBackRef;

    // Currently applied scissor rectangle
    bool mForceSetScissor;
    bool mScissorEnabled;
    gl::Rectangle mCurScissor;

    // Currently applied viewport
    bool mForceSetViewport;
    gl::Rectangle mCurViewport;
    float mCurNear;
    float mCurFar;

    // Currently applied primitive topology
    D3D11_PRIMITIVE_TOPOLOGY mCurrentPrimitiveTopology;

    // Currently applied index buffer
    ID3D11Buffer *mAppliedIB;
    DXGI_FORMAT mAppliedIBFormat;
    unsigned int mAppliedIBOffset;

    // Currently applied transform feedback buffers
    size_t mAppliedNumXFBBindings;
    ID3D11Buffer *mAppliedTFBuffers[gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS]; // Tracks the current D3D buffers
                                                                                        // in use for streamout
    GLintptr mAppliedTFOffsets[gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS]; // Tracks the current GL-specified
                                                                                   // buffer offsets to transform feedback
                                                                                   // buffers
    UINT mCurrentD3DOffsets[gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS];  // Tracks the D3D buffer offsets,
                                                                                 // which may differ from GLs, due
                                                                                 // to different append behavior

    // Currently applied shaders
    uintptr_t mAppliedVertexShader;
    uintptr_t mAppliedGeometryShader;
    uintptr_t mAppliedPixelShader;

    dx_VertexConstants mVertexConstants;
    dx_VertexConstants mAppliedVertexConstants;
    ID3D11Buffer *mDriverConstantBufferVS;
    ID3D11Buffer *mCurrentVertexConstantBuffer;
    unsigned int mCurrentConstantBufferVS[gl::IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS];
    GLintptr mCurrentConstantBufferVSOffset[gl::IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS];
    GLsizeiptr mCurrentConstantBufferVSSize[gl::IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS];

    dx_PixelConstants mPixelConstants;
    dx_PixelConstants mAppliedPixelConstants;
    ID3D11Buffer *mDriverConstantBufferPS;
    ID3D11Buffer *mCurrentPixelConstantBuffer;
    unsigned int mCurrentConstantBufferPS[gl::IMPLEMENTATION_MAX_FRAGMENT_SHADER_UNIFORM_BUFFERS];
    GLintptr mCurrentConstantBufferPSOffset[gl::IMPLEMENTATION_MAX_FRAGMENT_SHADER_UNIFORM_BUFFERS];
    GLsizeiptr mCurrentConstantBufferPSSize[gl::IMPLEMENTATION_MAX_FRAGMENT_SHADER_UNIFORM_BUFFERS];

    ID3D11Buffer *mCurrentGeometryConstantBuffer;

    // Vertex, index and input layouts
    VertexDataManager *mVertexDataManager;
    IndexDataManager *mIndexDataManager;
    InputLayoutCache mInputLayoutCache;

    StreamingIndexBufferInterface *mLineLoopIB;
    StreamingIndexBufferInterface *mTriangleFanIB;

    // Texture copy resources
    Blit11 *mBlit;
    PixelTransfer11 *mPixelTransfer;

    // Masked clear resources
    Clear11 *mClear;

    // Perform trim for D3D resources
    Trim11 *mTrim;

    // Sync query
    ID3D11Query *mSyncQuery;

    // Constant buffer offset support
    bool mSupportsConstantBufferOffsets;

    ID3D11Device *mDevice;
    D3D_FEATURE_LEVEL mFeatureLevel;
    ID3D11DeviceContext *mDeviceContext;
    ID3D11DeviceContext1 *mDeviceContext1;
    IDXGIAdapter *mDxgiAdapter;
    DXGI_ADAPTER_DESC mAdapterDescription;
    char mDescription[128];
    DXGIFactory *mDxgiFactory;
    ID3D11Debug *mDebug;

    DebugAnnotator11 mAnnotator;
};

}
#endif // LIBANGLE_RENDERER_D3D_D3D11_RENDERER11_H_
