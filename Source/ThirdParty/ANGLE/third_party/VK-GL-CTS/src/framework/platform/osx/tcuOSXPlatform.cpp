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
 * \brief OS X platform.
 *//*--------------------------------------------------------------------*/

#include "tcuOSXPlatform.hpp"
#include "tcuRenderTarget.hpp"

#include "tcuOSXVulkanPlatform.hpp"

#include "gluDefs.hpp"
#include "gluPlatform.hpp"
#include "gluRenderContext.hpp"
#include "gluRenderConfig.hpp"
#include "glwFunctions.hpp"
#include "glwInitFunctions.hpp"
#include "deDynamicLibrary.hpp"
#include "glwEnums.hpp"

#include <string>

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/CGLContext.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLRenderers.h>

#define OPENGL_LIBRARY_PATH "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"

namespace tcu
{

class CGLRenderContext : public glu::RenderContext
{
public:
    CGLRenderContext(const glu::RenderConfig &config);
    ~CGLRenderContext(void);

    glu::ContextType getType(void) const
    {
        return m_type;
    }
    const glw::Functions &getFunctions(void) const
    {
        return m_functions;
    }
    const tcu::RenderTarget &getRenderTarget(void) const
    {
        return m_renderTarget;
    }
    void postIterate(void)
    {
    }

private:
    const glu::ContextType m_type;
    CGLContextObj m_context;
    glw::Functions m_functions;
    RenderTarget m_renderTarget;
};

class CGLContextFactory : public glu::ContextFactory
{
public:
    CGLContextFactory(void) : glu::ContextFactory("cgl", "CGL Context (surfaceless, use fbo)")
    {
    }

    glu::RenderContext *createContext(const glu::RenderConfig &config, const tcu::CommandLine &,
                                      const glu::RenderContext *) const
    {
        return new CGLRenderContext(config);
    }
};

class OSXGLPlatform : public glu::Platform
{
public:
    OSXGLPlatform(void)
    {
        m_contextFactoryRegistry.registerFactory(new CGLContextFactory());
    }

    ~OSXGLPlatform(void)
    {
    }
};

class OSXPlatform : public tcu::Platform
{
public:
    OSXPlatform(void) : m_gluPlatform(), m_vkPlatform()
    {
    }

    ~OSXPlatform(void)
    {
    }

    const glu::Platform &getGLPlatform(void) const
    {
        return m_gluPlatform;
    }
    const vk::Platform &getVulkanPlatform(void) const
    {
        return m_vkPlatform;
    }

private:
    OSXGLPlatform m_gluPlatform;
    osx::VulkanPlatform m_vkPlatform;
};

namespace
{

class GLFunctionLoader : public glw::FunctionLoader
{
public:
    GLFunctionLoader(const char *path) : m_library(path)
    {
    }

    glw::GenericFuncType get(const char *name) const
    {
        return m_library.getFunction(name);
    }

private:
    de::DynamicLibrary m_library;
};

} // namespace

static CGLOpenGLProfile getCGLProfile(glu::ContextType type)
{
    if (type.getAPI().getProfile() != glu::PROFILE_CORE)
        throw NotSupportedError("Requested OpenGL profile is not supported in CGL");

    if (type.getAPI().getMajorVersion() == 4)
        return kCGLOGLPVersion_GL4_Core;
    else if (type.getAPI().getMajorVersion() == 3)
        return kCGLOGLPVersion_GL3_Core;
    else
        throw NotSupportedError("Requested OpenGL version is not supported in CGL");
}

static glu::ApiType getVersion(const glw::Functions &gl)
{
    int major = 0;
    int minor = 0;
    gl.getIntegerv(GL_MAJOR_VERSION, &major);
    gl.getIntegerv(GL_MINOR_VERSION, &minor);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to query exact GL version");
    return glu::ApiType::core(major, minor);
}

CGLRenderContext::CGLRenderContext(const glu::RenderConfig &config)
    : m_type(config.type)
    , m_context(nullptr)
    , m_renderTarget(0, 0, tcu::PixelFormat(0, 0, 0, 0), 0, 0, 0)
{
    try
    {
        const CGLPixelFormatAttribute attribs[] = {kCGLPFAAccelerated, kCGLPFAOpenGLProfile,
                                                   (CGLPixelFormatAttribute)getCGLProfile(config.type),
                                                   (CGLPixelFormatAttribute)0};

        CGLPixelFormatObj pixelFormat;
        int numVScreens;

        if (CGLChoosePixelFormat(&attribs[0], &pixelFormat, &numVScreens) != kCGLNoError)
            throw NotSupportedError("No compatible pixel formats found");

        try
        {
            if (CGLCreateContext(pixelFormat, nullptr, &m_context) != kCGLNoError)
                throw ResourceError("Failed to create CGL context");

            if (CGLSetCurrentContext(m_context) != kCGLNoError)
                throw ResourceError("Failed to set current CGL context");
        }
        catch (...)
        {
            CGLReleasePixelFormat(pixelFormat);
            throw;
        }

        CGLReleasePixelFormat(pixelFormat);

        {
            GLFunctionLoader loader(OPENGL_LIBRARY_PATH);
            glu::initFunctions(&m_functions, &loader, config.type.getAPI());
        }

        {
            const glu::ApiType actualApi = getVersion(m_functions);
            if (!contextSupports(glu::ContextType(actualApi, glu::ContextFlags(0)), config.type.getAPI()))
                throw tcu::NotSupportedError("OpenGL version not supported");
        }
    }
    catch (...)
    {
        if (m_context)
        {
            CGLSetCurrentContext(nullptr);
            CGLDestroyContext(m_context);
        }
        throw;
    }
}

CGLRenderContext::~CGLRenderContext(void)
{
    CGLSetCurrentContext(nullptr);
    if (m_context)
        CGLDestroyContext(m_context);
}

} // namespace tcu

tcu::Platform *createPlatform(void)
{
    return new tcu::OSXPlatform();
}
