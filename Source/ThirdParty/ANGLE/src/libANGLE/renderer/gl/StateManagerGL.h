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
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/gl/functionsgl_typedefs.h"

#include <map>

namespace gl
{
struct Caps;
struct Data;
class State;
}

namespace rx
{

class FunctionsGL;

class StateManagerGL : angle::NonCopyable
{
  public:
    StateManagerGL(const FunctionsGL *functions, const gl::Caps &rendererCaps);

    void useProgram(GLuint program);
    void bindVertexArray(GLuint vao);
    void bindBuffer(GLenum type, GLuint buffer);
    void activeTexture(size_t unit);
    void bindTexture(GLenum type, GLuint texture);
    void setPixelUnpackState(GLint alignment, GLint rowLength);
    void bindFramebuffer(GLenum type, GLuint framebuffer);
    void bindRenderbuffer(GLenum type, GLuint renderbuffer);

    void setClearState(const gl::State &state, GLbitfield mask);

    gl::Error setDrawArraysState(const gl::Data &data, GLint first, GLsizei count);
    gl::Error setDrawElementsState(const gl::Data &data, GLsizei count, GLenum type, const GLvoid *indices,
                                   const GLvoid **outIndices);

  private:
    gl::Error setGenericDrawState(const gl::Data &data);

    void setScissorTestEnabled(bool enabled);
    void setScissor(const gl::Rectangle &scissor);

    void setViewport(const gl::Rectangle &viewport);
    void setDepthRange(float near, float far);

    void setBlendEnabled(bool enabled);
    void setBlendColor(const gl::ColorF &blendColor);
    void setBlendFuncs(GLenum sourceBlendRGB, GLenum destBlendRGB, GLenum sourceBlendAlpha, GLenum destBlendAlpha);
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
    void setMultisampleEnabled(bool enabled);
    void setRasterizerDiscardEnabled(bool enabled);
    void setLineWidth(float width);

    void setPrimitiveRestartEnabled(bool enabled);

    void setClearColor(const gl::ColorF &clearColor);
    void setClearDepth(float clearDepth);
    void setClearStencil(GLint clearStencil);

    const FunctionsGL *mFunctions;

    GLuint mProgram;
    GLuint mVAO;
    std::map<GLenum, GLuint> mBuffers;

    size_t mTextureUnitIndex;
    std::map<GLenum, std::vector<GLuint>> mTextures;

    GLint mUnpackAlignment;
    GLint mUnpackRowLength;

    std::map<GLenum, GLuint> mFramebuffers;
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
    bool mMultisampleEnabled;
    bool mRasterizerDiscardEnabled;
    float mLineWidth;

    bool mPrimitiveRestartEnabled;

    gl::ColorF mClearColor;
    float mClearDepth;
    GLint mClearStencil;
};

}

#endif // LIBANGLE_RENDERER_GL_STATEMANAGERGL_H_
