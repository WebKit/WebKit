//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Surface.cpp: Implements the egl::Surface class, representing a drawing surface
// such as the client area of a window, including any back buffers.
// Implements EGLSurface and related functionality. [EGL 1.4] section 2.2 page 3.

#include "libANGLE/Surface.h"

#include <EGL/eglext.h>

#include <iostream>

#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/Texture.h"
#include "libANGLE/Thread.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/EGLImplFactory.h"

namespace egl
{

SurfaceState::SurfaceState(const egl::Config *configIn, const AttributeMap &attributesIn)
    : defaultFramebuffer(nullptr), config(configIn), attributes(attributesIn)
{
}

Surface::Surface(EGLint surfaceType, const egl::Config *config, const AttributeMap &attributes)
    : FramebufferAttachmentObject(),
      mState(config, attributes),
      mImplementation(nullptr),
      mCurrentCount(0),
      mDestroyed(false),
      mType(surfaceType),
      mPostSubBufferRequested(false),
      mLargestPbuffer(false),
      mGLColorspace(EGL_GL_COLORSPACE_LINEAR),
      mVGAlphaFormat(EGL_VG_ALPHA_FORMAT_NONPRE),
      mVGColorspace(EGL_VG_COLORSPACE_sRGB),
      mMipmapTexture(false),
      mMipmapLevel(0),
      mHorizontalResolution(EGL_UNKNOWN),
      mVerticalResolution(EGL_UNKNOWN),
      mMultisampleResolve(EGL_MULTISAMPLE_RESOLVE_DEFAULT),
      mFixedSize(false),
      mFixedWidth(0),
      mFixedHeight(0),
      mTextureFormat(EGL_NO_TEXTURE),
      mTextureTarget(EGL_NO_TEXTURE),
      // FIXME: Determine actual pixel aspect ratio
      mPixelAspectRatio(static_cast<EGLint>(1.0 * EGL_DISPLAY_SCALING)),
      mRenderBuffer(EGL_BACK_BUFFER),
      mSwapBehavior(EGL_NONE),
      mOrientation(0),
      mTexture(),
      mBackFormat(config->renderTargetFormat),
      mDSFormat(config->depthStencilFormat)
{
    mPostSubBufferRequested = (attributes.get(EGL_POST_SUB_BUFFER_SUPPORTED_NV, EGL_FALSE) == EGL_TRUE);
    mFlexibleSurfaceCompatibilityRequested =
        (attributes.get(EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE, EGL_FALSE) == EGL_TRUE);

    if (mType == EGL_PBUFFER_BIT)
    {
        mLargestPbuffer = (attributes.get(EGL_LARGEST_PBUFFER, EGL_FALSE) == EGL_TRUE);
    }

    mGLColorspace =
        static_cast<EGLenum>(attributes.get(EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_LINEAR));
    mVGAlphaFormat =
        static_cast<EGLenum>(attributes.get(EGL_VG_ALPHA_FORMAT, EGL_VG_ALPHA_FORMAT_NONPRE));
    mVGColorspace = static_cast<EGLenum>(attributes.get(EGL_VG_COLORSPACE, EGL_VG_COLORSPACE_sRGB));
    mMipmapTexture = (attributes.get(EGL_MIPMAP_TEXTURE, EGL_FALSE) == EGL_TRUE);

    mDirectComposition = (attributes.get(EGL_DIRECT_COMPOSITION_ANGLE, EGL_FALSE) == EGL_TRUE);

    mRobustResourceInitialization =
        (attributes.get(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE, EGL_FALSE) == EGL_TRUE);

    mFixedSize = (attributes.get(EGL_FIXED_SIZE_ANGLE, EGL_FALSE) == EGL_TRUE);
    if (mFixedSize)
    {
        mFixedWidth  = static_cast<size_t>(attributes.get(EGL_WIDTH, 0));
        mFixedHeight = static_cast<size_t>(attributes.get(EGL_HEIGHT, 0));
    }

    if (mType != EGL_WINDOW_BIT)
    {
        mTextureFormat = static_cast<EGLenum>(attributes.get(EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE));
        mTextureTarget = static_cast<EGLenum>(attributes.get(EGL_TEXTURE_TARGET, EGL_NO_TEXTURE));
    }

    mOrientation = static_cast<EGLint>(attributes.get(EGL_SURFACE_ORIENTATION_ANGLE, 0));
}

Surface::~Surface()
{
}

rx::FramebufferAttachmentObjectImpl *Surface::getAttachmentImpl() const
{
    return mImplementation;
}

Error Surface::destroyImpl(const Display *display)
{
    if (mState.defaultFramebuffer)
    {
        mState.defaultFramebuffer->destroyDefault(display);
    }
    if (mImplementation)
    {
        mImplementation->destroy(display);
    }

    if (mTexture.get())
    {
        if (mImplementation)
        {
            ANGLE_TRY(mImplementation->releaseTexImage(EGL_BACK_BUFFER));
        }
        auto glErr = mTexture->releaseTexImageFromSurface(display->getProxyContext());
        if (glErr.isError())
        {
            return Error(EGL_BAD_SURFACE);
        }
        mTexture.set(nullptr, nullptr);
    }

    if (mState.defaultFramebuffer)
    {
        mState.defaultFramebuffer->onDestroy(display->getProxyContext());
    }
    SafeDelete(mState.defaultFramebuffer);
    SafeDelete(mImplementation);

    delete this;
    return NoError();
}

Error Surface::initialize(const Display *display)
{
    ANGLE_TRY(mImplementation->initialize(display));

    // Initialized here since impl is nullptr in the constructor.
    // Must happen after implementation initialize for Android.
    mSwapBehavior = mImplementation->getSwapBehavior();

    // Must happen after implementation initialize for OSX.
    mState.defaultFramebuffer = createDefaultFramebuffer(display);
    ASSERT(mState.defaultFramebuffer != nullptr);

    return NoError();
}

Error Surface::setIsCurrent(const gl::Context *context, bool isCurrent)
{
    if (isCurrent)
    {
        mCurrentCount++;
        return NoError();
    }

    ASSERT(mCurrentCount > 0);
    mCurrentCount--;
    if (mCurrentCount == 0 && mDestroyed)
    {
        ASSERT(context);
        return destroyImpl(context->getCurrentDisplay());
    }
    return NoError();
}

Error Surface::onDestroy(const Display *display)
{
    mDestroyed = true;
    if (mCurrentCount == 0)
    {
        return destroyImpl(display);
    }
    return NoError();
}

EGLint Surface::getType() const
{
    return mType;
}

Error Surface::swap(const gl::Context *context)
{
    return mImplementation->swap(context);
}

Error Surface::swapWithDamage(const gl::Context *context, EGLint *rects, EGLint n_rects)
{
    return mImplementation->swapWithDamage(context, rects, n_rects);
}

Error Surface::postSubBuffer(const gl::Context *context,
                             EGLint x,
                             EGLint y,
                             EGLint width,
                             EGLint height)
{
    return mImplementation->postSubBuffer(context, x, y, width, height);
}

Error Surface::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    return mImplementation->querySurfacePointerANGLE(attribute, value);
}

EGLint Surface::isPostSubBufferSupported() const
{
    return mPostSubBufferRequested && mImplementation->isPostSubBufferSupported();
}

void Surface::setSwapInterval(EGLint interval)
{
    mImplementation->setSwapInterval(interval);
}

void Surface::setMipmapLevel(EGLint level)
{
    // Level is set but ignored
    UNIMPLEMENTED();
    mMipmapLevel = level;
}

void Surface::setMultisampleResolve(EGLenum resolve)
{
    // Behaviour is set but ignored
    UNIMPLEMENTED();
    mMultisampleResolve = resolve;
}

void Surface::setSwapBehavior(EGLenum behavior)
{
    // Behaviour is set but ignored
    UNIMPLEMENTED();
    mSwapBehavior = behavior;
}

const Config *Surface::getConfig() const
{
    return mState.config;
}

EGLint Surface::getPixelAspectRatio() const
{
    return mPixelAspectRatio;
}

EGLenum Surface::getRenderBuffer() const
{
    return mRenderBuffer;
}

EGLenum Surface::getSwapBehavior() const
{
    return mSwapBehavior;
}

EGLenum Surface::getTextureFormat() const
{
    return mTextureFormat;
}

EGLenum Surface::getTextureTarget() const
{
    return mTextureTarget;
}

bool Surface::getLargestPbuffer() const
{
    return mLargestPbuffer;
}

EGLenum Surface::getGLColorspace() const
{
    return mGLColorspace;
}

EGLenum Surface::getVGAlphaFormat() const
{
    return mVGAlphaFormat;
}

EGLenum Surface::getVGColorspace() const
{
    return mVGColorspace;
}

bool Surface::getMipmapTexture() const
{
    return mMipmapTexture;
}

EGLint Surface::getMipmapLevel() const
{
    return mMipmapLevel;
}

EGLint Surface::getHorizontalResolution() const
{
    return mHorizontalResolution;
}

EGLint Surface::getVerticalResolution() const
{
    return mVerticalResolution;
}

EGLenum Surface::getMultisampleResolve() const
{
    return mMultisampleResolve;
}

EGLint Surface::isFixedSize() const
{
    return mFixedSize;
}

EGLint Surface::getWidth() const
{
    return mFixedSize ? static_cast<EGLint>(mFixedWidth) : mImplementation->getWidth();
}

EGLint Surface::getHeight() const
{
    return mFixedSize ? static_cast<EGLint>(mFixedHeight) : mImplementation->getHeight();
}

Error Surface::bindTexImage(const gl::Context *context, gl::Texture *texture, EGLint buffer)
{
    ASSERT(!mTexture.get());
    ANGLE_TRY(mImplementation->bindTexImage(texture, buffer));

    auto glErr = texture->bindTexImageFromSurface(context, this);
    if (glErr.isError())
    {
        return Error(EGL_BAD_SURFACE);
    }
    mTexture.set(context, texture);

    return NoError();
}

Error Surface::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    ASSERT(context);

    ANGLE_TRY(mImplementation->releaseTexImage(buffer));

    ASSERT(mTexture.get());
    auto glErr = mTexture->releaseTexImageFromSurface(context);
    if (glErr.isError())
    {
        return Error(EGL_BAD_SURFACE);
    }
    mTexture.set(context, nullptr);

    return NoError();
}

Error Surface::getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc)
{
    return mImplementation->getSyncValues(ust, msc, sbc);
}

void Surface::releaseTexImageFromTexture(const gl::Context *context)
{
    ASSERT(mTexture.get());
    mTexture.set(context, nullptr);
}

gl::Extents Surface::getAttachmentSize(const gl::ImageIndex & /*target*/) const
{
    return gl::Extents(getWidth(), getHeight(), 1);
}

const gl::Format &Surface::getAttachmentFormat(GLenum binding, const gl::ImageIndex &target) const
{
    return (binding == GL_BACK ? mBackFormat : mDSFormat);
}

GLsizei Surface::getAttachmentSamples(const gl::ImageIndex &target) const
{
    return getConfig()->samples;
}

GLuint Surface::getId() const
{
    UNREACHABLE();
    return 0;
}

gl::Framebuffer *Surface::createDefaultFramebuffer(const Display *display)
{
    return new gl::Framebuffer(display, this);
}

gl::InitState Surface::initState(const gl::ImageIndex & /*imageIndex*/) const
{
    // TODO(jmadill): Lazy surface init.
    return gl::InitState::Initialized;
}

void Surface::setInitState(const gl::ImageIndex & /*imageIndex*/, gl::InitState /*initState*/)
{
    // No-op.
}

WindowSurface::WindowSurface(rx::EGLImplFactory *implFactory,
                             const egl::Config *config,
                             EGLNativeWindowType window,
                             const AttributeMap &attribs)
    : Surface(EGL_WINDOW_BIT, config, attribs)
{
    mImplementation = implFactory->createWindowSurface(mState, window, attribs);
}

WindowSurface::~WindowSurface()
{
}

PbufferSurface::PbufferSurface(rx::EGLImplFactory *implFactory,
                               const Config *config,
                               const AttributeMap &attribs)
    : Surface(EGL_PBUFFER_BIT, config, attribs)
{
    mImplementation = implFactory->createPbufferSurface(mState, attribs);
}

PbufferSurface::PbufferSurface(rx::EGLImplFactory *implFactory,
                               const Config *config,
                               EGLenum buftype,
                               EGLClientBuffer clientBuffer,
                               const AttributeMap &attribs)
    : Surface(EGL_PBUFFER_BIT, config, attribs)
{
    mImplementation =
        implFactory->createPbufferFromClientBuffer(mState, buftype, clientBuffer, attribs);
}

PbufferSurface::~PbufferSurface()
{
}

PixmapSurface::PixmapSurface(rx::EGLImplFactory *implFactory,
                             const Config *config,
                             NativePixmapType nativePixmap,
                             const AttributeMap &attribs)
    : Surface(EGL_PIXMAP_BIT, config, attribs)
{
    mImplementation = implFactory->createPixmapSurface(mState, nativePixmap, attribs);
}

PixmapSurface::~PixmapSurface()
{
}

// SurfaceDeleter implementation.

SurfaceDeleter::SurfaceDeleter(const Display *display) : mDisplay(display)
{
}

SurfaceDeleter::~SurfaceDeleter()
{
}

void SurfaceDeleter::operator()(Surface *surface)
{
    ANGLE_SWALLOW_ERR(surface->onDestroy(mDisplay));
}

}  // namespace egl
