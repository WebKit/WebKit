//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StateManagerGL.h: Defines a class for caching applied OpenGL state

#include "libANGLE/renderer/gl/StateManagerGL.h"

#include <limits>
#include <string.h>

#include "common/bitset_utils.h"
#include "common/mathutil.h"
#include "common/matrix_utils.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/Query.h"
#include "libANGLE/renderer/gl/BufferGL.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/ProgramGL.h"
#include "libANGLE/renderer/gl/SamplerGL.h"
#include "libANGLE/renderer/gl/TextureGL.h"
#include "libANGLE/renderer/gl/TransformFeedbackGL.h"
#include "libANGLE/renderer/gl/VertexArrayGL.h"
#include "libANGLE/renderer/gl/QueryGL.h"

namespace rx
{

static const GLenum QueryTypes[] = {GL_ANY_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, GL_TIME_ELAPSED,
                                    GL_COMMANDS_COMPLETED_CHROMIUM};

StateManagerGL::IndexedBufferBinding::IndexedBufferBinding() : offset(0), size(0), buffer(0)
{
}

StateManagerGL::StateManagerGL(const FunctionsGL *functions, const gl::Caps &rendererCaps)
    : mFunctions(functions),
      mProgram(0),
      mVAO(0),
      mVertexAttribCurrentValues(rendererCaps.maxVertexAttributes),
      mBuffers(),
      mIndexedBuffers(),
      mTextureUnitIndex(0),
      mTextures(),
      mSamplers(rendererCaps.maxCombinedTextureImageUnits, 0),
      mTransformFeedback(0),
      mQueries(),
      mPrevDrawTransformFeedback(nullptr),
      mCurrentQueries(),
      mPrevDrawContext(0),
      mUnpackAlignment(4),
      mUnpackRowLength(0),
      mUnpackSkipRows(0),
      mUnpackSkipPixels(0),
      mUnpackImageHeight(0),
      mUnpackSkipImages(0),
      mPackAlignment(4),
      mPackRowLength(0),
      mPackSkipRows(0),
      mPackSkipPixels(0),
      mFramebuffers(angle::FramebufferBindingSingletonMax, 0),
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
      mStencilFrontRef(0),
      mStencilFrontValueMask(static_cast<GLuint>(-1)),
      mStencilFrontStencilFailOp(GL_KEEP),
      mStencilFrontStencilPassDepthFailOp(GL_KEEP),
      mStencilFrontStencilPassDepthPassOp(GL_KEEP),
      mStencilFrontWritemask(static_cast<GLuint>(-1)),
      mStencilBackFunc(GL_ALWAYS),
      mStencilBackRef(0),
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
      mRasterizerDiscardEnabled(false),
      mLineWidth(1.0f),
      mPrimitiveRestartEnabled(false),
      mClearColor(0.0f, 0.0f, 0.0f, 0.0f),
      mClearDepth(1.0f),
      mClearStencil(0),
      mFramebufferSRGBEnabled(false),
      mDitherEnabled(true),
      mTextureCubemapSeamlessEnabled(false),
      mMultisamplingEnabled(true),
      mSampleAlphaToOneEnabled(false),
      mCoverageModulation(GL_NONE),
      mPathStencilFunc(GL_ALWAYS),
      mPathStencilRef(0),
      mPathStencilMask(std::numeric_limits<GLuint>::max()),
      mLocalDirtyBits()
{
    ASSERT(mFunctions);

    mTextures[GL_TEXTURE_2D].resize(rendererCaps.maxCombinedTextureImageUnits);
    mTextures[GL_TEXTURE_CUBE_MAP].resize(rendererCaps.maxCombinedTextureImageUnits);
    mTextures[GL_TEXTURE_2D_ARRAY].resize(rendererCaps.maxCombinedTextureImageUnits);
    mTextures[GL_TEXTURE_3D].resize(rendererCaps.maxCombinedTextureImageUnits);
    mTextures[GL_TEXTURE_2D_MULTISAMPLE].resize(rendererCaps.maxCombinedTextureImageUnits);

    mIndexedBuffers[GL_UNIFORM_BUFFER].resize(rendererCaps.maxCombinedUniformBlocks);

    for (GLenum queryType : QueryTypes)
    {
        mQueries[queryType] = 0;
    }

    // Initialize point sprite state for desktop GL
    if (mFunctions->standard == STANDARD_GL_DESKTOP)
    {
        mFunctions->enable(GL_PROGRAM_POINT_SIZE);

        // GL_POINT_SPRITE was deprecated in the core profile. Point rasterization is always
        // performed
        // as though POINT_SPRITE were enabled.
        if ((mFunctions->profile & GL_CONTEXT_CORE_PROFILE_BIT) == 0)
        {
            mFunctions->enable(GL_POINT_SPRITE);
        }
    }

    angle::Matrix<GLfloat>::setToIdentity(mPathMatrixProj);
    angle::Matrix<GLfloat>::setToIdentity(mPathMatrixMV);
}

void StateManagerGL::deleteProgram(GLuint program)
{
    if (program != 0)
    {
        if (mProgram == program)
        {
            useProgram(0);
        }

        mFunctions->deleteProgram(program);
    }
}

void StateManagerGL::deleteVertexArray(GLuint vao)
{
    if (vao != 0)
    {
        if (mVAO == vao)
        {
            bindVertexArray(0, 0);
        }

        mFunctions->deleteVertexArrays(1, &vao);
    }
}

void StateManagerGL::deleteTexture(GLuint texture)
{
    if (texture != 0)
    {
        for (const auto &textureTypeIter : mTextures)
        {
            const std::vector<GLuint> &textureVector = textureTypeIter.second;
            for (size_t textureUnitIndex = 0; textureUnitIndex < textureVector.size(); textureUnitIndex++)
            {
                if (textureVector[textureUnitIndex] == texture)
                {
                    activeTexture(textureUnitIndex);
                    bindTexture(textureTypeIter.first, 0);
                }
            }
        }

        mFunctions->deleteTextures(1, &texture);
    }
}

void StateManagerGL::deleteSampler(GLuint sampler)
{
    if (sampler != 0)
    {
        for (size_t unit = 0; unit < mSamplers.size(); unit++)
        {
            if (mSamplers[unit] == sampler)
            {
                bindSampler(unit, 0);
            }
        }

        mFunctions->deleteSamplers(1, &sampler);
    }
}

void StateManagerGL::deleteBuffer(GLuint buffer)
{
    if (buffer != 0)
    {
        for (const auto &bufferTypeIter : mBuffers)
        {
            if (bufferTypeIter.second == buffer)
            {
                bindBuffer(bufferTypeIter.first, 0);
            }
        }

        for (const auto &bufferTypeIter : mIndexedBuffers)
        {
            for (size_t bindIndex = 0; bindIndex < bufferTypeIter.second.size(); bindIndex++)
            {
                if (bufferTypeIter.second[bindIndex].buffer == buffer)
                {
                    bindBufferBase(bufferTypeIter.first, bindIndex, 0);
                }
            }
        }

        mFunctions->deleteBuffers(1, &buffer);
    }
}

void StateManagerGL::deleteFramebuffer(GLuint fbo)
{
    if (fbo != 0)
    {
        for (size_t binding = 0; binding < mFramebuffers.size(); ++binding)
        {
            if (mFramebuffers[binding] == fbo)
            {
                GLenum enumValue = angle::FramebufferBindingToEnum(
                    static_cast<angle::FramebufferBinding>(binding));
                bindFramebuffer(enumValue, 0);
            }
            mFunctions->deleteFramebuffers(1, &fbo);
        }
    }
}

void StateManagerGL::deleteRenderbuffer(GLuint rbo)
{
    if (rbo != 0)
    {
        if (mRenderbuffer == rbo)
        {
            bindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        mFunctions->deleteRenderbuffers(1, &rbo);
    }
}

void StateManagerGL::deleteTransformFeedback(GLuint transformFeedback)
{
    if (transformFeedback != 0)
    {
        if (mTransformFeedback == transformFeedback)
        {
            bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
        }

        if (mPrevDrawTransformFeedback != nullptr &&
            mPrevDrawTransformFeedback->getTransformFeedbackID() == transformFeedback)
        {
            mPrevDrawTransformFeedback = nullptr;
        }

        mFunctions->deleteTransformFeedbacks(1, &transformFeedback);
    }
}

void StateManagerGL::deleteQuery(GLuint query)
{
    if (query != 0)
    {
        for (auto &activeQuery : mQueries)
        {
            GLuint activeQueryID = activeQuery.second;
            if (activeQueryID == query)
            {
                GLenum type = activeQuery.first;
                endQuery(type, query);
            }
        }
    }
}

void StateManagerGL::useProgram(GLuint program)
{
    if (mProgram != program)
    {
        forceUseProgram(program);
    }
}

void StateManagerGL::forceUseProgram(GLuint program)
{
    mProgram = program;
    mFunctions->useProgram(mProgram);
}

void StateManagerGL::bindVertexArray(GLuint vao, GLuint elementArrayBuffer)
{
    if (mVAO != vao)
    {
        mVAO = vao;
        mBuffers[GL_ELEMENT_ARRAY_BUFFER] = elementArrayBuffer;
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

void StateManagerGL::bindBufferBase(GLenum type, size_t index, GLuint buffer)
{
    auto &binding = mIndexedBuffers[type][index];
    if (binding.buffer != buffer || binding.offset != static_cast<size_t>(-1) ||
        binding.size != static_cast<size_t>(-1))
    {
        binding.buffer = buffer;
        binding.offset = static_cast<size_t>(-1);
        binding.size = static_cast<size_t>(-1);
        mFunctions->bindBufferBase(type, static_cast<GLuint>(index), buffer);
    }
}

void StateManagerGL::bindBufferRange(GLenum type,
                                     size_t index,
                                     GLuint buffer,
                                     size_t offset,
                                     size_t size)
{
    auto &binding = mIndexedBuffers[type][index];
    if (binding.buffer != buffer || binding.offset != offset || binding.size != size)
    {
        binding.buffer = buffer;
        binding.offset = offset;
        binding.size = size;
        mFunctions->bindBufferRange(type, static_cast<GLuint>(index), buffer, offset, size);
    }
}

void StateManagerGL::activeTexture(size_t unit)
{
    if (mTextureUnitIndex != unit)
    {
        mTextureUnitIndex = unit;
        mFunctions->activeTexture(GL_TEXTURE0 + static_cast<GLenum>(mTextureUnitIndex));
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

void StateManagerGL::bindSampler(size_t unit, GLuint sampler)
{
    if (mSamplers[unit] != sampler)
    {
        mSamplers[unit] = sampler;
        mFunctions->bindSampler(static_cast<GLuint>(unit), sampler);
    }
}

void StateManagerGL::setPixelUnpackState(const gl::PixelUnpackState &unpack)
{
    GLuint unpackBufferID          = 0;
    const gl::Buffer *unpackBuffer = unpack.pixelBuffer.get();
    if (unpackBuffer != nullptr)
    {
        unpackBufferID = GetImplAs<BufferGL>(unpackBuffer)->getBufferID();
    }
    setPixelUnpackState(unpack.alignment, unpack.rowLength, unpack.skipRows, unpack.skipPixels,
                        unpack.imageHeight, unpack.skipImages, unpackBufferID);
}

void StateManagerGL::setPixelUnpackState(GLint alignment,
                                         GLint rowLength,
                                         GLint skipRows,
                                         GLint skipPixels,
                                         GLint imageHeight,
                                         GLint skipImages,
                                         GLuint unpackBuffer)
{
    if (mUnpackAlignment != alignment)
    {
        mUnpackAlignment = alignment;
        mFunctions->pixelStorei(GL_UNPACK_ALIGNMENT, mUnpackAlignment);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_UNPACK_ALIGNMENT);
    }

    if (mUnpackRowLength != rowLength)
    {
        mUnpackRowLength = rowLength;
        mFunctions->pixelStorei(GL_UNPACK_ROW_LENGTH, mUnpackRowLength);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_UNPACK_ROW_LENGTH);
    }

    if (mUnpackSkipRows != skipRows)
    {
        mUnpackSkipRows = skipRows;
        mFunctions->pixelStorei(GL_UNPACK_SKIP_ROWS, mUnpackSkipRows);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_UNPACK_SKIP_ROWS);
    }

    if (mUnpackSkipPixels != skipPixels)
    {
        mUnpackSkipPixels = skipPixels;
        mFunctions->pixelStorei(GL_UNPACK_SKIP_PIXELS, mUnpackSkipPixels);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_UNPACK_SKIP_PIXELS);
    }

    if (mUnpackImageHeight != imageHeight)
    {
        mUnpackImageHeight = imageHeight;
        mFunctions->pixelStorei(GL_UNPACK_IMAGE_HEIGHT, mUnpackImageHeight);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_UNPACK_IMAGE_HEIGHT);
    }

    if (mUnpackSkipImages != skipImages)
    {
        mUnpackSkipImages = skipImages;
        mFunctions->pixelStorei(GL_UNPACK_SKIP_IMAGES, mUnpackSkipImages);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_UNPACK_SKIP_IMAGES);
    }

    bindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
}

void StateManagerGL::setPixelPackState(const gl::PixelPackState &pack)
{
    GLuint packBufferID          = 0;
    const gl::Buffer *packBuffer = pack.pixelBuffer.get();
    if (packBuffer != nullptr)
    {
        packBufferID = GetImplAs<BufferGL>(packBuffer)->getBufferID();
    }
    setPixelPackState(pack.alignment, pack.rowLength, pack.skipRows, pack.skipPixels, packBufferID);
}

void StateManagerGL::setPixelPackState(GLint alignment,
                                       GLint rowLength,
                                       GLint skipRows,
                                       GLint skipPixels,
                                       GLuint packBuffer)
{
    if (mPackAlignment != alignment)
    {
        mPackAlignment = alignment;
        mFunctions->pixelStorei(GL_PACK_ALIGNMENT, mPackAlignment);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_PACK_ALIGNMENT);
    }

    if (mPackRowLength != rowLength)
    {
        mPackRowLength = rowLength;
        mFunctions->pixelStorei(GL_PACK_ROW_LENGTH, mPackRowLength);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_UNPACK_ROW_LENGTH);
    }

    if (mPackSkipRows != skipRows)
    {
        mPackSkipRows = skipRows;
        mFunctions->pixelStorei(GL_PACK_SKIP_ROWS, mPackSkipRows);

        // TODO: set dirty bit once one exists
    }

    if (mPackSkipPixels != skipPixels)
    {
        mPackSkipPixels = skipPixels;
        mFunctions->pixelStorei(GL_PACK_SKIP_PIXELS, mPackSkipPixels);

        // TODO: set dirty bit once one exists
    }

    bindBuffer(GL_PIXEL_PACK_BUFFER, packBuffer);
}

void StateManagerGL::bindFramebuffer(GLenum type, GLuint framebuffer)
{
    if (type == GL_FRAMEBUFFER)
    {
        if (mFramebuffers[angle::FramebufferBindingRead] != framebuffer ||
            mFramebuffers[angle::FramebufferBindingDraw] != framebuffer)
        {
            mFramebuffers[angle::FramebufferBindingRead] = framebuffer;
            mFramebuffers[angle::FramebufferBindingDraw] = framebuffer;
            mFunctions->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        }
    }
    else
    {
        angle::FramebufferBinding binding = angle::EnumToFramebufferBinding(type);

        if (mFramebuffers[binding] != framebuffer)
        {
            mFramebuffers[binding] = framebuffer;
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

void StateManagerGL::bindTransformFeedback(GLenum type, GLuint transformFeedback)
{
    ASSERT(type == GL_TRANSFORM_FEEDBACK);
    if (mTransformFeedback != transformFeedback)
    {
        // Pause the current transform feedback if one is active.
        // To handle virtualized contexts, StateManagerGL needs to be able to bind a new transform
        // feedback at any time, even if there is one active.
        if (mPrevDrawTransformFeedback != nullptr &&
            mPrevDrawTransformFeedback->getTransformFeedbackID() != transformFeedback)
        {
            mPrevDrawTransformFeedback->syncPausedState(true);
            mPrevDrawTransformFeedback = nullptr;
        }

        mTransformFeedback = transformFeedback;
        mFunctions->bindTransformFeedback(type, mTransformFeedback);
    }
}

void StateManagerGL::beginQuery(GLenum type, GLuint query)
{
    // Make sure this is a valid query type and there is no current active query of this type
    ASSERT(mQueries.find(type) != mQueries.end());
    ASSERT(mQueries[type] == 0);
    ASSERT(query != 0);

    mQueries[type] = query;
    mFunctions->beginQuery(type, query);
}

void StateManagerGL::endQuery(GLenum type, GLuint query)
{
    ASSERT(mQueries[type] == query);
    mQueries[type] = 0;
    mFunctions->endQuery(type);
}

void StateManagerGL::onBeginQuery(QueryGL *query)
{
    mCurrentQueries.insert(query);
}

void StateManagerGL::onDeleteQueryObject(QueryGL *query)
{
    mCurrentQueries.erase(query);
}

gl::Error StateManagerGL::setDrawArraysState(const gl::ContextState &data,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei instanceCount)
{
    const gl::State &state = data.getState();

    const gl::Program *program = state.getProgram();

    const gl::VertexArray *vao = state.getVertexArray();
    const VertexArrayGL *vaoGL = GetImplAs<VertexArrayGL>(vao);

    gl::Error error = vaoGL->syncDrawArraysState(program->getActiveAttribLocationsMask(), first,
                                                 count, instanceCount);
    if (error.isError())
    {
        return error;
    }

    bindVertexArray(vaoGL->getVertexArrayID(), vaoGL->getAppliedElementArrayBufferID());

    return setGenericDrawState(data);
}

gl::Error StateManagerGL::setDrawElementsState(const gl::ContextState &data,
                                               GLsizei count,
                                               GLenum type,
                                               const GLvoid *indices,
                                               GLsizei instanceCount,
                                               const GLvoid **outIndices)
{
    const gl::State &state = data.getState();

    const gl::Program *program = state.getProgram();

    const gl::VertexArray *vao = state.getVertexArray();
    const VertexArrayGL *vaoGL = GetImplAs<VertexArrayGL>(vao);

    gl::Error error =
        vaoGL->syncDrawElementsState(program->getActiveAttribLocationsMask(), count, type, indices,
                                     instanceCount, state.isPrimitiveRestartEnabled(), outIndices);
    if (error.isError())
    {
        return error;
    }

    bindVertexArray(vaoGL->getVertexArrayID(), vaoGL->getAppliedElementArrayBufferID());

    return setGenericDrawState(data);
}

gl::Error StateManagerGL::setDrawIndirectState(const gl::ContextState &data, GLenum type)
{
    const gl::State &state = data.getState();

    const gl::VertexArray *vao = state.getVertexArray();
    const VertexArrayGL *vaoGL = GetImplAs<VertexArrayGL>(vao);

    if (type != GL_NONE)
    {
        ANGLE_TRY(vaoGL->syncElementArrayState());
    }
    bindVertexArray(vaoGL->getVertexArrayID(), vaoGL->getAppliedElementArrayBufferID());

    gl::Buffer *drawIndirectBuffer = state.getDrawIndirectBuffer();
    ASSERT(drawIndirectBuffer);
    const BufferGL *bufferGL = GetImplAs<BufferGL>(drawIndirectBuffer);
    bindBuffer(GL_DRAW_INDIRECT_BUFFER, bufferGL->getBufferID());

    return setGenericDrawState(data);
}

gl::Error StateManagerGL::setDispatchComputeState(const gl::ContextState &data)
{
    setGenericShaderState(data);
    return gl::NoError();
}

void StateManagerGL::pauseTransformFeedback()
{
    if (mPrevDrawTransformFeedback != nullptr)
    {
        mPrevDrawTransformFeedback->syncPausedState(true);
    }
}

void StateManagerGL::pauseAllQueries()
{
    for (QueryGL *prevQuery : mCurrentQueries)
    {
        prevQuery->pause();
    }
}

void StateManagerGL::pauseQuery(GLenum type)
{
    for (QueryGL *prevQuery : mCurrentQueries)
    {
        if (prevQuery->getType() == type)
        {
            prevQuery->pause();
        }
    }
}

void StateManagerGL::resumeAllQueries()
{
    for (QueryGL *prevQuery : mCurrentQueries)
    {
        prevQuery->resume();
    }
}

void StateManagerGL::resumeQuery(GLenum type)
{
    for (QueryGL *prevQuery : mCurrentQueries)
    {
        if (prevQuery->getType() == type)
        {
            prevQuery->resume();
        }
    }
}

gl::Error StateManagerGL::onMakeCurrent(const gl::ContextState &data)
{
    const gl::State &state = data.getState();

    // If the context has changed, pause the previous context's queries
    if (data.getContextID() != mPrevDrawContext)
    {
        pauseAllQueries();
    }
    mCurrentQueries.clear();
    mPrevDrawTransformFeedback = nullptr;
    mPrevDrawContext           = data.getContextID();

    // Set the current query state
    for (GLenum queryType : QueryTypes)
    {
        gl::Query *query = state.getActiveQuery(queryType);
        if (query != nullptr)
        {
            QueryGL *queryGL = GetImplAs<QueryGL>(query);
            queryGL->resume();

            mCurrentQueries.insert(queryGL);
        }
    }

    // Seamless cubemaps are required for ES3 and higher contexts. It should be the cheapest to set
    // this state here since MakeCurrent is expected to be called less frequently than draw calls.
    setTextureCubemapSeamlessEnabled(data.getClientMajorVersion() >= 3);

    return gl::NoError();
}

void StateManagerGL::setGenericShaderState(const gl::ContextState &data)
{
    const gl::State &state = data.getState();

    // Sync the current program state
    const gl::Program *program = state.getProgram();
    const ProgramGL *programGL = GetImplAs<ProgramGL>(program);
    useProgram(programGL->getProgramID());

    for (size_t uniformBlockIndex = 0; uniformBlockIndex < program->getActiveUniformBlockCount();
         uniformBlockIndex++)
    {
        GLuint binding = program->getUniformBlockBinding(static_cast<GLuint>(uniformBlockIndex));
        const auto &uniformBuffer = state.getIndexedUniformBuffer(binding);

        if (uniformBuffer.get() != nullptr)
        {
            BufferGL *bufferGL = GetImplAs<BufferGL>(uniformBuffer.get());

            if (uniformBuffer.getSize() == 0)
            {
                bindBufferBase(GL_UNIFORM_BUFFER, binding, bufferGL->getBufferID());
            }
            else
            {
                bindBufferRange(GL_UNIFORM_BUFFER, binding, bufferGL->getBufferID(),
                                uniformBuffer.getOffset(), uniformBuffer.getSize());
            }
        }
    }

    for (const gl::SamplerBinding &samplerUniform : program->getSamplerBindings())
    {
        GLenum textureType = samplerUniform.textureType;
        for (GLuint textureUnitIndex : samplerUniform.boundTextureUnits)
        {
            gl::Texture *texture = state.getSamplerTexture(textureUnitIndex, textureType);
            if (texture != nullptr)
            {
                const TextureGL *textureGL = GetImplAs<TextureGL>(texture);

                if (mTextures[textureType][textureUnitIndex] != textureGL->getTextureID() ||
                    texture->hasAnyDirtyBit() || textureGL->hasAnyDirtyBit())
                {
                    activeTexture(textureUnitIndex);
                    bindTexture(textureType, textureGL->getTextureID());

                    // TODO: Call this from the gl:: layer once other backends use dirty bits for
                    // texture state.
                    texture->syncImplState();
                }
            }
            else
            {
                if (mTextures[textureType][textureUnitIndex] != 0)
                {
                    activeTexture(textureUnitIndex);
                    bindTexture(textureType, 0);
                }
            }

            const gl::Sampler *sampler = state.getSampler(textureUnitIndex);
            if (sampler != nullptr)
            {
                const SamplerGL *samplerGL = GetImplAs<SamplerGL>(sampler);
                samplerGL->syncState(sampler->getSamplerState());
                bindSampler(textureUnitIndex, samplerGL->getSamplerID());
            }
            else
            {
                bindSampler(textureUnitIndex, 0);
            }
        }
    }
}

gl::Error StateManagerGL::setGenericDrawState(const gl::ContextState &data)
{
    setGenericShaderState(data);

    const gl::State &state = data.getState();

    const gl::Framebuffer *framebuffer = state.getDrawFramebuffer();
    const FramebufferGL *framebufferGL = GetImplAs<FramebufferGL>(framebuffer);
    bindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferGL->getFramebufferID());

    // Set the current transform feedback state
    gl::TransformFeedback *transformFeedback = state.getCurrentTransformFeedback();
    if (transformFeedback)
    {
        TransformFeedbackGL *transformFeedbackGL =
            GetImplAs<TransformFeedbackGL>(transformFeedback);
        bindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbackGL->getTransformFeedbackID());
        transformFeedbackGL->syncActiveState(transformFeedback->isActive(),
                                             transformFeedback->getPrimitiveMode());
        transformFeedbackGL->syncPausedState(transformFeedback->isPaused());
        mPrevDrawTransformFeedback = transformFeedbackGL;
    }
    else
    {
        bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
        mPrevDrawTransformFeedback = nullptr;
    }

    return gl::NoError();
}

void StateManagerGL::setAttributeCurrentData(size_t index,
                                             const gl::VertexAttribCurrentValueData &data)
{
    if (mVertexAttribCurrentValues[index] != data)
    {
        mVertexAttribCurrentValues[index] = data;
        switch (mVertexAttribCurrentValues[index].Type)
        {
            case GL_FLOAT:
                mFunctions->vertexAttrib4fv(static_cast<GLuint>(index),
                                            mVertexAttribCurrentValues[index].FloatValues);
                break;
            case GL_INT:
                mFunctions->vertexAttribI4iv(static_cast<GLuint>(index),
                                            mVertexAttribCurrentValues[index].IntValues);
                break;
            case GL_UNSIGNED_INT:
                mFunctions->vertexAttribI4uiv(static_cast<GLuint>(index),
                                             mVertexAttribCurrentValues[index].UnsignedIntValues);
                break;
          default: UNREACHABLE();
        }

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_CURRENT_VALUE_0 + index);
    }
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_SCISSOR_TEST_ENABLED);
    }
}

void StateManagerGL::setScissor(const gl::Rectangle &scissor)
{
    if (scissor != mScissor)
    {
        mScissor = scissor;
        mFunctions->scissor(mScissor.x, mScissor.y, mScissor.width, mScissor.height);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_SCISSOR);
    }
}

void StateManagerGL::setViewport(const gl::Rectangle &viewport)
{
    if (viewport != mViewport)
    {
        mViewport = viewport;
        mFunctions->viewport(mViewport.x, mViewport.y, mViewport.width, mViewport.height);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_VIEWPORT);
    }
}

void StateManagerGL::setDepthRange(float near, float far)
{
    if (mNear != near || mFar != far)
    {
        mNear = near;
        mFar = far;

        // The glDepthRangef function isn't available until OpenGL 4.1.  Prefer it when it is
        // available because OpenGL ES only works in floats.
        if (mFunctions->depthRangef)
        {
            mFunctions->depthRangef(mNear, mFar);
        }
        else
        {
            ASSERT(mFunctions->depthRange);
            mFunctions->depthRange(mNear, mFar);
        }

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_DEPTH_RANGE);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_BLEND_ENABLED);
    }
}

void StateManagerGL::setBlendColor(const gl::ColorF &blendColor)
{
    if (mBlendColor != blendColor)
    {
        mBlendColor = blendColor;
        mFunctions->blendColor(mBlendColor.red, mBlendColor.green, mBlendColor.blue, mBlendColor.alpha);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_BLEND_COLOR);
    }
}

void StateManagerGL::setBlendFuncs(GLenum sourceBlendRGB,
                                   GLenum destBlendRGB,
                                   GLenum sourceBlendAlpha,
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_BLEND_FUNCS);
    }
}

void StateManagerGL::setBlendEquations(GLenum blendEquationRGB, GLenum blendEquationAlpha)
{
    if (mBlendEquationRGB != blendEquationRGB || mBlendEquationAlpha != blendEquationAlpha)
    {
        mBlendEquationRGB = blendEquationRGB;
        mBlendEquationAlpha = blendEquationAlpha;

        mFunctions->blendEquationSeparate(mBlendEquationRGB, mBlendEquationAlpha);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_BLEND_EQUATIONS);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_COLOR_MASK);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_SAMPLE_COVERAGE_ENABLED);
    }
}

void StateManagerGL::setSampleCoverage(float value, bool invert)
{
    if (mSampleCoverageValue != value || mSampleCoverageInvert != invert)
    {
        mSampleCoverageValue = value;
        mSampleCoverageInvert = invert;
        mFunctions->sampleCoverage(mSampleCoverageValue, mSampleCoverageInvert);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_SAMPLE_COVERAGE);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_DEPTH_TEST_ENABLED);
    }
}

void StateManagerGL::setDepthFunc(GLenum depthFunc)
{
    if (mDepthFunc != depthFunc)
    {
        mDepthFunc = depthFunc;
        mFunctions->depthFunc(mDepthFunc);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_DEPTH_FUNC);
    }
}

void StateManagerGL::setDepthMask(bool mask)
{
    if (mDepthMask != mask)
    {
        mDepthMask = mask;
        mFunctions->depthMask(mDepthMask);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_DEPTH_MASK);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_STENCIL_TEST_ENABLED);
    }
}

void StateManagerGL::setStencilFrontWritemask(GLuint mask)
{
    if (mStencilFrontWritemask != mask)
    {
        mStencilFrontWritemask = mask;
        mFunctions->stencilMaskSeparate(GL_FRONT, mStencilFrontWritemask);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_STENCIL_WRITEMASK_FRONT);
    }
}

void StateManagerGL::setStencilBackWritemask(GLuint mask)
{
    if (mStencilBackWritemask != mask)
    {
        mStencilBackWritemask = mask;
        mFunctions->stencilMaskSeparate(GL_BACK, mStencilBackWritemask);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_STENCIL_WRITEMASK_BACK);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_STENCIL_FUNCS_FRONT);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_STENCIL_FUNCS_BACK);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_STENCIL_OPS_FRONT);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_STENCIL_OPS_BACK);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_CULL_FACE_ENABLED);
    }
}

void StateManagerGL::setCullFace(GLenum cullFace)
{
    if (mCullFace != cullFace)
    {
        mCullFace = cullFace;
        mFunctions->cullFace(mCullFace);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_CULL_FACE);
    }
}

void StateManagerGL::setFrontFace(GLenum frontFace)
{
    if (mFrontFace != frontFace)
    {
        mFrontFace = frontFace;
        mFunctions->frontFace(mFrontFace);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_FRONT_FACE);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED);
    }
}

void StateManagerGL::setPolygonOffset(float factor, float units)
{
    if (mPolygonOffsetFactor != factor || mPolygonOffsetUnits != units)
    {
        mPolygonOffsetFactor = factor;
        mPolygonOffsetUnits = units;
        mFunctions->polygonOffset(mPolygonOffsetFactor, mPolygonOffsetUnits);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_POLYGON_OFFSET);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED);
    }
}

void StateManagerGL::setLineWidth(float width)
{
    if (mLineWidth != width)
    {
        mLineWidth = width;
        mFunctions->lineWidth(mLineWidth);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_LINE_WIDTH);
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

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_PRIMITIVE_RESTART_ENABLED);
    }
}

void StateManagerGL::setClearDepth(float clearDepth)
{
    if (mClearDepth != clearDepth)
    {
        mClearDepth = clearDepth;

        // The glClearDepthf function isn't available until OpenGL 4.1.  Prefer it when it is
        // available because OpenGL ES only works in floats.
        if (mFunctions->clearDepthf)
        {
            mFunctions->clearDepthf(mClearDepth);
        }
        else
        {
            ASSERT(mFunctions->clearDepth);
            mFunctions->clearDepth(mClearDepth);
        }

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_CLEAR_DEPTH);
    }
}

void StateManagerGL::setClearColor(const gl::ColorF &clearColor)
{
    if (mClearColor != clearColor)
    {
        mClearColor = clearColor;
        mFunctions->clearColor(mClearColor.red, mClearColor.green, mClearColor.blue, mClearColor.alpha);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_CLEAR_COLOR);
    }
}

void StateManagerGL::setClearStencil(GLint clearStencil)
{
    if (mClearStencil != clearStencil)
    {
        mClearStencil = clearStencil;
        mFunctions->clearStencil(mClearStencil);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_CLEAR_STENCIL);
    }
}

void StateManagerGL::syncState(const gl::ContextState &data,
                               const gl::State::DirtyBits &glDirtyBits)
{
    const gl::State &state = data.getState();

    // The the current framebuffer binding sometimes requires resetting the srgb blending
    if (glDirtyBits[gl::State::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING] &&
        mFunctions->standard == STANDARD_GL_DESKTOP)
    {
        mLocalDirtyBits.set(gl::State::DIRTY_BIT_FRAMEBUFFER_SRGB);
    }

    const auto &glAndLocalDirtyBits = (glDirtyBits | mLocalDirtyBits);

    if (!glAndLocalDirtyBits.any())
    {
        return;
    }

    // TODO(jmadill): Investigate only syncing vertex state for active attributes
    for (auto dirtyBit : angle::IterateBitSet(glAndLocalDirtyBits))
    {
        switch (dirtyBit)
        {
            case gl::State::DIRTY_BIT_SCISSOR_TEST_ENABLED:
                setScissorTestEnabled(state.isScissorTestEnabled());
                break;
            case gl::State::DIRTY_BIT_SCISSOR:
                setScissor(state.getScissor());
                break;
            case gl::State::DIRTY_BIT_VIEWPORT:
                setViewport(state.getViewport());
                break;
            case gl::State::DIRTY_BIT_DEPTH_RANGE:
                setDepthRange(state.getNearPlane(), state.getFarPlane());
                break;
            case gl::State::DIRTY_BIT_BLEND_ENABLED:
                setBlendEnabled(state.isBlendEnabled());
                break;
            case gl::State::DIRTY_BIT_BLEND_COLOR:
                setBlendColor(state.getBlendColor());
                break;
            case gl::State::DIRTY_BIT_BLEND_FUNCS:
            {
                const auto &blendState = state.getBlendState();
                setBlendFuncs(blendState.sourceBlendRGB, blendState.destBlendRGB,
                              blendState.sourceBlendAlpha, blendState.destBlendAlpha);
                break;
            }
            case gl::State::DIRTY_BIT_BLEND_EQUATIONS:
            {
                const auto &blendState = state.getBlendState();
                setBlendEquations(blendState.blendEquationRGB, blendState.blendEquationAlpha);
                break;
            }
            case gl::State::DIRTY_BIT_COLOR_MASK:
            {
                const auto &blendState = state.getBlendState();
                setColorMask(blendState.colorMaskRed, blendState.colorMaskGreen,
                             blendState.colorMaskBlue, blendState.colorMaskAlpha);
                break;
            }
            case gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED:
                setSampleAlphaToCoverageEnabled(state.isSampleAlphaToCoverageEnabled());
                break;
            case gl::State::DIRTY_BIT_SAMPLE_COVERAGE_ENABLED:
                setSampleCoverageEnabled(state.isSampleCoverageEnabled());
                break;
            case gl::State::DIRTY_BIT_SAMPLE_COVERAGE:
                setSampleCoverage(state.getSampleCoverageValue(), state.getSampleCoverageInvert());
                break;
            case gl::State::DIRTY_BIT_DEPTH_TEST_ENABLED:
                setDepthTestEnabled(state.isDepthTestEnabled());
                break;
            case gl::State::DIRTY_BIT_DEPTH_FUNC:
                setDepthFunc(state.getDepthStencilState().depthFunc);
                break;
            case gl::State::DIRTY_BIT_DEPTH_MASK:
                setDepthMask(state.getDepthStencilState().depthMask);
                break;
            case gl::State::DIRTY_BIT_STENCIL_TEST_ENABLED:
                setStencilTestEnabled(state.isStencilTestEnabled());
                break;
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_FRONT:
            {
                const auto &depthStencilState = state.getDepthStencilState();
                setStencilFrontFuncs(depthStencilState.stencilFunc, state.getStencilRef(),
                                     depthStencilState.stencilMask);
                break;
            }
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_BACK:
            {
                const auto &depthStencilState = state.getDepthStencilState();
                setStencilBackFuncs(depthStencilState.stencilBackFunc, state.getStencilBackRef(),
                                    depthStencilState.stencilBackMask);
                break;
            }
            case gl::State::DIRTY_BIT_STENCIL_OPS_FRONT:
            {
                const auto &depthStencilState = state.getDepthStencilState();
                setStencilFrontOps(depthStencilState.stencilFail,
                                   depthStencilState.stencilPassDepthFail,
                                   depthStencilState.stencilPassDepthPass);
                break;
            }
            case gl::State::DIRTY_BIT_STENCIL_OPS_BACK:
            {
                const auto &depthStencilState = state.getDepthStencilState();
                setStencilBackOps(depthStencilState.stencilBackFail,
                                  depthStencilState.stencilBackPassDepthFail,
                                  depthStencilState.stencilBackPassDepthPass);
                break;
            }
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_FRONT:
                setStencilFrontWritemask(state.getDepthStencilState().stencilWritemask);
                break;
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_BACK:
                setStencilBackWritemask(state.getDepthStencilState().stencilBackWritemask);
                break;
            case gl::State::DIRTY_BIT_CULL_FACE_ENABLED:
                setCullFaceEnabled(state.isCullFaceEnabled());
                break;
            case gl::State::DIRTY_BIT_CULL_FACE:
                setCullFace(state.getRasterizerState().cullMode);
                break;
            case gl::State::DIRTY_BIT_FRONT_FACE:
                setFrontFace(state.getRasterizerState().frontFace);
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED:
                setPolygonOffsetFillEnabled(state.isPolygonOffsetFillEnabled());
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET:
            {
                const auto &rasterizerState = state.getRasterizerState();
                setPolygonOffset(rasterizerState.polygonOffsetFactor,
                                 rasterizerState.polygonOffsetUnits);
                break;
            }
            case gl::State::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED:
                setRasterizerDiscardEnabled(state.isRasterizerDiscardEnabled());
                break;
            case gl::State::DIRTY_BIT_LINE_WIDTH:
                setLineWidth(state.getLineWidth());
                break;
            case gl::State::DIRTY_BIT_PRIMITIVE_RESTART_ENABLED:
                setPrimitiveRestartEnabled(state.isPrimitiveRestartEnabled());
                break;
            case gl::State::DIRTY_BIT_CLEAR_COLOR:
                setClearColor(state.getColorClearValue());
                break;
            case gl::State::DIRTY_BIT_CLEAR_DEPTH:
                setClearDepth(state.getDepthClearValue());
                break;
            case gl::State::DIRTY_BIT_CLEAR_STENCIL:
                setClearStencil(state.getStencilClearValue());
                break;
            case gl::State::DIRTY_BIT_UNPACK_ALIGNMENT:
                // TODO(jmadill): split this
                setPixelUnpackState(state.getUnpackState());
                break;
            case gl::State::DIRTY_BIT_UNPACK_ROW_LENGTH:
                // TODO(jmadill): split this
                setPixelUnpackState(state.getUnpackState());
                break;
            case gl::State::DIRTY_BIT_UNPACK_IMAGE_HEIGHT:
                // TODO(jmadill): split this
                setPixelUnpackState(state.getUnpackState());
                break;
            case gl::State::DIRTY_BIT_UNPACK_SKIP_IMAGES:
                // TODO(jmadill): split this
                setPixelUnpackState(state.getUnpackState());
                break;
            case gl::State::DIRTY_BIT_UNPACK_SKIP_ROWS:
                // TODO(jmadill): split this
                setPixelUnpackState(state.getUnpackState());
                break;
            case gl::State::DIRTY_BIT_UNPACK_SKIP_PIXELS:
                // TODO(jmadill): split this
                setPixelUnpackState(state.getUnpackState());
                break;
            case gl::State::DIRTY_BIT_UNPACK_BUFFER_BINDING:
                // TODO(jmadill): split this
                setPixelUnpackState(state.getUnpackState());
                break;
            case gl::State::DIRTY_BIT_PACK_ALIGNMENT:
                // TODO(jmadill): split this
                setPixelPackState(state.getPackState());
                break;
            case gl::State::DIRTY_BIT_PACK_REVERSE_ROW_ORDER:
                // TODO(jmadill): split this
                setPixelPackState(state.getPackState());
                break;
            case gl::State::DIRTY_BIT_PACK_ROW_LENGTH:
                // TODO(jmadill): split this
                setPixelPackState(state.getPackState());
                break;
            case gl::State::DIRTY_BIT_PACK_SKIP_ROWS:
                // TODO(jmadill): split this
                setPixelPackState(state.getPackState());
                break;
            case gl::State::DIRTY_BIT_PACK_SKIP_PIXELS:
                // TODO(jmadill): split this
                setPixelPackState(state.getPackState());
                break;
            case gl::State::DIRTY_BIT_PACK_BUFFER_BINDING:
                // TODO(jmadill): split this
                setPixelPackState(state.getPackState());
                break;
            case gl::State::DIRTY_BIT_DITHER_ENABLED:
                setDitherEnabled(state.isDitherEnabled());
                break;
            case gl::State::DIRTY_BIT_GENERATE_MIPMAP_HINT:
                // TODO(jmadill): implement this
                break;
            case gl::State::DIRTY_BIT_SHADER_DERIVATIVE_HINT:
                // TODO(jmadill): implement this
                break;
            case gl::State::DIRTY_BIT_READ_FRAMEBUFFER_BINDING:
                // TODO(jmadill): implement this
                break;
            case gl::State::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING:
                // TODO(jmadill): implement this
                break;
            case gl::State::DIRTY_BIT_RENDERBUFFER_BINDING:
                // TODO(jmadill): implement this
                break;
            case gl::State::DIRTY_BIT_VERTEX_ARRAY_BINDING:
                // TODO(jmadill): implement this
                break;
            case gl::State::DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING:
                // TODO: implement this
                break;
            case gl::State::DIRTY_BIT_PROGRAM_BINDING:
                // TODO(jmadill): implement this
                break;
            case gl::State::DIRTY_BIT_MULTISAMPLING:
                setMultisamplingStateEnabled(state.isMultisamplingEnabled());
                break;
            case gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_ONE:
                setSampleAlphaToOneStateEnabled(state.isSampleAlphaToOneEnabled());
            case gl::State::DIRTY_BIT_COVERAGE_MODULATION:
                setCoverageModulation(state.getCoverageModulation());
                break;
            case gl::State::DIRTY_BIT_PATH_RENDERING_MATRIX_MV:
                setPathRenderingModelViewMatrix(
                    state.getPathRenderingMatrix(GL_PATH_MODELVIEW_MATRIX_CHROMIUM));
                break;
            case gl::State::DIRTY_BIT_PATH_RENDERING_MATRIX_PROJ:
                setPathRenderingProjectionMatrix(
                    state.getPathRenderingMatrix(GL_PATH_PROJECTION_MATRIX_CHROMIUM));
                break;
            case gl::State::DIRTY_BIT_PATH_RENDERING_STENCIL_STATE:
                setPathRenderingStencilState(state.getPathStencilFunc(), state.getPathStencilRef(),
                                             state.getPathStencilMask());
                break;
            case gl::State::DIRTY_BIT_FRAMEBUFFER_SRGB:
                setFramebufferSRGBEnabledForFramebuffer(
                    data, state.getFramebufferSRGB(),
                    GetImplAs<FramebufferGL>(state.getDrawFramebuffer()));
                break;
            default:
            {
                ASSERT(dirtyBit >= gl::State::DIRTY_BIT_CURRENT_VALUE_0 &&
                       dirtyBit < gl::State::DIRTY_BIT_CURRENT_VALUE_MAX);
                size_t attribIndex =
                    static_cast<size_t>(dirtyBit) - gl::State::DIRTY_BIT_CURRENT_VALUE_0;
                setAttributeCurrentData(attribIndex, state.getVertexAttribCurrentValue(
                                                         static_cast<unsigned int>(attribIndex)));
                break;
            }
        }

        mLocalDirtyBits.reset();
    }
}

void StateManagerGL::setFramebufferSRGBEnabled(const gl::ContextState &data, bool enabled)
{
    if (!data.getExtensions().sRGBWriteControl)
    {
        return;
    }

    if (mFramebufferSRGBEnabled != enabled)
    {
        mFramebufferSRGBEnabled = enabled;
        if (mFramebufferSRGBEnabled)
        {
            mFunctions->enable(GL_FRAMEBUFFER_SRGB);
        }
        else
        {
            mFunctions->disable(GL_FRAMEBUFFER_SRGB);
        }
        mLocalDirtyBits.set(gl::State::DIRTY_BIT_FRAMEBUFFER_SRGB);
    }
}

void StateManagerGL::setFramebufferSRGBEnabledForFramebuffer(const gl::ContextState &data,
                                                             bool enabled,
                                                             const FramebufferGL *framebuffer)
{
    if (mFunctions->standard == STANDARD_GL_DESKTOP && framebuffer->isDefault())
    {
        // Obey the framebuffer sRGB state for blending on all framebuffers except the default
        // framebuffer on Desktop OpenGL.
        // When SRGB blending is enabled, only SRGB capable formats will use it but the default
        // framebuffer will always use it if it is enabled.
        // TODO(geofflang): Update this when the framebuffer binding dirty changes, when it exists.
        setFramebufferSRGBEnabled(data, false);
    }
    else
    {
        setFramebufferSRGBEnabled(data, enabled);
    }
}

void StateManagerGL::setDitherEnabled(bool enabled)
{
    if (mDitherEnabled != enabled)
    {
        mDitherEnabled = enabled;
        if (mDitherEnabled)
        {
            mFunctions->enable(GL_DITHER);
        }
        else
        {
            mFunctions->disable(GL_DITHER);
        }
    }
}

void StateManagerGL::setMultisamplingStateEnabled(bool enabled)
{
    if (mMultisamplingEnabled != enabled)
    {
        mMultisamplingEnabled = enabled;
        if (mMultisamplingEnabled)
        {
            mFunctions->enable(GL_MULTISAMPLE_EXT);
        }
        else
        {
            mFunctions->disable(GL_MULTISAMPLE_EXT);
        }
        mLocalDirtyBits.set(gl::State::DIRTY_BIT_MULTISAMPLING);
    }
}

void StateManagerGL::setSampleAlphaToOneStateEnabled(bool enabled)
{
    if (mSampleAlphaToOneEnabled != enabled)
    {
        mSampleAlphaToOneEnabled = enabled;
        if (mSampleAlphaToOneEnabled)
        {
            mFunctions->enable(GL_SAMPLE_ALPHA_TO_ONE);
        }
        else
        {
            mFunctions->disable(GL_SAMPLE_ALPHA_TO_ONE);
        }
        mLocalDirtyBits.set(gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_ONE);
    }
}

void StateManagerGL::setCoverageModulation(GLenum components)
{
    if (mCoverageModulation != components)
    {
        mCoverageModulation = components;
        mFunctions->coverageModulationNV(components);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_COVERAGE_MODULATION);
    }
}

void StateManagerGL::setPathRenderingModelViewMatrix(const GLfloat *m)
{
    if (memcmp(mPathMatrixMV, m, sizeof(mPathMatrixMV)) != 0)
    {
        memcpy(mPathMatrixMV, m, sizeof(mPathMatrixMV));
        mFunctions->matrixLoadEXT(GL_PATH_MODELVIEW_CHROMIUM, m);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_PATH_RENDERING_MATRIX_MV);
    }
}

void StateManagerGL::setPathRenderingProjectionMatrix(const GLfloat *m)
{
    if (memcmp(mPathMatrixProj, m, sizeof(mPathMatrixProj)) != 0)
    {
        memcpy(mPathMatrixProj, m, sizeof(mPathMatrixProj));
        mFunctions->matrixLoadEXT(GL_PATH_PROJECTION_CHROMIUM, m);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_PATH_RENDERING_MATRIX_PROJ);
    }
}

void StateManagerGL::setPathRenderingStencilState(GLenum func, GLint ref, GLuint mask)
{
    if (func != mPathStencilFunc || ref != mPathStencilRef || mask != mPathStencilMask)
    {
        mPathStencilFunc = func;
        mPathStencilRef  = ref;
        mPathStencilMask = mask;
        mFunctions->pathStencilFuncNV(func, ref, mask);

        mLocalDirtyBits.set(gl::State::DIRTY_BIT_PATH_RENDERING_STENCIL_STATE);
    }
}

void StateManagerGL::setTextureCubemapSeamlessEnabled(bool enabled)
{
    if (mTextureCubemapSeamlessEnabled != enabled)
    {
        mTextureCubemapSeamlessEnabled = enabled;
        if (mTextureCubemapSeamlessEnabled)
        {
            mFunctions->enable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        }
        else
        {
            mFunctions->disable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        }
    }
}

}
