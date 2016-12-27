//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StateManagerGL.h: Defines a class for caching applied OpenGL state

#ifndef LIBANGLE_RENDERER_GL_STATEMANAGERGL_H_
#define LIBANGLE_RENDERER_GL_STATEMANAGERGL_H_

#include "common/debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/State.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/gl/functionsgl_typedefs.h"

#include <map>

namespace gl
{
struct Caps;
class ContextState;
class State;
}

namespace rx
{

class FunctionsGL;
class TransformFeedbackGL;
class QueryGL;

class StateManagerGL final : angle::NonCopyable
{
  public:
    StateManagerGL(const FunctionsGL *functions, const gl::Caps &rendererCaps);

    void deleteProgram(GLuint program);
    void deleteVertexArray(GLuint vao);
    void deleteTexture(GLuint texture);
    void deleteSampler(GLuint sampler);
    void deleteBuffer(GLuint buffer);
    void deleteFramebuffer(GLuint fbo);
    void deleteRenderbuffer(GLuint rbo);
    void deleteTransformFeedback(GLuint transformFeedback);
    void deleteQuery(GLuint query);

    void useProgram(GLuint program);
    void forceUseProgram(GLuint program);
    void bindVertexArray(GLuint vao, GLuint elementArrayBuffer);
    void bindBuffer(GLenum type, GLuint buffer);
    void bindBufferBase(GLenum type, size_t index, GLuint buffer);
    void bindBufferRange(GLenum type, size_t index, GLuint buffer, size_t offset, size_t size);
    void activeTexture(size_t unit);
    void bindTexture(GLenum type, GLuint texture);
    void bindSampler(size_t unit, GLuint sampler);
    void bindFramebuffer(GLenum type, GLuint framebuffer);
    void bindRenderbuffer(GLenum type, GLuint renderbuffer);
    void bindTransformFeedback(GLenum type, GLuint transformFeedback);
    void beginQuery(GLenum type, GLuint query);
    void endQuery(GLenum type, GLuint query);
    void onBeginQuery(QueryGL *query);

    void setAttributeCurrentData(size_t index, const gl::VertexAttribCurrentValueData &data);

    void setScissorTestEnabled(bool enabled);
    void setScissor(const gl::Rectangle &scissor);

    void setViewport(const gl::Rectangle &viewport);
    void setDepthRange(float near, float far);

    void setBlendEnabled(bool enabled);
    void setBlendColor(const gl::ColorF &blendColor);
    void setBlendFuncs(GLenum sourceBlendRGB,
                       GLenum destBlendRGB,
                       GLenum sourceBlendAlpha,
                       GLenum destBlendAlpha);
    void setBlendEquations(GLenum blendEquationRGB, GLenum blendEquationAlpha);
    void setColorMask(bool red, bool green, bool blue, bool alpha);
    void setSampleAlphaToCoverageEnabled(bool enabled);
    void setSampleCoverageEnabled(bool enabled);
    void setSampleCoverage(float value, bool invert);

    void setDepthTestEnabled(bool enabled);
    void setDepthFunc(GLenum depthFunc);
    void setDepthMask(bool mask);
    void setStencilTestEnabled(bool enabled);
    void setStencilFrontWritemask(GLuint mask);
    void setStencilBackWritemask(GLuint mask);
    void setStencilFrontFuncs(GLenum func, GLint ref, GLuint mask);
    void setStencilBackFuncs(GLenum func, GLint ref, GLuint mask);
    void setStencilFrontOps(GLenum sfail, GLenum dpfail, GLenum dppass);
    void setStencilBackOps(GLenum sfail, GLenum dpfail, GLenum dppass);

    void setCullFaceEnabled(bool enabled);
    void setCullFace(GLenum cullFace);
    void setFrontFace(GLenum frontFace);
    void setPolygonOffsetFillEnabled(bool enabled);
    void setPolygonOffset(float factor, float units);
    void setRasterizerDiscardEnabled(bool enabled);
    void setLineWidth(float width);

    void setPrimitiveRestartEnabled(bool enabled);

    void setClearColor(const gl::ColorF &clearColor);
    void setClearDepth(float clearDepth);
    void setClearStencil(GLint clearStencil);

    void setPixelUnpackState(const gl::PixelUnpackState &unpack);
    void setPixelUnpackState(GLint alignment,
                             GLint rowLength,
                             GLint skipRows,
                             GLint skipPixels,
                             GLint imageHeight,
                             GLint skipImages,
                             GLuint unpackBuffer);
    void setPixelPackState(const gl::PixelPackState &pack);
    void setPixelPackState(GLint alignment,
                           GLint rowLength,
                           GLint skipRows,
                           GLint skipPixels,
                           GLuint packBuffer);

    void setFramebufferSRGBEnabled(bool enabled);

    void setDitherEnabled(bool enabled);

    void setMultisamplingStateEnabled(bool enabled);
    void setSampleAlphaToOneStateEnabled(bool enabled);

    void setCoverageModulation(GLenum components);

    void setPathRenderingModelViewMatrix(const GLfloat *m);
    void setPathRenderingProjectionMatrix(const GLfloat *m);
    void setPathRenderingStencilState(GLenum func, GLint ref, GLuint mask);

    void onDeleteQueryObject(QueryGL *query);

    gl::Error setDrawArraysState(const gl::ContextState &data,
                                 GLint first,
                                 GLsizei count,
                                 GLsizei instanceCount);
    gl::Error setDrawElementsState(const gl::ContextState &data,
                                   GLsizei count,
                                   GLenum type,
                                   const GLvoid *indices,
                                   GLsizei instanceCount,
                                   const GLvoid **outIndices);

    gl::Error pauseTransformFeedback(const gl::ContextState &data);
    gl::Error onMakeCurrent(const gl::ContextState &data);

    void syncState(const gl::State &state, const gl::State::DirtyBits &glDirtyBits);

    GLuint getBoundBuffer(GLenum type);

  private:
    gl::Error setGenericDrawState(const gl::ContextState &data);

    void setTextureCubemapSeamlessEnabled(bool enabled);

    const FunctionsGL *mFunctions;

    GLuint mProgram;

    GLuint mVAO;
    std::vector<gl::VertexAttribCurrentValueData> mVertexAttribCurrentValues;

    std::map<GLenum, GLuint> mBuffers;

    struct IndexedBufferBinding
    {
        IndexedBufferBinding();

        size_t offset;
        size_t size;
        GLuint buffer;
    };
    std::map<GLenum, std::vector<IndexedBufferBinding>> mIndexedBuffers;

    size_t mTextureUnitIndex;
    std::map<GLenum, std::vector<GLuint>> mTextures;
    std::vector<GLuint> mSamplers;

    GLuint mTransformFeedback;

    std::map<GLenum, GLuint> mQueries;

    TransformFeedbackGL *mPrevDrawTransformFeedback;
    std::set<QueryGL *> mCurrentQueries;
    uintptr_t mPrevDrawContext;

    GLint mUnpackAlignment;
    GLint mUnpackRowLength;
    GLint mUnpackSkipRows;
    GLint mUnpackSkipPixels;
    GLint mUnpackImageHeight;
    GLint mUnpackSkipImages;

    GLint mPackAlignment;
    GLint mPackRowLength;
    GLint mPackSkipRows;
    GLint mPackSkipPixels;

    // TODO(jmadill): Convert to std::array when available
    std::vector<GLenum> mFramebuffers;
    GLuint mRenderbuffer;

    bool mScissorTestEnabled;
    gl::Rectangle mScissor;

    gl::Rectangle mViewport;
    float mNear;
    float mFar;

    bool mBlendEnabled;
    gl::ColorF mBlendColor;
    GLenum mSourceBlendRGB;
    GLenum mDestBlendRGB;
    GLenum mSourceBlendAlpha;
    GLenum mDestBlendAlpha;
    GLenum mBlendEquationRGB;
    GLenum mBlendEquationAlpha;
    bool mColorMaskRed;
    bool mColorMaskGreen;
    bool mColorMaskBlue;
    bool mColorMaskAlpha;
    bool mSampleAlphaToCoverageEnabled;
    bool mSampleCoverageEnabled;
    float mSampleCoverageValue;
    bool mSampleCoverageInvert;

    bool mDepthTestEnabled;
    GLenum mDepthFunc;
    bool mDepthMask;
    bool mStencilTestEnabled;
    GLenum mStencilFrontFunc;
    GLint mStencilFrontRef;
    GLuint mStencilFrontValueMask;
    GLenum mStencilFrontStencilFailOp;
    GLenum mStencilFrontStencilPassDepthFailOp;
    GLenum mStencilFrontStencilPassDepthPassOp;
    GLuint mStencilFrontWritemask;
    GLenum mStencilBackFunc;
    GLint mStencilBackRef;
    GLuint mStencilBackValueMask;
    GLenum mStencilBackStencilFailOp;
    GLenum mStencilBackStencilPassDepthFailOp;
    GLenum mStencilBackStencilPassDepthPassOp;
    GLuint mStencilBackWritemask;

    bool mCullFaceEnabled;
    GLenum mCullFace;
    GLenum mFrontFace;
    bool mPolygonOffsetFillEnabled;
    GLfloat mPolygonOffsetFactor;
    GLfloat mPolygonOffsetUnits;
    bool mRasterizerDiscardEnabled;
    float mLineWidth;

    bool mPrimitiveRestartEnabled;

    gl::ColorF mClearColor;
    float mClearDepth;
    GLint mClearStencil;

    bool mFramebufferSRGBEnabled;
    bool mDitherEnabled;
    bool mTextureCubemapSeamlessEnabled;

    bool mMultisamplingEnabled;
    bool mSampleAlphaToOneEnabled;

    GLenum mCoverageModulation;

    GLfloat mPathMatrixMV[16];
    GLfloat mPathMatrixProj[16];
    GLenum mPathStencilFunc;
    GLint mPathStencilRef;
    GLuint mPathStencilMask;

    gl::State::DirtyBits mLocalDirtyBits;
};

}

#endif // LIBANGLE_RENDERER_GL_STATEMANAGERGL_H_
