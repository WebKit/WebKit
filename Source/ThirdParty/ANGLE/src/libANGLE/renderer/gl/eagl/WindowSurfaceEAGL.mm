//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WindowSurfaceEAGL.cpp: EAGL implementation of egl::Surface

#import "common/platform.h"

#if defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)

#    import "libANGLE/renderer/gl/eagl/WindowSurfaceEAGL.h"

#    import "common/debug.h"
#    import "libANGLE/Context.h"
#    import "libANGLE/renderer/gl/FramebufferGL.h"
#    import "libANGLE/renderer/gl/RendererGL.h"
#    import "libANGLE/renderer/gl/StateManagerGL.h"
#    import "libANGLE/renderer/gl/eagl/DisplayEAGL.h"

#    import <OpenGLES/EAGL.h>
#    import <QuartzCore/QuartzCore.h>

// TODO(anglebug.com/4275): It's not clear why this needs to be an EAGLLayer.

@interface WebSwapLayer : CAEAGLLayer {
    EAGLContextObj mDisplayContext;

    bool initialized;
    rx::SharedSwapState *mSwapState;
    const rx::FunctionsGL *mFunctions;

    GLuint mReadFramebuffer;
}
- (id)initWithSharedState:(rx::SharedSwapState *)swapState
              withContext:(EAGLContextObj)displayContext
            withFunctions:(const rx::FunctionsGL *)functions;
@end

@implementation WebSwapLayer
- (id)initWithSharedState:(rx::SharedSwapState *)swapState
              withContext:(EAGLContextObj)displayContext
            withFunctions:(const rx::FunctionsGL *)functions
{
    self = [super init];
    if (self != nil)
    {
        mDisplayContext = displayContext;

        initialized = false;
        mSwapState  = swapState;
        mFunctions  = functions;

        [self setFrame:CGRectMake(0, 0, mSwapState->textures[0].width,
                                  mSwapState->textures[0].height)];
    }
    return self;
}

- (void)display
{
    pthread_mutex_lock(&mSwapState->mutex);
    {
        if (mSwapState->lastRendered->swapId > mSwapState->beingPresented->swapId)
        {
            std::swap(mSwapState->lastRendered, mSwapState->beingPresented);
        }
    }
    pthread_mutex_unlock(&mSwapState->mutex);

    [EAGLContext setCurrentContext:mDisplayContext];

    if (!initialized)
    {
        initialized = true;

        mFunctions->genFramebuffers(1, &mReadFramebuffer);
    }

    const auto &texture = *mSwapState->beingPresented;

    if ([self frame].size.width != texture.width || [self frame].size.height != texture.height)
    {
        [self setFrame:CGRectMake(0, 0, texture.width, texture.height)];

        // TODO(anglebug.com/4275): If this continues to remain an EAGLLayer, then this is
        // where we'd probably want to create the renderbuffer storage.
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

    mFunctions->bindRenderbuffer(GL_RENDERBUFFER, texture.texture);
    [mDisplayContext presentRenderbuffer:GL_RENDERBUFFER];
    [EAGLContext setCurrentContext:nil];
}
@end

namespace rx
{

WindowSurfaceEAGL::WindowSurfaceEAGL(const egl::SurfaceState &state,
                                     RendererGL *renderer,
                                     EGLNativeWindowType layer,
                                     EAGLContextObj context)
    : SurfaceGL(state),
      mSwapLayer(nil),
      mCurrentSwapId(0),
      mLayer(reinterpret_cast<CALayer *>(layer)),
      mContext(context),
      mFunctions(renderer->getFunctions()),
      mStateManager(renderer->getStateManager()),
      mDSRenderbuffer(0)
{
    pthread_mutex_init(&mSwapState.mutex, nullptr);
}

WindowSurfaceEAGL::~WindowSurfaceEAGL()
{
    pthread_mutex_destroy(&mSwapState.mutex);

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

egl::Error WindowSurfaceEAGL::initialize(const egl::Display *display)
{
    unsigned width  = getWidth();
    unsigned height = getHeight();

    for (size_t i = 0; i < ArraySize(mSwapState.textures); ++i)
    {
        mFunctions->genTextures(1, &mSwapState.textures[i].texture);
        mStateManager->bindTexture(gl::TextureType::_2D, mSwapState.textures[i].texture);
        mFunctions->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                               GL_UNSIGNED_BYTE, nullptr);
        mSwapState.textures[i].width  = width;
        mSwapState.textures[i].height = height;
        mSwapState.textures[i].swapId = 0;
    }
    mSwapState.beingRendered  = &mSwapState.textures[0];
    mSwapState.lastRendered   = &mSwapState.textures[1];
    mSwapState.beingPresented = &mSwapState.textures[2];

    mSwapLayer = [[WebSwapLayer alloc] initWithSharedState:&mSwapState
                                               withContext:mContext
                                             withFunctions:mFunctions];
    [mLayer addSublayer:mSwapLayer];
    [mSwapLayer setContentsScale:[mLayer contentsScale]];

    mFunctions->genRenderbuffers(1, &mDSRenderbuffer);
    mStateManager->bindRenderbuffer(GL_RENDERBUFFER, mDSRenderbuffer);
    mFunctions->renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceEAGL::makeCurrent(const gl::Context *context)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceEAGL::swap(const gl::Context *context)
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    functions->flush();
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
        stateManager->bindTexture(gl::TextureType::_2D, texture.texture);
        functions->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                              GL_UNSIGNED_BYTE, nullptr);

        stateManager->bindRenderbuffer(GL_RENDERBUFFER, mDSRenderbuffer);
        functions->renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        texture.width  = width;
        texture.height = height;
    }

    FramebufferGL *framebufferGL = GetImplAs<FramebufferGL>(context->getFramebuffer({0}));
    stateManager->bindFramebuffer(GL_FRAMEBUFFER, framebufferGL->getFramebufferID());
    functions->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                    mSwapState.beingRendered->texture, 0);

    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceEAGL::postSubBuffer(const gl::Context *context,
                                            EGLint x,
                                            EGLint y,
                                            EGLint width,
                                            EGLint height)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceEAGL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceEAGL::bindTexImage(const gl::Context *context,
                                           gl::Texture *texture,
                                           EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceEAGL::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

void WindowSurfaceEAGL::setSwapInterval(EGLint interval)
{
    // TODO(cwallez) investigate implementing swap intervals other than 0
}

EGLint WindowSurfaceEAGL::getWidth() const
{
    return static_cast<EGLint>(CGRectGetWidth([mLayer frame]) * [mLayer contentsScale]);
}

EGLint WindowSurfaceEAGL::getHeight() const
{
    return static_cast<EGLint>(CGRectGetHeight([mLayer frame]) * [mLayer contentsScale]);
}

EGLint WindowSurfaceEAGL::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return EGL_FALSE;
}

EGLint WindowSurfaceEAGL::getSwapBehavior() const
{
    return EGL_BUFFER_DESTROYED;
}

FramebufferImpl *WindowSurfaceEAGL::createDefaultFramebuffer(const gl::Context *context,
                                                             const gl::FramebufferState &state)
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    GLuint framebuffer = 0;
    functions->genFramebuffers(1, &framebuffer);
    stateManager->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    functions->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                    mSwapState.beingRendered->texture, 0);
    functions->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                       mDSRenderbuffer);

    return new FramebufferGL(state, framebuffer, true, false);
}

}  // namespace rx

#endif  // defined(ANGLE_PLATFORM_IOS)
