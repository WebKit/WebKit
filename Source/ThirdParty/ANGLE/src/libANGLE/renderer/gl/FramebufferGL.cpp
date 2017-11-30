//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FramebufferGL.cpp: Implements the class methods for FramebufferGL.

#include "libANGLE/renderer/gl/FramebufferGL.h"

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/State.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/gl/BlitGL.h"
#include "libANGLE/renderer/gl/ClearMultiviewGL.h"
#include "libANGLE/renderer/gl/ContextGL.h"
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

namespace
{

void BindFramebufferAttachment(const FunctionsGL *functions,
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
                texture->getTarget() == GL_TEXTURE_2D_MULTISAMPLE ||
                texture->getTarget() == GL_TEXTURE_RECTANGLE_ANGLE)
            {
                functions->framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoint,
                                                texture->getTarget(), textureGL->getTextureID(),
                                                attachment->mipLevel());
            }
            else if (texture->getTarget() == GL_TEXTURE_CUBE_MAP)
            {
                functions->framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoint,
                                                attachment->cubeMapFace(),
                                                textureGL->getTextureID(), attachment->mipLevel());
            }
            else if (texture->getTarget() == GL_TEXTURE_2D_ARRAY ||
                     texture->getTarget() == GL_TEXTURE_3D)
            {
                if (attachment->getMultiviewLayout() == GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE)
                {
                    ASSERT(functions->framebufferTexture);
                    functions->framebufferTexture(GL_FRAMEBUFFER, attachmentPoint,
                                                  textureGL->getTextureID(),
                                                  attachment->mipLevel());
                }
                else
                {
                    functions->framebufferTextureLayer(GL_FRAMEBUFFER, attachmentPoint,
                                                       textureGL->getTextureID(),
                                                       attachment->mipLevel(), attachment->layer());
                }
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

bool AreAllLayersActive(const FramebufferAttachment &attachment)
{
    int baseViewIndex = attachment.getBaseViewIndex();
    if (baseViewIndex != 0)
    {
        return false;
    }
    const ImageIndex &imageIndex = attachment.getTextureImageIndex();
    int numLayers =
        static_cast<int>(attachment.getTexture()->getDepth(imageIndex.type, imageIndex.mipIndex));
    return (attachment.getNumViews() == numLayers);
}

bool RequiresMultiviewClear(const FramebufferState &state, bool scissorTestEnabled)
{
    // Get one attachment and check whether all layers are attached.
    const FramebufferAttachment *attachment = nullptr;
    bool allTextureArraysAreFullyAttached   = true;
    for (const FramebufferAttachment &colorAttachment : state.getColorAttachments())
    {
        if (colorAttachment.isAttached())
        {
            if (colorAttachment.getMultiviewLayout() == GL_NONE)
            {
                return false;
            }
            attachment = &colorAttachment;
            allTextureArraysAreFullyAttached =
                allTextureArraysAreFullyAttached && AreAllLayersActive(*attachment);
        }
    }

    const FramebufferAttachment *depthAttachment = state.getDepthAttachment();
    if (depthAttachment)
    {
        if (depthAttachment->getMultiviewLayout() == GL_NONE)
        {
            return false;
        }
        attachment = depthAttachment;
        allTextureArraysAreFullyAttached =
            allTextureArraysAreFullyAttached && AreAllLayersActive(*attachment);
    }
    const FramebufferAttachment *stencilAttachment = state.getStencilAttachment();
    if (stencilAttachment)
    {
        if (stencilAttachment->getMultiviewLayout() == GL_NONE)
        {
            return false;
        }
        attachment = stencilAttachment;
        allTextureArraysAreFullyAttached =
            allTextureArraysAreFullyAttached && AreAllLayersActive(*attachment);
    }

    if (attachment == nullptr)
    {
        return false;
    }
    switch (attachment->getMultiviewLayout())
    {
        case GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE:
            // If all layers of each texture array are active, then there is no need to issue a
            // special multiview clear.
            return !allTextureArraysAreFullyAttached;
        case GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE:
            return (scissorTestEnabled == true);
        default:
            UNREACHABLE();
    }
    return false;
}

}  // namespace

FramebufferGL::FramebufferGL(const FramebufferState &state,
                             const FunctionsGL *functions,
                             StateManagerGL *stateManager,
                             const WorkaroundsGL &workarounds,
                             BlitGL *blitter,
                             ClearMultiviewGL *multiviewClearer,
                             bool isDefault)
    : FramebufferImpl(state),
      mFunctions(functions),
      mStateManager(stateManager),
      mWorkarounds(workarounds),
      mBlitter(blitter),
      mMultiviewClearer(multiviewClearer),
      mFramebufferID(0),
      mIsDefault(isDefault),
      mAppliedEnabledDrawBuffers(1)
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
                             ClearMultiviewGL *multiviewClearer,
                             StateManagerGL *stateManager)
    : FramebufferImpl(state),
      mFunctions(functions),
      mStateManager(stateManager),
      mWorkarounds(workarounds),
      mBlitter(blitter),
      mMultiviewClearer(multiviewClearer),
      mFramebufferID(id),
      mIsDefault(true),
      mAppliedEnabledDrawBuffers(1)
{
}

FramebufferGL::~FramebufferGL()
{
    mStateManager->deleteFramebuffer(mFramebufferID);
    mFramebufferID = 0;
}

Error FramebufferGL::discard(const gl::Context *context, size_t count, const GLenum *attachments)
{
    // glInvalidateFramebuffer accepts the same enums as glDiscardFramebufferEXT
    return invalidate(context, count, attachments);
}

Error FramebufferGL::invalidate(const gl::Context *context, size_t count, const GLenum *attachments)
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
    else if (mFunctions->discardFramebufferEXT)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        mFunctions->discardFramebufferEXT(GL_FRAMEBUFFER, static_cast<GLsizei>(count),
                                          finalAttachmentsPtr);
    }

    return gl::NoError();
}

Error FramebufferGL::invalidateSub(const gl::Context *context,
                                   size_t count,
                                   const GLenum *attachments,
                                   const gl::Rectangle &area)
{

    const GLenum *finalAttachmentsPtr = attachments;

    std::vector<GLenum> modifiedAttachments;
    if (modifyInvalidateAttachmentsForEmulatedDefaultFBO(count, attachments, &modifiedAttachments))
    {
        finalAttachmentsPtr = modifiedAttachments.data();
    }

    // Since this function is just a hint and not available until OpenGL 4.3, only call it if it is
    // available.
    if (mFunctions->invalidateSubFramebuffer)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        mFunctions->invalidateSubFramebuffer(GL_FRAMEBUFFER, static_cast<GLsizei>(count),
                                             finalAttachmentsPtr, area.x, area.y, area.width,
                                             area.height);
    }

    return gl::NoError();
}

Error FramebufferGL::clear(const gl::Context *context, GLbitfield mask)
{
    syncClearState(context, mask);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    if (!RequiresMultiviewClear(mState, context->getGLState().isScissorTestEnabled()))
    {
        mFunctions->clear(mask);
    }
    else
    {
        mMultiviewClearer->clearMultiviewFBO(mState, context->getGLState().getScissor(),
                                             ClearMultiviewGL::ClearCommandType::Clear, mask,
                                             GL_NONE, 0, nullptr, 0.0f, 0);
    }

    return gl::NoError();
}

Error FramebufferGL::clearBufferfv(const gl::Context *context,
                                   GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat *values)
{
    syncClearBufferState(context, buffer, drawbuffer);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    if (!RequiresMultiviewClear(mState, context->getGLState().isScissorTestEnabled()))
    {
        mFunctions->clearBufferfv(buffer, drawbuffer, values);
    }
    else
    {
        mMultiviewClearer->clearMultiviewFBO(mState, context->getGLState().getScissor(),
                                             ClearMultiviewGL::ClearCommandType::ClearBufferfv,
                                             static_cast<GLbitfield>(0u), buffer, drawbuffer,
                                             reinterpret_cast<const uint8_t *>(values), 0.0f, 0);
    }

    return gl::NoError();
}

Error FramebufferGL::clearBufferuiv(const gl::Context *context,
                                    GLenum buffer,
                                    GLint drawbuffer,
                                    const GLuint *values)
{
    syncClearBufferState(context, buffer, drawbuffer);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    if (!RequiresMultiviewClear(mState, context->getGLState().isScissorTestEnabled()))
    {
        mFunctions->clearBufferuiv(buffer, drawbuffer, values);
    }
    else
    {
        mMultiviewClearer->clearMultiviewFBO(mState, context->getGLState().getScissor(),
                                             ClearMultiviewGL::ClearCommandType::ClearBufferuiv,
                                             static_cast<GLbitfield>(0u), buffer, drawbuffer,
                                             reinterpret_cast<const uint8_t *>(values), 0.0f, 0);
    }

    return gl::NoError();
}

Error FramebufferGL::clearBufferiv(const gl::Context *context,
                                   GLenum buffer,
                                   GLint drawbuffer,
                                   const GLint *values)
{
    syncClearBufferState(context, buffer, drawbuffer);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    if (!RequiresMultiviewClear(mState, context->getGLState().isScissorTestEnabled()))
    {
        mFunctions->clearBufferiv(buffer, drawbuffer, values);
    }
    else
    {
        mMultiviewClearer->clearMultiviewFBO(mState, context->getGLState().getScissor(),
                                             ClearMultiviewGL::ClearCommandType::ClearBufferiv,
                                             static_cast<GLbitfield>(0u), buffer, drawbuffer,
                                             reinterpret_cast<const uint8_t *>(values), 0.0f, 0);
    }

    return gl::NoError();
}

Error FramebufferGL::clearBufferfi(const gl::Context *context,
                                   GLenum buffer,
                                   GLint drawbuffer,
                                   GLfloat depth,
                                   GLint stencil)
{
    syncClearBufferState(context, buffer, drawbuffer);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    if (!RequiresMultiviewClear(mState, context->getGLState().isScissorTestEnabled()))
    {
        mFunctions->clearBufferfi(buffer, drawbuffer, depth, stencil);
    }
    else
    {
        mMultiviewClearer->clearMultiviewFBO(mState, context->getGLState().getScissor(),
                                             ClearMultiviewGL::ClearCommandType::ClearBufferfi,
                                             static_cast<GLbitfield>(0u), buffer, drawbuffer,
                                             nullptr, depth, stencil);
    }

    return gl::NoError();
}

GLenum FramebufferGL::getImplementationColorReadFormat(const gl::Context *context) const
{
    const auto *readAttachment = mState.getReadAttachment();
    const Format &format       = readAttachment->getFormat();
    return format.info->getReadPixelsFormat();
}

GLenum FramebufferGL::getImplementationColorReadType(const gl::Context *context) const
{
    const auto *readAttachment = mState.getReadAttachment();
    const Format &format       = readAttachment->getFormat();
    return format.info->getReadPixelsType(context->getClientVersion());
}

Error FramebufferGL::readPixels(const gl::Context *context,
                                const gl::Rectangle &origArea,
                                GLenum format,
                                GLenum type,
                                void *ptrOrOffset)
{
    // Clip read area to framebuffer.
    const gl::Extents fbSize = getState().getReadAttachment()->getSize();
    const gl::Rectangle fbRect(0, 0, fbSize.width, fbSize.height);
    gl::Rectangle area;
    if (!ClipRectangle(origArea, fbRect, &area))
    {
        // nothing to read
        return gl::NoError();
    }

    PixelPackState packState     = context->getGLState().getPackState();
    const gl::Buffer *packBuffer =
        context->getGLState().getTargetBuffer(gl::BufferBinding::PixelPack);

    nativegl::ReadPixelsFormat readPixelsFormat =
        nativegl::GetReadPixelsFormat(mFunctions, mWorkarounds, format, type);
    GLenum readFormat = readPixelsFormat.format;
    GLenum readType   = readPixelsFormat.type;

    mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, mFramebufferID);

    bool useOverlappingRowsWorkaround = mWorkarounds.packOverlappingRowsSeparatelyPackBuffer &&
                                        packBuffer && packState.rowLength != 0 &&
                                        packState.rowLength < area.width;

    GLubyte *pixels = reinterpret_cast<GLubyte *>(ptrOrOffset);
    int leftClip    = area.x - origArea.x;
    int topClip     = area.y - origArea.y;
    if (leftClip || topClip)
    {
        // Adjust destination to match portion clipped off left and/or top.
        const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(readFormat, readType);

        GLuint rowBytes = 0;
        ANGLE_TRY_RESULT(glFormat.computeRowPitch(readType, origArea.width, packState.alignment,
                                                  packState.rowLength),
                         rowBytes);
        pixels += leftClip * glFormat.pixelBytes + topClip * rowBytes;
    }

    if (packState.rowLength == 0 && area.width != origArea.width)
    {
        // No rowLength was specified so it will derive from read width, but clipping changed the
        // read width.  Use the original width so we fill the user's buffer as they intended.
        packState.rowLength = origArea.width;
    }

    // We want to use rowLength, but that might not be supported.
    bool cannotSetDesiredRowLength =
        packState.rowLength && !GetImplAs<ContextGL>(context)->getNativeExtensions().packSubimage;

    gl::Error retVal = gl::NoError();
    if (cannotSetDesiredRowLength || useOverlappingRowsWorkaround)
    {
        retVal = readPixelsRowByRow(context, area, readFormat, readType, packState, pixels);
    }
    else
    {
        gl::ErrorOrResult<bool> useLastRowPaddingWorkaround = false;
        if (mWorkarounds.packLastRowSeparatelyForPaddingInclusion)
        {
            useLastRowPaddingWorkaround = ShouldApplyLastRowPaddingWorkaround(
                gl::Extents(area.width, area.height, 1), packState, packBuffer, readFormat,
                readType, false, pixels);
        }

        if (useLastRowPaddingWorkaround.isError())
        {
            retVal = useLastRowPaddingWorkaround.getError();
        }
        else
        {
            retVal = readPixelsAllAtOnce(context, area, readFormat, readType, packState, pixels,
                                         useLastRowPaddingWorkaround.getResult());
        }
    }

    return retVal;
}

Error FramebufferGL::blit(const gl::Context *context,
                          const gl::Rectangle &sourceArea,
                          const gl::Rectangle &destArea,
                          GLbitfield mask,
                          GLenum filter)
{
    const Framebuffer *sourceFramebuffer = context->getGLState().getReadFramebuffer();
    const Framebuffer *destFramebuffer   = context->getGLState().getDrawFramebuffer();

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
        bool sourceSRGB =
            colorReadAttachment != nullptr && colorReadAttachment->getColorEncoding() == GL_SRGB;
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
    mStateManager->setFramebufferSRGBEnabledForFramebuffer(context, true, this);

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

bool FramebufferGL::checkStatus(const gl::Context *context) const
{
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    GLenum status = mFunctions->checkFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        WARN() << "GL framebuffer returned incomplete.";
    }
    return (status == GL_FRAMEBUFFER_COMPLETE);
}

void FramebufferGL::syncState(const gl::Context *context, const Framebuffer::DirtyBits &dirtyBits)
{
    // Don't need to sync state for the default FBO.
    if (mIsDefault)
    {
        return;
    }

    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    // A pointer to one of the attachments for which the texture or the render buffer is not zero.
    const FramebufferAttachment *attachment = nullptr;

    for (auto dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
            {
                const FramebufferAttachment *newAttachment = mState.getDepthAttachment();
                BindFramebufferAttachment(mFunctions, GL_DEPTH_ATTACHMENT, newAttachment);
                if (newAttachment)
                {
                    attachment = newAttachment;
                }
                break;
            }
            case Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
            {
                const FramebufferAttachment *newAttachment = mState.getStencilAttachment();
                BindFramebufferAttachment(mFunctions, GL_STENCIL_ATTACHMENT, newAttachment);
                if (newAttachment)
                {
                    attachment = newAttachment;
                }
                break;
            }
            case Framebuffer::DIRTY_BIT_DRAW_BUFFERS:
            {
                const auto &drawBuffers = mState.getDrawBufferStates();
                mFunctions->drawBuffers(static_cast<GLsizei>(drawBuffers.size()),
                                        drawBuffers.data());
                mAppliedEnabledDrawBuffers = mState.getEnabledDrawBuffers();
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
                mFunctions->framebufferParameteri(
                    GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS,
                    gl::ConvertToGLBoolean(mState.getDefaultFixedSampleLocations()));
                break;
            default:
            {
                ASSERT(Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 == 0 &&
                       dirtyBit < Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX);
                size_t index =
                    static_cast<size_t>(dirtyBit - Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0);
                const FramebufferAttachment *newAttachment = mState.getColorAttachment(index);
                BindFramebufferAttachment(
                    mFunctions, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index), newAttachment);
                if (newAttachment)
                {
                    attachment = newAttachment;
                }
                break;
            }
        }
    }

    if (attachment)
    {
        const bool isSideBySide =
            (attachment->getMultiviewLayout() == GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE);
        mStateManager->setSideBySide(isSideBySide);
        mStateManager->setViewportOffsets(attachment->getMultiviewViewportOffsets());
        mStateManager->updateMultiviewBaseViewLayerIndexUniform(context->getGLState().getProgram(),
                                                                getState());
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

void FramebufferGL::maskOutInactiveOutputDrawBuffers(DrawBufferMask maxSet)
{
    auto targetAppliedDrawBuffers = mState.getEnabledDrawBuffers() & maxSet;
    if (mAppliedEnabledDrawBuffers != targetAppliedDrawBuffers)
    {
        mAppliedEnabledDrawBuffers = targetAppliedDrawBuffers;

        const auto &stateDrawBuffers = mState.getDrawBufferStates();
        GLsizei drawBufferCount      = static_cast<GLsizei>(stateDrawBuffers.size());
        ASSERT(drawBufferCount <= IMPLEMENTATION_MAX_DRAW_BUFFERS);

        GLenum drawBuffers[IMPLEMENTATION_MAX_DRAW_BUFFERS];
        for (GLenum i = 0; static_cast<int>(i) < drawBufferCount; ++i)
        {
            drawBuffers[i] = targetAppliedDrawBuffers[i] ? stateDrawBuffers[i] : GL_NONE;
        }

        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        mFunctions->drawBuffers(drawBufferCount, drawBuffers);
    }
}

void FramebufferGL::syncClearState(const gl::Context *context, GLbitfield mask)
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

            mStateManager->setFramebufferSRGBEnabled(context, hasSRGBAttachment);
        }
        else
        {
            mStateManager->setFramebufferSRGBEnabled(context, !mIsDefault);
        }
    }
}

void FramebufferGL::syncClearBufferState(const gl::Context *context,
                                         GLenum buffer,
                                         GLint drawBuffer)
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
                mStateManager->setFramebufferSRGBEnabled(context,
                                                         attachment->getColorEncoding() == GL_SRGB);
            }
        }
        else
        {
            mStateManager->setFramebufferSRGBEnabled(context, !mIsDefault);
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

gl::Error FramebufferGL::readPixelsRowByRow(const gl::Context *context,
                                            const gl::Rectangle &area,
                                            GLenum format,
                                            GLenum type,
                                            const gl::PixelPackState &pack,
                                            GLubyte *pixels) const
{
    const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(format, type);

    GLuint rowBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeRowPitch(type, area.width, pack.alignment, pack.rowLength),
                     rowBytes);
    GLuint skipBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeSkipBytes(rowBytes, 0, pack, false), skipBytes);

    gl::PixelPackState directPack;
    directPack.alignment   = 1;
    mStateManager->setPixelPackState(directPack);

    pixels += skipBytes;
    for (GLint y = area.y; y < area.y + area.height; ++y)
    {
        mFunctions->readPixels(area.x, y, area.width, 1, format, type, pixels);
        pixels += rowBytes;
    }

    return gl::NoError();
}

gl::Error FramebufferGL::readPixelsAllAtOnce(const gl::Context *context,
                                             const gl::Rectangle &area,
                                             GLenum format,
                                             GLenum type,
                                             const gl::PixelPackState &pack,
                                             GLubyte *pixels,
                                             bool readLastRowSeparately) const
{
    GLint height = area.height - readLastRowSeparately;
    if (height > 0)
    {
        mStateManager->setPixelPackState(pack);
        mFunctions->readPixels(area.x, area.y, area.width, height, format, type, pixels);
    }

    if (readLastRowSeparately)
    {
        const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(format, type);

        GLuint rowBytes = 0;
        ANGLE_TRY_RESULT(glFormat.computeRowPitch(type, area.width, pack.alignment, pack.rowLength),
                         rowBytes);
        GLuint skipBytes = 0;
        ANGLE_TRY_RESULT(glFormat.computeSkipBytes(rowBytes, 0, pack, false), skipBytes);

        gl::PixelPackState directPack;
        directPack.alignment = 1;
        mStateManager->setPixelPackState(directPack);

        pixels += skipBytes + (area.height - 1) * rowBytes;
        mFunctions->readPixels(area.x, area.y + area.height - 1, area.width, 1, format, type,
                               pixels);
    }

    return gl::NoError();
}
}  // namespace rx
