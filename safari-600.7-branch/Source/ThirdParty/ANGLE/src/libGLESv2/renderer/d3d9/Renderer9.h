//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderer9.h: Defines a back-end specific class for the D3D9 renderer.

#ifndef LIBGLESV2_RENDERER_RENDERER9_H_
#define LIBGLESV2_RENDERER_RENDERER9_H_

#include "common/angleutils.h"
#include "common/mathutil.h"
#include "libGLESv2/renderer/d3d/HLSLCompiler.h"
#include "libGLESv2/renderer/d3d9/ShaderCache.h"
#include "libGLESv2/renderer/d3d9/VertexDeclarationCache.h"
#include "libGLESv2/renderer/Renderer.h"
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
struct TranslatedAttribute;
class Blit9;

class Renderer9 : public Renderer
{
  public:
    Renderer9(egl::Display *display, HDC hDc, bool softwareDevice);
    virtual ~Renderer9();

    static Renderer9 *makeRenderer9(Renderer *renderer);

    virtual EGLint initialize();
    virtual bool resetDevice();

    virtual int generateConfigs(ConfigDesc **configDescList);
    virtual void deleteConfigs(ConfigDesc *configDescList);

    void startScene();
    void endScene();

    virtual void sync(bool block);

    virtual SwapChain *createSwapChain(HWND window, HANDLE shareHandle, GLenum backBufferFormat, GLenum depthBufferFormat);

    IDirect3DQuery9* allocateEventQuery();
    void freeEventQuery(IDirect3DQuery9* query);

    // resource creation
    IDirect3DVertexShader9 *createVertexShader(const DWORD *function, size_t length);
    IDirect3DPixelShader9 *createPixelShader(const DWORD *function, size_t length);
    HRESULT createVertexBuffer(UINT Length, DWORD Usage, IDirect3DVertexBuffer9 **ppVertexBuffer);
    HRESULT createIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, IDirect3DIndexBuffer9 **ppIndexBuffer);
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

    virtual bool applyRenderTarget(gl::Framebuffer *frameBuffer);
    virtual void applyShaders(gl::ProgramBinary *programBinary, bool rasterizerDiscard, bool transformFeedbackActive, const gl::VertexFormat inputLayout[]);
    virtual void applyUniforms(const gl::ProgramBinary &programBinary);
    virtual bool applyPrimitiveType(GLenum primitiveType, GLsizei elementCount);
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
    IDirect3DDevice9 *getDevice() { return mDevice; }
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
    DWORD getCapsDeclTypes() const;
    virtual int getMinSwapInterval() const;
    virtual int getMaxSwapInterval() const;

    virtual GLsizei getMaxSupportedSamples() const;
    virtual GLsizei getMaxSupportedFormatSamples(GLenum internalFormat) const;
    virtual GLsizei getNumSampleCounts(GLenum internalFormat) const;
    virtual void getSampleCounts(GLenum internalFormat, GLsizei bufSize, GLint *params) const;
    int getNearestSupportedSamples(D3DFORMAT format, int requested) const;
    
    virtual unsigned int getMaxRenderTargets() const;

    D3DFORMAT ConvertTextureInternalFormat(GLenum internalformat);

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

    // Buffer-to-texture and Texture-to-buffer copies
    virtual bool supportsFastCopyBufferToTexture(GLenum internalFormat) const;
    virtual bool fastCopyBufferToTexture(const gl::PixelUnpackState &unpack, unsigned int offset, RenderTarget *destRenderTarget,
                                         GLenum destinationFormat, GLenum sourcePixelsType, const gl::Box &destArea);

    // D3D9-renderer specific methods
    bool boxFilter(IDirect3DSurface9 *source, IDirect3DSurface9 *dest);

    D3DPOOL getTexturePool(DWORD usage) const;

    virtual bool getLUID(LUID *adapterLuid) const;
    virtual GLenum getNativeTextureFormat(GLenum internalFormat) const;
    virtual rx::VertexConversionType getVertexConversionType(const gl::VertexFormat &vertexFormat) const;
    virtual GLenum getVertexComponentType(const gl::VertexFormat &vertexFormat) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Renderer9);

    void deinitialize();

    void applyUniformnfv(gl::LinkedUniform *targetUniform, const GLfloat *v);
    void applyUniformniv(gl::LinkedUniform *targetUniform, const GLint *v);
    void applyUniformnbv(gl::LinkedUniform *targetUniform, const GLint *v);

    void drawLineLoop(GLsizei count, GLenum type, const GLvoid *indices, int minIndex, gl::Buffer *elementArrayBuffer);
    void drawIndexedPoints(GLsizei count, GLenum type, const GLvoid *indices, int minIndex, gl::Buffer *elementArrayBuffer);

    bool copyToRenderTarget(IDirect3DSurface9 *dest, IDirect3DSurface9 *source, bool fromManaged);
    gl::Renderbuffer *getNullColorbuffer(gl::Renderbuffer *depthbuffer);

    D3DPOOL getBufferPool(DWORD usage) const;

    HMODULE mD3d9Module;
    HDC mDc;

    void initializeDevice();
    D3DPRESENT_PARAMETERS getDefaultPresentParameters();
    void releaseDeviceResources();

    HRESULT getDeviceStatusCode();
    bool isRemovedDeviceResettable() const;
    bool resetRemovedDevice();

    UINT mAdapter;
    D3DDEVTYPE mDeviceType;
    bool mSoftwareDevice;   // FIXME: Deprecate
    IDirect3D9 *mD3d9;  // Always valid after successful initialization.
    IDirect3D9Ex *mD3d9Ex;  // Might be null if D3D9Ex is not supported.
    IDirect3DDevice9 *mDevice;
    IDirect3DDevice9Ex *mDeviceEx;  // Might be null if D3D9Ex is not supported.

    HLSLCompiler mCompiler;

    Blit9 *mBlit;

    HWND mDeviceWindow;

    bool mDeviceLost;
    D3DCAPS9 mDeviceCaps;
    D3DADAPTER_IDENTIFIER9 mAdapterIdentifier;

    D3DPRIMITIVETYPE mPrimitiveType;
    int mPrimitiveCount;
    GLsizei mRepeatDraw;

    bool mSceneStarted;
    bool mSupportsNonPower2Textures;
    bool mSupportsTextureFilterAnisotropy;
    int mMinSwapInterval;
    int mMaxSwapInterval;

    bool mOcclusionQuerySupport;
    bool mEventQuerySupport;
    bool mVertexTextureSupport;

    bool mDepthTextureSupport;

    bool mRGB565TextureSupport;

    bool mFloat32TextureSupport;
    bool mFloat32FilterSupport;
    bool mFloat32RenderSupport;

    bool mFloat16TextureSupport;
    bool mFloat16FilterSupport;
    bool mFloat16RenderSupport;

    bool mDXT1TextureSupport;
    bool mDXT3TextureSupport;
    bool mDXT5TextureSupport;

    bool mLuminanceTextureSupport;
    bool mLuminanceAlphaTextureSupport;

    bool mRGTextureSupport;

    struct MultisampleSupportInfo
    {
        bool supportedSamples[D3DMULTISAMPLE_16_SAMPLES + 1];
        unsigned int maxSupportedSamples;
    };
    typedef std::map<D3DFORMAT, MultisampleSupportInfo> MultisampleSupportMap;
    MultisampleSupportMap mMultiSampleSupport;
    unsigned int mMaxSupportedSamples;

    MultisampleSupportInfo getMultiSampleSupport(D3DFORMAT format);

    // current render target states
    unsigned int mAppliedRenderTargetSerial;
    unsigned int mAppliedDepthbufferSerial;
    unsigned int mAppliedStencilbufferSerial;
    bool mDepthStencilInitialized;
    bool mRenderTargetDescInitialized;
    rx::RenderTarget::Desc mRenderTargetDesc;
    unsigned int mCurStencilSize;
    unsigned int mCurDepthSize;

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
    bool mForceSetVertexSamplerStates[gl::IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];
    gl::SamplerState mCurVertexSamplerStates[gl::IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];

    bool mForceSetPixelSamplerStates[gl::MAX_TEXTURE_IMAGE_UNITS];
    gl::SamplerState mCurPixelSamplerStates[gl::MAX_TEXTURE_IMAGE_UNITS];

    // Currently applied textures
    unsigned int mCurVertexTextureSerials[gl::IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];
    unsigned int mCurPixelTextureSerials[gl::MAX_TEXTURE_IMAGE_UNITS];

    unsigned int mAppliedIBSerial;
    IDirect3DVertexShader9 *mAppliedVertexShader;
    IDirect3DPixelShader9 *mAppliedPixelShader;
    
    rx::dx_VertexConstants mVertexConstants;
    rx::dx_PixelConstants mPixelConstants;
    bool mDxUniformsDirty;

    // A pool of event queries that are currently unused.
    std::vector<IDirect3DQuery9*> mEventQueryPool;
    VertexShaderCache mVertexShaderCache;
    PixelShaderCache mPixelShaderCache;

    VertexDataManager *mVertexDataManager;
    VertexDeclarationCache mVertexDeclarationCache;

    IndexDataManager *mIndexDataManager;
    StreamingIndexBufferInterface *mLineLoopIB;

    enum { NUM_NULL_COLORBUFFER_CACHE_ENTRIES = 12 };
    struct NullColorbufferCacheEntry
    {
        UINT lruCount;
        int width;
        int height;
        gl::Renderbuffer *buffer;
    } mNullColorbufferCache[NUM_NULL_COLORBUFFER_CACHE_ENTRIES];
    UINT mMaxNullColorbufferLRU;

};

}
#endif // LIBGLESV2_RENDERER_RENDERER9_H_
