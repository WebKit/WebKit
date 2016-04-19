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
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/RenderTargetD3D.h"
#include "libANGLE/renderer/d3d/d3d11/DebugAnnotator11.h"
#include "libANGLE/renderer/d3d/d3d11/InputLayoutCache.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/RenderStateCache.h"
#include "libANGLE/renderer/d3d/d3d11/StateManager11.h"

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
class Buffer11;
class Clear11;
class PixelTransfer11;
class RenderTarget11;
class Trim11;
struct PackPixelsParams;

struct Renderer11DeviceCaps
{
    D3D_FEATURE_LEVEL featureLevel;
    bool supportsDXGI1_2;               // Support for DXGI 1.2
    bool supportsClearView;             // Support for ID3D11DeviceContext1::ClearView
    bool supportsConstantBufferOffsets; // Support for Constant buffer offset
    UINT B5G6R5support;                 // Bitfield of D3D11_FORMAT_SUPPORT values for DXGI_FORMAT_B5G6R5_UNORM
    UINT B4G4R4A4support;               // Bitfield of D3D11_FORMAT_SUPPORT values for DXGI_FORMAT_B4G4R4A4_UNORM
    UINT B5G5R5A1support;               // Bitfield of D3D11_FORMAT_SUPPORT values for DXGI_FORMAT_B5G5R5A1_UNORM
};

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
    void generateDisplayExtensions(egl::DisplayExtensions *outExtensions) const override;

    gl::Error flush() override;
    gl::Error finish() override;

    SwapChainD3D *createSwapChain(NativeWindow nativeWindow,
                                  HANDLE shareHandle,
                                  GLenum backBufferFormat,
                                  GLenum depthBufferFormat,
                                  EGLint orientation) override;

    CompilerImpl *createCompiler() override;

    virtual gl::Error generateSwizzle(gl::Texture *texture);
    virtual gl::Error setSamplerState(gl::SamplerType type, int index, gl::Texture *texture, const gl::SamplerState &sampler);
    virtual gl::Error setTexture(gl::SamplerType type, int index, gl::Texture *texture);

    gl::Error setUniformBuffers(const gl::Data &data,
                                const std::vector<GLint> &vertexUniformBuffers,
                                const std::vector<GLint> &fragmentUniformBuffers) override;

    gl::Error updateState(const gl::Data &data, GLenum drawMode) override;

    virtual bool applyPrimitiveType(GLenum mode, GLsizei count, bool usesPointSize);
    gl::Error applyRenderTarget(const gl::Framebuffer *frameBuffer) override;
    gl::Error applyUniforms(const ProgramD3D &programD3D,
                            GLenum drawMode,
                            const std::vector<D3DUniform *> &uniformArray) override;
    virtual gl::Error applyVertexBuffer(const gl::State &state,
                                        GLenum mode,
                                        GLint first,
                                        GLsizei count,
                                        GLsizei instances,
                                        TranslatedIndexData *indexInfo);
    gl::Error applyIndexBuffer(const gl::Data &data,
                               const GLvoid *indices,
                               GLsizei count,
                               GLenum mode,
                               GLenum type,
                               TranslatedIndexData *indexInfo) override;
    gl::Error applyTransformFeedbackBuffers(const gl::State &state) override;

    // lost device
    bool testDeviceLost() override;
    bool testDeviceResettable() override;

    std::string getRendererDescription() const override;
    DeviceIdentifier getAdapterIdentifier() const override;

    virtual unsigned int getReservedVertexUniformVectors() const;
    virtual unsigned int getReservedFragmentUniformVectors() const;
    virtual unsigned int getReservedVertexUniformBuffers() const;
    virtual unsigned int getReservedFragmentUniformBuffers() const;

    bool getShareHandleSupport() const;

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
    gl::Error createRenderTargetCopy(RenderTargetD3D *source, RenderTargetD3D **outRT) override;

    // Framebuffer creation
    FramebufferImpl *createFramebuffer(const gl::Framebuffer::Data &data) override;

    // Shader creation
    ShaderImpl *createShader(const gl::Shader::Data &data) override;
    ProgramImpl *createProgram(const gl::Program::Data &data) override;

    // Shader operations
    gl::Error loadExecutable(const void *function,
                             size_t length,
                             ShaderType type,
                             const std::vector<D3DVarying> &streamOutVaryings,
                             bool separatedOutputBuffers,
                             ShaderExecutableD3D **outExecutable) override;
    gl::Error compileToExecutable(gl::InfoLog &infoLog,
                                  const std::string &shaderHLSL,
                                  ShaderType type,
                                  const std::vector<D3DVarying> &streamOutVaryings,
                                  bool separatedOutputBuffers,
                                  const D3DCompilerWorkarounds &workarounds,
                                  ShaderExecutableD3D **outExectuable) override;
    UniformStorageD3D *createUniformStorage(size_t storageSize) override;

    // Image operations
    virtual ImageD3D *createImage();
    gl::Error generateMipmap(ImageD3D *dest, ImageD3D *source) override;
    gl::Error generateMipmapsUsingD3D(TextureStorage *storage,
                                      const gl::TextureState &textureState) override;
    virtual TextureStorage *createTextureStorage2D(SwapChainD3D *swapChain);
    TextureStorage *createTextureStorageEGLImage(EGLImageD3D *eglImage) override;
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
    VertexArrayImpl *createVertexArray(const gl::VertexArray::Data &data) override;

    // Query and Fence creation
    virtual QueryImpl *createQuery(GLenum type);
    virtual FenceNVImpl *createFenceNV();
    virtual FenceSyncImpl *createFenceSync();

    // Transform Feedback creation
    virtual TransformFeedbackImpl* createTransformFeedback();

    // Stream Creation
    StreamImpl *createStream(const egl::AttributeMap &attribs) override;

    // D3D11-renderer specific methods
    ID3D11Device *getDevice() { return mDevice; }
    void *getD3DDevice() override;
    ID3D11DeviceContext *getDeviceContext() { return mDeviceContext; };
    ID3D11DeviceContext1 *getDeviceContext1IfSupported() { return mDeviceContext1; };
    DXGIFactory *getDxgiFactory() { return mDxgiFactory; };

    RenderStateCache &getStateCache() { return mStateCache; }

    Blit11 *getBlitter() { return mBlit; }
    Clear11 *getClearer() { return mClear; }

    // Buffer-to-texture and Texture-to-buffer copies
    virtual bool supportsFastCopyBufferToTexture(GLenum internalFormat) const;
    virtual gl::Error fastCopyBufferToTexture(const gl::PixelUnpackState &unpack, unsigned int offset, RenderTargetD3D *destRenderTarget,
                                              GLenum destinationFormat, GLenum sourcePixelsType, const gl::Box &destArea);

    void markAllStateDirty();
    gl::Error packPixels(const TextureHelper11 &textureHelper,
                         const PackPixelsParams &params,
                         uint8_t *pixelsOut);

    bool getLUID(LUID *adapterLuid) const override;
    VertexConversionType getVertexConversionType(gl::VertexFormatType vertexFormatType) const override;
    GLenum getVertexComponentType(gl::VertexFormatType vertexFormatType) const override;
    gl::ErrorOrResult<unsigned int> getVertexSpaceRequired(const gl::VertexAttribute &attrib,
                                                           GLsizei count,
                                                           GLsizei instances) const override;

    gl::Error readFromAttachment(const gl::FramebufferAttachment &srcAttachment,
                                 const gl::Rectangle &sourceArea,
                                 GLenum format,
                                 GLenum type,
                                 GLuint outputPitch,
                                 const gl::PixelPackState &pack,
                                 uint8_t *pixels);

    gl::Error blitRenderbufferRect(const gl::Rectangle &readRect, const gl::Rectangle &drawRect, RenderTargetD3D *readRenderTarget,
                                   RenderTargetD3D *drawRenderTarget, GLenum filter, const gl::Rectangle *scissor,
                                   bool colorBlit, bool depthBlit, bool stencilBlit);

    bool isES3Capable() const;
    const Renderer11DeviceCaps &getRenderer11DeviceCaps() { return mRenderer11DeviceCaps; };

    RendererClass getRendererClass() const override { return RENDERER_D3D11; }
    InputLayoutCache *getInputLayoutCache() { return &mInputLayoutCache; }
    StateManager11 *getStateManager() { return &mStateManager; }

    void onSwap();
    void onBufferDelete(const Buffer11 *deleted);
    void onMakeCurrent(const gl::Data &data) override;

    egl::Error getEGLDevice(DeviceImpl **device) override;

  protected:
    void createAnnotator() override;
    gl::Error clearTextures(gl::SamplerType samplerType, size_t rangeStart, size_t rangeEnd) override;
    gl::Error applyShadersImpl(const gl::Data &data, GLenum drawMode) override;

    void syncState(const gl::State &state, const gl::State::DirtyBits &bitmask) override;

  private:
    gl::Error drawArraysImpl(const gl::Data &data,
                             GLenum mode,
                             GLint startVertex,
                             GLsizei count,
                             GLsizei instances) override;
    gl::Error drawElementsImpl(const gl::Data &data,
                               const TranslatedIndexData &indexInfo,
                               GLenum mode,
                               GLsizei count,
                               GLenum type,
                               const GLvoid *indices,
                               GLsizei instances) override;

    void generateCaps(gl::Caps *outCaps, gl::TextureCapsMap *outTextureCaps,
                      gl::Extensions *outExtensions,
                      gl::Limitations *outLimitations) const override;

    WorkaroundsD3D generateWorkarounds() const override;

    gl::Error drawLineLoop(const gl::Data &data,
                           GLsizei count,
                           GLenum type,
                           const GLvoid *indices,
                           const TranslatedIndexData *indexInfo,
                           int instances);
    gl::Error drawTriangleFan(const gl::Data &data,
                              GLsizei count,
                              GLenum type,
                              const GLvoid *indices,
                              int minIndex,
                              int instances);

    ID3D11Texture2D *resolveMultisampledTexture(ID3D11Texture2D *source, unsigned int subresource);

    void populateRenderer11DeviceCaps();

    void updateHistograms();

    class SamplerMetadataD3D11 final : angle::NonCopyable
    {
      public:
        SamplerMetadataD3D11();
        ~SamplerMetadataD3D11();

        struct dx_SamplerMetadata
        {
            int baseLevel;
            int internalFormatBits;
            int wrapModes;
            int padding;  // This just pads the struct to 16 bytes
        };
        static_assert(sizeof(dx_SamplerMetadata) == 16u,
                      "Sampler metadata struct must be one 4-vec / 16 bytes.");

        void initData(unsigned int samplerCount);
        void update(unsigned int samplerIndex, const gl::Texture &texture);

        const dx_SamplerMetadata *getData() const;
        size_t sizeBytes() const;
        bool isDirty() const { return mDirty; }
        void markClean() { mDirty = false; }

      private:
        std::vector<dx_SamplerMetadata> mSamplerMetadata;
        bool mDirty;
    };

    template <class TShaderConstants>
    void applyDriverConstantsIfNeeded(TShaderConstants *appliedConstants,
                                      const TShaderConstants &constants,
                                      SamplerMetadataD3D11 *samplerMetadata,
                                      size_t samplerMetadataReferencedBytes,
                                      ID3D11Buffer *driverConstantBuffer);

    HMODULE mD3d11Module;
    HMODULE mDxgiModule;
    HMODULE mDCompModule;
    std::vector<D3D_FEATURE_LEVEL> mAvailableFeatureLevels;
    D3D_DRIVER_TYPE mRequestedDriverType;
    bool mCreatedWithDeviceEXT;
    DeviceD3D *mEGLDevice;

    HLSLCompiler mCompiler;

    egl::Error initializeD3DDevice();
    void initializeDevice();
    void releaseDeviceResources();
    void release();

    d3d11::ANGLED3D11DeviceType getDeviceType() const;

    RenderStateCache mStateCache;

    // Currently applied sampler states
    std::vector<bool> mForceSetVertexSamplerStates;
    std::vector<gl::SamplerState> mCurVertexSamplerStates;

    std::vector<bool> mForceSetPixelSamplerStates;
    std::vector<gl::SamplerState> mCurPixelSamplerStates;

    StateManager11 mStateManager;

    // Currently applied primitive topology
    D3D11_PRIMITIVE_TOPOLOGY mCurrentPrimitiveTopology;

    // Currently applied index buffer
    ID3D11Buffer *mAppliedIB;
    DXGI_FORMAT mAppliedIBFormat;
    unsigned int mAppliedIBOffset;
    bool mAppliedIBChanged;

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

    dx_VertexConstants11 mAppliedVertexConstants;
    ID3D11Buffer *mDriverConstantBufferVS;
    SamplerMetadataD3D11 mSamplerMetadataVS;
    ID3D11Buffer *mCurrentVertexConstantBuffer;
    unsigned int mCurrentConstantBufferVS[gl::IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS];
    GLintptr mCurrentConstantBufferVSOffset[gl::IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS];
    GLsizeiptr mCurrentConstantBufferVSSize[gl::IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS];

    dx_PixelConstants11 mAppliedPixelConstants;
    ID3D11Buffer *mDriverConstantBufferPS;
    SamplerMetadataD3D11 mSamplerMetadataPS;
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

    // Created objects state tracking
    std::set<const Buffer11*> mAliveBuffers;

    double mLastHistogramUpdateTime;

    ID3D11Device *mDevice;
    Renderer11DeviceCaps mRenderer11DeviceCaps;
    ID3D11DeviceContext *mDeviceContext;
    ID3D11DeviceContext1 *mDeviceContext1;
    IDXGIAdapter *mDxgiAdapter;
    DXGI_ADAPTER_DESC mAdapterDescription;
    char mDescription[128];
    DXGIFactory *mDxgiFactory;
    ID3D11Debug *mDebug;

    std::vector<GLuint> mScratchIndexDataBuffer;

    mutable Optional<bool> mSupportsShareHandles;
};

}
#endif // LIBANGLE_RENDERER_D3D_D3D11_RENDERER11_H_
