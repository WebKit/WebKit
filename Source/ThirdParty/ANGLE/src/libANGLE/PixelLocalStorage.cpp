//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// PixelLocalStorage.cpp: Defines the renderer-agnostic container classes
// gl::PixelLocalStorage and gl::PixelLocalStoragePlane for
// ANGLE_shader_pixel_local_storage.

#include "libANGLE/PixelLocalStorage.h"

#include <numeric>
#include "libANGLE/Context.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/Texture.h"
#include "libANGLE/renderer/ContextImpl.h"

namespace gl
{
// RAII utilities for working with GL state.
namespace
{
class ScopedBindTexture2D
{
  public:
    ScopedBindTexture2D(Context *context, TextureID texture)
        : mContext(context),
          mSavedTexBinding2D(
              mContext->getState().getSamplerTextureId(mContext->getState().getActiveSampler(),
                                                       TextureType::_2D))
    {
        mContext->bindTexture(TextureType::_2D, texture);
    }

    ~ScopedBindTexture2D() { mContext->bindTexture(TextureType::_2D, mSavedTexBinding2D); }

  private:
    Context *const mContext;
    TextureID mSavedTexBinding2D;
};

class ScopedRestoreDrawFramebuffer
{
  public:
    ScopedRestoreDrawFramebuffer(Context *context)
        : mContext(context), mSavedFramebuffer(mContext->getState().getDrawFramebuffer())
    {
        ASSERT(mSavedFramebuffer);
    }

    ~ScopedRestoreDrawFramebuffer() { mContext->bindDrawFramebuffer(mSavedFramebuffer->id()); }

  private:
    Context *const mContext;
    Framebuffer *const mSavedFramebuffer;
};

class ScopedDisableScissor
{
  public:
    ScopedDisableScissor(Context *context)
        : mContext(context), mScissorTestEnabled(mContext->getState().isScissorTestEnabled())
    {
        if (mScissorTestEnabled)
        {
            mContext->disable(GL_SCISSOR_TEST);
        }
    }

    ~ScopedDisableScissor()
    {
        if (mScissorTestEnabled)
        {
            mContext->enable(GL_SCISSOR_TEST);
        }
    }

  private:
    Context *const mContext;
    const GLint mScissorTestEnabled;
};
}  // namespace

PixelLocalStoragePlane::~PixelLocalStoragePlane()
{
    // Call deinitialize or onContextObjectsLost first!
    ASSERT(mMemorylessTextureID.value == 0);
    // Call deinitialize or onFramebufferDestroyed first!
    ASSERT(mTextureRef == nullptr);
}

void PixelLocalStoragePlane::onContextObjectsLost()
{
    // We normally call deleteTexture on the memoryless plane texture ID, since we own it, but in
    // this case we can let it go.
    mMemorylessTextureID = TextureID();
}

void PixelLocalStoragePlane::onFramebufferDestroyed(const Context *context)
{
    if (mTextureRef != nullptr)
    {
        mTextureRef->release(context);
        mTextureRef = nullptr;
    }
}

void PixelLocalStoragePlane::deinitialize(Context *context)
{
    mInternalformat = GL_NONE;
    mMemoryless     = false;
    if (mMemorylessTextureID.value != 0)
    {
        // The app could have technically deleted mMemorylessTextureID by guessing its value and
        // calling glDeleteTextures, but it seems unnecessary to worry about that here. (Worst case
        // we delete one of their textures.) This also isn't a problem in WebGL.
        context->deleteTexture(mMemorylessTextureID);
        mMemorylessTextureID = TextureID();
    }
    if (mTextureRef != nullptr)
    {
        mTextureRef->release(context);
        mTextureRef = nullptr;
    }
}

void PixelLocalStoragePlane::setMemoryless(Context *context, GLenum internalformat)
{
    deinitialize(context);
    mInternalformat    = internalformat;
    mMemoryless        = true;
    mTextureImageIndex = ImageIndex::MakeFromType(TextureType::_2D, 0, 0);
    // The backing texture will get allocated lazily, once we know what dimensions it should be.
    ASSERT(mMemorylessTextureID.value == 0);
    ASSERT(mTextureRef == nullptr);
}

void PixelLocalStoragePlane::setTextureBacked(Context *context, Texture *tex, int level, int layer)
{
    deinitialize(context);
    ASSERT(tex->getImmutableFormat());
    mInternalformat    = tex->getState().getBaseLevelDesc().format.info->internalFormat;
    mMemoryless        = false;
    mTextureImageIndex = ImageIndex::MakeFromType(tex->getType(), level, layer);
    mTextureRef        = tex;
    mTextureRef->addRef();
}

bool PixelLocalStoragePlane::isTextureIDDeleted(const Context *context) const
{
    // We can tell if the texture has been deleted by looking up mTextureRef's ID on the Context. If
    // they don't match, it's been deleted.
    ASSERT(!isDeinitialized() || mTextureRef == nullptr);
    return mTextureRef != nullptr && context->getTexture(mTextureRef->id()) != mTextureRef;
}

GLint PixelLocalStoragePlane::getIntegeri(const Context *context, GLenum target, GLuint index) const
{
    if (!isDeinitialized())
    {
        bool memoryless = isMemoryless() || isTextureIDDeleted(context);
        switch (target)
        {
            case GL_PIXEL_LOCAL_FORMAT_ANGLE:
                return mInternalformat;
            case GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE:
                return memoryless ? 0 : mTextureRef->id().value;
            case GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE:
                return memoryless ? 0 : mTextureImageIndex.getLevelIndex();
            case GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE:
                return memoryless ? 0 : mTextureImageIndex.getLayerIndex();
        }
    }
    // Since GL_NONE == 0, PLS queries all return 0 when the plane is deinitialized.
    static_assert(GL_NONE == 0, "Expecting GL_NONE to be zero.");
    return 0;
}

bool PixelLocalStoragePlane::getTextureImageExtents(const Context *context, Extents *extents) const
{
    if (isDeinitialized() || isMemoryless() || isTextureIDDeleted(context))
    {
        return false;
    }
    ASSERT(mTextureRef != nullptr);
    *extents =
        mTextureRef->getExtents(mTextureImageIndex.getTarget(), mTextureImageIndex.getLevelIndex());
    extents->depth = 0;
    return true;
}

void PixelLocalStoragePlane::ensureBackingIfMemoryless(Context *context, Extents plsExtents)
{
    ASSERT(!isDeinitialized());
    ASSERT(!isTextureIDDeleted(context));  // Convert to memoryless first in this case.
    if (!isMemoryless())
    {
        ASSERT(mTextureRef != nullptr);
        return;
    }

    // Internal textures backing memoryless planes are always 2D and not mipmapped.
    ASSERT(mTextureImageIndex.getType() == TextureType::_2D);
    ASSERT(mTextureImageIndex.getLevelIndex() == 0);
    ASSERT(mTextureImageIndex.getLayerIndex() == 0);
    const bool hasMemorylessTextureId = mMemorylessTextureID.value != 0;
    const bool hasTextureRef          = mTextureRef != nullptr;
    ASSERT(hasMemorylessTextureId == hasTextureRef);

    // Do we need to allocate a new backing texture?
    if (mTextureRef == nullptr ||
        static_cast<GLsizei>(mTextureRef->getWidth(TextureTarget::_2D, 0)) != plsExtents.width ||
        static_cast<GLsizei>(mTextureRef->getHeight(TextureTarget::_2D, 0)) != plsExtents.height)
    {
        // Call setMemoryless() to release our current data.
        setMemoryless(context, mInternalformat);
        ASSERT(mTextureRef == nullptr);
        ASSERT(mMemorylessTextureID.value == 0);

        // Create a new texture that backs the memoryless plane.
        context->genTextures(1, &mMemorylessTextureID);
        {
            ScopedBindTexture2D scopedBindTexture2D(context, mMemorylessTextureID);
            context->bindTexture(TextureType::_2D, mMemorylessTextureID);
            context->texStorage2D(TextureType::_2D, 1, mInternalformat, plsExtents.width,
                                  plsExtents.height);
        }

        mTextureRef = context->getTexture(mMemorylessTextureID);
        ASSERT(mTextureRef != nullptr);
        ASSERT(mTextureRef->id() == mMemorylessTextureID);
        mTextureRef->addRef();
    }
}

void PixelLocalStoragePlane::attachToDrawFramebuffer(Context *context,
                                                     Extents plsExtents,
                                                     GLenum colorAttachment)
{
    ASSERT(!isDeinitialized());
    ensureBackingIfMemoryless(context, plsExtents);
    ASSERT(mTextureRef != nullptr);
    if (mTextureImageIndex.usesTex3D())  // GL_TEXTURE_3D or GL_TEXTURE_2D_ARRAY.
    {
        context->framebufferTextureLayer(GL_DRAW_FRAMEBUFFER, colorAttachment, mTextureRef->id(),
                                         mTextureImageIndex.getLevelIndex(),
                                         mTextureImageIndex.getLayerIndex());
    }
    else
    {
        context->framebufferTexture2D(GL_DRAW_FRAMEBUFFER, colorAttachment,
                                      mTextureImageIndex.getTarget(), mTextureRef->id(),
                                      mTextureImageIndex.getLevelIndex());
    }
}

void PixelLocalStoragePlane::performLoadOperationClear(Context *context,
                                                       GLint drawBuffer,
                                                       GLenum loadop,
                                                       const void *data)
{
    ASSERT(loadop == GL_ZERO || loadop == GL_CLEAR_ANGLE);
    // The GL scissor test must be disabled, since the intention is to clear the entire surface.
    ASSERT(!context->getState().isScissorTestEnabled());
    switch (mInternalformat)
    {
        case GL_RGBA8:
        case GL_R32F:
        {
            GLfloat clearValue[4]{};
            if (loadop == GL_CLEAR_ANGLE)
            {
                memcpy(clearValue, data, sizeof(clearValue));
            }
            context->clearBufferfv(GL_COLOR, drawBuffer, clearValue);
            break;
        }
        case GL_RGBA8I:
        {
            GLint clearValue[4]{};
            if (loadop == GL_CLEAR_ANGLE)
            {
                memcpy(clearValue, data, sizeof(clearValue));
            }
            context->clearBufferiv(GL_COLOR, drawBuffer, clearValue);
            break;
        }
        case GL_RGBA8UI:
        case GL_R32UI:
        {
            GLuint clearValue[4]{};
            if (loadop == GL_CLEAR_ANGLE)
            {
                memcpy(clearValue, data, sizeof(clearValue));
            }
            context->clearBufferuiv(GL_COLOR, drawBuffer, clearValue);
            break;
        }
        default:
            // Invalid PLS internalformats should not have made it this far.
            UNREACHABLE();
    }
}

void PixelLocalStoragePlane::bindToImage(Context *context,
                                         Extents plsExtents,
                                         GLuint unit,
                                         bool needsR32Packing)
{
    ASSERT(!isDeinitialized());
    ensureBackingIfMemoryless(context, plsExtents);
    ASSERT(mTextureRef != nullptr);
    GLenum imageBindingFormat = mInternalformat;
    if (needsR32Packing)
    {
        // D3D and ES require us to pack all PLS formats into r32f, r32i, or r32ui images.
        switch (imageBindingFormat)
        {
            case GL_RGBA8:
            case GL_RGBA8UI:
                imageBindingFormat = GL_R32UI;
                break;
            case GL_RGBA8I:
                imageBindingFormat = GL_R32I;
                break;
        }
    }
    if (mTextureRef->getType() != TextureType::_2D)
    {
        // TODO(anglebug.com/7279): Texture types other than GL_TEXTURE_2D will take a lot of
        // consideration to support on all backends. Hold of on fully implementing them until the
        // other backends are in place.
        UNIMPLEMENTED();
    }
    context->bindImageTexture(unit, mTextureRef->id(), mTextureImageIndex.getLevelIndex(), GL_FALSE,
                              mTextureImageIndex.getLayerIndex(), GL_READ_WRITE,
                              imageBindingFormat);
}

PixelLocalStorage::PixelLocalStorage() {}
PixelLocalStorage::~PixelLocalStorage() {}

void PixelLocalStorage::onFramebufferDestroyed(const Context *context)
{
    if (context->getRefCount() == 0)
    {
        // If the Context's refcount is zero, we know it's in a teardown state and we can just let
        // go of our GL objects -- they get cleaned up as part of context teardown. Otherwise, the
        // Context should have called deleteContextObjects before reaching this point.
        onContextObjectsLost();
        for (PixelLocalStoragePlane &plane : mPlanes)
        {
            plane.onContextObjectsLost();
        }
    }
    for (PixelLocalStoragePlane &plane : mPlanes)
    {
        plane.onFramebufferDestroyed(context);
    }
}

void PixelLocalStorage::deleteContextObjects(Context *context)
{
    onDeleteContextObjects(context);
    for (PixelLocalStoragePlane &plane : mPlanes)
    {
        plane.deinitialize(context);
    }
}

void PixelLocalStorage::begin(Context *context,
                              GLsizei n,
                              const GLenum loadops[],
                              const void *cleardata)
{
    // Convert planes whose backing texture has been deleted to memoryless, and find the pixel local
    // storage rendering dimensions.
    Extents plsExtents;
    bool hasPLSExtents = false;
    for (int i = 0; i < n; ++i)
    {
        if (loadops[i] == GL_DISABLE_ANGLE)
        {
            continue;
        }
        PixelLocalStoragePlane &plane = mPlanes[i];
        if (plane.isTextureIDDeleted(context))
        {
            // [ANGLE_shader_pixel_local_storage] Section 4.4.2.X "Configuring Pixel Local Storage
            // on a Framebuffer": When a texture object is deleted, any pixel local storage plane to
            // which it was bound is automatically converted to a memoryless plane of matching
            // internalformat.
            plane.setMemoryless(context, plane.getInternalformat());
        }
        if (!hasPLSExtents && plane.getTextureImageExtents(context, &plsExtents))
        {
            hasPLSExtents = true;
        }
    }
    if (!hasPLSExtents)
    {
        // All PLS planes are memoryless. Use the rendering area of the framebuffer instead.
        plsExtents =
            context->getState().getDrawFramebuffer()->getState().getAttachmentExtentsIntersection();
        ASSERT(plsExtents.depth == 0);
    }

    onBegin(context, n, loadops, reinterpret_cast<const char *>(cleardata), plsExtents);
    mNumActivePLSPlanes = n;
}

void PixelLocalStorage::end(Context *context)
{
    onEnd(context, mNumActivePLSPlanes);
    mNumActivePLSPlanes = 0;
}

void PixelLocalStorage::barrier(Context *context)
{
    ASSERT(!context->getExtensions().shaderPixelLocalStorageCoherentANGLE);
    onBarrier(context);
}

namespace
{
// Implements pixel local storage with image load/store shader operations.
class PixelLocalStorageImageLoadStore : public PixelLocalStorage
{
  public:
    PixelLocalStorageImageLoadStore(bool needsR32Packing) : mNeedsR32Packing(needsR32Packing) {}

    // Call deleteContextObjects or onContextObjectsLost first!
    ~PixelLocalStorageImageLoadStore() override
    {
        ASSERT(mScratchFramebufferForClearing.value == 0);
    }

    void onContextObjectsLost() override
    {
        mScratchFramebufferForClearing = FramebufferID();  // Let go of GL objects.
    }

    void onDeleteContextObjects(Context *context) override
    {
        if (mScratchFramebufferForClearing.value != 0)
        {
            context->deleteFramebuffer(mScratchFramebufferForClearing);
            mScratchFramebufferForClearing = FramebufferID();
        }
    }

    void onBegin(Context *context,
                 GLsizei n,
                 const GLenum loadops[],
                 const char *cleardata,
                 Extents plsExtents) override
    {
        // Save the image bindings so we can restore them during onEnd().
        const State &state = context->getState();
        ASSERT(static_cast<size_t>(n) <= state.getImageUnits().size());
        mSavedImageBindings.clear();
        mSavedImageBindings.reserve(n);
        for (int i = 0; i < n; ++i)
        {
            mSavedImageBindings.emplace_back(state.getImageUnit(i));
        }

        // Save the default framebuffer width/height so we can restore it during onEnd().
        Framebuffer *framebuffer       = state.getDrawFramebuffer();
        mSavedFramebufferDefaultWidth  = framebuffer->getDefaultWidth();
        mSavedFramebufferDefaultHeight = framebuffer->getDefaultHeight();

        // Specify the framebuffer width/height explicitly in case we end up rendering exclusively
        // to shader images.
        context->framebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH,
                                       plsExtents.width);
        context->framebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT,
                                       plsExtents.height);

        // Guard GL state and bind a scratch framebuffer in case we need to reallocate or clear any
        // PLS planes.
        const size_t maxDrawBuffers = context->getCaps().maxDrawBuffers;
        ScopedRestoreDrawFramebuffer ScopedRestoreDrawFramebuffer(context);
        if (mScratchFramebufferForClearing.value == 0)
        {
            context->genFramebuffers(1, &mScratchFramebufferForClearing);
            context->bindFramebuffer(GL_DRAW_FRAMEBUFFER, mScratchFramebufferForClearing);
            // Turn on all draw buffers on the scratch framebuffer for clearing.
            DrawBuffersVector<GLenum> drawBuffers(maxDrawBuffers);
            std::iota(drawBuffers.begin(), drawBuffers.end(), GL_COLOR_ATTACHMENT0);
            context->drawBuffers(static_cast<int>(drawBuffers.size()), drawBuffers.data());
        }
        else
        {
            context->bindFramebuffer(GL_DRAW_FRAMEBUFFER, mScratchFramebufferForClearing);
        }
        ScopedDisableScissor scopedDisableScissor(context);

        // Bind and clear the PLS planes.
        size_t maxClearedAttachments = 0;
        for (int i = 0; i < n;)
        {
            angle::FixedVector<int, IMPLEMENTATION_MAX_DRAW_BUFFERS> pendingClears;
            for (; pendingClears.size() < maxDrawBuffers && i < n; ++i)
            {
                GLenum loadop = loadops[i];
                if (loadop == GL_DISABLE_ANGLE)
                {
                    continue;
                }
                PixelLocalStoragePlane &plane = getPlane(i);
                ASSERT(!plane.isDeinitialized());
                plane.bindToImage(context, plsExtents, i, mNeedsR32Packing);
                if (loadop == GL_ZERO || loadop == GL_CLEAR_ANGLE)
                {
                    plane.attachToDrawFramebuffer(
                        context, plsExtents,
                        GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(pendingClears.size()));
                    pendingClears.push_back(i);  // Defer the clear for later.
                }
            }
            // Clear in batches to be more efficient with GL state.
            for (size_t drawBufferIdx = 0; drawBufferIdx < pendingClears.size(); ++drawBufferIdx)
            {
                int plsIdx = pendingClears[drawBufferIdx];
                getPlane(plsIdx).performLoadOperationClear(
                    context, static_cast<GLint>(drawBufferIdx), loadops[plsIdx],
                    cleardata + plsIdx * 4 * 4);
            }
            maxClearedAttachments = std::max(maxClearedAttachments, pendingClears.size());
        }

        // Detach the cleared PLS textures from the scratch framebuffer.
        for (size_t i = 0; i < maxClearedAttachments; ++i)
        {
            context->framebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                          GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i),
                                          TextureTarget::_2D, TextureID(), 0);
        }

        // Unlike other barriers, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT also synchronizes all types of
        // memory accesses that happened before the barrier:
        //
        //   SHADER_IMAGE_ACCESS_BARRIER_BIT: Memory accesses using shader built-in image load and
        //   store functions issued after the barrier will reflect data written by shaders prior to
        //   the barrier. Additionally, image stores issued after the barrier will not execute until
        //   all memory accesses (e.g., loads, stores, texture fetches, vertex fetches) initiated
        //   prior to the barrier complete.
        //
        // So we don't any barriers other than GL_SHADER_IMAGE_ACCESS_BARRIER_BIT during begin().
        context->memoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void onEnd(Context *context, GLsizei numActivePLSPlanes) override
    {
        // Restore the image bindings. Since glBindImageTexture and any commands that modify
        // textures are banned while PLS is active, these will all still be alive and valid.
        ASSERT(mSavedImageBindings.size() == static_cast<size_t>(numActivePLSPlanes));
        for (GLuint unit = 0; unit < mSavedImageBindings.size(); ++unit)
        {
            ImageUnit &binding = mSavedImageBindings[unit];
            context->bindImageTexture(unit, binding.texture.id(), binding.level, binding.layered,
                                      binding.layer, binding.access, binding.format);

            // BindingPointers have to be explicitly cleaned up.
            binding.texture.set(context, nullptr);
        }
        mSavedImageBindings.clear();

        // Restore the default framebuffer width/height.
        context->framebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH,
                                       mSavedFramebufferDefaultWidth);
        context->framebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT,
                                       mSavedFramebufferDefaultHeight);

        // We need ALL_BARRIER_BITS during end() because GL_SHADER_IMAGE_ACCESS_BARRIER_BIT doesn't
        // synchronize all types of memory accesses that can happen after the barrier.
        context->memoryBarrier(GL_ALL_BARRIER_BITS);
    }

    void onBarrier(Context *context) override
    {
        context->memoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

  private:
    // D3D and ES require us to pack all PLS formats into r32f, r32i, or r32ui images.
    const bool mNeedsR32Packing;
    FramebufferID mScratchFramebufferForClearing{};

    // Saved values to restore during onEnd().
    GLint mSavedFramebufferDefaultWidth;
    GLint mSavedFramebufferDefaultHeight;
    std::vector<ImageUnit> mSavedImageBindings;
};
}  // namespace

std::unique_ptr<PixelLocalStorage> PixelLocalStorage::Make(const Context *context)
{
    bool needsR32Packing = context->getImplementation()->getNativePixelLocalStorageType() ==
                           ShPixelLocalStorageType::ImageStoreR32PackedFormats;
    return std::make_unique<PixelLocalStorageImageLoadStore>(needsR32Packing);
}
}  // namespace gl
