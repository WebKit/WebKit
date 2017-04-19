//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FramebufferGL.cpp: Implements the class methods for FramebufferGL.

#include "libANGLE/renderer/gl/FramebufferGL.h"

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/State.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/gl/BlitGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/RenderbufferGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/TextureGL.h"
#include "libANGLE/renderer/gl/WorkaroundsGL.h"
#include "libANGLE/renderer/gl/formatutilsgl.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"
#include "platform/Platform.h"

using namespace gl;
using angle::CheckedNumeric;

namespace rx
{

FramebufferGL::FramebufferGL(const FramebufferState &state,
                             const FunctionsGL *functions,
                             StateManagerGL *stateManager,
                             const WorkaroundsGL &workarounds,
                             BlitGL *blitter,
                             bool isDefault)
    : FramebufferImpl(state),
      mFunctions(functions),
      mStateManager(stateManager),
      mWorkarounds(workarounds),
      mBlitter(blitter),
      mFramebufferID(0),
      mIsDefault(isDefault)
{
    if (!mIsDefault)
    {
        mFunctions->genFramebuffers(1, &mFramebufferID);
    }
}

FramebufferGL::FramebufferGL(GLuint id,
                             const FramebufferState &state,
                             const FunctionsGL *functions,
                             const WorkaroundsGL &workarounds,
                             BlitGL *blitter,
                             StateManagerGL *stateManager)
    : FramebufferImpl(state),
      mFunctions(functions),
      mStateManager(stateManager),
      mWorkarounds(workarounds),
      mBlitter(blitter),
      mFramebufferID(id),
      mIsDefault(true)
{
}

FramebufferGL::~FramebufferGL()
{
    mStateManager->deleteFramebuffer(mFramebufferID);
    mFramebufferID = 0;
}

static void BindFramebufferAttachment(const FunctionsGL *functions,
                                      GLenum attachmentPoint,
                                      const FramebufferAttachment *attachment)
{
    if (attachment)
    {
        if (attachment->type() == GL_TEXTURE)
        {
            const Texture *texture     = attachment->getTexture();
            const TextureGL *textureGL = GetImplAs<TextureGL>(texture);

            if (texture->getTarget() == GL_TEXTURE_2D ||
                texture->getTarget() == GL_TEXTURE_2D_MULTISAMPLE)
            {
                functions->framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoint,
                                                texture->getTarget(), textureGL->getTextureID(),
                                                attachment->mipLevel());
            }
            else if (texture->getTarget() == GL_TEXTURE_CUBE_MAP)
            {
                functions->framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoint, attachment->cubeMapFace(),
                                                textureGL->getTextureID(), attachment->mipLevel());
            }
            else if (texture->getTarget() == GL_TEXTURE_2D_ARRAY || texture->getTarget() == GL_TEXTURE_3D)
            {
                functions->framebufferTextureLayer(GL_FRAMEBUFFER, attachmentPoint, textureGL->getTextureID(),
                                                   attachment->mipLevel(), attachment->layer());
            }
            else
            {
                UNREACHABLE();
            }
        }
        else if (attachment->type() == GL_RENDERBUFFER)
        {
            const Renderbuffer *renderbuffer     = attachment->getRenderbuffer();
            const RenderbufferGL *renderbufferGL = GetImplAs<RenderbufferGL>(renderbuffer);

            functions->framebufferRenderbuffer(GL_FRAMEBUFFER, attachmentPoint, GL_RENDERBUFFER,
                                               renderbufferGL->getRenderbufferID());
        }
        else
        {
            UNREACHABLE();
        }
    }
    else
    {
        // Unbind this attachment
        functions->framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoint, GL_TEXTURE_2D, 0, 0);
    }
}

Error FramebufferGL::discard(size_t count, const GLenum *attachments)
{
    // glInvalidateFramebuffer accepts the same enums as glDiscardFramebufferEXT
    return invalidate(count, attachments);
}

Error FramebufferGL::invalidate(size_t count, const GLenum *attachments)
{
    const GLenum *finalAttachmentsPtr = attachments;

    std::vector<GLenum> modifiedAttachments;
    if (modifyInvalidateAttachmentsForEmulatedDefaultFBO(count, attachments, &modifiedAttachments))
    {
        finalAttachmentsPtr = modifiedAttachments.data();
    }

    // Since this function is just a hint, only call a native function if it exists.
    if (mFunctions->invalidateFramebuffer)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        mFunctions->invalidateFramebuffer(GL_FRAMEBUFFER, static_cast<GLsizei>(count),
                                          finalAttachmentsPtr);
    }
    else if (mFunctions->discardFramebuffer)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        mFunctions->discardFramebuffer(GL_FRAMEBUFFER, static_cast<GLsizei>(count),
                                       finalAttachmentsPtr);
    }

    return gl::NoError();
}

Error FramebufferGL::invalidateSub(size_t count,
                                   const GLenum *attachments,
                                   const gl::Rectangle &area)
{

    const GLenum *finalAttachmentsPtr = attachments;

    std::vector<GLenum> modifiedAttachments;
    if (modifyInvalidateAttachmentsForEmulatedDefaultFBO(count, attachments, &modifiedAttachments))
    {
        finalAttachmentsPtr = modifiedAttachments.data();
    }

    // Since this function is just a hint and not available until OpenGL 4.3, only call it if it is available.
    if (mFunctions->invalidateSubFramebuffer)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        mFunctions->invalidateSubFramebuffer(GL_FRAMEBUFFER, static_cast<GLsizei>(count),
                                             finalAttachmentsPtr, area.x, area.y, area.width,
                                             area.height);
    }

    return NoError();
}

Error FramebufferGL::clear(ContextImpl *context, GLbitfield mask)
{
    syncClearState(context, mask);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctions->clear(mask);

    return NoError();
}

Error FramebufferGL::clearBufferfv(ContextImpl *context,
                                   GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat *values)
{
    syncClearBufferState(context, buffer, drawbuffer);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctions->clearBufferfv(buffer, drawbuffer, values);

    return NoError();
}

Error FramebufferGL::clearBufferuiv(ContextImpl *context,
                                    GLenum buffer,
                                    GLint drawbuffer,
                                    const GLuint *values)
{
    syncClearBufferState(context, buffer, drawbuffer);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctions->clearBufferuiv(buffer, drawbuffer, values);

    return NoError();
}

Error FramebufferGL::clearBufferiv(ContextImpl *context,
                                   GLenum buffer,
                                   GLint drawbuffer,
                                   const GLint *values)
{
    syncClearBufferState(context, buffer, drawbuffer);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctions->clearBufferiv(buffer, drawbuffer, values);

    return NoError();
}

Error FramebufferGL::clearBufferfi(ContextImpl *context,
                                   GLenum buffer,
                                   GLint drawbuffer,
                                   GLfloat depth,
                                   GLint stencil)
{
    syncClearBufferState(context, buffer, drawbuffer);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctions->clearBufferfi(buffer, drawbuffer, depth, stencil);

    return NoError();
}

GLenum FramebufferGL::getImplementationColorReadFormat() const
{
    const auto *readAttachment = mState.getReadAttachment();
    const Format &format       = readAttachment->getFormat();
    return format.info->getReadPixelsFormat();
}

GLenum FramebufferGL::getImplementationColorReadType() const
{
    const auto *readAttachment = mState.getReadAttachment();
    const Format &format       = readAttachment->getFormat();
    return format.info->getReadPixelsType();
}

Error FramebufferGL::readPixels(ContextImpl *context,
                                const gl::Rectangle &area,
                                GLenum format,
                                GLenum type,
                                GLvoid *pixels) const
{
    // TODO: don't sync the pixel pack state here once the dirty bits contain the pixel pack buffer
    // binding
    const PixelPackState &packState = context->getGLState().getPackState();
    mStateManager->setPixelPackState(packState);

    nativegl::ReadPixelsFormat readPixelsFormat =
        nativegl::GetReadPixelsFormat(mFunctions, mWorkarounds, format, type);
    GLenum readFormat = readPixelsFormat.format;
    GLenum readType   = readPixelsFormat.type;

    mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, mFramebufferID);

    if (mWorkarounds.packOverlappingRowsSeparatelyPackBuffer && packState.pixelBuffer.get() &&
        packState.rowLength != 0 && packState.rowLength < area.width)
    {
        return readPixelsRowByRowWorkaround(area, readFormat, readType, packState, pixels);
    }

    if (mWorkarounds.packLastRowSeparatelyForPaddingInclusion)
    {
        gl::Extents size(area.width, area.height, 1);

        bool apply;
        ANGLE_TRY_RESULT(ShouldApplyLastRowPaddingWorkaround(size, packState, readFormat, readType,
                                                             false, pixels),
                         apply);

        if (apply)
        {
            return readPixelsPaddingWorkaround(area, readFormat, readType, packState, pixels);
        }
    }

    mFunctions->readPixels(area.x, area.y, area.width, area.height, readFormat, readType, pixels);

    return gl::NoError();
}

Error FramebufferGL::blit(ContextImpl *context,
                          const gl::Rectangle &sourceArea,
                          const gl::Rectangle &destArea,
                          GLbitfield mask,
                          GLenum filter)
{
    const Framebuffer *sourceFramebuffer     = context->getGLState().getReadFramebuffer();
    const Framebuffer *destFramebuffer       = context->getGLState().getDrawFramebuffer();

    const FramebufferAttachment *colorReadAttachment = sourceFramebuffer->getReadColorbuffer();

    GLsizei readAttachmentSamples = 0;
    if (colorReadAttachment != nullptr)
    {
        readAttachmentSamples = colorReadAttachment->getSamples();
    }

    bool needManualColorBlit = false;

    // TODO(cwallez) when the filter is LINEAR and both source and destination are SRGB, we
    // could avoid doing a manual blit.

    // Prior to OpenGL 4.4 BlitFramebuffer (section 18.3.1 of GL 4.3 core profile) reads:
    //      When values are taken from the read buffer, no linearization is performed, even
    //      if the format of the buffer is SRGB.
    // Starting from OpenGL 4.4 (section 18.3.1) it reads:
    //      When values are taken from the read buffer, if FRAMEBUFFER_SRGB is enabled and the
    //      value of FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING for the framebuffer attachment
    //      corresponding to the read buffer is SRGB, the red, green, and blue components are
    //      converted from the non-linear sRGB color space according [...].
    {
        bool sourceSRGB = colorReadAttachment != nullptr &&
                          colorReadAttachment->getColorEncoding() == GL_SRGB;
        needManualColorBlit =
            needManualColorBlit || (sourceSRGB && mFunctions->isAtMostGL(gl::Version(4, 3)));
    }

    // Prior to OpenGL 4.2 BlitFramebuffer (section 4.3.2 of GL 4.1 core profile) reads:
    //      Blit operations bypass the fragment pipeline. The only fragment operations which
    //      affect a blit are the pixel ownership test and scissor test.
    // Starting from OpenGL 4.2 (section 4.3.2) it reads:
    //      When values are written to the draw buffers, blit operations bypass the fragment
    //      pipeline. The only fragment operations which affect a blit are the pixel ownership
    //      test,  the scissor test and sRGB conversion.
    if (!needManualColorBlit)
    {
        bool destSRGB = false;
        for (size_t i = 0; i < destFramebuffer->getDrawbufferStateCount(); ++i)
        {
            const FramebufferAttachment *attachment = destFramebuffer->getDrawBuffer(i);
            if (attachment && attachment->getColorEncoding() == GL_SRGB)
            {
                destSRGB = true;
                break;
            }
        }

        needManualColorBlit =
            needManualColorBlit || (destSRGB && mFunctions->isAtMostGL(gl::Version(4, 1)));
    }

    // Enable FRAMEBUFFER_SRGB if needed
    mStateManager->setFramebufferSRGBEnabledForFramebuffer(context->getContextState(), true, this);

    GLenum blitMask = mask;
    if (needManualColorBlit && (mask & GL_COLOR_BUFFER_BIT) && readAttachmentSamples <= 1)
    {
        ANGLE_TRY(mBlitter->blitColorBufferWithShader(sourceFramebuffer, destFramebuffer,
                                                      sourceArea, destArea, filter));
        blitMask &= ~GL_COLOR_BUFFER_BIT;
    }

    if (blitMask == 0)
    {
        return gl::NoError();
    }

    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(sourceFramebuffer);
    mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, sourceFramebufferGL->getFramebufferID());
    mStateManager->bindFramebuffer(GL_DRAW_FRAMEBUFFER, mFramebufferID);

    mFunctions->blitFramebuffer(sourceArea.x, sourceArea.y, sourceArea.x1(), sourceArea.y1(),
                                destArea.x, destArea.y, destArea.x1(), destArea.y1(), blitMask,
                                filter);

    return gl::NoError();
}

gl::Error FramebufferGL::getSamplePosition(size_t index, GLfloat *xy) const
{
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctions->getMultisamplefv(GL_SAMPLE_POSITION, static_cast<GLuint>(index), xy);
    return gl::NoError();
}

bool FramebufferGL::checkStatus() const
{
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    GLenum status = mFunctions->checkFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        WARN() << "GL framebuffer returned incomplete.";
    }
    return (status == GL_FRAMEBUFFER_COMPLETE);
}

void FramebufferGL::syncState(ContextImpl *contextImpl, const Framebuffer::DirtyBits &dirtyBits)
{
    // Don't need to sync state for the default FBO.
    if (mIsDefault)
    {
        return;
    }

    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    for (auto dirtyBit : angle::IterateBitSet(dirtyBits))
    {
        switch (dirtyBit)
        {
            case Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
                BindFramebufferAttachment(mFunctions, GL_DEPTH_ATTACHMENT,
                                          mState.getDepthAttachment());
                break;
            case Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
                BindFramebufferAttachment(mFunctions, GL_STENCIL_ATTACHMENT,
                                          mState.getStencilAttachment());
                break;
            case Framebuffer::DIRTY_BIT_DRAW_BUFFERS:
            {
                const auto &drawBuffers = mState.getDrawBufferStates();
                mFunctions->drawBuffers(static_cast<GLsizei>(drawBuffers.size()),
                                        drawBuffers.data());
                break;
            }
            case Framebuffer::DIRTY_BIT_READ_BUFFER:
                mFunctions->readBuffer(mState.getReadBufferState());
                break;
            case Framebuffer::DIRTY_BIT_DEFAULT_WIDTH:
                mFunctions->framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH,
                                                  mState.getDefaultWidth());
                break;
            case Framebuffer::DIRTY_BIT_DEFAULT_HEIGHT:
                mFunctions->framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT,
                                                  mState.getDefaultHeight());
                break;
            case Framebuffer::DIRTY_BIT_DEFAULT_SAMPLES:
                mFunctions->framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_SAMPLES,
                                                  mState.getDefaultSamples());
                break;
            case Framebuffer::DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS:
                mFunctions->framebufferParameteri(GL_FRAMEBUFFER,
                                                  GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS,
                                                  mState.getDefaultFixedSampleLocations());
                break;
            default:
            {
                ASSERT(Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 == 0 &&
                       dirtyBit < Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX);
                size_t index =
                    static_cast<size_t>(dirtyBit - Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0);
                BindFramebufferAttachment(mFunctions,
                                          static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index),
                                          mState.getColorAttachment(index));
                break;
            }
        }
    }
}

GLuint FramebufferGL::getFramebufferID() const
{
    return mFramebufferID;
}

bool FramebufferGL::isDefault() const
{
    return mIsDefault;
}

void FramebufferGL::syncClearState(ContextImpl *context, GLbitfield mask)
{
    if (mFunctions->standard == STANDARD_GL_DESKTOP)
    {
        if (mWorkarounds.doesSRGBClearsOnLinearFramebufferAttachments &&
            (mask & GL_COLOR_BUFFER_BIT) != 0 && !mIsDefault)
        {
            bool hasSRGBAttachment = false;
            for (const auto &attachment : mState.getColorAttachments())
            {
                if (attachment.isAttached() && attachment.getColorEncoding() == GL_SRGB)
                {
                    hasSRGBAttachment = true;
                    break;
                }
            }

            mStateManager->setFramebufferSRGBEnabled(context->getContextState(), hasSRGBAttachment);
        }
        else
        {
            mStateManager->setFramebufferSRGBEnabled(context->getContextState(), !mIsDefault);
        }
    }
}

void FramebufferGL::syncClearBufferState(ContextImpl *context, GLenum buffer, GLint drawBuffer)
{
    if (mFunctions->standard == STANDARD_GL_DESKTOP)
    {
        if (mWorkarounds.doesSRGBClearsOnLinearFramebufferAttachments && buffer == GL_COLOR &&
            !mIsDefault)
        {
            // If doing a clear on a color buffer, set SRGB blend enabled only if the color buffer
            // is an SRGB format.
            const auto &drawbufferState  = mState.getDrawBufferStates();
            const auto &colorAttachments = mState.getColorAttachments();

            const FramebufferAttachment *attachment = nullptr;
            if (drawbufferState[drawBuffer] >= GL_COLOR_ATTACHMENT0 &&
                drawbufferState[drawBuffer] < GL_COLOR_ATTACHMENT0 + colorAttachments.size())
            {
                size_t attachmentIdx =
                    static_cast<size_t>(drawbufferState[drawBuffer] - GL_COLOR_ATTACHMENT0);
                attachment = &colorAttachments[attachmentIdx];
            }

            if (attachment != nullptr)
            {
                mStateManager->setFramebufferSRGBEnabled(context->getContextState(),
                                                         attachment->getColorEncoding() == GL_SRGB);
            }
        }
        else
        {
            mStateManager->setFramebufferSRGBEnabled(context->getContextState(), !mIsDefault);
        }
    }
}

bool FramebufferGL::modifyInvalidateAttachmentsForEmulatedDefaultFBO(
    size_t count,
    const GLenum *attachments,
    std::vector<GLenum> *modifiedAttachments) const
{
    bool needsModification = mIsDefault && mFramebufferID != 0;
    if (!needsModification)
    {
        return false;
    }

    modifiedAttachments->resize(count);
    for (size_t i = 0; i < count; i++)
    {
        switch (attachments[i])
        {
            case GL_COLOR:
                (*modifiedAttachments)[i] = GL_COLOR_ATTACHMENT0;
                break;

            case GL_DEPTH:
                (*modifiedAttachments)[i] = GL_DEPTH_ATTACHMENT;
                break;

            case GL_STENCIL:
                (*modifiedAttachments)[i] = GL_STENCIL_ATTACHMENT;
                break;

            default:
                UNREACHABLE();
                break;
        }
    }

    return true;
}

gl::Error FramebufferGL::readPixelsRowByRowWorkaround(const gl::Rectangle &area,
                                                      GLenum format,
                                                      GLenum type,
                                                      const gl::PixelPackState &pack,
                                                      GLvoid *pixels) const
{
    intptr_t offset = reinterpret_cast<intptr_t>(pixels);

    const gl::InternalFormat &glFormat =
        gl::GetInternalFormatInfo(gl::GetSizedInternalFormat(format, type));
    GLuint rowBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeRowPitch(type, area.width, pack.alignment, pack.rowLength),
                     rowBytes);
    GLuint skipBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeSkipBytes(rowBytes, 0, pack, false), skipBytes);

    gl::PixelPackState directPack;
    directPack.pixelBuffer = pack.pixelBuffer;
    directPack.alignment   = 1;
    mStateManager->setPixelPackState(directPack);
    directPack.pixelBuffer.set(nullptr);

    offset += skipBytes;
    for (GLint row = 0; row < area.height; ++row)
    {
        mFunctions->readPixels(area.x, row + area.y, area.width, 1, format, type,
                               reinterpret_cast<GLvoid *>(offset));
        offset += row * rowBytes;
    }

    return gl::NoError();
}

gl::Error FramebufferGL::readPixelsPaddingWorkaround(const gl::Rectangle &area,
                                                     GLenum format,
                                                     GLenum type,
                                                     const gl::PixelPackState &pack,
                                                     GLvoid *pixels) const
{
    const gl::InternalFormat &glFormat =
        gl::GetInternalFormatInfo(gl::GetSizedInternalFormat(format, type));
    GLuint rowBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeRowPitch(type, area.width, pack.alignment, pack.rowLength),
                     rowBytes);
    GLuint skipBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeSkipBytes(rowBytes, 0, pack, false), skipBytes);

    // Get all by the last row
    if (area.height > 1)
    {
        mFunctions->readPixels(area.x, area.y, area.width, area.height - 1, format, type, pixels);
    }

    // Get the last row manually
    gl::PixelPackState directPack;
    directPack.pixelBuffer = pack.pixelBuffer;
    directPack.alignment   = 1;
    mStateManager->setPixelPackState(directPack);
    directPack.pixelBuffer.set(nullptr);

    intptr_t lastRowOffset =
        reinterpret_cast<intptr_t>(pixels) + skipBytes + (area.height - 1) * rowBytes;
    mFunctions->readPixels(area.x, area.y + area.height - 1, area.width, 1, format, type,
                           reinterpret_cast<GLvoid *>(lastRowOffset));

    return gl::NoError();
}
}  // namespace rx
