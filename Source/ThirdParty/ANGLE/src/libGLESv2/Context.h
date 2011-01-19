//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.h: Defines the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#ifndef LIBGLESV2_CONTEXT_H_
#define LIBGLESV2_CONTEXT_H_

#define GL_APICALL
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define EGLAPI
#include <EGL/egl.h>
#include <d3d9.h>

#include <map>

#include "common/angleutils.h"
#include "libGLESv2/ResourceManager.h"
#include "libGLESv2/RefCountObject.h"

namespace egl
{
class Display;
class Surface;
class Config;
}

namespace gl
{
struct TranslatedAttribute;
struct TranslatedIndexData;

class Buffer;
class Shader;
class Program;
class Texture;
class Texture2D;
class TextureCubeMap;
class Framebuffer;
class Renderbuffer;
class RenderbufferStorage;
class Colorbuffer;
class Depthbuffer;
class Stencilbuffer;
class DepthStencilbuffer;
class VertexDataManager;
class IndexDataManager;
class Blit;
class Fence;

enum
{
    MAX_VERTEX_ATTRIBS = 16,
    MAX_VERTEX_UNIFORM_VECTORS = 256 - 2,   // 256 is the minimum for SM2, and in practice the maximum for DX9. Reserve space for dx_HalfPixelSize and dx_DepthRange.
    MAX_VARYING_VECTORS_SM2 = 8,
    MAX_VARYING_VECTORS_SM3 = 10,
    MAX_COMBINED_TEXTURE_IMAGE_UNITS = 16,
    MAX_VERTEX_TEXTURE_IMAGE_UNITS = 0,
    MAX_TEXTURE_IMAGE_UNITS = 16,
    MAX_FRAGMENT_UNIFORM_VECTORS_SM2 = 32 - 3,    // Reserve space for dx_Viewport, dx_Depth, and dx_DepthRange. dx_PointOrLines and dx_FrontCCW use separate bool registers.
    MAX_FRAGMENT_UNIFORM_VECTORS_SM3 = 224 - 3,
    MAX_DRAW_BUFFERS = 1,

    IMPLEMENTATION_COLOR_READ_FORMAT = GL_RGB,
    IMPLEMENTATION_COLOR_READ_TYPE = GL_UNSIGNED_SHORT_5_6_5
};

const float ALIASED_LINE_WIDTH_RANGE_MIN = 1.0f;
const float ALIASED_LINE_WIDTH_RANGE_MAX = 1.0f;
const float ALIASED_POINT_SIZE_RANGE_MIN = 1.0f;
const float ALIASED_POINT_SIZE_RANGE_MAX_SM2 = 1.0f;
const float ALIASED_POINT_SIZE_RANGE_MAX_SM3 = 64.0f;

struct Color
{
    float red;
    float green;
    float blue;
    float alpha;
};

// Helper structure describing a single vertex attribute
class VertexAttribute
{
  public:
    VertexAttribute() : mType(GL_FLOAT), mSize(0), mNormalized(false), mStride(0), mPointer(NULL), mArrayEnabled(false)
    {
        mCurrentValue[0] = 0.0f;
        mCurrentValue[1] = 0.0f;
        mCurrentValue[2] = 0.0f;
        mCurrentValue[3] = 1.0f;
    }

    int typeSize() const
    {
        switch (mType)
        {
          case GL_BYTE:           return mSize * sizeof(GLbyte);
          case GL_UNSIGNED_BYTE:  return mSize * sizeof(GLubyte);
          case GL_SHORT:          return mSize * sizeof(GLshort);
          case GL_UNSIGNED_SHORT: return mSize * sizeof(GLushort);
          case GL_FIXED:          return mSize * sizeof(GLfixed);
          case GL_FLOAT:          return mSize * sizeof(GLfloat);
          default: UNREACHABLE(); return mSize * sizeof(GLfloat);
        }
    }

    GLsizei stride() const
    {
        return mStride ? mStride : typeSize();
    }

    // From glVertexAttribPointer
    GLenum mType;
    GLint mSize;
    bool mNormalized;
    GLsizei mStride;   // 0 means natural stride

    union
    {
        const void *mPointer;
        intptr_t mOffset;
    };

    BindingPointer<Buffer> mBoundBuffer;   // Captured when glVertexAttribPointer is called.

    bool mArrayEnabled;   // From glEnable/DisableVertexAttribArray
    float mCurrentValue[4];   // From glVertexAttrib
};

typedef VertexAttribute VertexAttributeArray[MAX_VERTEX_ATTRIBS];

// Helper structure to store all raw state
struct State
{
    Color colorClearValue;
    GLclampf depthClearValue;
    int stencilClearValue;

    bool cullFace;
    GLenum cullMode;
    GLenum frontFace;
    bool depthTest;
    GLenum depthFunc;
    bool blend;
    GLenum sourceBlendRGB;
    GLenum destBlendRGB;
    GLenum sourceBlendAlpha;
    GLenum destBlendAlpha;
    GLenum blendEquationRGB;
    GLenum blendEquationAlpha;
    Color blendColor;
    bool stencilTest;
    GLenum stencilFunc;
    GLint stencilRef;
    GLuint stencilMask;
    GLenum stencilFail;
    GLenum stencilPassDepthFail;
    GLenum stencilPassDepthPass;
    GLuint stencilWritemask;
    GLenum stencilBackFunc;
    GLint stencilBackRef;
    GLuint stencilBackMask;
    GLenum stencilBackFail;
    GLenum stencilBackPassDepthFail;
    GLenum stencilBackPassDepthPass;
    GLuint stencilBackWritemask;
    bool polygonOffsetFill;
    GLfloat polygonOffsetFactor;
    GLfloat polygonOffsetUnits;
    bool sampleAlphaToCoverage;
    bool sampleCoverage;
    GLclampf sampleCoverageValue;
    bool sampleCoverageInvert;
    bool scissorTest;
    bool dither;

    GLfloat lineWidth;

    GLenum generateMipmapHint;
    GLenum fragmentShaderDerivativeHint;

    GLint viewportX;
    GLint viewportY;
    GLsizei viewportWidth;
    GLsizei viewportHeight;
    float zNear;
    float zFar;

    GLint scissorX;
    GLint scissorY;
    GLsizei scissorWidth;
    GLsizei scissorHeight;

    bool colorMaskRed;
    bool colorMaskGreen;
    bool colorMaskBlue;
    bool colorMaskAlpha;
    bool depthMask;

    int activeSampler;   // Active texture unit selector - GL_TEXTURE0
    BindingPointer<Buffer> arrayBuffer;
    BindingPointer<Buffer> elementArrayBuffer;
    GLuint readFramebuffer;
    GLuint drawFramebuffer;
    BindingPointer<Renderbuffer> renderbuffer;
    GLuint currentProgram;

    VertexAttribute vertexAttribute[MAX_VERTEX_ATTRIBS];
    BindingPointer<Texture> samplerTexture[SAMPLER_TYPE_COUNT][MAX_TEXTURE_IMAGE_UNITS];

    GLint unpackAlignment;
    GLint packAlignment;
};

class Context
{
  public:
    Context(const egl::Config *config, const gl::Context *shareContext);

    ~Context();

    void makeCurrent(egl::Display *display, egl::Surface *surface);

    void markAllStateDirty();

    // State manipulation
    void setClearColor(float red, float green, float blue, float alpha);

    void setClearDepth(float depth);

    void setClearStencil(int stencil);

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

    void setColorMask(bool red, bool green, bool blue, bool alpha);
    void setDepthMask(bool mask);

    void setActiveSampler(int active);

    GLuint getReadFramebufferHandle() const;
    GLuint getDrawFramebufferHandle() const;
    GLuint getRenderbufferHandle() const;

    GLuint getArrayBufferHandle() const;

    void setEnableVertexAttribArray(unsigned int attribNum, bool enabled);
    const VertexAttribute &getVertexAttribState(unsigned int attribNum);
    void setVertexAttribState(unsigned int attribNum, Buffer *boundBuffer, GLint size, GLenum type,
                              bool normalized, GLsizei stride, const void *pointer);
    const void *getVertexAttribPointer(unsigned int attribNum) const;

    const VertexAttributeArray &getVertexAttributes();

    void setUnpackAlignment(GLint alignment);
    GLint getUnpackAlignment() const;

    void setPackAlignment(GLint alignment);
    GLint getPackAlignment() const;

    // These create  and destroy methods are merely pass-throughs to 
    // ResourceManager, which owns these object types
    GLuint createBuffer();
    GLuint createShader(GLenum type);
    GLuint createProgram();
    GLuint createTexture();
    GLuint createRenderbuffer();

    void deleteBuffer(GLuint buffer);
    void deleteShader(GLuint shader);
    void deleteProgram(GLuint program);
    void deleteTexture(GLuint texture);
    void deleteRenderbuffer(GLuint renderbuffer);

    // Framebuffers are owned by the Context, so these methods do not pass through
    GLuint createFramebuffer();
    void deleteFramebuffer(GLuint framebuffer);

    // Fences are owned by the Context.
    GLuint createFence();
    void deleteFence(GLuint fence);

    void bindArrayBuffer(GLuint buffer);
    void bindElementArrayBuffer(GLuint buffer);
    void bindTexture2D(GLuint texture);
    void bindTextureCubeMap(GLuint texture);
    void bindReadFramebuffer(GLuint framebuffer);
    void bindDrawFramebuffer(GLuint framebuffer);
    void bindRenderbuffer(GLuint renderbuffer);
    void useProgram(GLuint program);

    void setFramebufferZero(Framebuffer *framebuffer);

    void setRenderbufferStorage(RenderbufferStorage *renderbuffer);

    void setVertexAttrib(GLuint index, const GLfloat *values);

    Buffer *getBuffer(GLuint handle);
    Fence *getFence(GLuint handle);
    Shader *getShader(GLuint handle);
    Program *getProgram(GLuint handle);
    Texture *getTexture(GLuint handle);
    Framebuffer *getFramebuffer(GLuint handle);
    Renderbuffer *getRenderbuffer(GLuint handle);

    Buffer *getArrayBuffer();
    Buffer *getElementArrayBuffer();
    Program *getCurrentProgram();
    Texture2D *getTexture2D();
    TextureCubeMap *getTextureCubeMap();
    Texture *getSamplerTexture(unsigned int sampler, SamplerType type);
    Framebuffer *getReadFramebuffer();
    Framebuffer *getDrawFramebuffer();

    bool getFloatv(GLenum pname, GLfloat *params);
    bool getIntegerv(GLenum pname, GLint *params);
    bool getBooleanv(GLenum pname, GLboolean *params);

    bool getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams);

    bool applyRenderTarget(bool ignoreViewport);
    void applyState(GLenum drawMode);
    GLenum applyVertexBuffer(GLint first, GLsizei count);
    GLenum applyIndexBuffer(const void *indices, GLsizei count, GLenum mode, GLenum type, TranslatedIndexData *indexInfo);
    void applyShaders();
    void applyTextures();

    void readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
    void clear(GLbitfield mask);
    void drawArrays(GLenum mode, GLint first, GLsizei count);
    void drawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
    void finish();
    void flush();

	// Draw the last segment of a line loop
    void drawClosingLine(unsigned int first, unsigned int last);
    void drawClosingLine(GLsizei count, GLenum type, const void *indices);

    void recordInvalidEnum();
    void recordInvalidValue();
    void recordInvalidOperation();
    void recordOutOfMemory();
    void recordInvalidFramebufferOperation();

    GLenum getError();

    bool supportsShaderModel3() const;
    int getMaximumVaryingVectors() const;
    int getMaximumFragmentUniformVectors() const;
    int getMaximumRenderbufferDimension() const;
    int getMaximumTextureDimension() const;
    int getMaximumCubeTextureDimension() const;
    int getMaximumTextureLevel() const;
    GLsizei getMaxSupportedSamples() const;
    int getNearestSupportedSamples(D3DFORMAT format, int requested) const;
    const char *getExtensionString() const;
    bool supportsEventQueries() const;
    bool supportsCompressedTextures() const;
    bool supportsFloatTextures() const;
    bool supportsFloatLinearFilter() const;
    bool supportsFloatRenderableTextures() const;
    bool supportsHalfFloatTextures() const;
    bool supportsHalfFloatLinearFilter() const;
    bool supportsHalfFloatRenderableTextures() const;
    bool supportsLuminanceTextures() const;
    bool supportsLuminanceAlphaTextures() const;
    bool supports32bitIndices() const;

    void blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, 
                         GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                         GLbitfield mask);

    Blit *getBlitter() { return mBlit; }

    const D3DCAPS9 &getDeviceCaps() { return mDeviceCaps; }

  private:
    DISALLOW_COPY_AND_ASSIGN(Context);

    void lookupAttributeMapping(TranslatedAttribute *attributes);

    void detachBuffer(GLuint buffer);
    void detachTexture(GLuint texture);
    void detachFramebuffer(GLuint framebuffer);
    void detachRenderbuffer(GLuint renderbuffer);

    Texture *getIncompleteTexture(SamplerType type);

    bool cullSkipsDraw(GLenum drawMode);
    bool isTriangleMode(GLenum drawMode);

    const egl::Config *const mConfig;

    State   mState;

    BindingPointer<Texture2D> mTexture2DZero;
    BindingPointer<TextureCubeMap> mTextureCubeMapZero;

    typedef std::map<GLuint, Framebuffer*> FramebufferMap;
    FramebufferMap mFramebufferMap;

    typedef std::map<GLuint, Fence*> FenceMap;
    FenceMap mFenceMap;

    void initExtensionString();
    std::string mExtensionString;

    VertexDataManager *mVertexDataManager;
    IndexDataManager *mIndexDataManager;

    Blit *mBlit;
    
    BindingPointer<Texture> mIncompleteTextures[SAMPLER_TYPE_COUNT];

    // Recorded errors
    bool mInvalidEnum;
    bool mInvalidValue;
    bool mInvalidOperation;
    bool mOutOfMemory;
    bool mInvalidFramebufferOperation;

    bool mHasBeenCurrent;

    unsigned int mAppliedProgram;
    unsigned int mAppliedRenderTargetSerial;
    unsigned int mAppliedDepthbufferSerial;
    unsigned int mAppliedStencilbufferSerial;
    bool mDepthStencilInitialized;

    bool mSupportsShaderModel3;
    int  mMaxRenderbufferDimension;
    int  mMaxTextureDimension;
    int  mMaxCubeTextureDimension;
    int  mMaxTextureLevel;
    std::map<D3DFORMAT, bool *> mMultiSampleSupport;
    GLsizei mMaxSupportedSamples;
    bool mSupportsEventQueries;
    bool mSupportsCompressedTextures;
    bool mSupportsFloatTextures;
    bool mSupportsFloatLinearFilter;
    bool mSupportsFloatRenderableTextures;
    bool mSupportsHalfFloatTextures;
    bool mSupportsHalfFloatLinearFilter;
    bool mSupportsHalfFloatRenderableTextures;
    bool mSupportsLuminanceTextures;
    bool mSupportsLuminanceAlphaTextures;
    bool mSupports32bitIndices;

    // state caching flags
    bool mClearStateDirty;
    bool mCullStateDirty;
    bool mDepthStateDirty;
    bool mMaskStateDirty;
    bool mPixelPackingStateDirty;
    bool mBlendStateDirty;
    bool mStencilStateDirty;
    bool mPolygonOffsetStateDirty;
    bool mScissorStateDirty;
    bool mSampleStateDirty;
    bool mFrontFaceDirty;
    bool mDitherStateDirty;

    IDirect3DStateBlock9 *mMaskedClearSavedState;

    D3DCAPS9 mDeviceCaps;

    ResourceManager *mResourceManager;
};
}

extern "C"
{
// Exported functions for use by EGL
gl::Context *glCreateContext(const egl::Config *config, const gl::Context *shareContext);
void glDestroyContext(gl::Context *context);
void glMakeCurrent(gl::Context *context, egl::Display *display, egl::Surface *surface);
gl::Context *glGetCurrentContext();
__eglMustCastToProperFunctionPointerType __stdcall glGetProcAddress(const char *procname);
}

#endif   // INCLUDE_CONTEXT_H_
