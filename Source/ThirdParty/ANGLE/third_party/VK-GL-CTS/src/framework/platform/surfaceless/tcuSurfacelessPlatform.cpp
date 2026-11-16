/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2015 Intel Corporation
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
 * \brief surfaceless platform
 *//*--------------------------------------------------------------------*/

#include "tcuSurfacelessPlatform.hpp"

#include <string>
#include <vector>
#include <sys/utsname.h>

#include "deDynamicLibrary.hpp"
#include "deMemory.h"
#include "deSTLUtil.hpp"
#include "egluUtil.hpp"
#include "egluGLUtil.hpp"
#include "eglwEnums.hpp"
#include "eglwLibrary.hpp"
#include "gluPlatform.hpp"
#include "gluRenderConfig.hpp"
#include "glwInitES20Direct.hpp"
#include "glwInitES30Direct.hpp"
#include "glwInitFunctions.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuPlatform.hpp"
#include "tcuRenderTarget.hpp"
#include "vkPlatform.hpp"

#include <EGL/egl.h>

using std::string;
using std::vector;

#if !defined(EGL_KHR_create_context)
#define EGL_CONTEXT_FLAGS_KHR 0x30FC
#define EGL_CONTEXT_MAJOR_VERSION_KHR 0x3098
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR 0x00000002
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR 0x00000002
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR 0x30FD
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR 0x31BD
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR 0x00000004
#define EGL_KHR_create_context 1
#define EGL_LOSE_CONTEXT_ON_RESET_KHR 0x31BF
#define EGL_NO_RESET_NOTIFICATION_KHR 0x31BE
#define EGL_OPENGL_ES3_BIT_KHR 0x00000040
#endif // EGL_KHR_create_context

// Default library names
#if !defined(DEQP_GLES2_LIBRARY_PATH)
#define DEQP_GLES2_LIBRARY_PATH "libGLESv2.so"
#endif

#if !defined(DEQP_GLES3_LIBRARY_PATH)
#define DEQP_GLES3_LIBRARY_PATH DEQP_GLES2_LIBRARY_PATH
#endif

#if !defined(DEQP_OPENGL_LIBRARY_PATH)
#define DEQP_OPENGL_LIBRARY_PATH "libGL.so"
#endif

#if !defined(DEQP_VULKAN_LIBRARY_PATH)
#ifdef CTS_USES_VULKANSC
#define DEQP_VULKAN_LIBRARY_BASENAME "libvulkansc"
#else
#define DEQP_VULKAN_LIBRARY_BASENAME "libvulkan"
#endif
#if (DE_OS == DE_OS_ANDROID)
#define DEQP_VULKAN_LIBRARY_SUFFIX ".so"
#else
#define DEQP_VULKAN_LIBRARY_SUFFIX ".so.1"
#endif
#define DEQP_VULKAN_LIBRARY_PATH DEQP_VULKAN_LIBRARY_BASENAME DEQP_VULKAN_LIBRARY_SUFFIX
#endif

namespace tcu
{
namespace surfaceless
{

class VulkanLibrary : public vk::Library
{
public:
    VulkanLibrary(const char *libraryPath)
        : m_library(libraryPath != nullptr ? libraryPath : DEQP_VULKAN_LIBRARY_PATH)
        , m_driver(m_library)
    {
    }

    const vk::PlatformInterface &getPlatformInterface(void) const
    {
        return m_driver;
    }
    const tcu::FunctionLibrary &getFunctionLibrary(void) const
    {
        return m_library;
    }

private:
    const tcu::DynamicFunctionLibrary m_library;
    const vk::PlatformDriver m_driver;
};

// Copied from tcuX11Platform.cpp
class VulkanPlatform : public vk::Platform
{
public:
    vk::Library *createLibrary(const char *libraryPath) const
    {
        return new VulkanLibrary(libraryPath);
    }

    void describePlatform(std::ostream &dst) const
    {
        utsname sysInfo;

        deMemset(&sysInfo, 0, sizeof(sysInfo));

        if (uname(&sysInfo) != 0)
            throw std::runtime_error("uname() failed");

        dst << "OS: " << sysInfo.sysname << " " << sysInfo.release << " " << sysInfo.version << "\n";
        dst << "CPU: " << sysInfo.machine << "\n";
    }
};

bool isEGLExtensionSupported(const eglw::Library &egl, eglw::EGLDisplay, const std::string &extName)
{
    const vector<string> exts = eglu::getClientExtensions(egl);
    return de::contains(exts.begin(), exts.end(), extName);
}

class GetProcFuncLoader : public glw::FunctionLoader
{
public:
    GetProcFuncLoader(const eglw::Library &egl) : m_egl(egl)
    {
    }

    glw::GenericFuncType get(const char *name) const
    {
        return (glw::GenericFuncType)m_egl.getProcAddress(name);
    }

protected:
    const eglw::Library &m_egl;
};

class DynamicFuncLoader : public glw::FunctionLoader
{
public:
    DynamicFuncLoader(de::DynamicLibrary *library) : m_library(library)
    {
    }

    glw::GenericFuncType get(const char *name) const
    {
        return (glw::GenericFuncType)m_library->getFunction(name);
    }

private:
    de::DynamicLibrary *m_library;
};

class Platform : public tcu::Platform, public glu::Platform
{
public:
    Platform(void);
    const glu::Platform &getGLPlatform(void) const
    {
        return *this;
    }
    const vk::Platform &getVulkanPlatform(void) const
    {
        return m_vkPlatform;
    }

private:
    VulkanPlatform m_vkPlatform;
};

class ContextFactory : public glu::ContextFactory
{
public:
    ContextFactory(void);
    glu::RenderContext *createContext(const glu::RenderConfig &config, const tcu::CommandLine &,
                                      const glu::RenderContext *) const;
};

class EglRenderContext : public glu::RenderContext
{
public:
    EglRenderContext(const glu::RenderConfig &config, const tcu::CommandLine &cmdLine,
                     const glu::RenderContext *sharedContext);
    ~EglRenderContext(void);

    glu::ContextType getType(void) const
    {
        return m_contextType;
    }
    eglw::EGLContext getEglContext(void) const
    {
        return m_eglContext;
    }
    const glw::Functions &getFunctions(void) const
    {
        return m_glFunctions;
    }
    const tcu::RenderTarget &getRenderTarget(void) const;
    void postIterate(void);
    virtual void makeCurrent(void);

    virtual glw::GenericFuncType getProcAddress(const char *name) const
    {
        return m_egl.getProcAddress(name);
    }

private:
    const eglw::DefaultLibrary m_egl;
    const glu::ContextType m_contextType;
    eglw::EGLDisplay m_eglDisplay;
    eglw::EGLContext m_eglContext;
    eglw::EGLSurface m_eglSurface;
    de::DynamicLibrary *m_glLibrary;
    glw::Functions m_glFunctions;
    tcu::RenderTarget m_renderTarget;
    eglw::EGLContext m_sharedEglContext;
};

Platform::Platform(void)
{
    m_contextFactoryRegistry.registerFactory(new ContextFactory());
}

ContextFactory::ContextFactory() : glu::ContextFactory("default", "EGL surfaceless context")
{
}

glu::RenderContext *ContextFactory::createContext(const glu::RenderConfig &config, const tcu::CommandLine &cmdLine,
                                                  const glu::RenderContext *sharedContext) const
{
    return new EglRenderContext(config, cmdLine, sharedContext);
}

EglRenderContext::EglRenderContext(const glu::RenderConfig &config, const tcu::CommandLine &cmdLine,
                                   const glu::RenderContext *sharedContext)
    : m_egl("libEGL.so")
    , m_contextType(config.type)
    , m_eglDisplay(EGL_NO_DISPLAY)
    , m_eglContext(EGL_NO_CONTEXT)
    , m_renderTarget(config.width, config.height,
                     tcu::PixelFormat(config.redBits, config.greenBits, config.blueBits, config.alphaBits),
                     config.depthBits, config.stencilBits, config.numSamples)

{
    vector<eglw::EGLint> context_attribs;
    vector<eglw::EGLint> frame_buffer_attribs;
    vector<eglw::EGLint> surface_attribs;

    const glu::ContextType &contextType = config.type;
    eglw::EGLint eglMajorVersion;
    eglw::EGLint eglMinorVersion;
    eglw::EGLint flags = 0;
    eglw::EGLint num_configs;
    eglw::EGLConfig egl_config = NULL;

    (void)cmdLine;

    m_eglDisplay = m_egl.getDisplay(NULL);
    EGLU_CHECK_MSG(m_egl, "eglGetDisplay()");
    if (m_eglDisplay == EGL_NO_DISPLAY)
        throw tcu::ResourceError("eglGetDisplay() failed");

    EGLU_CHECK_CALL(m_egl, initialize(m_eglDisplay, &eglMajorVersion, &eglMinorVersion));

    frame_buffer_attribs.push_back(EGL_RENDERABLE_TYPE);
    switch (contextType.getMajorVersion())
    {
    case 3:
        frame_buffer_attribs.push_back(EGL_OPENGL_ES3_BIT);
        break;
    case 2:
        frame_buffer_attribs.push_back(EGL_OPENGL_ES2_BIT);
        break;
    default:
        frame_buffer_attribs.push_back(EGL_OPENGL_ES_BIT);
    }

    frame_buffer_attribs.push_back(EGL_SURFACE_TYPE);
    switch (config.surfaceType)
    {
    case glu::RenderConfig::SURFACETYPE_DONT_CARE:
        frame_buffer_attribs.push_back(EGL_DONT_CARE);
        break;
    case glu::RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE:
    case glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC:
        frame_buffer_attribs.push_back(EGL_PBUFFER_BIT);
        surface_attribs.push_back(EGL_WIDTH);
        surface_attribs.push_back(config.width);
        surface_attribs.push_back(EGL_HEIGHT);
        surface_attribs.push_back(config.height);
        break;
    case glu::RenderConfig::SURFACETYPE_WINDOW:
        throw tcu::NotSupportedError("surfaceless platform does not support --deqp-surface-type=window");
    case glu::RenderConfig::SURFACETYPE_LAST:
        TCU_CHECK_INTERNAL(false);
    }

    surface_attribs.push_back(EGL_NONE);

    frame_buffer_attribs.push_back(EGL_RED_SIZE);
    frame_buffer_attribs.push_back(config.redBits);

    frame_buffer_attribs.push_back(EGL_GREEN_SIZE);
    frame_buffer_attribs.push_back(config.greenBits);

    frame_buffer_attribs.push_back(EGL_BLUE_SIZE);
    frame_buffer_attribs.push_back(config.blueBits);

    frame_buffer_attribs.push_back(EGL_ALPHA_SIZE);
    frame_buffer_attribs.push_back(config.alphaBits);

    frame_buffer_attribs.push_back(EGL_DEPTH_SIZE);
    frame_buffer_attribs.push_back(config.depthBits);

    frame_buffer_attribs.push_back(EGL_STENCIL_SIZE);
    frame_buffer_attribs.push_back(config.stencilBits);

    frame_buffer_attribs.push_back(EGL_SAMPLES);
    frame_buffer_attribs.push_back(config.numSamples);

    frame_buffer_attribs.push_back(EGL_NONE);

    if (!eglChooseConfig(m_eglDisplay, &frame_buffer_attribs[0], NULL, 0, &num_configs))
        throw tcu::ResourceError("surfaceless couldn't find any config");

    eglw::EGLConfig all_configs[num_configs];

    if (!eglChooseConfig(m_eglDisplay, &frame_buffer_attribs[0], all_configs, num_configs, &num_configs))
        throw tcu::ResourceError("surfaceless couldn't find any config");

    for (int i = 0; i < num_configs; i++)
    {
        EGLint red, green, blue, alpha, depth, stencil, samples;
        eglGetConfigAttrib(m_eglDisplay, all_configs[i], EGL_RED_SIZE, &red);
        eglGetConfigAttrib(m_eglDisplay, all_configs[i], EGL_GREEN_SIZE, &green);
        eglGetConfigAttrib(m_eglDisplay, all_configs[i], EGL_BLUE_SIZE, &blue);
        eglGetConfigAttrib(m_eglDisplay, all_configs[i], EGL_ALPHA_SIZE, &alpha);
        eglGetConfigAttrib(m_eglDisplay, all_configs[i], EGL_DEPTH_SIZE, &depth);
        eglGetConfigAttrib(m_eglDisplay, all_configs[i], EGL_STENCIL_SIZE, &stencil);
        eglGetConfigAttrib(m_eglDisplay, all_configs[i], EGL_SAMPLES, &samples);

        if ((glu::RenderConfig::DONT_CARE == config.redBits || red == config.redBits) &&
            (glu::RenderConfig::DONT_CARE == config.greenBits || green == config.greenBits) &&
            (glu::RenderConfig::DONT_CARE == config.blueBits || blue == config.blueBits) &&
            (glu::RenderConfig::DONT_CARE == config.alphaBits || alpha == config.alphaBits) &&
            (glu::RenderConfig::DONT_CARE == config.depthBits || depth == config.depthBits) &&
            (glu::RenderConfig::DONT_CARE == config.stencilBits || stencil == config.stencilBits) &&
            (glu::RenderConfig::DONT_CARE == config.numSamples || samples == config.numSamples))
        {
            egl_config = all_configs[i];
            break;
        }
    }

    if (!egl_config)
        throw tcu::ResourceError("surfaceless couldn't find a matching config");

    switch (config.surfaceType)
    {
    case glu::RenderConfig::SURFACETYPE_DONT_CARE:
        m_eglSurface = EGL_NO_SURFACE;
        break;
    case glu::RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE:
    case glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC:
        m_eglSurface = eglCreatePbufferSurface(m_eglDisplay, egl_config, &surface_attribs[0]);
        break;
    case glu::RenderConfig::SURFACETYPE_WINDOW:
    case glu::RenderConfig::SURFACETYPE_LAST:
        TCU_CHECK_INTERNAL(false);
    }

    context_attribs.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
    context_attribs.push_back(contextType.getMajorVersion());
    context_attribs.push_back(EGL_CONTEXT_MINOR_VERSION_KHR);
    context_attribs.push_back(contextType.getMinorVersion());

    switch (contextType.getProfile())
    {
    case glu::PROFILE_ES:
        EGLU_CHECK_CALL(m_egl, bindAPI(EGL_OPENGL_ES_API));
        break;
    case glu::PROFILE_CORE:
        EGLU_CHECK_CALL(m_egl, bindAPI(EGL_OPENGL_API));
        context_attribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
        context_attribs.push_back(EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR);
        break;
    case glu::PROFILE_COMPATIBILITY:
        EGLU_CHECK_CALL(m_egl, bindAPI(EGL_OPENGL_API));
        context_attribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
        context_attribs.push_back(EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR);
        break;
    case glu::PROFILE_LAST:
        TCU_CHECK_INTERNAL(false);
    }

    if ((contextType.getFlags() & glu::CONTEXT_DEBUG) != 0)
        flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;

    if ((contextType.getFlags() & glu::CONTEXT_ROBUST) != 0)
        flags |= EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR;

    if ((contextType.getFlags() & glu::CONTEXT_FORWARD_COMPATIBLE) != 0)
        flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

    context_attribs.push_back(EGL_CONTEXT_FLAGS_KHR);
    context_attribs.push_back(flags);

    context_attribs.push_back(EGL_NONE);

    const EglRenderContext *sharedEglRenderContext = dynamic_cast<const EglRenderContext *>(sharedContext);
    eglw::EGLContext sharedEglContext =
        sharedEglRenderContext ? sharedEglRenderContext->getEglContext() : EGL_NO_CONTEXT;
    m_sharedEglContext = sharedEglContext;

    m_eglContext = m_egl.createContext(m_eglDisplay, egl_config, sharedEglContext, &context_attribs[0]);
    EGLU_CHECK_MSG(m_egl, "eglCreateContext()");
    if (!m_eglContext)
        throw tcu::ResourceError("eglCreateContext failed");

    EGLU_CHECK_CALL(m_egl, makeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext));

    if ((eglMajorVersion == 1 && eglMinorVersion >= 5) ||
        isEGLExtensionSupported(m_egl, m_eglDisplay, "EGL_KHR_get_all_proc_addresses") ||
        isEGLExtensionSupported(m_egl, EGL_NO_DISPLAY, "EGL_KHR_client_get_all_proc_addresses"))
    {
        // Use eglGetProcAddress() for core functions
        GetProcFuncLoader funcLoader(m_egl);
        glu::initCoreFunctions(&m_glFunctions, &funcLoader, contextType.getAPI());
    }
#if !defined(DEQP_GLES2_RUNTIME_LOAD)
    else if (contextType.getAPI() == glu::ApiType::es(2, 0))
    {
        glw::initES20Direct(&m_glFunctions);
    }
#endif
#if !defined(DEQP_GLES3_RUNTIME_LOAD)
    else if (contextType.getAPI() == glu::ApiType::es(3, 0))
    {
        glw::initES30Direct(&m_glFunctions);
    }
#endif
    else
    {
        const char *libraryPath = NULL;

        if (glu::isContextTypeES(contextType))
        {
            if (contextType.getMinorVersion() <= 2)
                libraryPath = DEQP_GLES2_LIBRARY_PATH;
            else
                libraryPath = DEQP_GLES3_LIBRARY_PATH;
        }
        else
        {
            libraryPath = DEQP_OPENGL_LIBRARY_PATH;
        }

        m_glLibrary = new de::DynamicLibrary(libraryPath);

        DynamicFuncLoader funcLoader(m_glLibrary);
        glu::initCoreFunctions(&m_glFunctions, &funcLoader, contextType.getAPI());
    }

    {
        GetProcFuncLoader extLoader(m_egl);
        glu::initExtensionFunctions(&m_glFunctions, &extLoader, contextType.getAPI());
    }
}

EglRenderContext::~EglRenderContext(void)
{
    try
    {
        if (m_eglDisplay != EGL_NO_DISPLAY)
        {
            EGLU_CHECK_CALL(m_egl, makeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

            if (m_eglContext != EGL_NO_CONTEXT)
                EGLU_CHECK_CALL(m_egl, destroyContext(m_eglDisplay, m_eglContext));
        }

        if (!m_sharedEglContext)
        {
            EGLU_CHECK_CALL(m_egl, terminate(m_eglDisplay));
        }
    }
    catch (...)
    {
    }
}

const tcu::RenderTarget &EglRenderContext::getRenderTarget(void) const
{
    return m_renderTarget;
}

void EglRenderContext::makeCurrent(void)
{
    EGLU_CHECK_CALL(m_egl, makeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext));
}

void EglRenderContext::postIterate(void)
{
    this->getFunctions().finish();
}

} // namespace surfaceless
} // namespace tcu

tcu::Platform *createPlatform(void)
{
    return new tcu::surfaceless::Platform();
}
