#ifndef _TCUIOSPLATFORM_HPP
#define _TCUIOSPLATFORM_HPP
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

#include "tcuDefs.hpp"
#include "tcuPlatform.hpp"
#include "gluPlatform.hpp"
#include "gluRenderContext.hpp"
#include "gluContextFactory.hpp"
#include "gluObjectWrapper.hpp"
#include "tcuRenderTarget.hpp"
#include "glwFunctions.hpp"
#include "deMutex.hpp"

#import "tcuEAGLView.h"

#import <OpenGLES/EAGL.h>

namespace tcu
{
namespace ios
{

class ScreenManager
{
public:
    ScreenManager(tcuEAGLView *view);
    ~ScreenManager(void);

    CAEAGLLayer *acquireScreen(void);
    void releaseScreen(CAEAGLLayer *layer);

private:
    ScreenManager(const ScreenManager &);
    ScreenManager &operator=(const ScreenManager &);

    tcuEAGLView *m_view;
    de::Mutex m_viewLock;
};

class ContextFactory : public glu::ContextFactory
{
public:
    ContextFactory(ScreenManager *screenManager);
    ~ContextFactory(void);

    glu::RenderContext *createContext(const glu::RenderConfig &config, const tcu::CommandLine &cmdLine) const;

private:
    ScreenManager *const m_screenManager;
};

class Platform : public tcu::Platform, private glu::Platform
{
public:
    Platform(ScreenManager *screenManager);
    virtual ~Platform(void);

    const glu::Platform &getGLPlatform(void) const
    {
        return static_cast<const glu::Platform &>(*this);
    }
};

//! EAGLContext-backed rendering context. Doesn't have default framebuffer.
class RawContext : public glu::RenderContext
{
public:
    RawContext(glu::ContextType type);
    virtual ~RawContext(void);

    virtual glu::ContextType getType(void) const
    {
        return m_type;
    }
    virtual const glw::Functions &getFunctions(void) const
    {
        return m_functions;
    }
    virtual const RenderTarget &getRenderTarget(void) const
    {
        return m_emptyTarget;
    }
    virtual uint32_t getDefaultFramebuffer(void) const
    {
        DE_FATAL("No framebuffer");
        return 0;
    }
    virtual void postIterate(void);

protected:
    EAGLContext *getEAGLContext(void) const
    {
        return m_context;
    }

private:
    glu::ContextType m_type;
    EAGLContext *m_context;
    glw::Functions m_functions;
    tcu::RenderTarget m_emptyTarget;
};

class ScreenContext : public RawContext
{
public:
    ScreenContext(ScreenManager *screenManager, const glu::RenderConfig &config);
    ~ScreenContext(void);

    virtual const RenderTarget &getRenderTarget(void) const
    {
        return m_renderTarget;
    }
    virtual uint32_t getDefaultFramebuffer(void) const
    {
        return *m_framebuffer;
    }
    virtual void postIterate(void);

private:
    void createFramebuffer(const glu::RenderConfig &config);

    ScreenManager *m_screenManager;
    CAEAGLLayer *m_layer;

    glu::Framebuffer m_framebuffer;
    glu::Renderbuffer m_colorBuffer;
    glu::Renderbuffer m_depthStencilBuffer;
    tcu::RenderTarget m_renderTarget;
};

} // namespace ios
} // namespace tcu

#endif // _TCUIOSPLATFORM_H
