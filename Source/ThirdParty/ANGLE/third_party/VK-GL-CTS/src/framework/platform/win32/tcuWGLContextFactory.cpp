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
 * \brief WGL GL context factory.
 *//*--------------------------------------------------------------------*/

#include "tcuWGLContextFactory.hpp"

#include "gluRenderConfig.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuWin32Window.hpp"
#include "glwFunctions.hpp"
#include "glwInitFunctions.hpp"
#include "deString.h"

using std::vector;

namespace tcu
{
namespace wgl
{
namespace
{

enum
{
    DEFAULT_WINDOW_WIDTH  = 400,
    DEFAULT_WINDOW_HEIGHT = 300
};

class WGLFunctionLoader : public glw::FunctionLoader
{
public:
    WGLFunctionLoader(const wgl::Context &context) : m_context(context)
    {
    }

    glw::GenericFuncType get(const char *name) const
    {
        return (glw::GenericFuncType)m_context.getGLFunction(name);
    }

private:
    const wgl::Context &m_context;
};

class WGLContext : public glu::RenderContext
{
public:
    WGLContext(HINSTANCE instance, const wgl::Core &wglCore, const WGLContext *sharedContext,
               const glu::RenderConfig &config);
    ~WGLContext(void);

    glu::ContextType getType(void) const
    {
        return m_contextType;
    }
    const RenderTarget &getRenderTarget(void) const
    {
        return m_renderTarget;
    }
    void postIterate(void);
    const glw::Functions &getFunctions(void) const
    {
        return m_functions;
    }

    glw::GenericFuncType getProcAddress(const char *name) const;

    void makeCurrent(void);

private:
    WGLContext(const WGLContext &other);
    WGLContext &operator=(const WGLContext &other);

    glu::ContextType m_contextType;

    win32::Window m_window;
    wgl::Context *m_context;

    tcu::RenderTarget m_renderTarget;
    glw::Functions m_functions;
};

WGLContext::WGLContext(HINSTANCE instance, const wgl::Core &wglCore, const WGLContext *sharedContext,
                       const glu::RenderConfig &config)
    : m_contextType(config.type)
    , m_window(instance, config.width != glu::RenderConfig::DONT_CARE ? config.width : DEFAULT_WINDOW_WIDTH,
               config.height != glu::RenderConfig::DONT_CARE ? config.height : DEFAULT_WINDOW_HEIGHT)
    , m_context(nullptr)
{
    if (config.surfaceType != glu::RenderConfig::SURFACETYPE_WINDOW &&
        config.surfaceType != glu::RenderConfig::SURFACETYPE_DONT_CARE)
        throw NotSupportedError("Unsupported surface type");

    HDC deviceCtx   = m_window.getDeviceContext();
    int pixelFormat = 0;

    if (config.id != glu::RenderConfig::DONT_CARE)
        pixelFormat = config.id;
    else
        pixelFormat = wgl::choosePixelFormat(wglCore, deviceCtx, config);

    if (pixelFormat < 0)
        throw NotSupportedError("Compatible WGL pixel format not found");

    const wgl::Context *sharedCtx = nullptr;
    if (nullptr != sharedContext)
        sharedCtx = sharedContext->m_context;

    m_context =
        new wgl::Context(&wglCore, deviceCtx, sharedCtx, config.type, pixelFormat, config.resetNotificationStrategy);

    try
    {
        // Describe selected config & get render target props.
        const wgl::PixelFormatInfo info = wglCore.getPixelFormatInfo(deviceCtx, pixelFormat);
        const IVec2 size                = m_window.getSize();

        m_renderTarget = tcu::RenderTarget(
            size.x(), size.y(), tcu::PixelFormat(info.redBits, info.greenBits, info.blueBits, info.alphaBits),
            info.depthBits, info.stencilBits, info.sampleBuffers ? info.samples : 0);

        // Load functions
        {
            WGLFunctionLoader funcLoader(*m_context);
            glu::initFunctions(&m_functions, &funcLoader, config.type.getAPI());
        }

        if (config.windowVisibility != glu::RenderConfig::VISIBILITY_VISIBLE &&
            config.windowVisibility != glu::RenderConfig::VISIBILITY_HIDDEN)
            throw NotSupportedError("Unsupported window visibility mode");

        m_window.setVisible(config.windowVisibility != glu::RenderConfig::VISIBILITY_HIDDEN);
    }
    catch (...)
    {
        delete m_context;
        throw;
    }
}

WGLContext::~WGLContext(void)
{
    delete m_context;
}

glw::GenericFuncType WGLContext::getProcAddress(const char *name) const
{
    return m_context->getGLFunction(name);
}

void WGLContext::makeCurrent(void)
{
    m_context->makeCurrent();
}

void WGLContext::postIterate(void)
{
    m_context->swapBuffers();
    m_window.processEvents();
}

} // namespace

ContextFactory::ContextFactory(HINSTANCE instance)
    : glu::ContextFactory("wgl", "Windows WGL OpenGL context")
    , m_instance(instance)
    , m_wglCore(instance)
{
}

glu::RenderContext *ContextFactory::createContext(const glu::RenderConfig &config, const tcu::CommandLine &,
                                                  const glu::RenderContext *sharedContext) const
{
    const WGLContext *sharedWGLContext = static_cast<const WGLContext *>(sharedContext);
    return new WGLContext(m_instance, m_wglCore, sharedWGLContext, config);
}

} // namespace wgl
} // namespace tcu
