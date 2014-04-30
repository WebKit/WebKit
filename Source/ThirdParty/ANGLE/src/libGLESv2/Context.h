//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.h: Defines the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#ifndef LIBGLESV2_CONTEXT_H_
#define LIBGLESV2_CONTEXT_H_

#ifndef GL_APICALL
#define GL_APICALL
#endif
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define EGLAPI
#include <EGL/egl.h>

#include <string>
#include <set>
#include <map>
#include <unordered_map>

#include "common/angleutils.h"
#include "common/RefCountObject.h"
#include "libGLESv2/HandleAllocator.h"
#include "libGLESv2/angletypes.h"
#include "libGLESv2/Constants.h"
#include "libGLESv2/VertexAttribute.h"

namespace rx
{
class Renderer;
}

namespace egl
{
class Surface;
}

namespace gl
{
class Shader;
class Program;
class ProgramBinary;
class Texture;
class Texture2D;
class TextureCubeMap;
class Texture3D;
class Texture2DArray;
class Framebuffer;
class Renderbuffer;
class RenderbufferStorage;
class Colorbuffer;
class Depthbuffer;
class Stencilbuffer;
class DepthStencilbuffer;
class FenceNV;
class FenceSync;
class Query;
class ResourceManager;
class Buffer;
class VertexAttribute;
class VertexArray;
class Sampler;
class TransformFeedback;

// Helper structure to store all raw state
struct State
{
    ColorF colorClearValue;
    GLclampf depthClearValue;
    int stencilClearValue;

    RasterizerState rasterizer;
    bool scissorTest;
    Rectangle scissor;

    BlendState blend;
    ColorF blendColor;
    bool sampleCoverage;
    GLclampf sampleCoverageValue;
    bool sampleCoverageInvert;

    DepthStencilState depthStencil;
    GLint stencilRef;
    GLint stencilBackRef;

    GLfloat lineWidth;

    GLenum generateMipmapHint;
    GLenum fragmentShaderDerivativeHint;

    Rectangle viewport;
    float zNear;
    float zFar;

    unsigned int activeSampler;   // Active texture unit selector - GL_TEXTURE0
    BindingPointer<Buffer> arrayBuffer;
    GLuint readFramebuffer;
    GLuint drawFramebuffer;
    BindingPointer<Renderbuffer> renderbuffer;
    GLuint currentProgram;

    VertexAttribCurrentValueData vertexAttribCurrentValues[MAX_VERTEX_ATTRIBS]; // From glVertexAttrib
    unsigned int vertexArray;

    BindingPointer<Texture> samplerTexture[TEXTURE_TYPE_COUNT][IMPLEMENTATION_MAX_COMBINED_TEXTURE_IMAGE_UNITS];
    GLuint samplers[IMPLEMENTATION_MAX_COMBINED_TEXTURE_IMAGE_UNITS];

    typedef std::map< GLenum, BindingPointer<Query> > ActiveQueryMap;
    ActiveQueryMap activeQueries;

    BindingPointer<Buffer> genericUniformBuffer;
    OffsetBindingPointer<Buffer> uniformBuffers[IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS];

    BindingPointer<TransformFeedback> transformFeedback;
    BindingPointer<Buffer> genericTransformFeedbackBuffer;
    OffsetBindingPointer<Buffer> transformFeedbackBuffers[IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS];

    BindingPointer<Buffer> copyReadBuffer;
    BindingPointer<Buffer> copyWriteBuffer;

    PixelUnpackState unpack;
    PixelPackState pack;
};

class Context
{
  public:
    Context(int clientVersion, const gl::Context *shareContext, rx::Renderer *renderer, bool notifyResets, bool robustAccess);

    virtual ~Context();

    void makeCurrent(egl::Surface *surface);

    virtual void markContextLost();
    bool isContextLost();

    // State manipulation
    void setCap(GLenum cap, bool enabled);
    bool getCap(GLenum cap);

    void setClearColor(float red, float green, float blue, float alpha);

    void setClearDepth(float depth);

    void setClearStencil(int stencil);

    void setRasterizerDiscard(bool enabled);
    bool isRasterizerDiscardEnabled() const;

    void setCullFace(bool enabled);
    bool isCullFaceEnabled() const;

    void setCullMode(GLenum mode);

    void setFrontFace(GLenum front);

    void setDepthTest(bool enabled);
    bool isDepthTestEnabled() const;

    void setDepthFunc(GLenum depthFunc);

    void setDepthRange(float zNear, float zFar);
    
    void setBlend(bool enabled);
    bool isBlendEnabled() const;

    void setBlendFactors(GLenum sourceRGB, GLenum destRGB, GLenum sourceAlpha, GLenum destAlpha);
    void setBlendColor(float red, float green, float blue, float alpha);
    void setBlendEquation(GLenum rgbEquation, GLenum alphaEquation);

    void setStencilTest(bool enabled);
    bool isStencilTestEnabled() const;

    void setStencilParams(GLenum stencilFunc, GLint stencilRef, GLuint stencilMask);
    void setStencilBackParams(GLenum stencilBackFunc, GLint stencilBackRef, GLuint stencilBackMask);
    void setStencilWritemask(GLuint stencilWritemask);
    void setStencilBackWritemask(GLuint stencilBackWritemask);
    void setStencilOperations(GLenum stencilFail, GLenum stencilPassDepthFail, GLenum stencilPassDepthPass);
    void setStencilBackOperations(GLenum stencilBackFail, GLenum stencilBackPassDepthFail, GLenum stencilBackPassDepthPass);

    void setPolygonOffsetFill(bool enabled);
    bool isPolygonOffsetFillEnabled() const;

    void setPolygonOffsetParams(GLfloat factor, GLfloat units);

    void setSampleAlphaToCoverage(bool enabled);
    bool isSampleAlphaToCoverageEnabled() const;

    void setSampleCoverage(bool enabled);
    bool isSampleCoverageEnabled() const;

    void setSampleCoverageParams(GLclampf value, bool invert);

    void setScissorTest(bool enabled);
    bool isScissorTestEnabled() const;

    void setDither(bool enabled);
    bool isDitherEnabled() const;

    void setLineWidth(GLfloat width);

    void setGenerateMipmapHint(GLenum hint);
    void setFragmentShaderDerivativeHint(GLenum hint);

    void setViewportParams(GLint x, GLint y, GLsizei width, GLsizei height);

    void setScissorParams(GLint x, GLint y, GLsizei width, GLsizei height);
    void getScissorParams(GLint *x, GLint *y, GLsizei *width, GLsizei *height);

    void setColorMask(bool red, bool green, bool blue, bool alpha);
    void setDepthMask(bool mask);

    void setActiveSampler(unsigned int active);

    GLuint getReadFramebufferHandle() const;
    GLuint getDrawFramebufferHandle() const;
    GLuint getRenderbufferHandle() const;
    GLuint getVertexArrayHandle() const;
    GLuint getSamplerHandle(GLuint textureUnit) const;

    GLuint getArrayBufferHandle() const;

    GLuint getActiveQuery(GLenum target) const;

    void setEnableVertexAttribArray(unsigned int attribNum, bool enabled);
    const VertexAttribute &getVertexAttribState(unsigned int attribNum) const;
    const VertexAttribCurrentValueData &getVertexAttribCurrentValue(unsigned int attribNum) const;
    void setVertexAttribState(unsigned int attribNum, Buffer *boundBuffer, GLint size, GLenum type,
                              bool normalized, bool pureInteger, GLsizei stride, const void *pointer);
    const void *getVertexAttribPointer(unsigned int attribNum) const;

    void setUnpackAlignment(GLint alignment);
    GLint getUnpackAlignment() const;
    const PixelUnpackState &getUnpackState() const;

    void setPackAlignment(GLint alignment);
    GLint getPackAlignment() const;
    const PixelPackState &getPackState() const;

    void setPackReverseRowOrder(bool reverseRowOrder);
    bool getPackReverseRowOrder() const;

    // These create  and destroy methods are merely pass-throughs to 
    // ResourceManager, which owns these object types
    GLuint createBuffer();
    GLuint createShader(GLenum type);
    GLuint createProgram();
    GLuint createTexture();
    GLuint createRenderbuffer();
    GLuint createSampler();
    GLuint createTransformFeedback();
    GLsync createFenceSync(GLenum condition);

    void deleteBuffer(GLuint buffer);
    void deleteShader(GLuint shader);
    void deleteProgram(GLuint program);
    void deleteTexture(GLuint texture);
    void deleteRenderbuffer(GLuint renderbuffer);
    void deleteSampler(GLuint sampler);
    void deleteTransformFeedback(GLuint transformFeedback);
    void deleteFenceSync(GLsync fenceSync);

    // Framebuffers are owned by the Context, so these methods do not pass through
    GLuint createFramebuffer();
    void deleteFramebuffer(GLuint framebuffer);

    // NV Fences are owned by the Context.
    GLuint createFenceNV();
    void deleteFenceNV(GLuint fence);
    
    // Queries are owned by the Context;
    GLuint createQuery();
    void deleteQuery(GLuint query);

    // Vertex arrays are owned by the Context
    GLuint createVertexArray();
    void deleteVertexArray(GLuint vertexArray);

    void bindArrayBuffer(GLuint buffer);
    void bindElementArrayBuffer(GLuint buffer);
    void bindTexture2D(GLuint texture);
    void bindTextureCubeMap(GLuint texture);
    void bindTexture3D(GLuint texture);
    void bindTexture2DArray(GLuint texture);
    void bindReadFramebuffer(GLuint framebuffer);
    void bindDrawFramebuffer(GLuint framebuffer);
    void bindRenderbuffer(GLuint renderbuffer);
    void bindVertexArray(GLuint vertexArray);
    void bindSampler(GLuint textureUnit, GLuint sampler);
    void bindGenericUniformBuffer(GLuint buffer);
    void bindIndexedUniformBuffer(GLuint buffer, GLuint index, GLintptr offset, GLsizeiptr size);
    void bindGenericTransformFeedbackBuffer(GLuint buffer);
    void bindIndexedTransformFeedbackBuffer(GLuint buffer, GLuint index, GLintptr offset, GLsizeiptr size);
    void bindCopyReadBuffer(GLuint buffer);
    void bindCopyWriteBuffer(GLuint buffer);
    void bindPixelPackBuffer(GLuint buffer);
    void bindPixelUnpackBuffer(GLuint buffer);
    void useProgram(GLuint program);
    void linkProgram(GLuint program);
    void setProgramBinary(GLuint program, const void *binary, GLint length);
    void bindTransformFeedback(GLuint transformFeedback);

    void beginQuery(GLenum target, GLuint query);
    void endQuery(GLenum target);

    void setFramebufferZero(Framebuffer *framebuffer);

    void setRenderbufferStorage(GLsizei width, GLsizei height, GLenum internalformat, GLsizei samples);

    void setVertexAttribf(GLuint index, const GLfloat values[4]);
    void setVertexAttribu(GLuint index, const GLuint values[4]);
    void setVertexAttribi(GLuint index, const GLint values[4]);
    void setVertexAttribDivisor(GLuint index, GLuint divisor);

    void samplerParameteri(GLuint sampler, GLenum pname, GLint param);
    void samplerParameterf(GLuint sampler, GLenum pname, GLfloat param);
    GLint getSamplerParameteri(GLuint sampler, GLenum pname);
    GLfloat getSamplerParameterf(GLuint sampler, GLenum pname);

    Buffer *getBuffer(GLuint handle);
    FenceNV *getFenceNV(GLuint handle);
    FenceSync *getFenceSync(GLsync handle) const;
    Shader *getShader(GLuint handle) const;
    Program *getProgram(GLuint handle) const;
    Texture *getTexture(GLuint handle);
    Framebuffer *getFramebuffer(GLuint handle) const;
    Renderbuffer *getRenderbuffer(GLuint handle);
    VertexArray *getVertexArray(GLuint handle) const;
    Sampler *getSampler(GLuint handle) const;
    Query *getQuery(GLuint handle, bool create, GLenum type);
    TransformFeedback *getTransformFeedback(GLuint handle) const;

    Buffer *getTargetBuffer(GLenum target) const;
    Buffer *getArrayBuffer();
    Buffer *getElementArrayBuffer() const;
    ProgramBinary *getCurrentProgramBinary();

    Texture *getTargetTexture(GLenum target) const;
    Texture2D *getTexture2D() const;
    TextureCubeMap *getTextureCubeMap() const;
    Texture3D *getTexture3D() const;
    Texture2DArray *getTexture2DArray() const;

    Buffer *getGenericUniformBuffer();
    Buffer *getGenericTransformFeedbackBuffer();
    Buffer *getCopyReadBuffer();
    Buffer *getCopyWriteBuffer();
    Buffer *getPixelPackBuffer();
    Buffer *getPixelUnpackBuffer();
    Texture *getSamplerTexture(unsigned int sampler, TextureType type) const;

    Framebuffer *getTargetFramebuffer(GLenum target) const;
    GLuint getTargetFramebufferHandle(GLenum target) const;
    Framebuffer *getReadFramebuffer();
    Framebuffer *getDrawFramebuffer();
    VertexArray *getCurrentVertexArray() const;
    TransformFeedback *getCurrentTransformFeedback() const;

    bool isSampler(GLuint samplerName) const;

    bool getBooleanv(GLenum pname, GLboolean *params);
    bool getFloatv(GLenum pname, GLfloat *params);
    bool getIntegerv(GLenum pname, GLint *params);
    bool getInteger64v(GLenum pname, GLint64 *params);

    bool getIndexedIntegerv(GLenum target, GLuint index, GLint *data);
    bool getIndexedInteger64v(GLenum target, GLuint index, GLint64 *data);

    bool getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams);
    bool getIndexedQueryParameterInfo(GLenum target, GLenum *type, unsigned int *numParams);

    void clear(GLbitfield mask);
    void clearBufferfv(GLenum buffer, int drawbuffer, const float *values);
    void clearBufferuiv(GLenum buffer, int drawbuffer, const unsigned int *values);
    void clearBufferiv(GLenum buffer, int drawbuffer, const int *values);
    void clearBufferfi(GLenum buffer, int drawbuffer, float depth, int stencil);

    void readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei *bufSize, void* pixels);
    void drawArrays(GLenum mode, GLint first, GLsizei count, GLsizei instances);
    void drawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei instances);
    void sync(bool block);   // flush/finish

    void recordInvalidEnum();
    void recordInvalidValue();
    void recordInvalidOperation();
    void recordOutOfMemory();
    void recordInvalidFramebufferOperation();

    GLenum getError();
    GLenum getResetStatus();
    virtual bool isResetNotificationEnabled();

    virtual int getClientVersion() const;

    int getMajorShaderModel() const;
    float getMaximumPointSize() const;
    unsigned int getMaximumCombinedTextureImageUnits() const;
    unsigned int getMaximumCombinedUniformBufferBindings() const;
    int getMaximumRenderbufferDimension() const;
    int getMaximum2DTextureDimension() const;
    int getMaximumCubeTextureDimension() const;
    int getMaximum3DTextureDimension() const;
    int getMaximum2DArrayTextureLayers() const;
    int getMaximum2DTextureLevel() const;
    int getMaximumCubeTextureLevel() const;
    int getMaximum3DTextureLevel() const;
    int getMaximum2DArrayTextureLevel() const;
    unsigned int getMaximumRenderTargets() const;
    GLsizei getMaxSupportedSamples() const;
    GLsizei getMaxSupportedFormatSamples(GLenum internalFormat) const;
    GLsizei getNumSampleCounts(GLenum internalFormat) const;
    void getSampleCounts(GLenum internalFormat, GLsizei bufSize, GLint *params) const;
    unsigned int getMaxTransformFeedbackBufferBindings() const;
    GLintptr getUniformBufferOffsetAlignment() const;
    const char *getCombinedExtensionsString() const;
    const char *getExtensionString(const GLuint index) const;
    unsigned int getNumExtensions() const;
    const char *getRendererString() const;
    bool supportsEventQueries() const;
    bool supportsOcclusionQueries() const;
    bool supportsBGRATextures() const;
    bool supportsDXT1Textures() const;
    bool supportsDXT3Textures() const;
    bool supportsDXT5Textures() const;
    bool supportsFloat32Textures() const;
    bool supportsFloat32LinearFilter() const;
    bool supportsFloat32RenderableTextures() const;
    bool supportsFloat16Textures() const;
    bool supportsFloat16LinearFilter() const;
    bool supportsFloat16RenderableTextures() const;
    bool supportsLuminanceTextures() const;
    bool supportsLuminanceAlphaTextures() const;
    bool supportsRGTextures() const;
    bool supportsDepthTextures() const;
    bool supports32bitIndices() const;
    bool supportsNonPower2Texture() const;
    bool supportsInstancing() const;
    bool supportsTextureFilterAnisotropy() const;
    bool supportsPBOs() const;

    bool getCurrentReadFormatType(GLenum *internalFormat, GLenum *format, GLenum *type);

    float getTextureMaxAnisotropy() const;

    void blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                         GLbitfield mask, GLenum filter);

    void invalidateFrameBuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments,
                               GLint x, GLint y, GLsizei width, GLsizei height);

    bool hasMappedBuffer(GLenum target) const;

    rx::Renderer *getRenderer() { return mRenderer; }

  private:
    DISALLOW_COPY_AND_ASSIGN(Context);

    bool applyRenderTarget(GLenum drawMode, bool ignoreViewport);
    void applyState(GLenum drawMode);
    void applyShaders(ProgramBinary *programBinary, bool transformFeedbackActive);
    void applyTextures(ProgramBinary *programBinary);
    void applyTextures(ProgramBinary *programBinary, SamplerType type);
    bool applyUniformBuffers();
    bool applyTransformFeedbackBuffers();
    void markTransformFeedbackUsage();

    void detachBuffer(GLuint buffer);
    void detachTexture(GLuint texture);
    void detachFramebuffer(GLuint framebuffer);
    void detachRenderbuffer(GLuint renderbuffer);
    void detachVertexArray(GLuint vertexArray);
    void detachTransformFeedback(GLuint transformFeedback);
    void detachSampler(GLuint sampler);

    void generateSwizzles(ProgramBinary *programBinary);
    void generateSwizzles(ProgramBinary *programBinary, SamplerType type);
    bool getCurrentTextureAndSamplerState(ProgramBinary *programBinary, SamplerType type, int index, Texture **outTexture,
                                   TextureType *outTextureType, SamplerState *outSampler);
    Texture *getIncompleteTexture(TextureType type);

    bool skipDraw(GLenum drawMode);

    void initExtensionString();
    void initRendererString();

    typedef std::set<unsigned> FramebufferTextureSerialSet;
    FramebufferTextureSerialSet getBoundFramebufferTextureSerials();

    rx::Renderer *const mRenderer;

    int mClientVersion;

    State mState;

    BindingPointer<Texture2D> mTexture2DZero;
    BindingPointer<TextureCubeMap> mTextureCubeMapZero;
    BindingPointer<Texture3D> mTexture3DZero;
    BindingPointer<Texture2DArray> mTexture2DArrayZero;

    typedef std::unordered_map<GLuint, Framebuffer*> FramebufferMap;
    FramebufferMap mFramebufferMap;
    HandleAllocator mFramebufferHandleAllocator;

    typedef std::unordered_map<GLuint, FenceNV*> FenceNVMap;
    FenceNVMap mFenceNVMap;
    HandleAllocator mFenceNVHandleAllocator;

    typedef std::unordered_map<GLuint, Query*> QueryMap;
    QueryMap mQueryMap;
    HandleAllocator mQueryHandleAllocator;

    typedef std::unordered_map<GLuint, VertexArray*> VertexArrayMap;
    VertexArrayMap mVertexArrayMap;
    HandleAllocator mVertexArrayHandleAllocator;

    BindingPointer<TransformFeedback> mTransformFeedbackZero;
    typedef std::unordered_map<GLuint, TransformFeedback*> TransformFeedbackMap;
    TransformFeedbackMap mTransformFeedbackMap;
    HandleAllocator mTransformFeedbackAllocator;

    std::vector<std::string> mExtensionStringList;
    const char *mCombinedExtensionsString;
    const char *mRendererString;
    
    BindingPointer<Texture> mIncompleteTextures[TEXTURE_TYPE_COUNT];

    // Recorded errors
    bool mInvalidEnum;
    bool mInvalidValue;
    bool mInvalidOperation;
    bool mOutOfMemory;
    bool mInvalidFramebufferOperation;

    // Current/lost context flags
    bool mHasBeenCurrent;
    bool mContextLost;
    GLenum mResetStatus;
    GLenum mResetStrategy;
    bool mRobustAccess;

    BindingPointer<ProgramBinary> mCurrentProgramBinary;
    Framebuffer *mBoundDrawFramebuffer;

    int mMajorShaderModel;
    float mMaximumPointSize;
    bool mSupportsVertexTexture;
    bool mSupportsNonPower2Texture;
    bool mSupportsInstancing;
    int  mMaxViewportDimension;
    int  mMaxRenderbufferDimension;
    int  mMax2DTextureDimension;
    int  mMaxCubeTextureDimension;
    int  mMax3DTextureDimension;
    int  mMax2DArrayTextureLayers;
    int  mMax2DTextureLevel;
    int  mMaxCubeTextureLevel;
    int  mMax3DTextureLevel;
    int  mMax2DArrayTextureLevel;
    float mMaxTextureAnisotropy;
    bool mSupportsEventQueries;
    bool mSupportsOcclusionQueries;
    bool mSupportsBGRATextures;
    bool mSupportsDXT1Textures;
    bool mSupportsDXT3Textures;
    bool mSupportsDXT5Textures;
    bool mSupportsFloat32Textures;
    bool mSupportsFloat32LinearFilter;
    bool mSupportsFloat32RenderableTextures;
    bool mSupportsFloat16Textures;
    bool mSupportsFloat16LinearFilter;
    bool mSupportsFloat16RenderableTextures;
    bool mSupportsLuminanceTextures;
    bool mSupportsLuminanceAlphaTextures;
    bool mSupportsRGTextures;
    bool mSupportsDepthTextures;
    bool mSupports32bitIndices;
    bool mSupportsTextureFilterAnisotropy;
    bool mSupportsPBOs;
    int mNumCompressedTextureFormats;

    ResourceManager *mResourceManager;
};
}

#endif   // INCLUDE_CONTEXT_H_
