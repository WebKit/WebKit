//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WindowSurfaceCGL.cpp: CGL implementation of egl::Surface for windows

#include "libANGLE/renderer/gl/cgl/WindowSurfaceCGL.h"

#import <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>
#import <QuartzCore/QuartzCore.h>

#include "common/debug.h"
#include "libANGLE/renderer/gl/cgl/DisplayCGL.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"

@interface SwapLayer : CAOpenGLLayer
{
    CGLContextObj mDisplayContext;

    bool initialized;
    rx::SharedSwapState *mSwapState;
    const rx::FunctionsGL *mFunctions;

    GLuint mReadFramebuffer;
}
- (id)initWithSharedState:(rx::SharedSwapState *)swapState
              withContext:(CGLContextObj)displayContext
            withFunctions:(const rx::FunctionsGL *)functions;
@end

@implementation SwapLayer
- (id)initWithSharedState:(rx::SharedSwapState *)swapState
              withContext:(CGLContextObj)displayContext
            withFunctions:(const rx::FunctionsGL *)functions
    {
        self = [super init];
        if (self != nil)
        {
            self.asynchronous = YES;
            mDisplayContext   = displayContext;

            initialized = false;
            mSwapState  = swapState;
            mFunctions  = functions;

            [self setFrame:CGRectMake(0, 0, mSwapState->textures[0].width,
                                      mSwapState->textures[0].height)];
        }
        return self;
    }

    - (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
    {
        CGLPixelFormatAttribute attribs[] = {
            kCGLPFADisplayMask, static_cast<CGLPixelFormatAttribute>(mask), kCGLPFAOpenGLProfile,
            static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),
            static_cast<CGLPixelFormatAttribute>(0)};

        CGLPixelFormatObj pixelFormat = nullptr;
        GLint numFormats = 0;
        CGLChoosePixelFormat(attribs, &pixelFormat, &numFormats);

        return pixelFormat;
    }

    - (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
    {
        CGLContextObj context = nullptr;
        CGLCreateContext(pixelFormat, mDisplayContext, &context);
        return context;
    }

    - (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                    pixelFormat:(CGLPixelFormatObj)pixelFormat
                   forLayerTime:(CFTimeInterval)timeInterval
                    displayTime:(const CVTimeStamp *)timeStamp
    {
        BOOL result = NO;

        pthread_mutex_lock(&mSwapState->mutex);
        {
            if (mSwapState->lastRendered->swapId > mSwapState->beingPresented->swapId)
            {
                std::swap(mSwapState->lastRendered, mSwapState->beingPresented);
                result = YES;
            }
        }
        pthread_mutex_unlock(&mSwapState->mutex);

        return result;
    }

    - (void)drawInCGLContext:(CGLContextObj)glContext
                 pixelFormat:(CGLPixelFormatObj)pixelFormat
                forLayerTime:(CFTimeInterval)timeInterval
                 displayTime:(const CVTimeStamp *)timeStamp
    {
        CGLSetCurrentContext(glContext);
        if (!initialized)
        {
            initialized = true;

            mFunctions->genFramebuffers(1, &mReadFramebuffer);
        }

        const auto &texture = *mSwapState->beingPresented;
        if ([self frame].size.width != texture.width || [self frame].size.height != texture.height)
        {
            [self setFrame:CGRectMake(0, 0, texture.width, texture.height)];

            // Without this, the OSX compositor / window system doesn't see the resize.
            [self setNeedsDisplay];
        }

        // TODO(cwallez) support 2.1 contexts too that don't have blitFramebuffer nor the
        // GL_DRAW_FRAMEBUFFER_BINDING query
        GLint drawFBO;
        mFunctions->getIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);

        mFunctions->bindFramebuffer(GL_FRAMEBUFFER, mReadFramebuffer);
        mFunctions->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                         texture.texture, 0);

        mFunctions->bindFramebuffer(GL_READ_FRAMEBUFFER, mReadFramebuffer);
        mFunctions->bindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
        mFunctions->blitFramebuffer(0, 0, texture.width, texture.height, 0, 0, texture.width,
                                    texture.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // Call the super method to flush the context
        [super drawInCGLContext:glContext
                    pixelFormat:pixelFormat
                   forLayerTime:timeInterval
                    displayTime:timeStamp];
    }
    @end

    namespace rx
    {

    WindowSurfaceCGL::WindowSurfaceCGL(const egl::SurfaceState &state,
                                       RendererGL *renderer,
                                       CALayer *layer,
                                       const FunctionsGL *functions,
                                       CGLContextObj context)
        : SurfaceGL(state, renderer),
          mSwapLayer(nil),
          mCurrentSwapId(0),
          mLayer(layer),
          mContext(context),
          mFunctions(functions),
          mStateManager(renderer->getStateManager()),
          mRenderer(renderer),
          mWorkarounds(renderer->getWorkarounds()),
          mFramebuffer(0),
          mDSRenderbuffer(0)
    {
        pthread_mutex_init(&mSwapState.mutex, nullptr);
}

WindowSurfaceCGL::~WindowSurfaceCGL()
{
    pthread_mutex_destroy(&mSwapState.mutex);
    if (mFramebuffer != 0)
    {
        mFunctions->deleteFramebuffers(1, &mFramebuffer);
        mFramebuffer = 0;
    }

    if (mDSRenderbuffer != 0)
    {
        mFunctions->deleteRenderbuffers(1, &mDSRenderbuffer);
        mDSRenderbuffer = 0;
    }

    if (mSwapLayer != nil)
    {
        [mSwapLayer removeFromSuperlayer];
        [mSwapLayer release];
        mSwapLayer = nil;
    }

    for (size_t i = 0; i < ArraySize(mSwapState.textures); ++i)
    {
        if (mSwapState.textures[i].texture != 0)
        {
            mFunctions->deleteTextures(1, &mSwapState.textures[i].texture);
            mSwapState.textures[i].texture = 0;
        }
    }
}

egl::Error WindowSurfaceCGL::initialize()
{
    unsigned width  = getWidth();
    unsigned height = getHeight();

    for (size_t i = 0; i < ArraySize(mSwapState.textures); ++i)
    {
        mFunctions->genTextures(1, &mSwapState.textures[i].texture);
        mStateManager->bindTexture(GL_TEXTURE_2D, mSwapState.textures[i].texture);
        mFunctions->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                               GL_UNSIGNED_BYTE, nullptr);
        mSwapState.textures[i].width  = width;
        mSwapState.textures[i].height = height;
        mSwapState.textures[i].swapId = 0;
    }
    mSwapState.beingRendered  = &mSwapState.textures[0];
    mSwapState.lastRendered   = &mSwapState.textures[1];
    mSwapState.beingPresented = &mSwapState.textures[2];

    mSwapLayer = [[SwapLayer alloc] initWithSharedState:&mSwapState
                                            withContext:mContext
                                          withFunctions:mFunctions];
    [mLayer addSublayer:mSwapLayer];

    mFunctions->genRenderbuffers(1, &mDSRenderbuffer);
    mStateManager->bindRenderbuffer(GL_RENDERBUFFER, mDSRenderbuffer);
    mFunctions->renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    mFunctions->genFramebuffers(1, &mFramebuffer);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    mFunctions->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                     mSwapState.beingRendered->texture, 0);
    mFunctions->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                        mDSRenderbuffer);

    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceCGL::makeCurrent()
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceCGL::swap()
{
    mFunctions->flush();
    mSwapState.beingRendered->swapId = ++mCurrentSwapId;

    pthread_mutex_lock(&mSwapState.mutex);
    {
        std::swap(mSwapState.beingRendered, mSwapState.lastRendered);
    }
    pthread_mutex_unlock(&mSwapState.mutex);

    unsigned width  = getWidth();
    unsigned height = getHeight();
    auto &texture   = *mSwapState.beingRendered;

    if (texture.width != width || texture.height != height)
    {
        mStateManager->bindTexture(GL_TEXTURE_2D, texture.texture);
        mFunctions->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                               GL_UNSIGNED_BYTE, nullptr);

        mStateManager->bindRenderbuffer(GL_RENDERBUFFER, mDSRenderbuffer);
        mFunctions->renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        texture.width  = width;
        texture.height = height;
    }

    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    mFunctions->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                     mSwapState.beingRendered->texture, 0);

    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceCGL::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceCGL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceCGL::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceCGL::releaseTexImage(EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

void WindowSurfaceCGL::setSwapInterval(EGLint interval)
{
    // TODO(cwallez) investigate implementing swap intervals other than 0
}

EGLint WindowSurfaceCGL::getWidth() const
{
    return CGRectGetWidth([mLayer frame]);
}

EGLint WindowSurfaceCGL::getHeight() const
{
    return CGRectGetHeight([mLayer frame]);
}

EGLint WindowSurfaceCGL::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return EGL_FALSE;
}

EGLint WindowSurfaceCGL::getSwapBehavior() const
{
    return EGL_BUFFER_DESTROYED;
}

FramebufferImpl *WindowSurfaceCGL::createDefaultFramebuffer(const gl::FramebufferState &state)
{
    // TODO(cwallez) assert it happens only once?
    return new FramebufferGL(mFramebuffer, state, mFunctions, mWorkarounds, mRenderer->getBlitter(),
                             mStateManager);
}

}
