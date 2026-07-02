/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief iOS Platform implementation.
 *//*--------------------------------------------------------------------*/

#include "tcuIOSPlatform.hh"
#include "gluFboRenderContext.hpp"
#include "gluRenderConfig.hpp"

#include "glwInitES20Direct.hpp"
#include "glwInitES30Direct.hpp"

namespace tcu
{
namespace ios
{

// ScreenManager

ScreenManager::ScreenManager(tcuEAGLView *view) : m_view(view) {}

ScreenManager::~ScreenManager(void) {}

CAEAGLLayer *ScreenManager::acquireScreen(void)
{
    if (!m_viewLock.tryLock())
        throw ResourceError("View is already is in use");

    return [m_view getEAGLLayer];
}

void ScreenManager::releaseScreen(CAEAGLLayer *layer)
{
    DE_UNREF(layer);
    m_viewLock.unlock();
}

// ContextFactory

ContextFactory::ContextFactory(ScreenManager *screenManager)
    : glu::ContextFactory("eagl", "iOS EAGL Context"),
      m_screenManager(screenManager)
{
}

ContextFactory::~ContextFactory(void) {}

glu::RenderContext *
ContextFactory::createContext(const glu::RenderConfig &config,
                              const tcu::CommandLine &) const
{
    RawContext *rawContext = new RawContext(config.type);

    try
    {
        if (config.surfaceType ==
            glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC)
            return new glu::FboRenderContext(rawContext, config);
        else if (config.surfaceType == glu::RenderConfig::SURFACETYPE_WINDOW)
            return new ScreenContext(m_screenManager, config);
        else
            throw NotSupportedError("Unsupported surface type");
    }
    catch (...)
    {
        delete rawContext;
        throw;
    }
}

// Platform

Platform::Platform(ScreenManager *screenManager)
{
    m_contextFactoryRegistry.registerFactory(new ContextFactory(screenManager));
}

Platform::~Platform(void) {}

// RawContext

static EAGLRenderingAPI getEAGLApi(glu::ContextType type)
{
    if (type.getAPI() == glu::ApiType::es(3, 0))
        return kEAGLRenderingAPIOpenGLES3;
    else if (type.getAPI() == glu::ApiType::es(2, 0))
        return kEAGLRenderingAPIOpenGLES2;
    else
        throw NotSupportedError("Requested GL API is not supported on iOS");
}

RawContext::RawContext(glu::ContextType type)
    : m_type(type), m_context(nullptr),
      m_emptyTarget(0, 0, tcu::PixelFormat(0, 0, 0, 0), 0, 0, 0)
{
    const EAGLRenderingAPI eaglApi = getEAGLApi(type);

    m_context = [[EAGLContext alloc] initWithAPI:eaglApi];
    if (!m_context)
        throw ResourceError("Failed to create EAGL context");

    try
    {
        if (![EAGLContext setCurrentContext:m_context])
            throw ResourceError("Failed to set current EAGL context");

        if (type.getAPI() == glu::ApiType::es(3, 0))
            glw::initES30Direct(&m_functions);
        else if (type.getAPI() == glu::ApiType::es(2, 0))
            glw::initES20Direct(&m_functions);
        else
            throw InternalError("Unsupproted API for loading functions");
    }
    catch (...)
    {
        if ([EAGLContext currentContext] == m_context)
            [EAGLContext setCurrentContext:nil];

        [m_context release];
        throw;
    }
}

RawContext::~RawContext(void)
{
    if ([EAGLContext currentContext] == m_context)
        [EAGLContext setCurrentContext:nil];

    [m_context release];
}

void RawContext::postIterate(void) {}

NSString *chooseLayerColorFormat(const glu::RenderConfig &config)
{
    const bool cr = config.redBits != glu::RenderConfig::DONT_CARE;
    const bool cg = config.greenBits != glu::RenderConfig::DONT_CARE;
    const bool cb = config.blueBits != glu::RenderConfig::DONT_CARE;
    const bool ca = config.alphaBits != glu::RenderConfig::DONT_CARE;

    if ((!cr || config.redBits == 8) && (!cg || config.greenBits == 8) &&
        (!cb || config.blueBits == 8) && (!ca || config.alphaBits == 8))
        return kEAGLColorFormatRGBA8;

    if ((!cr || config.redBits == 5) && (!cg || config.greenBits == 6) &&
        (!cb || config.blueBits == 5) && (!ca || config.alphaBits == 0))
        return kEAGLColorFormatRGB565;

    return nil;
}

// ScreenContext

ScreenContext::ScreenContext(ScreenManager *screenManager,
                             const glu::RenderConfig &config)
    : RawContext(config.type), m_screenManager(screenManager), m_layer(nullptr),
      m_framebuffer(
          *this) // \note Perfectly safe to give reference to this RC as
                 // everything except postIterate() works at this point.
      ,
      m_colorBuffer(*this), m_depthStencilBuffer(*this)
{
    m_layer = m_screenManager->acquireScreen();
    try
    {
        createFramebuffer(config);
    }
    catch (...)
    {
        m_screenManager->releaseScreen(m_layer);
        throw;
    }
}

ScreenContext::~ScreenContext(void) { m_screenManager->releaseScreen(m_layer); }

void ScreenContext::createFramebuffer(const glu::RenderConfig &config)
{
    const glw::Functions &gl = getFunctions();
    const NSString *const colorFormat = chooseLayerColorFormat(config);
    const uint32_t depthStencilFormat = chooseDepthStencilFormat(config);
    tcu::PixelFormat pixelFormat;
    int width = 0;
    int height = 0;
    int depthBits = 0;
    int stencilBits = 0;

    if (config.numSamples > 0)
        throw NotSupportedError("Multisample config is not supported");

    if (colorFormat == nil)
        throw NotSupportedError("Unsupported color attachment format");

    if ((config.depthBits > 0 || config.stencilBits > 0) &&
        depthStencilFormat == 0)
        throw NotSupportedError(
            "Unsupported depth & stencil attachment format");

    m_layer.opaque = TRUE;
    m_layer.drawableProperties = [NSDictionary
        dictionaryWithObjectsAndKeys:colorFormat,
                                     kEAGLDrawablePropertyColorFormat,
                                     [NSNumber numberWithBool:FALSE],
                                     kEAGLDrawablePropertyRetainedBacking, nil];

    gl.bindRenderbuffer(GL_RENDERBUFFER, *m_colorBuffer);
    if (![getEAGLContext() renderbufferStorage:GL_RENDERBUFFER
                                  fromDrawable:(CAEAGLLayer *)m_layer])
        throw ResourceError("Failed to allocate color renderbuffer");
    GLU_EXPECT_NO_ERROR(gl.getError(), "Creating color renderbuffer");

    gl.getRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH,
                                  &width);
    gl.getRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT,
                                  &height);
    gl.getRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_RED_SIZE,
                                  &pixelFormat.redBits);
    gl.getRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_GREEN_SIZE,
                                  &pixelFormat.greenBits);
    gl.getRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_BLUE_SIZE,
                                  &pixelFormat.blueBits);
    gl.getRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_ALPHA_SIZE,
                                  &pixelFormat.alphaBits);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Querying surface size failed");

    if (depthStencilFormat != 0)
    {
        gl.bindRenderbuffer(GL_RENDERBUFFER, *m_depthStencilBuffer);
        gl.renderbufferStorage(GL_RENDERBUFFER, depthStencilFormat, width,
                               height);

        gl.getRenderbufferParameteriv(GL_RENDERBUFFER,
                                      GL_RENDERBUFFER_DEPTH_SIZE, &depthBits);
        gl.getRenderbufferParameteriv(
            GL_RENDERBUFFER, GL_RENDERBUFFER_STENCIL_SIZE, &stencilBits);

        GLU_EXPECT_NO_ERROR(gl.getError(),
                            "Creating depth / stencil renderbuffer");
    }

    gl.bindFramebuffer(GL_FRAMEBUFFER, *m_framebuffer);
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_RENDERBUFFER, *m_colorBuffer);

    if (depthStencilFormat != 0)
    {
        if (depthBits > 0)
            gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                       GL_RENDERBUFFER, *m_depthStencilBuffer);

        if (stencilBits > 0)
            gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                       GL_RENDERBUFFER, *m_depthStencilBuffer);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Creating framebuffer");

    if (gl.checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw NotSupportedError("Framebuffer is not complete");

    // Set up correct viewport for first test case.
    gl.viewport(0, 0, width, height);

    m_renderTarget = tcu::RenderTarget(width, height, pixelFormat, depthBits,
                                       stencilBits, 0);
}

void ScreenContext::postIterate(void)
{
    const glw::Functions &gl = getFunctions();
    gl.bindRenderbuffer(GL_RENDERBUFFER, *m_colorBuffer);

    if (![getEAGLContext() presentRenderbuffer:GL_RENDERBUFFER])
        throw ResourceError("presentRenderbuffer() failed");
}

} // namespace ios
} // namespace tcu
