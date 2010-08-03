//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.h: Defines the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#ifndef INCLUDE_CONTEXT_H_
#define INCLUDE_CONTEXT_H_

#define GL_APICALL
#include <GLES2/gl2.h>
#define EGLAPI
#include <EGL/egl.h>
#include <d3d9.h>

#include <map>

#include "common/angleutils.h"

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
class Colorbuffer;
class Depthbuffer;
class Stencilbuffer;
class VertexDataManager;
class IndexDataManager;
class BufferBackEnd;
class Blit;

enum
{
    MAX_VERTEX_ATTRIBS = 12,
    MAX_VERTEX_UNIFORM_VECTORS = 128,
    MAX_VARYING_VECTORS = 8,
    MAX_COMBINED_TEXTURE_IMAGE_UNITS = 16,
    MAX_VERTEX_TEXTURE_IMAGE_UNITS = 0,
    MAX_TEXTURE_IMAGE_UNITS = 16,
    MAX_FRAGMENT_UNIFORM_VECTORS = 16,
    MAX_RENDERBUFFER_SIZE = 4096,   // FIXME: Verify
    MAX_DRAW_BUFFERS = 1,

    IMPLEMENTATION_COLOR_READ_FORMAT = GL_RGB,
    IMPLEMENTATION_COLOR_READ_TYPE = GL_UNSIGNED_SHORT_5_6_5
};

const float ALIASED_LINE_WIDTH_RANGE_MIN = 1.0f;
const float ALIASED_LINE_WIDTH_RANGE_MAX = 1.0f;
const float ALIASED_POINT_SIZE_RANGE_MIN = 1.0f;
const float ALIASED_POINT_SIZE_RANGE_MAX = 1.0f;

enum SamplerType
{
    SAMPLER_2D,
    SAMPLER_CUBE,

    SAMPLER_TYPE_COUNT
};

struct Color
{
    float red;
    float green;
    float blue;
    float alpha;
};

// Helper structure describing a single vertex attribute
class AttributeState
{
  public:
    AttributeState()
        : mType(GL_FLOAT), mSize(0), mNormalized(false), mStride(0), mPointer(NULL), mBoundBuffer(0), mEnabled(false)
    {
        mCurrentValue[0] = 0;
        mCurrentValue[1] = 0;
        mCurrentValue[2] = 0;
        mCurrentValue[3] = 1;
    }

    // From VertexArrayPointer
    GLenum mType;
    GLint mSize;
    bool mNormalized;
    GLsizei mStride; // 0 means natural stride
    const void *mPointer;

    GLuint mBoundBuffer; // Captured when VertexArrayPointer is called.

    bool mEnabled; // From Enable/DisableVertexAttribArray

    float mCurrentValue[4]; // From VertexAttrib4f
};

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
    GLuint arrayBuffer;
    GLuint elementArrayBuffer;
    GLuint texture2D;
    GLuint textureCubeMap;
    GLuint framebuffer;
    GLuint renderbuffer;
    GLuint currentProgram;

    AttributeState vertexAttribute[MAX_VERTEX_ATTRIBS];
    GLuint samplerTexture[SAMPLER_TYPE_COUNT][MAX_TEXTURE_IMAGE_UNITS];

    GLint unpackAlignment;
    GLint packAlignment;
};

class Context
{
  public:
    Context(const egl::Config *config);

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

    void setViewportParams(GLint x, GLint y, GLsizei width, GLsizei height);

    void setScissorParams(GLint x, GLint y, GLsizei width, GLsizei height);

    void setColorMask(bool red, bool green, bool blue, bool alpha);
    void setDepthMask(bool mask);

    void setActiveSampler(int active);

    GLuint getFramebufferHandle() const;
    GLuint getRenderbufferHandle() const;

    GLuint getArrayBufferHandle() const;

    void setVertexAttribEnabled(unsigned int attribNum, bool enabled);
    const AttributeState &getVertexAttribState(unsigned int attribNum);
    void setVertexAttribState(unsigned int attribNum, GLuint boundBuffer, GLint size, GLenum type,
                              bool normalized, GLsizei stride, const void *pointer);
    const void *getVertexAttribPointer(unsigned int attribNum) const;

    const AttributeState *getVertexAttribBlock();

    void setUnpackAlignment(GLint alignment);
    GLint getUnpackAlignment() const;

    void setPackAlignment(GLint alignment);
    GLint getPackAlignment() const;

    GLuint createBuffer();
    GLuint createShader(GLenum type);
    GLuint createProgram();
    GLuint createTexture();
    GLuint createFramebuffer();
    GLuint createRenderbuffer();

    void deleteBuffer(GLuint buffer);
    void deleteShader(GLuint shader);
    void deleteProgram(GLuint program);
    void deleteTexture(GLuint texture);
    void deleteFramebuffer(GLuint framebuffer);
    void deleteRenderbuffer(GLuint renderbuffer);

    void bindArrayBuffer(GLuint buffer);
    void bindElementArrayBuffer(GLuint buffer);
    void bindTexture2D(GLuint texture);
    void bindTextureCubeMap(GLuint texture);
    void bindFramebuffer(GLuint framebuffer);
    void bindRenderbuffer(GLuint renderbuffer);
    void useProgram(GLuint program);

    void setFramebufferZero(Framebuffer *framebuffer);
    void setColorbufferZero(Colorbuffer *renderbuffer);
    void setDepthbufferZero(Depthbuffer *depthBuffer);
    void setStencilbufferZero(Stencilbuffer *stencilBuffer);
    void setRenderbuffer(Renderbuffer *renderbuffer);

    void setVertexAttrib(GLuint index, const GLfloat *values);

    Buffer *getBuffer(GLuint handle);
    Shader *getShader(GLuint handle);
    Program *getProgram(GLuint handle);
    Texture *getTexture(GLuint handle);
    Framebuffer *getFramebuffer(GLuint handle);
    Renderbuffer *getRenderbuffer(GLuint handle);
    Colorbuffer *getColorbuffer(GLuint handle);
    Depthbuffer *getDepthbuffer(GLuint handle);
    Stencilbuffer *getStencilbuffer(GLuint handle);

    Buffer *getArrayBuffer();
    Buffer *getElementArrayBuffer();
    Program *getCurrentProgram();
    Texture2D *getTexture2D();
    TextureCubeMap *getTextureCubeMap();
    Texture *getSamplerTexture(unsigned int sampler, SamplerType type);
    Framebuffer *getFramebuffer();

    bool getFloatv(GLenum pname, GLfloat *params);
    bool getIntegerv(GLenum pname, GLint *params);
    bool getBooleanv(GLenum pname, GLboolean *params);

    bool getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams);

    bool applyRenderTarget(bool ignoreViewport);
    void applyState(GLenum drawMode);
    GLenum applyVertexBuffer(GLenum mode, GLint first, GLsizei count, bool *useIndexing, TranslatedIndexData *indexInfo);
    GLenum applyVertexBuffer(const TranslatedIndexData &indexInfo);
    GLenum applyIndexBuffer(const void *indices, GLsizei count, GLenum mode, GLenum type, TranslatedIndexData *indexInfo);
    GLenum applyCountingIndexBuffer(GLenum mode, GLenum count, TranslatedIndexData *indexInfo);
    void applyShaders();
    void applyTextures();

    void readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
    void clear(GLbitfield mask);
    void drawArrays(GLenum mode, GLint first, GLsizei count);
    void drawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
    void finish();
    void flush();

    void recordInvalidEnum();
    void recordInvalidValue();
    void recordInvalidOperation();
    void recordOutOfMemory();
    void recordInvalidFramebufferOperation();

    GLenum getError();

    const char *getPixelShaderProfile();
    const char *getVertexShaderProfile();

    const char *getExtensionString() const;

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
    bool hasStencil();

    const egl::Config *const mConfig;

    State   mState;

    Texture2D *mTexture2DZero;
    TextureCubeMap *mTextureCubeMapZero;

    Colorbuffer *mColorbufferZero;
    Depthbuffer *mDepthbufferZero;
    Stencilbuffer *mStencilbufferZero;

    typedef std::map<GLuint, Buffer*> BufferMap;
    BufferMap mBufferMap;

    typedef std::map<GLuint, Shader*> ShaderMap;
    ShaderMap mShaderMap;

    typedef std::map<GLuint, Program*> ProgramMap;
    ProgramMap mProgramMap;

    typedef std::map<GLuint, Texture*> TextureMap;
    TextureMap mTextureMap;

    typedef std::map<GLuint, Framebuffer*> FramebufferMap;
    FramebufferMap mFramebufferMap;

    typedef std::map<GLuint, Renderbuffer*> RenderbufferMap;
    RenderbufferMap mRenderbufferMap;

    void initExtensionString();
    std::string mExtensionString;

    BufferBackEnd *mBufferBackEnd;
    VertexDataManager *mVertexDataManager;
    IndexDataManager *mIndexDataManager;

    Blit *mBlit;

    Texture *mIncompleteTextures[SAMPLER_TYPE_COUNT];

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

    const char *mPsProfile;
    const char *mVsProfile;

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
};
}

extern "C"
{
// Exported functions for use by EGL
gl::Context *glCreateContext(const egl::Config *config);
void glDestroyContext(gl::Context *context);
void glMakeCurrent(gl::Context *context, egl::Display *display, egl::Surface *surface);
gl::Context *glGetCurrentContext();
__eglMustCastToProperFunctionPointerType __stdcall glGetProcAddress(const char *procname);
}

#endif   // INCLUDE_CONTEXT_H_
