//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderer11.h: Defines a back-end specific class for the D3D11 renderer.

#ifndef LIBGLESV2_RENDERER_RENDERER11_H_
#define LIBGLESV2_RENDERER_RENDERER11_H_

#include "common/angleutils.h"
#include "libGLESv2/angletypes.h"
#include "common/mathutil.h"

#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/renderer/d3d/HLSLCompiler.h"
#include "libGLESv2/renderer/d3d11/RenderStateCache.h"
#include "libGLESv2/renderer/d3d11/InputLayoutCache.h"
#include "libGLESv2/renderer/RenderTarget.h"

namespace gl
{
class Renderbuffer;
}

namespace rx
{

class VertexDataManager;
class IndexDataManager;
class StreamingIndexBufferInterface;
class Blit11;
class Clear11;
class PixelTransfer11;
struct PackPixelsParams;

enum
{
    MAX_VERTEX_UNIFORM_VECTORS_D3D11 = 1024,
    MAX_FRAGMENT_UNIFORM_VECTORS_D3D11 = 1024
};

class Renderer11 : public Renderer
{
  public:
    Renderer11(egl::Display *display, HDC hDc);
    virtual ~Renderer11();

    static Renderer11 *makeRenderer11(Renderer *renderer);

    virtual EGLint initialize();
    virtual bool resetDevice();

    virtual int generateConfigs(ConfigDesc **configDescList);
    virtual void deleteConfigs(ConfigDesc *configDescList);

    virtual void sync(bool block);

    virtual SwapChain *createSwapChain(HWND window, HANDLE shareHandle, GLenum backBufferFormat, GLenum depthBufferFormat);

    virtual void generateSwizzle(gl::Texture *texture);
    virtual void setSamplerState(gl::SamplerType type, int index, const gl::SamplerState &sampler);
    virtual void setTexture(gl::SamplerType type, int index, gl::Texture *texture);

    virtual bool setUniformBuffers(const gl::Buffer *vertexUniformBuffers[], const gl::Buffer *fragmentUniformBuffers[]);

    virtual void setRasterizerState(const gl::RasterizerState &rasterState);
    virtual void setBlendState(gl::Framebuffer *framebuffer, const gl::BlendState &blendState, const gl::ColorF &blendColor,
                               unsigned int sampleMask);
    virtual void setDepthStencilState(const gl::DepthStencilState &depthStencilState, int stencilRef,
                                      int stencilBackRef, bool frontFaceCCW);

    virtual void setScissorRectangle(const gl::Rectangle &scissor, bool enabled);
    virtual bool setViewport(const gl::Rectangle &viewport, float zNear, float zFar, GLenum drawMode, GLenum frontFace,
                             bool ignoreViewport);

    virtual bool applyPrimitiveType(GLenum mode, GLsizei count);
    virtual bool applyRenderTarget(gl::Framebuffer *frameBuffer);
    virtual void applyShaders(gl::ProgramBinary *programBinary, bool rasterizerDiscard, bool transformFeedbackActive, const gl::VertexFormat inputLayout[]);
    virtual void applyUniforms(const gl::ProgramBinary &programBinary);
    virtual GLenum applyVertexBuffer(gl::ProgramBinary *programBinary, const gl::VertexAttribute vertexAttributes[], gl::VertexAttribCurrentValueData currentValues[],
                                     GLint first, GLsizei count, GLsizei instances);
    virtual GLenum applyIndexBuffer(const GLvoid *indices, gl::Buffer *elementArrayBuffer, GLsizei count, GLenum mode, GLenum type, TranslatedIndexData *indexInfo);
    virtual void applyTransformFeedbackBuffers(gl::Buffer *transformFeedbackBuffers[], GLintptr offsets[]);

    virtual void drawArrays(GLenum mode, GLsizei count, GLsizei instances, bool transformFeedbackActive);
    virtual void drawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices,
                              gl::Buffer *elementArrayBuffer, const TranslatedIndexData &indexInfo, GLsizei instances);

    virtual void clear(const gl::ClearParameters &clearParams, gl::Framebuffer *frameBuffer);

    virtual void markAllStateDirty();

    // lost device
    void notifyDeviceLost();
    virtual bool isDeviceLost();
    virtual bool testDeviceLost(bool notify);
    virtual bool testDeviceResettable();

    // Renderer capabilities
    virtual DWORD getAdapterVendor() const;
    virtual std::string getRendererDescription() const;
    virtual GUID getAdapterIdentifier() const;

    virtual bool getBGRATextureSupport() const;
    virtual bool getDXT1TextureSupport() const;
    virtual bool getDXT3TextureSupport() const;
    virtual bool getDXT5TextureSupport() const;
    virtual bool getEventQuerySupport() const;
    virtual bool getFloat32TextureSupport() const;
    virtual bool getFloat32TextureFilteringSupport() const;
    virtual bool getFloat32TextureRenderingSupport() const;
    virtual bool getFloat16TextureSupport() const;
    virtual bool getFloat16TextureFilteringSupport() const;
    virtual bool getFloat16TextureRenderingSupport() const;
    virtual bool getRGB565TextureSupport() const;
    virtual bool getLuminanceTextureSupport() const;
    virtual bool getLuminanceAlphaTextureSupport() const;
    virtual bool getRGTextureSupport() const;
    virtual unsigned int getMaxVertexTextureImageUnits() const;
    virtual unsigned int getMaxCombinedTextureImageUnits() const;
    virtual unsigned int getReservedVertexUniformVectors() const;
    virtual unsigned int getReservedFragmentUniformVectors() const;
    virtual unsigned int getMaxVertexUniformVectors() const;
    virtual unsigned int getMaxFragmentUniformVectors() const;
    virtual unsigned int getMaxVaryingVectors() const;
    virtual unsigned int getMaxVertexShaderUniformBuffers() const;
    virtual unsigned int getMaxFragmentShaderUniformBuffers() const;
    virtual unsigned int getReservedVertexUniformBuffers() const;
    virtual unsigned int getReservedFragmentUniformBuffers() const;
    unsigned int getReservedVaryings() const;
    virtual unsigned int getMaxTransformFeedbackBuffers() const;
    virtual unsigned int getMaxTransformFeedbackSeparateComponents() const;
    virtual unsigned int getMaxTransformFeedbackInterleavedComponents() const;
    virtual unsigned int getMaxUniformBufferSize() const;
    virtual bool getNonPower2TextureSupport() const;
    virtual bool getDepthTextureSupport() const;
    virtual bool getOcclusionQuerySupport() const;
    virtual bool getInstancingSupport() const;
    virtual bool getTextureFilterAnisotropySupport() const;
    virtual bool getPBOSupport() const;
    virtual float getTextureMaxAnisotropy() const;
    virtual bool getShareHandleSupport() const;
    virtual bool getDerivativeInstructionSupport() const;
    virtual bool getPostSubBufferSupport() const;
    virtual int getMaxRecommendedElementsIndices() const;
    virtual int getMaxRecommendedElementsVertices() const;

    virtual int getMajorShaderModel() const;
    virtual float getMaxPointSize() const;
    virtual int getMaxViewportDimension() const;
    virtual int getMaxTextureWidth() const;
    virtual int getMaxTextureHeight() const;
    virtual int getMaxTextureDepth() const;
    virtual int getMaxTextureArrayLayers() const;
    virtual bool get32BitIndexSupport() const;
    virtual int getMinSwapInterval() const;
    virtual int getMaxSwapInterval() const;

    virtual GLsizei getMaxSupportedSamples() const;
    virtual GLsizei getMaxSupportedFormatSamples(GLenum internalFormat) const;
    virtual GLsizei getNumSampleCounts(GLenum internalFormat) const;
    virtual void getSampleCounts(GLenum internalFormat, GLsizei bufSize, GLint *params) const;
    int getNearestSupportedSamples(DXGI_FORMAT format, unsigned int requested) const;

    virtual unsigned int getMaxRenderTargets() const;

    // Pixel operations
    virtual bool copyToRenderTarget(TextureStorageInterface2D *dest, TextureStorageInterface2D *source);
    virtual bool copyToRenderTarget(TextureStorageInterfaceCube *dest, TextureStorageInterfaceCube *source);
    virtual bool copyToRenderTarget(TextureStorageInterface3D *dest, TextureStorageInterface3D *source);
    virtual bool copyToRenderTarget(TextureStorageInterface2DArray *dest, TextureStorageInterface2DArray *source);

    virtual bool copyImage(gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                           GLint xoffset, GLint yoffset, TextureStorageInterface2D *storage, GLint level);
    virtual bool copyImage(gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                           GLint xoffset, GLint yoffset, TextureStorageInterfaceCube *storage, GLenum target, GLint level);
    virtual bool copyImage(gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                           GLint xoffset, GLint yoffset, GLint zOffset, TextureStorageInterface3D *storage, GLint level);
    virtual bool copyImage(gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                           GLint xoffset, GLint yoffset, GLint zOffset, TextureStorageInterface2DArray *storage, GLint level);

    virtual bool blitRect(gl::Framebuffer *readTarget, const gl::Rectangle &readRect, gl::Framebuffer *drawTarget, const gl::Rectangle &drawRect,
                          const gl::Rectangle *scissor, bool blitRenderTarget, bool blitDepth, bool blitStencil, GLenum filter);
    virtual void readPixels(gl::Framebuffer *framebuffer, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                            GLenum type, GLuint outputPitch, const gl::PixelPackState &pack, void* pixels);

    // RenderTarget creation
    virtual RenderTarget *createRenderTarget(SwapChain *swapChain, bool depth);
    virtual RenderTarget *createRenderTarget(int width, int height, GLenum format, GLsizei samples);

    // Shader operations
    virtual ShaderExecutable *loadExecutable(const void *function, size_t length, rx::ShaderType type,
                                             const std::vector<gl::LinkedVarying> &transformFeedbackVaryings,
                                             bool separatedOutputBuffers);
    virtual ShaderExecutable *compileToExecutable(gl::InfoLog &infoLog, const char *shaderHLSL, rx::ShaderType type,
                                                  const std::vector<gl::LinkedVarying> &transformFeedbackVaryings,
                                                  bool separatedOutputBuffers, D3DWorkaroundType workaround);
    virtual UniformStorage *createUniformStorage(size_t storageSize);

    // Image operations
    virtual Image *createImage();
    virtual void generateMipmap(Image *dest, Image *source);
    virtual TextureStorage *createTextureStorage2D(SwapChain *swapChain);
    virtual TextureStorage *createTextureStorage2D(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, int levels);
    virtual TextureStorage *createTextureStorageCube(GLenum internalformat, bool renderTarget, int size, int levels);
    virtual TextureStorage *createTextureStorage3D(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, GLsizei depth, int levels);
    virtual TextureStorage *createTextureStorage2DArray(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, GLsizei depth, int levels);

    // Buffer creation
    virtual VertexBuffer *createVertexBuffer();
    virtual IndexBuffer *createIndexBuffer();
    virtual BufferStorage *createBufferStorage();

    // Query and Fence creation
    virtual QueryImpl *createQuery(GLenum type);
    virtual FenceImpl *createFence();

    // D3D11-renderer specific methods
    ID3D11Device *getDevice() { return mDevice; }
    ID3D11DeviceContext *getDeviceContext() { return mDeviceContext; };
    IDXGIFactory *getDxgiFactory() { return mDxgiFactory; };

    Blit11 *getBlitter() { return mBlit; }

    // Buffer-to-texture and Texture-to-buffer copies
    virtual bool supportsFastCopyBufferToTexture(GLenum internalFormat) const;
    virtual bool fastCopyBufferToTexture(const gl::PixelUnpackState &unpack, unsigned int offset, RenderTarget *destRenderTarget,
                                         GLenum destinationFormat, GLenum sourcePixelsType, const gl::Box &destArea);

    bool getRenderTargetResource(gl::Renderbuffer *colorbuffer, unsigned int *subresourceIndex, ID3D11Texture2D **resource);
    void unapplyRenderTargets();
    void setOneTimeRenderTarget(ID3D11RenderTargetView *renderTargetView);
    void packPixels(ID3D11Texture2D *readTexture, const PackPixelsParams &params, void *pixelsOut);

    virtual bool getLUID(LUID *adapterLuid) const;
    virtual GLenum getNativeTextureFormat(GLenum internalFormat) const;
    virtual rx::VertexConversionType getVertexConversionType(const gl::VertexFormat &vertexFormat) const;
    virtual GLenum getVertexComponentType(const gl::VertexFormat &vertexFormat) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Renderer11);

    void drawLineLoop(GLsizei count, GLenum type, const GLvoid *indices, int minIndex, gl::Buffer *elementArrayBuffer);
    void drawTriangleFan(GLsizei count, GLenum type, const GLvoid *indices, int minIndex, gl::Buffer *elementArrayBuffer, int instances);

    void readTextureData(ID3D11Texture2D *texture, unsigned int subResource, const gl::Rectangle &area, GLenum format,
                         GLenum type, GLuint outputPitch, const gl::PixelPackState &pack, void *pixels);

    rx::Range getViewportBounds() const;

    bool blitRenderbufferRect(const gl::Rectangle &readRect, const gl::Rectangle &drawRect, RenderTarget *readRenderTarget,
                              RenderTarget *drawRenderTarget, GLenum filter, const gl::Rectangle *scissor,
                              bool colorBlit, bool depthBlit, bool stencilBlit);
    ID3D11Texture2D *resolveMultisampledTexture(ID3D11Texture2D *source, unsigned int subresource);

    static void invalidateRenderbufferSwizzles(gl::Renderbuffer *renderBuffer, int mipLevel);
    static void invalidateFramebufferSwizzles(gl::Framebuffer *framebuffer);

    HMODULE mD3d11Module;
    HMODULE mDxgiModule;
    HDC mDc;

    HLSLCompiler mCompiler;

    bool mDeviceLost;

    void initializeDevice();
    void releaseDeviceResources();
    int getMinorShaderModel() const;
    void release();

    RenderStateCache mStateCache;

    // Support flags
    bool mFloat16TextureSupport;
    bool mFloat16FilterSupport;
    bool mFloat16RenderSupport;

    bool mFloat32TextureSupport;
    bool mFloat32FilterSupport;
    bool mFloat32RenderSupport;

    bool mDXT1TextureSupport;
    bool mDXT3TextureSupport;
    bool mDXT5TextureSupport;

    bool mRGTextureSupport;

    bool mDepthTextureSupport;

    // Multisample format support
    struct MultisampleSupportInfo
    {
        unsigned int qualityLevels[D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT];
        unsigned int maxSupportedSamples;
    };
    MultisampleSupportInfo getMultisampleSupportInfo(DXGI_FORMAT format);

    typedef std::unordered_map<DXGI_FORMAT, MultisampleSupportInfo> MultisampleSupportMap;
    MultisampleSupportMap mMultisampleSupportMap;

    unsigned int mMaxSupportedSamples;

    // current render target states
    unsigned int mAppliedRenderTargetSerials[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS];
    unsigned int mAppliedDepthbufferSerial;
    unsigned int mAppliedStencilbufferSerial;
    bool mDepthStencilInitialized;
    bool mRenderTargetDescInitialized;
    rx::RenderTarget::Desc mRenderTargetDesc;
    unsigned int mCurDepthSize;
    unsigned int mCurStencilSize;

    // Currently applied sampler states
    bool mForceSetVertexSamplerStates[gl::IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];
    gl::SamplerState mCurVertexSamplerStates[gl::IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];

    bool mForceSetPixelSamplerStates[gl::MAX_TEXTURE_IMAGE_UNITS];
    gl::SamplerState mCurPixelSamplerStates[gl::MAX_TEXTURE_IMAGE_UNITS];

    // Currently applied textures
    ID3D11ShaderResourceView *mCurVertexSRVs[gl::IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];
    ID3D11ShaderResourceView *mCurPixelSRVs[gl::MAX_TEXTURE_IMAGE_UNITS];

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
    ID3D11Buffer *mAppliedTFBuffers[gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS];
    GLintptr mAppliedTFOffsets[gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS];

    // Currently applied shaders
    ID3D11VertexShader *mAppliedVertexShader;
    ID3D11GeometryShader *mAppliedGeometryShader;
    ID3D11GeometryShader *mCurPointGeometryShader;
    ID3D11PixelShader *mAppliedPixelShader;

    dx_VertexConstants mVertexConstants;
    dx_VertexConstants mAppliedVertexConstants;
    ID3D11Buffer *mDriverConstantBufferVS;
    ID3D11Buffer *mCurrentVertexConstantBuffer;
    unsigned int mCurrentConstantBufferVS[gl::IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS];

    dx_PixelConstants mPixelConstants;
    dx_PixelConstants mAppliedPixelConstants;
    ID3D11Buffer *mDriverConstantBufferPS;
    ID3D11Buffer *mCurrentPixelConstantBuffer;
    unsigned int mCurrentConstantBufferPS[gl::IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS];

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

    // Sync query
    ID3D11Query *mSyncQuery;

    ID3D11Device *mDevice;
    D3D_FEATURE_LEVEL mFeatureLevel;
    ID3D11DeviceContext *mDeviceContext;
    IDXGIAdapter *mDxgiAdapter;
    DXGI_ADAPTER_DESC mAdapterDescription;
    char mDescription[128];
    IDXGIFactory *mDxgiFactory;

    // Cached device caps
    bool mBGRATextureSupport;
};

}
#endif // LIBGLESV2_RENDERER_RENDERER11_H_
