//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StateManagerGL.h: Defines a class for caching applied OpenGL state

#include "libANGLE/renderer/gl/StateManagerGL.h"

#include "libANGLE/Data.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/ProgramGL.h"
#include "libANGLE/renderer/gl/TextureGL.h"
#include "libANGLE/renderer/gl/VertexArrayGL.h"

namespace rx
{

StateManagerGL::StateManagerGL(const FunctionsGL *functions, const gl::Caps &rendererCaps)
    : mFunctions(functions),
      mProgram(0),
      mVAO(0),
      mBuffers(),
      mTextureUnitIndex(0),
      mTextures(),
      mUnpackAlignment(4),
      mUnpackRowLength(0),
      mFramebuffers(),
      mRenderbuffer(0),
      mScissorTestEnabled(false),
      mScissor(0, 0, 0, 0),
      mViewport(0, 0, 0, 0),
      mNear(0.0f),
      mFar(1.0f),
      mBlendEnabled(false),
      mBlendColor(0, 0, 0, 0),
      mSourceBlendRGB(GL_ONE),
      mDestBlendRGB(GL_ZERO),
      mSourceBlendAlpha(GL_ONE),
      mDestBlendAlpha(GL_ZERO),
      mBlendEquationRGB(GL_FUNC_ADD),
      mBlendEquationAlpha(GL_FUNC_ADD),
      mColorMaskRed(true),
      mColorMaskGreen(true),
      mColorMaskBlue(true),
      mColorMaskAlpha(true),
      mSampleAlphaToCoverageEnabled(false),
      mSampleCoverageEnabled(false),
      mSampleCoverageValue(1.0f),
      mSampleCoverageInvert(false),
      mDepthTestEnabled(false),
      mDepthFunc(GL_LESS),
      mDepthMask(true),
      mStencilTestEnabled(false),
      mStencilFrontFunc(GL_ALWAYS),
      mStencilFrontValueMask(static_cast<GLuint>(-1)),
      mStencilFrontStencilFailOp(GL_KEEP),
      mStencilFrontStencilPassDepthFailOp(GL_KEEP),
      mStencilFrontStencilPassDepthPassOp(GL_KEEP),
      mStencilFrontWritemask(static_cast<GLuint>(-1)),
      mStencilBackFunc(GL_ALWAYS),
      mStencilBackValueMask(static_cast<GLuint>(-1)),
      mStencilBackStencilFailOp(GL_KEEP),
      mStencilBackStencilPassDepthFailOp(GL_KEEP),
      mStencilBackStencilPassDepthPassOp(GL_KEEP),
      mStencilBackWritemask(static_cast<GLuint>(-1)),
      mCullFaceEnabled(false),
      mCullFace(GL_BACK),
      mFrontFace(GL_CCW),
      mPolygonOffsetFillEnabled(false),
      mPolygonOffsetFactor(0.0f),
      mPolygonOffsetUnits(0.0f),
      mMultisampleEnabled(true),
      mRasterizerDiscardEnabled(false),
      mLineWidth(1.0f),
      mPrimitiveRestartEnabled(false),
      mClearColor(0.0f, 0.0f, 0.0f, 0.0f),
      mClearDepth(1.0f),
      mClearStencil(0)
{
    ASSERT(mFunctions);

    mTextures[GL_TEXTURE_2D].resize(rendererCaps.maxCombinedTextureImageUnits);
    mTextures[GL_TEXTURE_CUBE_MAP].resize(rendererCaps.maxCombinedTextureImageUnits);
    mTextures[GL_TEXTURE_2D_ARRAY].resize(rendererCaps.maxCombinedTextureImageUnits);
    mTextures[GL_TEXTURE_3D].resize(rendererCaps.maxCombinedTextureImageUnits);

    mFramebuffers[GL_READ_FRAMEBUFFER] = 0;
    mFramebuffers[GL_DRAW_FRAMEBUFFER] = 0;
}

void StateManagerGL::useProgram(GLuint program)
{
    if (mProgram != program)
    {
        mProgram = program;
        mFunctions->useProgram(mProgram);
    }
}

void StateManagerGL::bindVertexArray(GLuint vao)
{
    if (mVAO != vao)
    {
        mVAO = vao;
        mFunctions->bindVertexArray(vao);
    }
}

void StateManagerGL::bindBuffer(GLenum type, GLuint buffer)
{
    if (mBuffers[type] != buffer)
    {
        mBuffers[type] = buffer;
        mFunctions->bindBuffer(type, buffer);
    }
}

void StateManagerGL::activeTexture(size_t unit)
{
    if (mTextureUnitIndex != unit)
    {
        mTextureUnitIndex = unit;
        mFunctions->activeTexture(GL_TEXTURE0 + mTextureUnitIndex);
    }
}

void StateManagerGL::bindTexture(GLenum type, GLuint texture)
{
    if (mTextures[type][mTextureUnitIndex] != texture)
    {
        mTextures[type][mTextureUnitIndex] = texture;
        mFunctions->bindTexture(type, texture);
    }
}

void StateManagerGL::setPixelUnpackState(GLint alignment, GLint rowLength)
{
    if (mUnpackAlignment != alignment)
    {
        mUnpackAlignment = alignment;
        mFunctions->pixelStorei(GL_UNPACK_ALIGNMENT, mUnpackAlignment);
    }

    if (mUnpackRowLength != rowLength)
    {
        mUnpackRowLength = rowLength;
        mFunctions->pixelStorei(GL_UNPACK_ROW_LENGTH, mUnpackRowLength);
    }
}

void StateManagerGL::bindFramebuffer(GLenum type, GLuint framebuffer)
{
    if (type == GL_FRAMEBUFFER)
    {
        if (mFramebuffers[GL_READ_FRAMEBUFFER] != framebuffer ||
            mFramebuffers[GL_DRAW_FRAMEBUFFER] != framebuffer)
        {
            mFramebuffers[GL_READ_FRAMEBUFFER] = framebuffer;
            mFramebuffers[GL_DRAW_FRAMEBUFFER] = framebuffer;
            mFunctions->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        }
    }
    else
    {
        if (mFramebuffers[type] != framebuffer)
        {
            mFramebuffers[type] = framebuffer;
            mFunctions->bindFramebuffer(type, framebuffer);
        }
    }
}

void StateManagerGL::bindRenderbuffer(GLenum type, GLuint renderbuffer)
{
    ASSERT(type == GL_RENDERBUFFER);
    if (mRenderbuffer != renderbuffer)
    {
        mRenderbuffer = renderbuffer;
        mFunctions->bindRenderbuffer(type, mRenderbuffer);
    }
}

void StateManagerGL::setClearState(const gl::State &state, GLbitfield mask)
{
    // Only apply the state required to do a clear
    const gl::RasterizerState &rasterizerState = state.getRasterizerState();
    setRasterizerDiscardEnabled(rasterizerState.rasterizerDiscard);
    if (!rasterizerState.rasterizerDiscard)
    {
        setScissor(state.getScissor());
        setViewport(state.getViewport());

        if ((mask & GL_COLOR_BUFFER_BIT) != 0)
        {
            setClearColor(state.getColorClearValue());

            const gl::BlendState &blendState = state.getBlendState();
            setColorMask(blendState.colorMaskRed, blendState.colorMaskGreen, blendState.colorMaskBlue, blendState.colorMaskAlpha);
        }

        if ((mask & GL_DEPTH_BUFFER_BIT) != 0)
        {
            setClearDepth(state.getDepthClearValue());
            setDepthMask(state.getDepthStencilState().depthMask);
        }

        if ((mask & GL_STENCIL_BUFFER_BIT) != 0)
        {
            setClearStencil(state.getStencilClearValue());
            setStencilFrontWritemask(state.getDepthStencilState().stencilWritemask);
            setStencilBackWritemask(state.getDepthStencilState().stencilBackWritemask);
        }
    }
}

gl::Error StateManagerGL::setDrawArraysState(const gl::Data &data, GLint first, GLsizei count)
{
    const gl::State &state = *data.state;

    const gl::VertexArray *vao = state.getVertexArray();
    const VertexArrayGL *vaoGL = GetImplAs<VertexArrayGL>(vao);
    vaoGL->syncDrawArraysState(first, count);
    bindVertexArray(vaoGL->getVertexArrayID());

    return setGenericDrawState(data);
}

gl::Error StateManagerGL::setDrawElementsState(const gl::Data &data, GLsizei count, GLenum type, const GLvoid *indices,
                                               const GLvoid **outIndices)
{
    const gl::State &state = *data.state;

    const gl::VertexArray *vao = state.getVertexArray();
    const VertexArrayGL *vaoGL = GetImplAs<VertexArrayGL>(vao);

    gl::Error error = vaoGL->syncDrawElementsState(count, type, indices, outIndices);
    if (error.isError())
    {
        return error;
    }

    bindVertexArray(vaoGL->getVertexArrayID());

    return setGenericDrawState(data);
}

gl::Error StateManagerGL::setGenericDrawState(const gl::Data &data)
{
    const gl::State &state = *data.state;
    const gl::Caps &caps = *data.caps;

    const gl::Program *program = state.getProgram();
    const ProgramGL *programGL = GetImplAs<ProgramGL>(program);
    useProgram(programGL->getProgramID());

    // TODO: Only apply textures referenced by the program.
    for (auto textureTypeIter = mTextures.begin(); textureTypeIter != mTextures.end(); textureTypeIter++)
    {
        GLenum textureType = textureTypeIter->first;

        // Determine if this texture type can exist in the source context
        bool validTextureType = (textureType == GL_TEXTURE_2D || textureType == GL_TEXTURE_CUBE_MAP ||
                                 (textureType == GL_TEXTURE_2D_ARRAY && data.clientVersion >= 3) ||
                                 (textureType == GL_TEXTURE_3D && data.clientVersion >= 3));

        const std::vector<GLuint> &textureVector = textureTypeIter->second;
        for (size_t textureUnitIndex = 0; textureUnitIndex < textureVector.size(); textureUnitIndex++)
        {
            const gl::Texture *texture = nullptr;

            bool validTextureUnit = textureUnitIndex < caps.maxCombinedTextureImageUnits;
            if (validTextureType && validTextureUnit)
            {
                texture = state.getSamplerTexture(textureUnitIndex, textureType);
            }

            if (texture != nullptr)
            {
                const TextureGL *textureGL = GetImplAs<TextureGL>(texture);
                textureGL->syncSamplerState(texture->getSamplerState());

                if (mTextures[textureType][textureUnitIndex] != textureGL->getTextureID())
                {
                    activeTexture(textureUnitIndex);
                    bindTexture(textureType, textureGL->getTextureID());
                }

                // TODO: apply sampler object if one is bound
            }
            else
            {
                if (mTextures[textureType][textureUnitIndex] != 0)
                {
                    activeTexture(textureUnitIndex);
                    bindTexture(textureType, 0);
                }
            }
        }
    }

    const gl::Framebuffer *framebuffer = state.getDrawFramebuffer();
    const FramebufferGL *framebufferGL = GetImplAs<FramebufferGL>(framebuffer);
    bindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferGL->getFramebufferID());

    setScissorTestEnabled(state.isScissorTestEnabled());
    if (state.isScissorTestEnabled())
    {
        setScissor(state.getScissor());
    }

    setViewport(state.getViewport());
    setDepthRange(state.getNearPlane(), state.getFarPlane());

    const gl::BlendState &blendState = state.getBlendState();
    setBlendEnabled(blendState.blend);
    if (blendState.blend)
    {
        setBlendColor(state.getBlendColor());
        setBlendFuncs(blendState.sourceBlendRGB, blendState.destBlendRGB, blendState.sourceBlendAlpha, blendState.destBlendAlpha);
        setBlendEquations(blendState.blendEquationRGB, blendState.blendEquationAlpha);
        setColorMask(blendState.colorMaskRed, blendState.colorMaskGreen, blendState.colorMaskBlue, blendState.colorMaskAlpha);
    }
    setSampleAlphaToCoverageEnabled(blendState.sampleAlphaToCoverage);
    setSampleCoverageEnabled(state.isSampleCoverageEnabled());
    setSampleCoverage(state.getSampleCoverageValue(), state.getSampleCoverageInvert());

    const gl::DepthStencilState &depthStencilState = state.getDepthStencilState();
    setDepthTestEnabled(depthStencilState.depthTest);
    if (depthStencilState.depthTest)
    {
        setDepthFunc(depthStencilState.depthFunc);
        setDepthMask(depthStencilState.depthMask);
    }

    setStencilTestEnabled(depthStencilState.stencilTest);
    if (depthStencilState.stencilTest)
    {
        setStencilFrontWritemask(state.getDepthStencilState().stencilWritemask);
        setStencilBackWritemask(state.getDepthStencilState().stencilBackWritemask);
        setStencilFrontFuncs(depthStencilState.stencilFunc, state.getStencilRef(), depthStencilState.stencilMask);
        setStencilBackFuncs(depthStencilState.stencilBackFunc, state.getStencilBackRef(), depthStencilState.stencilBackMask);
        setStencilFrontOps(depthStencilState.stencilFail, depthStencilState.stencilPassDepthFail, depthStencilState.stencilPassDepthPass);
        setStencilBackOps(depthStencilState.stencilBackFail, depthStencilState.stencilBackPassDepthFail, depthStencilState.stencilBackPassDepthPass);
    }

    const gl::RasterizerState &rasterizerState = state.getRasterizerState();
    setCullFaceEnabled(rasterizerState.cullFace);
    if (rasterizerState.cullFace)
    {
        setCullFace(rasterizerState.cullMode);
        setFrontFace(rasterizerState.frontFace);
    }

    setPolygonOffsetFillEnabled(rasterizerState.polygonOffsetFill);
    if (rasterizerState.polygonOffsetFill)
    {
        setPolygonOffset(rasterizerState.polygonOffsetFactor, rasterizerState.polygonOffsetUnits);
    }

    setMultisampleEnabled(rasterizerState.multiSample);
    setRasterizerDiscardEnabled(rasterizerState.rasterizerDiscard);
    setLineWidth(state.getLineWidth());

    setPrimitiveRestartEnabled(state.isPrimitiveRestartEnabled());

    return gl::Error(GL_NO_ERROR);
}

void StateManagerGL::setScissorTestEnabled(bool enabled)
{
    if (mScissorTestEnabled != enabled)
    {
        mScissorTestEnabled = enabled;
        if (mScissorTestEnabled)
        {
            mFunctions->enable(GL_SCISSOR_TEST);
        }
        else
        {
            mFunctions->disable(GL_SCISSOR_TEST);
        }
    }
}

void StateManagerGL::setScissor(const gl::Rectangle &scissor)
{
    if (scissor != mScissor)
    {
        mScissor = scissor;
        mFunctions->scissor(mScissor.x, mScissor.y, mScissor.width, mScissor.height);
    }
}

void StateManagerGL::setViewport(const gl::Rectangle &viewport)
{
    if (viewport != mViewport)
    {
        mViewport = viewport;
        mFunctions->viewport(mViewport.x, mViewport.y, mViewport.width, mViewport.height);
    }
}

void StateManagerGL::setDepthRange(float near, float far)
{
    if (mNear != near || mFar != far)
    {
        mNear = near;
        mFar = far;
        mFunctions->depthRange(mNear, mFar);
    }
}

void StateManagerGL::setBlendEnabled(bool enabled)
{
    if (mBlendEnabled != enabled)
    {
        mBlendEnabled = enabled;
        if (mBlendEnabled)
        {
            mFunctions->enable(GL_BLEND);
        }
        else
        {
            mFunctions->disable(GL_BLEND);
        }
    }
}

void StateManagerGL::setBlendColor(const gl::ColorF &blendColor)
{
    if (mBlendColor != blendColor)
    {
        mBlendColor = blendColor;
        mFunctions->blendColor(mBlendColor.red, mBlendColor.green, mBlendColor.blue, mBlendColor.alpha);
    }
}

void StateManagerGL::setBlendFuncs(GLenum sourceBlendRGB, GLenum destBlendRGB, GLenum sourceBlendAlpha,
                                   GLenum destBlendAlpha)
{
    if (mSourceBlendRGB != sourceBlendRGB || mDestBlendRGB != destBlendRGB ||
        mSourceBlendAlpha != sourceBlendAlpha || mDestBlendAlpha != destBlendAlpha)
    {
        mSourceBlendRGB = sourceBlendRGB;
        mDestBlendRGB = destBlendRGB;
        mSourceBlendAlpha = sourceBlendAlpha;
        mDestBlendAlpha = destBlendAlpha;

        mFunctions->blendFuncSeparate(mSourceBlendRGB, mDestBlendRGB, mSourceBlendAlpha, mDestBlendAlpha);
    }
}

void StateManagerGL::setBlendEquations(GLenum blendEquationRGB, GLenum blendEquationAlpha)
{
    if (mBlendEquationRGB != blendEquationRGB || mBlendEquationAlpha != blendEquationAlpha)
    {
        mBlendEquationRGB = blendEquationRGB;
        mBlendEquationAlpha = mDestBlendAlpha;

        mFunctions->blendEquationSeparate(mBlendEquationRGB, mBlendEquationAlpha);
    }
}

void StateManagerGL::setColorMask(bool red, bool green, bool blue, bool alpha)
{
    if (mColorMaskRed != red || mColorMaskGreen != green || mColorMaskBlue != blue || mColorMaskAlpha != alpha)
    {
        mColorMaskRed = red;
        mColorMaskGreen = green;
        mColorMaskBlue = blue;
        mColorMaskAlpha = alpha;
        mFunctions->colorMask(mColorMaskRed, mColorMaskGreen, mColorMaskBlue, mColorMaskAlpha);
    }
}

void StateManagerGL::setSampleAlphaToCoverageEnabled(bool enabled)
{
    if (mSampleAlphaToCoverageEnabled != enabled)
    {
        mSampleAlphaToCoverageEnabled = enabled;
        if (mSampleAlphaToCoverageEnabled)
        {
            mFunctions->enable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        }
        else
        {
            mFunctions->disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        }
    }
}

void StateManagerGL::setSampleCoverageEnabled(bool enabled)
{
    if (mSampleCoverageEnabled != enabled)
    {
        mSampleCoverageEnabled = enabled;
        if (mSampleCoverageEnabled)
        {
            mFunctions->enable(GL_SAMPLE_COVERAGE);
        }
        else
        {
            mFunctions->disable(GL_SAMPLE_COVERAGE);
        }
    }
}

void StateManagerGL::setSampleCoverage(float value, bool invert)
{
    if (mSampleCoverageValue != value || mSampleCoverageInvert != invert)
    {
        mSampleCoverageValue = value;
        mSampleCoverageInvert = invert;
        mFunctions->sampleCoverage(mSampleCoverageValue, mSampleCoverageInvert);
    }
}

void StateManagerGL::setDepthTestEnabled(bool enabled)
{
    if (mDepthTestEnabled != enabled)
    {
        mDepthTestEnabled = enabled;
        if (mDepthTestEnabled)
        {
            mFunctions->enable(GL_DEPTH_TEST);
        }
        else
        {
            mFunctions->disable(GL_DEPTH_TEST);
        }
    }
}

void StateManagerGL::setDepthFunc(GLenum depthFunc)
{
    if (mDepthFunc != depthFunc)
    {
        mDepthFunc = depthFunc;
        mFunctions->depthFunc(mDepthFunc);
    }
}

void StateManagerGL::setDepthMask(bool mask)
{
    if (mDepthMask != mask)
    {
        mDepthMask = mask;
        mFunctions->depthMask(mDepthMask);
    }
}

void StateManagerGL::setStencilTestEnabled(bool enabled)
{
    if (mStencilTestEnabled != enabled)
    {
        mStencilTestEnabled = enabled;
        if (mStencilTestEnabled)
        {
            mFunctions->enable(GL_STENCIL_TEST);
        }
        else
        {
            mFunctions->disable(GL_STENCIL_TEST);
        }
    }
}

void StateManagerGL::setStencilFrontWritemask(GLuint mask)
{
    if (mStencilFrontWritemask != mask)
    {
        mStencilFrontWritemask = mask;
        mFunctions->stencilMaskSeparate(GL_FRONT, mStencilFrontWritemask);
    }
}

void StateManagerGL::setStencilBackWritemask(GLuint mask)
{
    if (mStencilBackWritemask != mask)
    {
        mStencilBackWritemask = mask;
        mFunctions->stencilMaskSeparate(GL_BACK, mStencilBackWritemask);
    }
}

void StateManagerGL::setStencilFrontFuncs(GLenum func, GLint ref, GLuint mask)
{
    if (mStencilFrontFunc != func || mStencilFrontRef != ref || mStencilFrontValueMask != mask)
    {
        mStencilFrontFunc = func;
        mStencilFrontRef = ref;
        mStencilFrontValueMask = mask;
        mFunctions->stencilFuncSeparate(GL_FRONT, mStencilFrontFunc, mStencilFrontRef, mStencilFrontValueMask);
    }
}

void StateManagerGL::setStencilBackFuncs(GLenum func, GLint ref, GLuint mask)
{
    if (mStencilBackFunc != func || mStencilBackRef != ref || mStencilBackValueMask != mask)
    {
        mStencilBackFunc = func;
        mStencilBackRef = ref;
        mStencilBackValueMask = mask;
        mFunctions->stencilFuncSeparate(GL_BACK, mStencilBackFunc, mStencilBackRef, mStencilBackValueMask);
    }
}

void StateManagerGL::setStencilFrontOps(GLenum sfail, GLenum dpfail, GLenum dppass)
{
    if (mStencilFrontStencilFailOp != sfail || mStencilFrontStencilPassDepthFailOp != dpfail || mStencilFrontStencilPassDepthPassOp != dppass)
    {
        mStencilFrontStencilFailOp = sfail;
        mStencilFrontStencilPassDepthFailOp = dpfail;
        mStencilFrontStencilPassDepthPassOp = dppass;
        mFunctions->stencilOpSeparate(GL_FRONT, mStencilFrontStencilFailOp, mStencilFrontStencilPassDepthFailOp, mStencilFrontStencilPassDepthPassOp);
    }
}

void StateManagerGL::setStencilBackOps(GLenum sfail, GLenum dpfail, GLenum dppass)
{
    if (mStencilBackStencilFailOp != sfail || mStencilBackStencilPassDepthFailOp != dpfail || mStencilBackStencilPassDepthPassOp != dppass)
    {
        mStencilBackStencilFailOp = sfail;
        mStencilBackStencilPassDepthFailOp = dpfail;
        mStencilBackStencilPassDepthPassOp = dppass;
        mFunctions->stencilOpSeparate(GL_BACK, mStencilBackStencilFailOp, mStencilBackStencilPassDepthFailOp, mStencilBackStencilPassDepthPassOp);
    }
}

void StateManagerGL::setCullFaceEnabled(bool enabled)
{
    if (mCullFaceEnabled != enabled)
    {
        mCullFaceEnabled = enabled;
        if (mCullFaceEnabled)
        {
            mFunctions->enable(GL_CULL_FACE);
        }
        else
        {
            mFunctions->disable(GL_CULL_FACE);
        }
    }
}

void StateManagerGL::setCullFace(GLenum cullFace)
{
    if (mCullFace != cullFace)
    {
        mCullFace = cullFace;
        mFunctions->cullFace(mCullFace);
    }
}

void StateManagerGL::setFrontFace(GLenum frontFace)
{
    if (mFrontFace != frontFace)
    {
        mFrontFace = frontFace;
        mFunctions->frontFace(mFrontFace);
    }
}

void StateManagerGL::setPolygonOffsetFillEnabled(bool enabled)
{
    if (mPolygonOffsetFillEnabled != enabled)
    {
        mPolygonOffsetFillEnabled = enabled;
        if (mPolygonOffsetFillEnabled)
        {
            mFunctions->enable(GL_POLYGON_OFFSET_FILL);
        }
        else
        {
            mFunctions->disable(GL_POLYGON_OFFSET_FILL);
        }
    }
}

void StateManagerGL::setPolygonOffset(float factor, float units)
{
    if (mPolygonOffsetFactor != factor || mPolygonOffsetUnits != units)
    {
        mPolygonOffsetFactor = factor;
        mPolygonOffsetUnits = units;
        mFunctions->polygonOffset(mPolygonOffsetFactor, mPolygonOffsetUnits);
    }
}

void StateManagerGL::setMultisampleEnabled(bool enabled)
{
    if (mMultisampleEnabled != enabled)
    {
        mMultisampleEnabled = enabled;
        if (mMultisampleEnabled)
        {
            mFunctions->enable(GL_MULTISAMPLE);
        }
        else
        {
            mFunctions->disable(GL_MULTISAMPLE);
        }
    }
}

void StateManagerGL::setRasterizerDiscardEnabled(bool enabled)
{
    if (mRasterizerDiscardEnabled != enabled)
    {
        mRasterizerDiscardEnabled = enabled;
        if (mRasterizerDiscardEnabled)
        {
            mFunctions->enable(GL_RASTERIZER_DISCARD);
        }
        else
        {
            mFunctions->disable(GL_RASTERIZER_DISCARD);
        }
    }
}

void StateManagerGL::setLineWidth(float width)
{
    if (mLineWidth != width)
    {
        mLineWidth = width;
        mFunctions->lineWidth(mLineWidth);
    }
}

void StateManagerGL::setPrimitiveRestartEnabled(bool enabled)
{
    if (mPrimitiveRestartEnabled != enabled)
    {
        mPrimitiveRestartEnabled = enabled;

        if (mPrimitiveRestartEnabled)
        {
            mFunctions->enable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        }
        else
        {
            mFunctions->disable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        }
    }
}

void StateManagerGL::setClearDepth(float clearDepth)
{
    if (mClearDepth != clearDepth)
    {
        mClearDepth = clearDepth;
        mFunctions->clearDepth(mClearDepth);
    }
}

void StateManagerGL::setClearColor(const gl::ColorF &clearColor)
{
    if (mClearColor != clearColor)
    {
        mClearColor = clearColor;
        mFunctions->clearColor(mClearColor.red, mClearColor.green, mClearColor.blue, mClearColor.alpha);
    }
}

void StateManagerGL::setClearStencil(GLint clearStencil)
{
    if (mClearStencil != clearStencil)
    {
        mClearStencil = clearStencil;
        mFunctions->clearStencil(mClearStencil);
    }
}

}
