//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FramebufferGL.cpp: Implements the class methods for FramebufferGL.

#include "libANGLE/renderer/gl/FramebufferGL.h"

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "libANGLE/ErrorStrings.h"
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
#include "libANGLE/renderer/gl/formatutilsgl.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"
#include "platform/FeaturesGL_autogen.h"
#include "platform/PlatformMethods.h"

using namespace gl;
using angle::CheckedNumeric;

namespace rx
{

namespace
{

struct BlitFramebufferBounds
{
    gl::Rectangle sourceBounds;
    gl::Rectangle sourceRegion;

    gl::Rectangle destBounds;
    gl::Rectangle destRegion;

    bool xFlipped;
    bool yFlipped;
};

static BlitFramebufferBounds GetBlitFramebufferBounds(const gl::Context *context,
                                                      const gl::Rectangle &sourceArea,
                                                      const gl::Rectangle &destArea)
{
    BlitFramebufferBounds bounds;

    const Framebuffer *sourceFramebuffer = context->getState().getReadFramebuffer();
    const Framebuffer *destFramebuffer   = context->getState().getDrawFramebuffer();

    gl::Extents readSize = sourceFramebuffer->getExtents();
    gl::Extents drawSize = destFramebuffer->getExtents();

    bounds.sourceBounds = gl::Rectangle(0, 0, readSize.width, readSize.height);
    bounds.sourceRegion = sourceArea.removeReversal();

    bounds.destBounds = gl::Rectangle(0, 0, drawSize.width, drawSize.height);
    bounds.destRegion = destArea.removeReversal();

    bounds.xFlipped = sourceArea.isReversedX() != destArea.isReversedX();
    bounds.yFlipped = sourceArea.isReversedY() != destArea.isReversedY();

    return bounds;
}

void BindFramebufferAttachment(const FunctionsGL *functions,
                               GLenum attachmentPoint,
                               const FramebufferAttachment *attachment,
                               const angle::FeaturesGL &features)
{
    if (attachment)
    {
        if (attachment->type() == GL_TEXTURE)
        {
            const Texture *texture     = attachment->getTexture();
            const TextureGL *textureGL = GetImplAs<TextureGL>(texture);

            if (texture->getType() == TextureType::_2D ||
                texture->getType() == TextureType::_2DMultisample ||
                texture->getType() == TextureType::Rectangle ||
                texture->getType() == TextureType::External)
            {
                if (attachment->isRenderToTexture())
                {
                    if (functions->framebufferTexture2DMultisampleEXT)
                    {
                        functions->framebufferTexture2DMultisampleEXT(
                            GL_FRAMEBUFFER, attachmentPoint, ToGLenum(texture->getType()),
                            textureGL->getTextureID(), attachment->mipLevel(),
                            attachment->getSamples());
                    }
                    else
                    {
                        ASSERT(functions->framebufferTexture2DMultisampleIMG);
                        functions->framebufferTexture2DMultisampleIMG(
                            GL_FRAMEBUFFER, attachmentPoint, ToGLenum(texture->getType()),
                            textureGL->getTextureID(), attachment->mipLevel(),
                            attachment->getSamples());
                    }
                }
                else
                {
                    functions->framebufferTexture2D(
                        GL_FRAMEBUFFER, attachmentPoint, ToGLenum(texture->getType()),
                        textureGL->getTextureID(), attachment->mipLevel());
                }
            }
            else if (attachment->isLayered())
            {
                TextureType textureType = texture->getType();
                ASSERT(textureType == TextureType::_2DArray || textureType == TextureType::_3D ||
                       textureType == TextureType::CubeMap ||
                       textureType == TextureType::_2DMultisampleArray ||
                       textureType == TextureType::CubeMapArray);
                functions->framebufferTexture(GL_FRAMEBUFFER, attachmentPoint,
                                              textureGL->getTextureID(), attachment->mipLevel());
            }
            else if (texture->getType() == TextureType::CubeMap)
            {
                functions->framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoint,
                                                ToGLenum(attachment->cubeMapFace()),
                                                textureGL->getTextureID(), attachment->mipLevel());
            }
            else if (texture->getType() == TextureType::_2DArray ||
                     texture->getType() == TextureType::_3D ||
                     texture->getType() == TextureType::_2DMultisampleArray ||
                     texture->getType() == TextureType::CubeMapArray)
            {
                if (attachment->isMultiview())
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

            if (features.alwaysUnbindFramebufferTexture2D.enabled)
            {
                functions->framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoint, GL_TEXTURE_2D, 0,
                                                0);
            }

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
    int numLayers                = static_cast<int>(
        attachment.getTexture()->getDepth(imageIndex.getTarget(), imageIndex.getLevelIndex()));
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
            if (!colorAttachment.isMultiview())
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
        if (!depthAttachment->isMultiview())
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
        if (!stencilAttachment->isMultiview())
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
    if (attachment->isMultiview())
    {
        // If all layers of each texture array are active, then there is no need to issue a
        // special multiview clear.
        return !allTextureArraysAreFullyAttached;
    }
    return false;
}

bool IsEmulatedAlphaChannelTextureAttachment(const FramebufferAttachment *attachment)
{
    if (!attachment || attachment->type() != GL_TEXTURE)
    {
        return false;
    }

    const Texture *texture     = attachment->getTexture();
    const TextureGL *textureGL = GetImplAs<TextureGL>(texture);
    return textureGL->hasEmulatedAlphaChannel(attachment->getTextureImageIndex());
}

class [[nodiscard]] ScopedEXTTextureNorm16ReadbackWorkaround
{
  public:
    ScopedEXTTextureNorm16ReadbackWorkaround()
        : tmpPixels(nullptr), clientPixels(nullptr), enabled(false)
    {}

    ~ScopedEXTTextureNorm16ReadbackWorkaround()
    {
        if (tmpPixels)
        {
            delete[] tmpPixels;
        }
    }

    angle::Result Initialize(const gl::Context *context,
                             const gl::Rectangle &area,
                             GLenum originalReadFormat,
                             GLenum format,
                             GLenum type,
                             GLuint skipBytes,
                             GLuint rowBytes,
                             GLuint pixelBytes,
                             GLubyte *pixels)
    {
        // Separate from constructor as there may be checked math result exception that needs to
        // early return
        ASSERT(tmpPixels == nullptr);
        ASSERT(clientPixels == nullptr);

        ContextGL *contextGL              = GetImplAs<ContextGL>(context);
        const angle::FeaturesGL &features = GetFeaturesGL(context);

        enabled = features.readPixelsUsingImplementationColorReadFormatForNorm16.enabled &&
                  type == GL_UNSIGNED_SHORT && originalReadFormat == GL_RGBA &&
                  (format == GL_RED || format == GL_RG);

        clientPixels = pixels;

        if (enabled)
        {
            CheckedNumeric<GLuint> checkedRowBytes(rowBytes);
            CheckedNumeric<GLuint> checkedRows(area.height);
            CheckedNumeric<GLuint> checkedSkipBytes(skipBytes);
            auto checkedAllocatedBytes = checkedSkipBytes + checkedRowBytes * checkedRows;
            if (rowBytes < area.width * pixelBytes)
            {
                checkedAllocatedBytes += area.width * pixelBytes - rowBytes;
            }
            ANGLE_CHECK_GL_MATH(contextGL, checkedAllocatedBytes.IsValid());
            const GLuint allocatedBytes = checkedAllocatedBytes.ValueOrDie();
            tmpPixels                   = new GLubyte[allocatedBytes];
            memset(tmpPixels, 0, allocatedBytes);
        }

        return angle::Result::Continue;
    }

    GLubyte *Pixels() const { return tmpPixels ? tmpPixels : clientPixels; }

    bool IsEnabled() const { return enabled; }

  private:
    // Temporarily allocated pixel readback buffer
    GLubyte *tmpPixels;
    // Client pixel array pointer passed to readPixels
    GLubyte *clientPixels;

    bool enabled;
};

// Workaround to rearrange pixels read by RED/RG to RGBA for RGBA/UNSIGNED_SHORT pixel type
// combination
angle::Result RearrangeEXTTextureNorm16Pixels(const gl::Context *context,
                                              const gl::Rectangle &area,
                                              GLenum originalReadFormat,
                                              GLenum format,
                                              GLenum type,
                                              GLuint skipBytes,
                                              GLuint rowBytes,
                                              GLuint pixelBytes,
                                              const gl::PixelPackState &pack,
                                              GLubyte *clientPixels,
                                              GLubyte *tmpPixels)
{
    ASSERT(tmpPixels != nullptr);
    ASSERT(originalReadFormat == GL_RGBA);
    ASSERT(format == GL_RED_EXT || format == GL_RG_EXT);
    ASSERT(type == GL_UNSIGNED_SHORT);

    ContextGL *contextGL = GetImplAs<ContextGL>(context);

    const gl::InternalFormat &glFormatOriginal =
        gl::GetInternalFormatInfo(originalReadFormat, type);

    GLuint originalReadFormatRowBytes = 0;
    ANGLE_CHECK_GL_MATH(
        contextGL, glFormatOriginal.computeRowPitch(type, area.width, pack.alignment,
                                                    pack.rowLength, &originalReadFormatRowBytes));
    GLuint originalReadFormatSkipBytes = 0;
    ANGLE_CHECK_GL_MATH(contextGL,
                        glFormatOriginal.computeSkipBytes(type, originalReadFormatRowBytes, 0, pack,
                                                          false, &originalReadFormatSkipBytes));

    GLuint originalReadFormatPixelBytes = glFormatOriginal.computePixelBytes(type);
    GLuint alphaChannelBytes            = glFormatOriginal.alphaBits / 8;

    ASSERT(originalReadFormatPixelBytes > pixelBytes);
    ASSERT(originalReadFormatPixelBytes > alphaChannelBytes);
    ASSERT(alphaChannelBytes != 0);
    ASSERT(glFormatOriginal.alphaBits % 8 == 0);

    // Populating rearrangedPixels values from pixels
    GLubyte *srcRowStart = tmpPixels;
    GLubyte *dstRowStart = clientPixels;

    srcRowStart += skipBytes;
    dstRowStart += originalReadFormatSkipBytes;

    for (GLint y = 0; y < area.height; ++y)
    {
        GLubyte *src = srcRowStart;
        GLubyte *dst = dstRowStart;
        for (GLint x = 0; x < area.width; ++x)
        {
            GLushort *srcPixel = reinterpret_cast<GLushort *>(src);
            GLushort *dstPixel = reinterpret_cast<GLushort *>(dst);
            dstPixel[0]        = srcPixel[0];
            dstPixel[1]        = format == GL_RG ? srcPixel[1] : 0;
            // Set other channel of RGBA to 0 (GB when format == GL_RED, B when format == GL_RG)
            dstPixel[2] = 0;
            // Set alpha channel to 1
            dstPixel[3] = 0xFFFF;

            src += pixelBytes;
            dst += originalReadFormatPixelBytes;
        }

        srcRowStart += rowBytes;
        dstRowStart += originalReadFormatRowBytes;
    }

    return angle::Result::Continue;
}

bool IsValidUnsignedShortReadPixelsFormat(GLenum readFormat, const gl::Context *context)
{
    return (readFormat == GL_RED) || (readFormat == GL_RG) || (readFormat == GL_RGBA) ||
           ((readFormat == GL_DEPTH_COMPONENT) && (context->getExtensions().readDepthNV));
}

}  // namespace

FramebufferGL::FramebufferGL(const gl::FramebufferState &data, GLuint id, bool emulatedAlpha)
    : FramebufferImpl(data),
      mFramebufferID(id),
      mHasEmulatedAlphaAttachment(emulatedAlpha),
      mAppliedEnabledDrawBuffers(1)
{
    ASSERT((isDefault() && id == 0) || !isDefault());
}

FramebufferGL::~FramebufferGL()
{
    ASSERT(mFramebufferID == 0);
}

void FramebufferGL::destroy(const gl::Context *context)
{
    if (mFramebufferID)
    {
        ASSERT(!isDefault());
        StateManagerGL *stateManager = GetStateManagerGL(context);
        stateManager->deleteFramebuffer(mFramebufferID);
        mFramebufferID = 0;
    }
}

angle::Result FramebufferGL::discard(const gl::Context *context,
                                     size_t count,
                                     const GLenum *attachments)
{
    // glInvalidateFramebuffer accepts the same enums as glDiscardFramebufferEXT
    return invalidate(context, count, attachments);
}

angle::Result FramebufferGL::invalidate(const gl::Context *context,
                                        size_t count,
                                        const GLenum *attachments)
{
    const GLenum *finalAttachmentsPtr = attachments;

    std::vector<GLenum> modifiedAttachments;
    if (modifyInvalidateAttachmentsForEmulatedDefaultFBO(count, attachments, &modifiedAttachments))
    {
        finalAttachmentsPtr = modifiedAttachments.data();
    }

    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    // Since this function is just a hint, only call a native function if it exists.
    if (functions->invalidateFramebuffer)
    {
        stateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        functions->invalidateFramebuffer(GL_FRAMEBUFFER, static_cast<GLsizei>(count),
                                         finalAttachmentsPtr);
    }
    else if (functions->discardFramebufferEXT)
    {
        stateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        functions->discardFramebufferEXT(GL_FRAMEBUFFER, static_cast<GLsizei>(count),
                                         finalAttachmentsPtr);
    }

    return angle::Result::Continue;
}

angle::Result FramebufferGL::invalidateSub(const gl::Context *context,
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

    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    // Since this function is just a hint and not available until OpenGL 4.3, only call it if it is
    // available.
    if (functions->invalidateSubFramebuffer)
    {
        stateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        functions->invalidateSubFramebuffer(GL_FRAMEBUFFER, static_cast<GLsizei>(count),
                                            finalAttachmentsPtr, area.x, area.y, area.width,
                                            area.height);
    }

    return angle::Result::Continue;
}

angle::Result FramebufferGL::clear(const gl::Context *context, GLbitfield mask)
{
    ContextGL *contextGL         = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    syncClearState(context, mask);
    stateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    if (!RequiresMultiviewClear(mState, context->getState().isScissorTestEnabled()))
    {
        functions->clear(mask);
    }
    else
    {
        ClearMultiviewGL *multiviewClearer = GetMultiviewClearer(context);
        multiviewClearer->clearMultiviewFBO(mState, context->getState().getScissor(),
                                            ClearMultiviewGL::ClearCommandType::Clear, mask,
                                            GL_NONE, 0, nullptr, 0.0f, 0);
    }

    contextGL->markWorkSubmitted();
    return angle::Result::Continue;
}

angle::Result FramebufferGL::clearBufferfv(const gl::Context *context,
                                           GLenum buffer,
                                           GLint drawbuffer,
                                           const GLfloat *values)
{
    ContextGL *contextGL         = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    syncClearBufferState(context, buffer, drawbuffer);
    stateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    if (!RequiresMultiviewClear(mState, context->getState().isScissorTestEnabled()))
    {
        functions->clearBufferfv(buffer, drawbuffer, values);
    }
    else
    {
        ClearMultiviewGL *multiviewClearer = GetMultiviewClearer(context);
        multiviewClearer->clearMultiviewFBO(mState, context->getState().getScissor(),
                                            ClearMultiviewGL::ClearCommandType::ClearBufferfv,
                                            static_cast<GLbitfield>(0u), buffer, drawbuffer,
                                            reinterpret_cast<const uint8_t *>(values), 0.0f, 0);
    }

    contextGL->markWorkSubmitted();
    return angle::Result::Continue;
}

angle::Result FramebufferGL::clearBufferuiv(const gl::Context *context,
                                            GLenum buffer,
                                            GLint drawbuffer,
                                            const GLuint *values)
{
    ContextGL *contextGL         = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    syncClearBufferState(context, buffer, drawbuffer);
    stateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    if (!RequiresMultiviewClear(mState, context->getState().isScissorTestEnabled()))
    {
        functions->clearBufferuiv(buffer, drawbuffer, values);
    }
    else
    {
        ClearMultiviewGL *multiviewClearer = GetMultiviewClearer(context);
        multiviewClearer->clearMultiviewFBO(mState, context->getState().getScissor(),
                                            ClearMultiviewGL::ClearCommandType::ClearBufferuiv,
                                            static_cast<GLbitfield>(0u), buffer, drawbuffer,
                                            reinterpret_cast<const uint8_t *>(values), 0.0f, 0);
    }

    contextGL->markWorkSubmitted();
    return angle::Result::Continue;
}

angle::Result FramebufferGL::clearBufferiv(const gl::Context *context,
                                           GLenum buffer,
                                           GLint drawbuffer,
                                           const GLint *values)
{
    ContextGL *contextGL         = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    syncClearBufferState(context, buffer, drawbuffer);
    stateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    if (!RequiresMultiviewClear(mState, context->getState().isScissorTestEnabled()))
    {
        functions->clearBufferiv(buffer, drawbuffer, values);
    }
    else
    {
        ClearMultiviewGL *multiviewClearer = GetMultiviewClearer(context);
        multiviewClearer->clearMultiviewFBO(mState, context->getState().getScissor(),
                                            ClearMultiviewGL::ClearCommandType::ClearBufferiv,
                                            static_cast<GLbitfield>(0u), buffer, drawbuffer,
                                            reinterpret_cast<const uint8_t *>(values), 0.0f, 0);
    }

    contextGL->markWorkSubmitted();
    return angle::Result::Continue;
}

angle::Result FramebufferGL::clearBufferfi(const gl::Context *context,
                                           GLenum buffer,
                                           GLint drawbuffer,
                                           GLfloat depth,
                                           GLint stencil)
{
    ContextGL *contextGL         = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    syncClearBufferState(context, buffer, drawbuffer);
    stateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    if (!RequiresMultiviewClear(mState, context->getState().isScissorTestEnabled()))
    {
        functions->clearBufferfi(buffer, drawbuffer, depth, stencil);
    }
    else
    {
        ClearMultiviewGL *multiviewClearer = GetMultiviewClearer(context);
        multiviewClearer->clearMultiviewFBO(mState, context->getState().getScissor(),
                                            ClearMultiviewGL::ClearCommandType::ClearBufferfi,
                                            static_cast<GLbitfield>(0u), buffer, drawbuffer,
                                            nullptr, depth, stencil);
    }

    contextGL->markWorkSubmitted();
    return angle::Result::Continue;
}

angle::Result FramebufferGL::readPixels(const gl::Context *context,
                                        const gl::Rectangle &area,
                                        GLenum format,
                                        GLenum type,
                                        const gl::PixelPackState &pack,
                                        gl::Buffer *packBuffer,
                                        void *pixels)
{
    ContextGL *contextGL              = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions      = GetFunctionsGL(context);
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    const angle::FeaturesGL &features = GetFeaturesGL(context);
    gl::PixelPackState packState      = pack;

    // Clip read area to framebuffer.
    const auto *readAttachment = mState.getReadPixelsAttachment(format);
    const gl::Extents fbSize   = readAttachment->getSize();
    const gl::Rectangle fbRect(0, 0, fbSize.width, fbSize.height);
    gl::Rectangle clippedArea;
    if (!ClipRectangle(area, fbRect, &clippedArea))
    {
        // nothing to read
        return angle::Result::Continue;
    }

    GLenum attachmentReadFormat =
        readAttachment->getFormat().info->getReadPixelsFormat(context->getExtensions());
    nativegl::ReadPixelsFormat readPixelsFormat =
        nativegl::GetReadPixelsFormat(functions, features, attachmentReadFormat, format, type);
    GLenum readFormat = readPixelsFormat.format;
    GLenum readType   = readPixelsFormat.type;
    if (features.readPixelsUsingImplementationColorReadFormatForNorm16.enabled &&
        readType == GL_UNSIGNED_SHORT)
    {
        ANGLE_CHECK(contextGL, IsValidUnsignedShortReadPixelsFormat(readFormat, context),
                    "glReadPixels: GL_IMPLEMENTATION_COLOR_READ_FORMAT advertised by the driver is "
                    "not handled by RGBA16 readPixels workaround.",
                    GL_INVALID_OPERATION);
    }

    GLenum framebufferTarget =
        stateManager->getHasSeparateFramebufferBindings() ? GL_READ_FRAMEBUFFER : GL_FRAMEBUFFER;
    stateManager->bindFramebuffer(framebufferTarget, mFramebufferID);

    bool useOverlappingRowsWorkaround = features.packOverlappingRowsSeparatelyPackBuffer.enabled &&
                                        packBuffer && packState.rowLength != 0 &&
                                        packState.rowLength < clippedArea.width;

    GLubyte *outPtr = static_cast<GLubyte *>(pixels);
    int leftClip    = clippedArea.x - area.x;
    int topClip     = clippedArea.y - area.y;
    if (leftClip || topClip)
    {
        // Adjust destination to match portion clipped off left and/or top.
        const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(readFormat, readType);

        GLuint rowBytes = 0;
        ANGLE_CHECK_GL_MATH(contextGL,
                            glFormat.computeRowPitch(readType, area.width, packState.alignment,
                                                     packState.rowLength, &rowBytes));
        outPtr += leftClip * glFormat.pixelBytes + topClip * rowBytes;
    }

    if (packState.rowLength == 0 && clippedArea.width != area.width)
    {
        // No rowLength was specified so it will derive from read width, but clipping changed the
        // read width.  Use the original width so we fill the user's buffer as they intended.
        packState.rowLength = area.width;
    }

    // We want to use rowLength, but that might not be supported.
    bool cannotSetDesiredRowLength =
        packState.rowLength && !GetImplAs<ContextGL>(context)->getNativeExtensions().packSubimageNV;

    bool usePackSkipWorkaround = features.emulatePackSkipRowsAndPackSkipPixels.enabled &&
                                 (packState.skipRows != 0 || packState.skipPixels != 0);

    if (cannotSetDesiredRowLength || useOverlappingRowsWorkaround || usePackSkipWorkaround)
    {
        return readPixelsRowByRow(context, clippedArea, format, readFormat, readType, packState,
                                  outPtr);
    }

    bool useLastRowPaddingWorkaround = false;
    if (features.packLastRowSeparatelyForPaddingInclusion.enabled)
    {
        ANGLE_TRY(ShouldApplyLastRowPaddingWorkaround(
            contextGL, gl::Extents(clippedArea.width, clippedArea.height, 1), packState, packBuffer,
            readFormat, readType, false, outPtr, &useLastRowPaddingWorkaround));
    }

    return readPixelsAllAtOnce(context, clippedArea, format, readFormat, readType, packState,
                               outPtr, useLastRowPaddingWorkaround);
}

angle::Result FramebufferGL::blit(const gl::Context *context,
                                  const gl::Rectangle &sourceArea,
                                  const gl::Rectangle &destArea,
                                  GLbitfield mask,
                                  GLenum filter)
{
    ContextGL *contextGL              = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions      = GetFunctionsGL(context);
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    const angle::FeaturesGL &features = GetFeaturesGL(context);

    const Framebuffer *sourceFramebuffer = context->getState().getReadFramebuffer();
    const Framebuffer *destFramebuffer   = context->getState().getDrawFramebuffer();

    const FramebufferAttachment *colorReadAttachment = sourceFramebuffer->getReadColorAttachment();

    GLsizei readAttachmentSamples = 0;
    if (colorReadAttachment != nullptr)
    {
        // Blitting requires that the textures be single sampled. getSamples will return
        // emulated sample number, but the EXT_multisampled_render_to_texture extension will
        // take care of resolving the texture, so even if emulated samples > 0, we should still
        // be able to blit as long as the underlying resource samples is single sampled.
        readAttachmentSamples = colorReadAttachment->getResourceSamples();
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
            needManualColorBlit || (sourceSRGB && functions->isAtMostGL(gl::Version(4, 3)));
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
            needManualColorBlit || (destSRGB && functions->isAtMostGL(gl::Version(4, 1)));
    }

    // If the destination has an emulated alpha channel, we need to blit with a shader with alpha
    // writes disabled.
    if (mHasEmulatedAlphaAttachment)
    {
        needManualColorBlit = true;
    }

    // Enable FRAMEBUFFER_SRGB if needed
    stateManager->setFramebufferSRGBEnabledForFramebuffer(context, true, this);

    GLenum blitMask = mask;
    if (needManualColorBlit && (mask & GL_COLOR_BUFFER_BIT) && readAttachmentSamples <= 1)
    {
        BlitGL *blitter = GetBlitGL(context);
        ANGLE_TRY(blitter->blitColorBufferWithShader(context, sourceFramebuffer, destFramebuffer,
                                                     sourceArea, destArea, filter,
                                                     !mHasEmulatedAlphaAttachment));
        blitMask &= ~GL_COLOR_BUFFER_BIT;
    }

    if (blitMask == 0)
    {
        return angle::Result::Continue;
    }

    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(sourceFramebuffer);
    stateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, sourceFramebufferGL->getFramebufferID());
    stateManager->bindFramebuffer(GL_DRAW_FRAMEBUFFER, mFramebufferID);

    gl::Rectangle finalSourceArea(sourceArea);
    gl::Rectangle finalDestArea(destArea);

    if (features.adjustSrcDstRegionForBlitFramebuffer.enabled)
    {
        angle::Result result = adjustSrcDstRegion(context, finalSourceArea, finalDestArea,
                                                  &finalSourceArea, &finalDestArea);
        if (result != angle::Result::Continue)
        {
            return result;
        }
    }
    if (features.clipSrcRegionForBlitFramebuffer.enabled)
    {
        angle::Result result = clipSrcRegion(context, finalSourceArea, finalDestArea,
                                             &finalSourceArea, &finalDestArea);
        if (result != angle::Result::Continue)
        {
            return result;
        }
    }

    functions->blitFramebuffer(finalSourceArea.x, finalSourceArea.y, finalSourceArea.x1(),
                               finalSourceArea.y1(), finalDestArea.x, finalDestArea.y,
                               finalDestArea.x1(), finalDestArea.y1(), blitMask, filter);

    contextGL->markWorkSubmitted();
    return angle::Result::Continue;
}

angle::Result FramebufferGL::adjustSrcDstRegion(const gl::Context *context,
                                                const gl::Rectangle &sourceArea,
                                                const gl::Rectangle &destArea,
                                                gl::Rectangle *newSourceArea,
                                                gl::Rectangle *newDestArea)
{
    BlitFramebufferBounds bounds = GetBlitFramebufferBounds(context, sourceArea, destArea);

    if (bounds.destRegion.width == 0 || bounds.sourceRegion.width == 0 ||
        bounds.destRegion.height == 0 || bounds.sourceRegion.height == 0)
    {
        return angle::Result::Stop;
    }
    if (!ClipRectangle(bounds.destBounds, bounds.destRegion, nullptr))
    {
        return angle::Result::Stop;
    }

    if (!bounds.destBounds.encloses(bounds.destRegion))
    {
        // destRegion is not within destBounds. We want to adjust it to a
        // reasonable size. This is done by halving the destRegion until it is at
        // most twice the size of the framebuffer. We cut it in half instead
        // of arbitrarily shrinking it to fit so that we don't end up with
        // non-power-of-two scale factors which could mess up pixel interpolation.
        // Naively clipping the dst rect and then proportionally sizing the
        // src rect yields incorrect results.

        GLuint destXHalvings = 0;
        GLuint destYHalvings = 0;
        GLint destOriginX    = bounds.destRegion.x;
        GLint destOriginY    = bounds.destRegion.y;

        GLint destClippedWidth = bounds.destRegion.width;
        while (destClippedWidth > 2 * bounds.destBounds.width)
        {
            destClippedWidth = destClippedWidth / 2;
            destXHalvings++;
        }

        GLint destClippedHeight = bounds.destRegion.height;
        while (destClippedHeight > 2 * bounds.destBounds.height)
        {
            destClippedHeight = destClippedHeight / 2;
            destYHalvings++;
        }

        // Before this block, we check that the two rectangles intersect.
        // Now, compute the location of a new region origin such that we use the
        // scaled dimensions but the new region has the same intersection as the
        // original region.

        GLint left   = bounds.destRegion.x0();
        GLint right  = bounds.destRegion.x1();
        GLint top    = bounds.destRegion.y0();
        GLint bottom = bounds.destRegion.y1();

        GLint extraXOffset = 0;
        if (left >= 0 && left < bounds.destBounds.width)
        {
            // Left edge is in-bounds
            destOriginX = bounds.destRegion.x;
        }
        else if (right > 0 && right <= bounds.destBounds.width)
        {
            // Right edge is in-bounds
            destOriginX = right - destClippedWidth;
        }
        else
        {
            // Region completely spans bounds
            extraXOffset = (bounds.destRegion.width - destClippedWidth) / 2;
            destOriginX  = bounds.destRegion.x + extraXOffset;
        }

        GLint extraYOffset = 0;
        if (top >= 0 && top < bounds.destBounds.height)
        {
            // Top edge is in-bounds
            destOriginY = bounds.destRegion.y;
        }
        else if (bottom > 0 && bottom <= bounds.destBounds.height)
        {
            // Bottom edge is in-bounds
            destOriginY = bottom - destClippedHeight;
        }
        else
        {
            // Region completely spans bounds
            extraYOffset = (bounds.destRegion.height - destClippedHeight) / 2;
            destOriginY  = bounds.destRegion.y + extraYOffset;
        }

        // Offsets from the bottom left corner of the original region to
        // the bottom left corner of the clipped region.
        // This value (after it is scaled) is the respective offset we will apply
        // to the src origin.

        CheckedNumeric<GLuint> checkedXOffset(destOriginX - bounds.destRegion.x - extraXOffset / 2);
        CheckedNumeric<GLuint> checkedYOffset(destOriginY - bounds.destRegion.y - extraYOffset / 2);

        // if X/Y is reversed, use the top/right out-of-bounds region to compute
        // the origin offset instead of the left/bottom out-of-bounds region
        if (bounds.xFlipped)
        {
            checkedXOffset =
                (bounds.destRegion.x1() - (destOriginX + destClippedWidth) + extraXOffset / 2);
        }
        if (bounds.yFlipped)
        {
            checkedYOffset =
                (bounds.destRegion.y1() - (destOriginY + destClippedHeight) + extraYOffset / 2);
        }

        // These offsets should never overflow
        GLuint xOffset, yOffset;
        if (!checkedXOffset.AssignIfValid(&xOffset) || !checkedYOffset.AssignIfValid(&yOffset))
        {
            UNREACHABLE();
            return angle::Result::Stop;
        }

        bounds.destRegion =
            gl::Rectangle(destOriginX, destOriginY, destClippedWidth, destClippedHeight);

        // Adjust the src region by the same factor
        bounds.sourceRegion = gl::Rectangle(bounds.sourceRegion.x + (xOffset >> destXHalvings),
                                            bounds.sourceRegion.y + (yOffset >> destYHalvings),
                                            bounds.sourceRegion.width >> destXHalvings,
                                            bounds.sourceRegion.height >> destYHalvings);

        // if the src was scaled to 0, set it to 1 so the src is non-empty
        if (bounds.sourceRegion.width == 0)
        {
            bounds.sourceRegion.width = 1;
        }
        if (bounds.sourceRegion.height == 0)
        {
            bounds.sourceRegion.height = 1;
        }
    }

    if (!bounds.sourceBounds.encloses(bounds.sourceRegion))
    {
        // sourceRegion is not within sourceBounds. We want to adjust it to a
        // reasonable size. This is done by halving the sourceRegion until it is at
        // most twice the size of the framebuffer. We cut it in half instead
        // of arbitrarily shrinking it to fit so that we don't end up with
        // non-power-of-two scale factors which could mess up pixel interpolation.
        // Naively clipping the source rect and then proportionally sizing the
        // dest rect yields incorrect results.

        GLuint sourceXHalvings = 0;
        GLuint sourceYHalvings = 0;
        GLint sourceOriginX    = bounds.sourceRegion.x;
        GLint sourceOriginY    = bounds.sourceRegion.y;

        GLint sourceClippedWidth = bounds.sourceRegion.width;
        while (sourceClippedWidth > 2 * bounds.sourceBounds.width)
        {
            sourceClippedWidth = sourceClippedWidth / 2;
            sourceXHalvings++;
        }

        GLint sourceClippedHeight = bounds.sourceRegion.height;
        while (sourceClippedHeight > 2 * bounds.sourceBounds.height)
        {
            sourceClippedHeight = sourceClippedHeight / 2;
            sourceYHalvings++;
        }

        // Before this block, we check that the two rectangles intersect.
        // Now, compute the location of a new region origin such that we use the
        // scaled dimensions but the new region has the same intersection as the
        // original region.

        GLint left   = bounds.sourceRegion.x0();
        GLint right  = bounds.sourceRegion.x1();
        GLint top    = bounds.sourceRegion.y0();
        GLint bottom = bounds.sourceRegion.y1();

        GLint extraXOffset = 0;
        if (left >= 0 && left < bounds.sourceBounds.width)
        {
            // Left edge is in-bounds
            sourceOriginX = bounds.sourceRegion.x;
        }
        else if (right > 0 && right <= bounds.sourceBounds.width)
        {
            // Right edge is in-bounds
            sourceOriginX = right - sourceClippedWidth;
        }
        else
        {
            // Region completely spans bounds
            extraXOffset  = (bounds.sourceRegion.width - sourceClippedWidth) / 2;
            sourceOriginX = bounds.sourceRegion.x + extraXOffset;
        }

        GLint extraYOffset = 0;
        if (top >= 0 && top < bounds.sourceBounds.height)
        {
            // Top edge is in-bounds
            sourceOriginY = bounds.sourceRegion.y;
        }
        else if (bottom > 0 && bottom <= bounds.sourceBounds.height)
        {
            // Bottom edge is in-bounds
            sourceOriginY = bottom - sourceClippedHeight;
        }
        else
        {
            // Region completely spans bounds
            extraYOffset  = (bounds.sourceRegion.height - sourceClippedHeight) / 2;
            sourceOriginY = bounds.sourceRegion.y + extraYOffset;
        }

        // Offsets from the bottom left corner of the original region to
        // the bottom left corner of the clipped region.
        // This value (after it is scaled) is the respective offset we will apply
        // to the dest origin.

        CheckedNumeric<GLuint> checkedXOffset(sourceOriginX - bounds.sourceRegion.x -
                                              extraXOffset / 2);
        CheckedNumeric<GLuint> checkedYOffset(sourceOriginY - bounds.sourceRegion.y -
                                              extraYOffset / 2);

        // if X/Y is reversed, use the top/right out-of-bounds region to compute
        // the origin offset instead of the left/bottom out-of-bounds region
        if (bounds.xFlipped)
        {
            checkedXOffset = (bounds.sourceRegion.x1() - (sourceOriginX + sourceClippedWidth) +
                              extraXOffset / 2);
        }
        if (bounds.yFlipped)
        {
            checkedYOffset = (bounds.sourceRegion.y1() - (sourceOriginY + sourceClippedHeight) +
                              extraYOffset / 2);
        }

        // These offsets should never overflow
        GLuint xOffset, yOffset;
        if (!checkedXOffset.AssignIfValid(&xOffset) || !checkedYOffset.AssignIfValid(&yOffset))
        {
            UNREACHABLE();
            return angle::Result::Stop;
        }

        bounds.sourceRegion =
            gl::Rectangle(sourceOriginX, sourceOriginY, sourceClippedWidth, sourceClippedHeight);

        // Adjust the dest region by the same factor
        bounds.destRegion = gl::Rectangle(bounds.destRegion.x + (xOffset >> sourceXHalvings),
                                          bounds.destRegion.y + (yOffset >> sourceYHalvings),
                                          bounds.destRegion.width >> sourceXHalvings,
                                          bounds.destRegion.height >> sourceYHalvings);
    }
    // Set the src and dst endpoints. If they were previously flipped,
    // set them as flipped.
    *newSourceArea = bounds.sourceRegion.flip(sourceArea.isReversedX(), sourceArea.isReversedY());
    *newDestArea   = bounds.destRegion.flip(destArea.isReversedX(), destArea.isReversedY());

    return angle::Result::Continue;
}

angle::Result FramebufferGL::clipSrcRegion(const gl::Context *context,
                                           const gl::Rectangle &sourceArea,
                                           const gl::Rectangle &destArea,
                                           gl::Rectangle *newSourceArea,
                                           gl::Rectangle *newDestArea)
{
    BlitFramebufferBounds bounds = GetBlitFramebufferBounds(context, sourceArea, destArea);

    if (bounds.destRegion.width == 0 || bounds.sourceRegion.width == 0 ||
        bounds.destRegion.height == 0 || bounds.sourceRegion.height == 0)
    {
        return angle::Result::Stop;
    }
    if (!ClipRectangle(bounds.destBounds, bounds.destRegion, nullptr))
    {
        return angle::Result::Stop;
    }

    if (!bounds.sourceBounds.encloses(bounds.sourceRegion))
    {
        // If pixels lying outside the read framebuffer, adjust src region
        // and dst region to appropriate in-bounds regions respectively.
        gl::Rectangle realSourceRegion;
        if (!ClipRectangle(bounds.sourceRegion, bounds.sourceBounds, &realSourceRegion))
        {
            return angle::Result::Stop;
        }
        GLuint xOffset = realSourceRegion.x - bounds.sourceRegion.x;
        GLuint yOffset = realSourceRegion.y - bounds.sourceRegion.y;

        // if X/Y is reversed, use the top/right out-of-bounds region for mapping
        // to dst region, instead of left/bottom out-of-bounds region for mapping.
        if (bounds.xFlipped)
        {
            xOffset = bounds.sourceRegion.x1() - realSourceRegion.x1();
        }
        if (bounds.yFlipped)
        {
            yOffset = bounds.sourceRegion.y1() - realSourceRegion.y1();
        }

        GLfloat destMappingWidth = static_cast<GLfloat>(realSourceRegion.width) *
                                   bounds.destRegion.width / bounds.sourceRegion.width;
        GLfloat destMappingHeight = static_cast<GLfloat>(realSourceRegion.height) *
                                    bounds.destRegion.height / bounds.sourceRegion.height;
        GLfloat destMappingXOffset =
            static_cast<GLfloat>(xOffset) * bounds.destRegion.width / bounds.sourceRegion.width;
        GLfloat destMappingYOffset =
            static_cast<GLfloat>(yOffset) * bounds.destRegion.height / bounds.sourceRegion.height;

        GLuint destMappingX0 =
            static_cast<GLuint>(std::round(bounds.destRegion.x + destMappingXOffset));
        GLuint destMappingY0 =
            static_cast<GLuint>(std::round(bounds.destRegion.y + destMappingYOffset));

        GLuint destMappingX1 = static_cast<GLuint>(
            std::round(bounds.destRegion.x + destMappingXOffset + destMappingWidth));
        GLuint destMappingY1 = static_cast<GLuint>(
            std::round(bounds.destRegion.y + destMappingYOffset + destMappingHeight));

        bounds.destRegion =
            gl::Rectangle(destMappingX0, destMappingY0, destMappingX1 - destMappingX0,
                          destMappingY1 - destMappingY0);

        bounds.sourceRegion = realSourceRegion;
    }
    // Set the src and dst endpoints. If they were previously flipped,
    // set them as flipped.
    *newSourceArea = bounds.sourceRegion.flip(sourceArea.isReversedX(), sourceArea.isReversedY());
    *newDestArea   = bounds.destRegion.flip(destArea.isReversedX(), destArea.isReversedY());

    return angle::Result::Continue;
}

angle::Result FramebufferGL::getSamplePosition(const gl::Context *context,
                                               size_t index,
                                               GLfloat *xy) const
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    stateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    functions->getMultisamplefv(GL_SAMPLE_POSITION, static_cast<GLuint>(index), xy);
    return angle::Result::Continue;
}

bool FramebufferGL::shouldSyncStateBeforeCheckStatus() const
{
    return true;
}

gl::FramebufferStatus FramebufferGL::checkStatus(const gl::Context *context) const
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    stateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    GLenum status = functions->checkFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        WARN() << "GL framebuffer returned incomplete: " << gl::FmtHex(status);
        return gl::FramebufferStatus::Incomplete(GL_FRAMEBUFFER_UNSUPPORTED,
                                                 gl::err::kFramebufferIncompleteDriverUnsupported);
    }

    return gl::FramebufferStatus::Complete();
}

angle::Result FramebufferGL::syncState(const gl::Context *context,
                                       GLenum binding,
                                       const gl::Framebuffer::DirtyBits &dirtyBits,
                                       gl::Command command)
{
    // Don't need to sync state for the default FBO.
    if (isDefault())
    {
        return angle::Result::Continue;
    }

    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    stateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);

    // A pointer to one of the attachments for which the texture or the render buffer is not zero.
    const FramebufferAttachment *attachment = nullptr;

    for (auto dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
            {
                const FramebufferAttachment *newAttachment = mState.getDepthAttachment();
                BindFramebufferAttachment(functions, GL_DEPTH_ATTACHMENT, newAttachment,
                                          GetFeaturesGL(context));
                if (newAttachment)
                {
                    attachment = newAttachment;
                }
                break;
            }
            case Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
            {
                const FramebufferAttachment *newAttachment = mState.getStencilAttachment();
                BindFramebufferAttachment(functions, GL_STENCIL_ATTACHMENT, newAttachment,
                                          GetFeaturesGL(context));
                if (newAttachment)
                {
                    attachment = newAttachment;
                }
                break;
            }
            case Framebuffer::DIRTY_BIT_DRAW_BUFFERS:
            {
                const auto &drawBuffers = mState.getDrawBufferStates();
                functions->drawBuffers(static_cast<GLsizei>(drawBuffers.size()),
                                       drawBuffers.data());
                mAppliedEnabledDrawBuffers = mState.getEnabledDrawBuffers();
                break;
            }
            case Framebuffer::DIRTY_BIT_READ_BUFFER:
                functions->readBuffer(mState.getReadBufferState());
                break;
            case Framebuffer::DIRTY_BIT_DEFAULT_WIDTH:
                functions->framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH,
                                                 mState.getDefaultWidth());
                break;
            case Framebuffer::DIRTY_BIT_DEFAULT_HEIGHT:
                functions->framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT,
                                                 mState.getDefaultHeight());
                break;
            case Framebuffer::DIRTY_BIT_DEFAULT_SAMPLES:
                functions->framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_SAMPLES,
                                                 mState.getDefaultSamples());
                break;
            case Framebuffer::DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS:
                functions->framebufferParameteri(
                    GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS,
                    gl::ConvertToGLBoolean(mState.getDefaultFixedSampleLocations()));
                break;
            case Framebuffer::DIRTY_BIT_DEFAULT_LAYERS:
                functions->framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_LAYERS_EXT,
                                                 mState.getDefaultLayers());
                break;
            case Framebuffer::DIRTY_BIT_FLIP_Y:
                functions->framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_FLIP_Y_MESA,
                                                 gl::ConvertToGLBoolean(mState.getFlipY()));
                break;
            default:
            {
                static_assert(Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 == 0, "FB dirty bits");
                if (dirtyBit < Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX)
                {
                    size_t index =
                        static_cast<size_t>(dirtyBit - Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0);
                    const FramebufferAttachment *newAttachment = mState.getColorAttachment(index);
                    BindFramebufferAttachment(functions,
                                              static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index),
                                              newAttachment, GetFeaturesGL(context));
                    if (newAttachment)
                    {
                        attachment = newAttachment;
                    }

                    // Hiding an alpha channel is only supported when it's the first attachment
                    // currently. Assert that these emulated textures are not bound to a framebuffer
                    // using MRT.
                    if (index == 0)
                    {
                        mHasEmulatedAlphaAttachment =
                            IsEmulatedAlphaChannelTextureAttachment(attachment);
                    }
                    ASSERT(index == 0 || !IsEmulatedAlphaChannelTextureAttachment(attachment));
                }
                break;
            }
        }
    }

    if (attachment && mState.id() == context->getState().getDrawFramebuffer()->id())
    {
        stateManager->updateMultiviewBaseViewLayerIndexUniform(context->getState().getProgram(),
                                                               getState());
    }

    return angle::Result::Continue;
}

void FramebufferGL::updateDefaultFramebufferID(GLuint framebufferID)
{
    // We only update framebufferID for a default frambuffer, and the framebufferID is created
    // externally. ANGLE doesn't owne it.
    ASSERT(isDefault());
    mFramebufferID = framebufferID;
}

bool FramebufferGL::hasEmulatedAlphaChannelTextureAttachment() const
{
    return mHasEmulatedAlphaAttachment;
}

void FramebufferGL::syncClearState(const gl::Context *context, GLbitfield mask)
{
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    const angle::FeaturesGL &features = GetFeaturesGL(context);

    if (features.doesSRGBClearsOnLinearFramebufferAttachments.enabled &&
        (mask & GL_COLOR_BUFFER_BIT) != 0 && !isDefault())
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

        stateManager->setFramebufferSRGBEnabled(context, hasSRGBAttachment);
    }
    else
    {
        stateManager->setFramebufferSRGBEnabled(context, !isDefault());
    }
}

void FramebufferGL::syncClearBufferState(const gl::Context *context,
                                         GLenum buffer,
                                         GLint drawBuffer)
{
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    const angle::FeaturesGL &features = GetFeaturesGL(context);

    if (features.doesSRGBClearsOnLinearFramebufferAttachments.enabled && buffer == GL_COLOR &&
        !isDefault())
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
            stateManager->setFramebufferSRGBEnabled(context,
                                                    attachment->getColorEncoding() == GL_SRGB);
        }
    }
    else
    {
        stateManager->setFramebufferSRGBEnabled(context, !isDefault());
    }
}

bool FramebufferGL::modifyInvalidateAttachmentsForEmulatedDefaultFBO(
    size_t count,
    const GLenum *attachments,
    std::vector<GLenum> *modifiedAttachments) const
{
    bool needsModification = isDefault() && mFramebufferID != 0;
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

angle::Result FramebufferGL::readPixelsRowByRow(const gl::Context *context,
                                                const gl::Rectangle &area,
                                                GLenum originalReadFormat,
                                                GLenum format,
                                                GLenum type,
                                                const gl::PixelPackState &pack,
                                                GLubyte *pixels) const
{
    ContextGL *contextGL              = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions      = GetFunctionsGL(context);
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    GLubyte *originalReadFormatPixels = pixels;

    const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(format, type);

    GLuint rowBytes = 0;
    ANGLE_CHECK_GL_MATH(contextGL, glFormat.computeRowPitch(type, area.width, pack.alignment,
                                                            pack.rowLength, &rowBytes));
    GLuint skipBytes = 0;
    ANGLE_CHECK_GL_MATH(contextGL,
                        glFormat.computeSkipBytes(type, rowBytes, 0, pack, false, &skipBytes));

    ScopedEXTTextureNorm16ReadbackWorkaround workaround;
    angle::Result result =
        workaround.Initialize(context, area, originalReadFormat, format, type, skipBytes, rowBytes,
                              glFormat.computePixelBytes(type), pixels);
    if (result != angle::Result::Continue)
    {
        return result;
    }

    gl::PixelPackState directPack;
    directPack.alignment = 1;
    ANGLE_TRY(stateManager->setPixelPackState(context, directPack));

    GLubyte *readbackPixels = workaround.Pixels();
    readbackPixels += skipBytes;
    for (GLint y = area.y; y < area.y + area.height; ++y)
    {
        ANGLE_GL_TRY(context,
                     functions->readPixels(area.x, y, area.width, 1, format, type, readbackPixels));
        readbackPixels += rowBytes;
    }

    if (workaround.IsEnabled())
    {
        return RearrangeEXTTextureNorm16Pixels(
            context, area, originalReadFormat, format, type, skipBytes, rowBytes,
            glFormat.computePixelBytes(type), pack, originalReadFormatPixels, workaround.Pixels());
    }

    return angle::Result::Continue;
}

angle::Result FramebufferGL::readPixelsAllAtOnce(const gl::Context *context,
                                                 const gl::Rectangle &area,
                                                 GLenum originalReadFormat,
                                                 GLenum format,
                                                 GLenum type,
                                                 const gl::PixelPackState &pack,
                                                 GLubyte *pixels,
                                                 bool readLastRowSeparately) const
{
    ContextGL *contextGL              = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions      = GetFunctionsGL(context);
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    GLubyte *originalReadFormatPixels = pixels;

    const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(format, type);

    GLuint rowBytes = 0;
    ANGLE_CHECK_GL_MATH(contextGL, glFormat.computeRowPitch(type, area.width, pack.alignment,
                                                            pack.rowLength, &rowBytes));
    GLuint skipBytes = 0;
    ANGLE_CHECK_GL_MATH(contextGL,
                        glFormat.computeSkipBytes(type, rowBytes, 0, pack, false, &skipBytes));

    ScopedEXTTextureNorm16ReadbackWorkaround workaround;
    angle::Result result =
        workaround.Initialize(context, area, originalReadFormat, format, type, skipBytes, rowBytes,
                              glFormat.computePixelBytes(type), pixels);
    if (result != angle::Result::Continue)
    {
        return result;
    }

    GLint height = area.height - readLastRowSeparately;
    if (height > 0)
    {
        ANGLE_TRY(stateManager->setPixelPackState(context, pack));
        ANGLE_GL_TRY(context, functions->readPixels(area.x, area.y, area.width, height, format,
                                                    type, workaround.Pixels()));
    }

    if (readLastRowSeparately)
    {
        gl::PixelPackState directPack;
        directPack.alignment = 1;
        ANGLE_TRY(stateManager->setPixelPackState(context, directPack));

        GLubyte *readbackPixels = workaround.Pixels();
        readbackPixels += skipBytes + (area.height - 1) * rowBytes;
        ANGLE_GL_TRY(context, functions->readPixels(area.x, area.y + area.height - 1, area.width, 1,
                                                    format, type, readbackPixels));
    }

    if (workaround.IsEnabled())
    {
        return RearrangeEXTTextureNorm16Pixels(
            context, area, originalReadFormat, format, type, skipBytes, rowBytes,
            glFormat.computePixelBytes(type), pack, originalReadFormatPixels, workaround.Pixels());
    }

    return angle::Result::Continue;
}
}  // namespace rx
