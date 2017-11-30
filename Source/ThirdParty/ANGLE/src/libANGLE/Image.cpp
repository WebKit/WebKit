//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image.cpp: Implements the egl::Image class representing the EGLimage object.

#include "libANGLE/Image.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/Texture.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/renderer/EGLImplFactory.h"
#include "libANGLE/renderer/ImageImpl.h"

namespace egl
{

namespace
{
gl::ImageIndex GetImageIndex(EGLenum eglTarget, const egl::AttributeMap &attribs)
{
    if (eglTarget == EGL_GL_RENDERBUFFER)
    {
        return gl::ImageIndex::MakeInvalid();
    }

    GLenum target = egl_gl::EGLImageTargetToGLTextureTarget(eglTarget);
    GLint mip     = static_cast<GLint>(attribs.get(EGL_GL_TEXTURE_LEVEL_KHR, 0));
    GLint layer   = static_cast<GLint>(attribs.get(EGL_GL_TEXTURE_ZOFFSET_KHR, 0));

    if (target == GL_TEXTURE_3D)
    {
        return gl::ImageIndex::Make3D(mip, layer);
    }
    else
    {
        ASSERT(layer == 0);
        return gl::ImageIndex::MakeGeneric(target, mip);
    }
}
}  // anonymous namespace

ImageSibling::ImageSibling(GLuint id)
    : RefCountObject(id), FramebufferAttachmentObject(), mSourcesOf(), mTargetOf()
{
}

ImageSibling::~ImageSibling()
{
    // EGL images should hold a ref to their targets and siblings, a Texture should not be deletable
    // while it is attached to an EGL image.
    // Child class should orphan images before destruction.
    ASSERT(mSourcesOf.empty());
    ASSERT(mTargetOf.get() == nullptr);
}

void ImageSibling::setTargetImage(const gl::Context *context, egl::Image *imageTarget)
{
    ASSERT(imageTarget != nullptr);
    mTargetOf.set(context, imageTarget);
    imageTarget->addTargetSibling(this);
}

gl::Error ImageSibling::orphanImages(const gl::Context *context)
{
    if (mTargetOf.get() != nullptr)
    {
        // Can't be a target and have sources.
        ASSERT(mSourcesOf.empty());

        ANGLE_TRY(mTargetOf->orphanSibling(context, this));
        mTargetOf.set(context, nullptr);
    }
    else
    {
        for (egl::Image *sourceImage : mSourcesOf)
        {
            ANGLE_TRY(sourceImage->orphanSibling(context, this));
        }
        mSourcesOf.clear();
    }

    return gl::NoError();
}

void ImageSibling::addImageSource(egl::Image *imageSource)
{
    ASSERT(imageSource != nullptr);
    mSourcesOf.insert(imageSource);
}

void ImageSibling::removeImageSource(egl::Image *imageSource)
{
    ASSERT(mSourcesOf.find(imageSource) != mSourcesOf.end());
    mSourcesOf.erase(imageSource);
}

bool ImageSibling::isEGLImageTarget() const
{
    return (mTargetOf.get() != nullptr);
}

gl::InitState ImageSibling::sourceEGLImageInitState() const
{
    ASSERT(isEGLImageTarget());
    return mTargetOf->sourceInitState();
}

void ImageSibling::setSourceEGLImageInitState(gl::InitState initState) const
{
    ASSERT(isEGLImageTarget());
    mTargetOf->setInitState(initState);
}

ImageState::ImageState(EGLenum target, ImageSibling *buffer, const AttributeMap &attribs)
    : imageIndex(GetImageIndex(target, attribs)), source(buffer), targets()
{
}

ImageState::~ImageState()
{
}

Image::Image(rx::EGLImplFactory *factory,
             EGLenum target,
             ImageSibling *buffer,
             const AttributeMap &attribs)
    : RefCountObject(0),
      mState(target, buffer, attribs),
      mImplementation(factory->createImage(mState, target, attribs)),
      mOrphanedAndNeedsInit(false)
{
    ASSERT(mImplementation != nullptr);
    ASSERT(buffer != nullptr);

    mState.source->addImageSource(this);
}

gl::Error Image::onDestroy(const gl::Context *context)
{
    // All targets should hold a ref to the egl image and it should not be deleted until there are
    // no siblings left.
    ASSERT(mState.targets.empty());

    // Tell the source that it is no longer used by this image
    if (mState.source.get() != nullptr)
    {
        mState.source->removeImageSource(this);
        mState.source.set(context, nullptr);
    }
    return gl::NoError();
}

Image::~Image()
{
    SafeDelete(mImplementation);
}

void Image::addTargetSibling(ImageSibling *sibling)
{
    mState.targets.insert(sibling);
}

gl::Error Image::orphanSibling(const gl::Context *context, ImageSibling *sibling)
{
    // notify impl
    ANGLE_TRY(mImplementation->orphan(context, sibling));

    if (mState.source.get() == sibling)
    {
        // If the sibling is the source, it cannot be a target.
        ASSERT(mState.targets.find(sibling) == mState.targets.end());
        mState.source.set(context, nullptr);
        mOrphanedAndNeedsInit =
            (sibling->initState(mState.imageIndex) == gl::InitState::MayNeedInit);
    }
    else
    {
        mState.targets.erase(sibling);
    }

    return gl::NoError();
}

const gl::Format &Image::getFormat() const
{
    return mState.source->getAttachmentFormat(GL_NONE, mState.imageIndex);
}

size_t Image::getWidth() const
{
    return mState.source->getAttachmentSize(mState.imageIndex).width;
}

size_t Image::getHeight() const
{
    return mState.source->getAttachmentSize(mState.imageIndex).height;
}

size_t Image::getSamples() const
{
    return mState.source->getAttachmentSamples(mState.imageIndex);
}

rx::ImageImpl *Image::getImplementation() const
{
    return mImplementation;
}

Error Image::initialize()
{
    return mImplementation->initialize();
}

bool Image::orphaned() const
{
    return (mState.source.get() == nullptr);
}

gl::InitState Image::sourceInitState() const
{
    if (orphaned())
    {
        return mOrphanedAndNeedsInit ? gl::InitState::MayNeedInit : gl::InitState::Initialized;
    }

    return mState.source->initState(mState.imageIndex);
}

void Image::setInitState(gl::InitState initState)
{
    if (orphaned())
    {
        mOrphanedAndNeedsInit = false;
    }

    return mState.source->setInitState(mState.imageIndex, initState);
}

}  // namespace egl
