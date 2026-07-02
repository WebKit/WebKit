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
 * \brief Platform that uses X11 via GLX.
 *//*--------------------------------------------------------------------*/

#include "tcuLnxX11GlxPlatform.hpp"

#include "tcuRenderTarget.hpp"
#include "glwInitFunctions.hpp"
#include "deUniquePtr.hpp"
#include "glwEnums.hpp"

#include <sstream>
#include <iterator>
#include <set>

#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>

#ifndef GLX_CONTEXT_OPENGL_NO_ERROR_ARB
#define GLX_CONTEXT_OPENGL_NO_ERROR_ARB 0x31B3
#endif

#ifndef PFNGLXSWAPINTERVALMESAPROC
#define PFNGLXSWAPINTERVALMESAPROC PFNGLXSWAPINTERVALSGIPROC
#endif

namespace tcu
{
namespace lnx
{
namespace x11
{
namespace glx
{

using de::MovePtr;
using de::UniquePtr;
using glu::ApiType;
using glu::ContextFactory;
using glu::ContextType;
using glu::RenderConfig;
using glu::RenderContext;
using std::istream_iterator;
using std::istringstream;
using std::ostringstream;
using std::set;
using std::string;
using tcu::CommandLine;
using tcu::RenderTarget;

typedef RenderConfig::Visibility Visibility;

template <typename T>
static inline T checkGLX(T value, const char *expr, const char *file, int line)
{
    if (!value)
        throw tcu::TestError("GLX call failed", expr, file, line);
    return value;
}

#define TCU_CHECK_GLX(EXPR) checkGLX(EXPR, #EXPR, __FILE__, __LINE__)
#define TCU_CHECK_GLX_CONFIG(EXPR) checkGLX((EXPR) == Success, #EXPR, __FILE__, __LINE__)

class GlxContextFactory : public glu::ContextFactory
{
public:
    GlxContextFactory(EventState &eventState);
    ~GlxContextFactory(void);
    RenderContext *createContext(const RenderConfig &config, const CommandLine &cmdLine,
                                 const glu::RenderContext *sharedContext) const;

    EventState &getEventState(void) const
    {
        return m_eventState;
    }

    const PFNGLXCREATECONTEXTATTRIBSARBPROC m_glXCreateContextAttribsARB;

private:
    EventState &m_eventState;
};

class GlxDisplay : public XlibDisplay
{
public:
    GlxDisplay(EventState &eventState, const char *name);
    int getGlxMajorVersion(void) const
    {
        return m_majorVersion;
    }
    int getGlxMinorVersion(void) const
    {
        return m_minorVersion;
    }
    bool isGlxExtensionSupported(const char *extName) const;

private:
    int m_errorBase;
    int m_eventBase;
    int m_majorVersion;
    int m_minorVersion;
    set<string> m_extensions;
};

class GlxVisual
{
public:
    GlxVisual(GlxDisplay &display, GLXFBConfig fbConfig);
    int getAttrib(int attribute);
    Visual *getXVisual(void)
    {
        return m_visual;
    }
    GLXContext createContext(const GlxContextFactory &factory, const ContextType &contextType,
                             const glu::RenderContext *sharedContext,
                             glu::ResetNotificationStrategy resetNotificationStrategy);
    GLXWindow createWindow(::Window xWindow);
    GlxDisplay &getGlxDisplay(void)
    {
        return m_display;
    }
    ::Display *getXDisplay(void)
    {
        return m_display.getXDisplay();
    }

private:
    GlxDisplay &m_display;
    ::Visual *m_visual;
    const GLXFBConfig m_fbConfig;
    glu::ResetNotificationStrategy resetStrategy;
};

class GlxDrawable
{
public:
    virtual ~GlxDrawable(void)
    {
    }

    virtual void processEvents(void)
    {
    }
    virtual void getDimensions(int *width, int *height) = 0;
    int getWidth(void);
    int getHeight(void);
    void swapBuffers(void)
    {
        glXSwapBuffers(getXDisplay(), getGLXDrawable());
    }

    virtual ::Display *getXDisplay(void)     = 0;
    virtual GLXDrawable getGLXDrawable(void) = 0;

protected:
    GlxDrawable()
    {
    }
    unsigned int getAttrib(int attribute);
};

class GlxWindow : public GlxDrawable
{
public:
    GlxWindow(GlxVisual &visual, const RenderConfig &cfg);
    ~GlxWindow(void);
    void processEvents(void)
    {
        m_x11Window.processEvents();
    }
    ::Display *getXDisplay(void)
    {
        return m_x11Display.getXDisplay();
    }
    void getDimensions(int *width, int *height);

protected:
    GLXDrawable getGLXDrawable()
    {
        return m_GLXDrawable;
    }

private:
    XlibDisplay &m_x11Display;
    XlibWindow m_x11Window;
    const GLXDrawable m_GLXDrawable;
};

class GlxRenderContext : public RenderContext
{
public:
    GlxRenderContext(const GlxContextFactory &factory, const RenderConfig &config,
                     const glu::RenderContext *sharedContext);
    ~GlxRenderContext(void);
    virtual ContextType getType(void) const;
    virtual void postIterate(void);
    virtual void makeCurrent(void);
    void clearCurrent(void);
    void swapInterval(int interval);
    virtual const glw::Functions &getFunctions(void) const;
    virtual const tcu::RenderTarget &getRenderTarget(void) const;
    virtual glw::GenericFuncType getProcAddress(const char *name) const;
    const GLXContext &getGLXContext(void) const;
    const GlxVisual &getGLXVisual(void) const;

private:
    GlxDisplay m_glxDisplay;
    GlxVisual m_glxVisual;
    ContextType m_type;
    GLXContext m_GLXContext;
    UniquePtr<GlxDrawable> m_glxDrawable;
    RenderTarget m_renderTarget;
    glw::Functions m_functions;
};

extern "C"
{
    static int tcuLnxX11GlxErrorHandler(::Display *display, XErrorEvent *event)
    {
        char buf[80];
        XGetErrorText(display, event->error_code, buf, sizeof(buf));
        tcu::print("X operation %u:%u failed: %s\n", event->request_code, event->minor_code, buf);
        return 0;
    }
}

GlxContextFactory::GlxContextFactory(EventState &eventState)
    : glu::ContextFactory("glx", "X11 GLX OpenGL Context")
    , m_glXCreateContextAttribsARB(reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(
          TCU_CHECK_GLX(glXGetProcAddress(reinterpret_cast<const GLubyte *>("glXCreateContextAttribsARB")))))
    , m_eventState(eventState)
{
    XSetErrorHandler(tcuLnxX11GlxErrorHandler);
}

RenderContext *GlxContextFactory::createContext(const RenderConfig &config, const CommandLine &cmdLine,
                                                const glu::RenderContext *sharedContext) const
{
    DE_UNREF(cmdLine);
    GlxRenderContext *const renderContext = new GlxRenderContext(*this, config, sharedContext);
    return renderContext;
}

GlxContextFactory::~GlxContextFactory(void)
{
}

GlxDisplay::GlxDisplay(EventState &eventState, const char *name) : XlibDisplay(eventState, name)
{
    const Bool supported = glXQueryExtension(m_display, &m_errorBase, &m_eventBase);
    if (!supported)
        TCU_THROW(NotSupportedError, "GLX protocol not supported by X server");

    TCU_CHECK_GLX(glXQueryVersion(m_display, &m_majorVersion, &m_minorVersion));

    {
        const int screen = XDefaultScreen(m_display);
        // nVidia doesn't seem to report client-side extensions correctly,
        // so use also server side
        const char *const server_extensions = TCU_CHECK_GLX(glXQueryServerString(m_display, screen, GLX_EXTENSIONS));
        const char *const client_extensions = TCU_CHECK_GLX(glXQueryExtensionsString(m_display, screen));
        istringstream srvExtStream(server_extensions);
        istringstream cliExtStream(client_extensions);
        m_extensions = set<string>(istream_iterator<string>(srvExtStream), istream_iterator<string>());
        m_extensions.insert(istream_iterator<string>(cliExtStream), istream_iterator<string>());
    }
}

bool GlxDisplay::isGlxExtensionSupported(const char *extName) const
{
    return m_extensions.find(extName) != m_extensions.end();
}

//! Throw `tcu::NotSupportedError` if `dpy` is not compatible with GLX
//! version `major`.`minor`.
static void checkGlxVersion(const GlxDisplay &dpy, int major, int minor)
{
    const int dpyMajor = dpy.getGlxMajorVersion();
    const int dpyMinor = dpy.getGlxMinorVersion();
    if (!(dpyMajor == major && dpyMinor >= minor))
    {
        ostringstream oss;
        oss << "Server GLX version " << dpyMajor << "." << dpyMinor << " not compatible with required version " << major
            << "." << minor;
        TCU_THROW(NotSupportedError, oss.str().c_str());
    }
}

//! Throw `tcu::NotSupportedError` if `dpy` does not support extension `extName`.
static void checkGlxExtension(const GlxDisplay &dpy, const char *extName)
{
    if (!dpy.isGlxExtensionSupported(extName))
    {
        ostringstream oss;
        oss << "GLX extension \"" << extName << "\" not supported";
        TCU_THROW(NotSupportedError, oss.str().c_str());
    }
}

GlxVisual::GlxVisual(GlxDisplay &display, GLXFBConfig fbConfig)
    : m_display(display)
    , m_visual(nullptr)
    , m_fbConfig(fbConfig)
{
    XVisualInfo *visualInfo = glXGetVisualFromFBConfig(getXDisplay(), fbConfig);

    if (!visualInfo)
        TCU_THROW(ResourceError, "glXGetVisualFromFBConfig() returned NULL");

    m_visual = visualInfo->visual;
    XFree(visualInfo);
}

int GlxVisual::getAttrib(int attribute)
{
    int fbvalue;
    TCU_CHECK_GLX_CONFIG(glXGetFBConfigAttrib(getXDisplay(), m_fbConfig, attribute, &fbvalue));
    return fbvalue;
}

GLXContext GlxVisual::createContext(const GlxContextFactory &factory, const ContextType &contextType,
                                    const glu::RenderContext *sharedContext,
                                    glu::ResetNotificationStrategy resetNotificationStrategy)
{
    std::vector<int> attribs;

    checkGlxVersion(m_display, 1, 4);
    checkGlxExtension(m_display, "GLX_ARB_create_context");
    checkGlxExtension(m_display, "GLX_ARB_create_context_profile");

    {
        const ApiType apiType = contextType.getAPI();
        int profileMask       = 0;

        switch (apiType.getProfile())
        {
        case glu::PROFILE_ES:
            checkGlxExtension(m_display, "GLX_EXT_create_context_es2_profile");
            profileMask = GLX_CONTEXT_ES2_PROFILE_BIT_EXT;
            break;
        case glu::PROFILE_CORE:
            profileMask = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
            break;
        case glu::PROFILE_COMPATIBILITY:
            profileMask = GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
            break;
        default:
            DE_FATAL("Impossible context profile");
        }

        attribs.push_back(GLX_CONTEXT_MAJOR_VERSION_ARB);
        attribs.push_back(apiType.getMajorVersion());
        attribs.push_back(GLX_CONTEXT_MINOR_VERSION_ARB);
        attribs.push_back(apiType.getMinorVersion());
        attribs.push_back(GLX_CONTEXT_PROFILE_MASK_ARB);
        attribs.push_back(profileMask);
    }

    // Context flags
    {
        int flags = 0;

        if ((contextType.getFlags() & glu::CONTEXT_FORWARD_COMPATIBLE) != 0)
        {
            if (glu::isContextTypeES(contextType))
                TCU_THROW(InternalError, "Only OpenGL core contexts can be forward-compatible");

            flags |= GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
        }

        if ((contextType.getFlags() & glu::CONTEXT_DEBUG) != 0)
            flags |= GLX_CONTEXT_DEBUG_BIT_ARB;

        if ((contextType.getFlags() & glu::CONTEXT_ROBUST) != 0)
            flags |= GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB;

        if ((contextType.getFlags() & glu::CONTEXT_NO_ERROR) != 0)
        {
            if (m_display.isGlxExtensionSupported("GLX_ARB_create_context_no_error"))
            {
                attribs.push_back(GLX_CONTEXT_OPENGL_NO_ERROR_ARB);
                attribs.push_back(True);
            }
            else
                TCU_THROW(NotSupportedError,
                          "GLX_ARB_create_context_no_error is required for creating no-error contexts");
        }

        if (flags != 0)
        {
            attribs.push_back(GLX_CONTEXT_FLAGS_ARB);
            attribs.push_back(flags);
        }
    }

    const GlxRenderContext *sharedGlxRenderContext = dynamic_cast<const GlxRenderContext *>(sharedContext);

    /* If there is a shared context, use same reset notification strategy. */
    glu::ResetNotificationStrategy usedResetNotificationStrategy =
        sharedGlxRenderContext ? sharedGlxRenderContext->getGLXVisual().resetStrategy : resetNotificationStrategy;

    if (usedResetNotificationStrategy != glu::RESET_NOTIFICATION_STRATEGY_NOT_SPECIFIED)
    {
        checkGlxExtension(m_display, "GLX_ARB_create_context_robustness");
        attribs.push_back(GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);

        if (usedResetNotificationStrategy == glu::RESET_NOTIFICATION_STRATEGY_NO_RESET_NOTIFICATION)
            attribs.push_back(GLX_NO_RESET_NOTIFICATION_ARB);
        else if (usedResetNotificationStrategy == glu::RESET_NOTIFICATION_STRATEGY_LOSE_CONTEXT_ON_RESET)
            attribs.push_back(GLX_LOSE_CONTEXT_ON_RESET_ARB);
        else
            TCU_THROW(InternalError, "Unknown reset notification strategy");
    }

    // Reset notification strategy used with this visual.
    resetStrategy = resetNotificationStrategy;

    // Terminate attrib list
    attribs.push_back(None);

    const GLXContext &sharedGLXContext = sharedGlxRenderContext ? sharedGlxRenderContext->getGLXContext() : nullptr;

    return TCU_CHECK_GLX(
        factory.m_glXCreateContextAttribsARB(getXDisplay(), m_fbConfig, sharedGLXContext, True, &attribs[0]));
}

GLXWindow GlxVisual::createWindow(::Window xWindow)
{
    return TCU_CHECK_GLX(glXCreateWindow(getXDisplay(), m_fbConfig, xWindow, NULL));
}

unsigned GlxDrawable::getAttrib(int attrib)
{
    unsigned int value = 0;
    glXQueryDrawable(getXDisplay(), getGLXDrawable(), attrib, &value);
    return value;
}

int GlxDrawable::getWidth(void)
{
    int width = 0;
    getDimensions(&width, nullptr);
    return width;
}

int GlxDrawable::getHeight(void)
{
    int height = 0;
    getDimensions(nullptr, &height);
    return height;
}

GlxWindow::GlxWindow(GlxVisual &visual, const RenderConfig &cfg)
    : m_x11Display(visual.getGlxDisplay())
    , m_x11Window(m_x11Display, cfg.width, cfg.height, visual.getXVisual())
    , m_GLXDrawable(visual.createWindow(m_x11Window.getXID()))
{
    m_x11Window.setVisibility(cfg.windowVisibility != RenderConfig::VISIBILITY_HIDDEN);
}

void GlxWindow::getDimensions(int *width, int *height)
{
    if (width != nullptr)
        *width = getAttrib(GLX_WIDTH);
    if (height != nullptr)
        *height = getAttrib(GLX_HEIGHT);

    // glXQueryDrawable may be buggy, so fall back to X geometry if needed
    if ((width != nullptr && *width == 0) || (height != nullptr && *height == 0))
        m_x11Window.getDimensions(width, height);
}

GlxWindow::~GlxWindow(void)
{
    glXDestroyWindow(m_x11Display.getXDisplay(), m_GLXDrawable);
}

static const struct Attribute
{
    int glxAttribute;
    int RenderConfig::*cfgMember;
} s_attribs[] = {
    {GLX_RED_SIZE, &RenderConfig::redBits},     {GLX_GREEN_SIZE, &RenderConfig::greenBits},
    {GLX_BLUE_SIZE, &RenderConfig::blueBits},   {GLX_ALPHA_SIZE, &RenderConfig::alphaBits},
    {GLX_DEPTH_SIZE, &RenderConfig::depthBits}, {GLX_STENCIL_SIZE, &RenderConfig::stencilBits},
    {GLX_SAMPLES, &RenderConfig::numSamples},   {GLX_FBCONFIG_ID, &RenderConfig::id},
};

static uint32_t surfaceTypeToDrawableBits(RenderConfig::SurfaceType type)
{
    switch (type)
    {
    case RenderConfig::SURFACETYPE_WINDOW:
        return GLX_WINDOW_BIT;
    case RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE:
        return GLX_PIXMAP_BIT;
    case RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC:
        return GLX_PBUFFER_BIT;
    case RenderConfig::SURFACETYPE_DONT_CARE:
        return GLX_WINDOW_BIT | GLX_PIXMAP_BIT | GLX_PBUFFER_BIT;
    default:
        DE_FATAL("Impossible case");
    }
    return 0;
}

static bool configMatches(GlxVisual &visual, const RenderConfig &renderCfg)
{
    if (renderCfg.id != RenderConfig::DONT_CARE)
        return visual.getAttrib(GLX_FBCONFIG_ID) == renderCfg.id;

    for (const Attribute *it = DE_ARRAY_BEGIN(s_attribs); it != DE_ARRAY_END(s_attribs); it++)
    {
        const int requested = renderCfg.*it->cfgMember;
        if (requested != RenderConfig::DONT_CARE && requested != visual.getAttrib(it->glxAttribute))
            return false;
    }

    {
        uint32_t bits = surfaceTypeToDrawableBits(renderCfg.surfaceType);

        if ((visual.getAttrib(GLX_DRAWABLE_TYPE) & bits) == 0)
            return false;

        // It shouldn't be possible to have GLX_WINDOW_BIT set without a visual,
        // but let's make sure.
        if (renderCfg.surfaceType == RenderConfig::SURFACETYPE_WINDOW && visual.getXVisual() == nullptr)
            return false;
    }

    return true;
}

class Rank
{
public:
    Rank(void) : m_value(0), m_bitsLeft(64)
    {
    }
    void add(size_t bits, uint32_t value);
    void sub(size_t bits, uint32_t value);
    uint64_t getValue(void)
    {
        return m_value;
    }

private:
    uint64_t m_value;
    size_t m_bitsLeft;
};

void Rank::add(size_t bits, uint32_t value)
{
    TCU_CHECK_INTERNAL(m_bitsLeft >= bits);
    m_bitsLeft -= bits;
    m_value = m_value << bits | de::min((1U << bits) - 1, value);
}

void Rank::sub(size_t bits, uint32_t value)
{
    TCU_CHECK_INTERNAL(m_bitsLeft >= bits);
    m_bitsLeft -= bits;
    m_value = m_value << bits | ((1U << bits) - 1 - de::min((1U << bits) - 1U, value));
}

static uint64_t configRank(GlxVisual &visual)
{
    // Quick checks.
    if (visual.getAttrib(GLX_DOUBLEBUFFER) == False || (visual.getAttrib(GLX_RENDER_TYPE) & GLX_RGBA_BIT) == 0)
        return 0;

    Rank rank;
    int caveat      = visual.getAttrib(GLX_CONFIG_CAVEAT);
    int redSize     = visual.getAttrib(GLX_RED_SIZE);
    int greenSize   = visual.getAttrib(GLX_GREEN_SIZE);
    int blueSize    = visual.getAttrib(GLX_BLUE_SIZE);
    int alphaSize   = visual.getAttrib(GLX_ALPHA_SIZE);
    int depthSize   = visual.getAttrib(GLX_DEPTH_SIZE);
    int stencilSize = visual.getAttrib(GLX_STENCIL_SIZE);
    int minRGB      = de::min(redSize, de::min(greenSize, blueSize));

    // Prefer conformant configurations.
    rank.add(1, (caveat != GLX_NON_CONFORMANT_CONFIG));

    // Prefer non-transparent configurations.
    rank.add(1, visual.getAttrib(GLX_TRANSPARENT_TYPE) == GLX_NONE);

    // Avoid stereo
    rank.add(1, visual.getAttrib(GLX_STEREO) == False);

    // Avoid overlays
    rank.add(1, visual.getAttrib(GLX_LEVEL) == 0);

    // Prefer to have some alpha.
    rank.add(1, alphaSize > 0);

    // Prefer to have a depth buffer.
    rank.add(1, depthSize > 0);

    // Prefer to have a stencil buffer.
    rank.add(1, stencilSize > 0);

    // Avoid slow configurations.
    rank.add(1, (caveat != GLX_SLOW_CONFIG));

    // Prefer larger, evenly distributed color depths
    rank.add(4, de::min(minRGB, alphaSize));

    // If alpha is low, choose best RGB
    rank.add(4, minRGB);

    // Prefer larger depth and stencil buffers
    rank.add(6, uint32_t(depthSize + stencilSize));

    // Avoid excessive sampling
    rank.sub(5, visual.getAttrib(GLX_SAMPLES));

    // Prefer True/DirectColor
    int visualType = visual.getAttrib(GLX_X_VISUAL_TYPE);
    rank.add(1, visualType == GLX_TRUE_COLOR || visualType == GLX_DIRECT_COLOR);

    return rank.getValue();
}

static GlxVisual chooseVisual(GlxDisplay &display, const RenderConfig &cfg)
{
    ::Display *dpy        = display.getXDisplay();
    uint64_t maxRank      = 0;
    GLXFBConfig maxConfig = nullptr;
    int numElems          = 0;

    GLXFBConfig *const fbConfigs = glXGetFBConfigs(dpy, DefaultScreen(dpy), &numElems);
    TCU_CHECK_MSG(fbConfigs != nullptr, "Couldn't query framebuffer configurations");

    for (int i = 0; i < numElems; i++)
    {
        try
        {
            GlxVisual visual(display, fbConfigs[i]);

            if (!configMatches(visual, cfg))
                continue;

            uint64_t cfgRank = configRank(visual);

            if (cfgRank > maxRank)
            {
                maxRank   = cfgRank;
                maxConfig = fbConfigs[i];
            }
        }
        catch (const tcu::ResourceError &)
        {
            // Some drivers report invalid visuals. Ignore them.
        }
    }
    XFree(fbConfigs);

    if (maxRank == 0)
        TCU_THROW(NotSupportedError, "Requested GLX configuration not found or unusable");

    return GlxVisual(display, maxConfig);
}

GlxDrawable *createDrawable(GlxVisual &visual, const RenderConfig &config)
{
    RenderConfig::SurfaceType surfaceType = config.surfaceType;

    if (surfaceType == RenderConfig::SURFACETYPE_DONT_CARE)
    {
        if (visual.getXVisual() == nullptr)
            // No visual, cannot create X window
            surfaceType = RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE;
        else
            surfaceType = RenderConfig::SURFACETYPE_WINDOW;
    }

    switch (surfaceType)
    {
    case RenderConfig::SURFACETYPE_DONT_CARE:
        DE_FATAL("Impossible case");
        break;

    case RenderConfig::SURFACETYPE_WINDOW:
        return new GlxWindow(visual, config);

    case RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE:
        // \todo [2013-11-28 lauri] Pixmaps

    case RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC:
        // \todo [2013-11-28 lauri] Pbuffers

    default:
        TCU_THROW(NotSupportedError, "Unsupported surface type");
    }

    return nullptr;
}

struct GlxFunctionLoader : public glw::FunctionLoader
{
    GlxFunctionLoader(void)
    {
    }

    glw::GenericFuncType get(const char *name) const
    {
        return glXGetProcAddress(reinterpret_cast<const GLubyte *>(name));
    }
};

GlxRenderContext::GlxRenderContext(const GlxContextFactory &factory, const RenderConfig &config,
                                   const glu::RenderContext *sharedContext)
    : m_glxDisplay(factory.getEventState(), nullptr)
    , m_glxVisual(chooseVisual(m_glxDisplay, config))
    , m_type(config.type)
    , m_GLXContext(m_glxVisual.createContext(factory, config.type, sharedContext, config.resetNotificationStrategy))
    , m_glxDrawable(createDrawable(m_glxVisual, config))
    , m_renderTarget(m_glxDrawable->getWidth(), m_glxDrawable->getHeight(),
                     PixelFormat(m_glxVisual.getAttrib(GLX_RED_SIZE), m_glxVisual.getAttrib(GLX_GREEN_SIZE),
                                 m_glxVisual.getAttrib(GLX_BLUE_SIZE), m_glxVisual.getAttrib(GLX_ALPHA_SIZE)),
                     m_glxVisual.getAttrib(GLX_DEPTH_SIZE), m_glxVisual.getAttrib(GLX_STENCIL_SIZE),
                     m_glxVisual.getAttrib(GLX_SAMPLES))
{
    const GlxFunctionLoader loader;
    makeCurrent();
    glu::initFunctions(&m_functions, &loader, config.type.getAPI());
    swapInterval(0);
}

GlxRenderContext::~GlxRenderContext(void)
{
    clearCurrent();
    if (m_GLXContext != nullptr)
        glXDestroyContext(m_glxDisplay.getXDisplay(), m_GLXContext);
}

void GlxRenderContext::makeCurrent(void)
{
    const GLXDrawable drawRead = m_glxDrawable->getGLXDrawable();
    TCU_CHECK_GLX(glXMakeContextCurrent(m_glxDisplay.getXDisplay(), drawRead, drawRead, m_GLXContext));
}

void GlxRenderContext::clearCurrent(void)
{
    TCU_CHECK_GLX(glXMakeContextCurrent(m_glxDisplay.getXDisplay(), None, None, nullptr));
}

glw::GenericFuncType GlxRenderContext::getProcAddress(const char *name) const
{
    return glXGetProcAddress(reinterpret_cast<const GLubyte *>(name));
}

void GlxRenderContext::swapInterval(int interval)
{
    if (m_glxVisual.getGlxDisplay().isGlxExtensionSupported("GLX_EXT_swap_control"))
    {
        PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = reinterpret_cast<PFNGLXSWAPINTERVALEXTPROC>(
            TCU_CHECK_GLX(glXGetProcAddress(reinterpret_cast<const GLubyte *>("glXSwapIntervalEXT"))));

        glXSwapIntervalEXT(m_glxVisual.getXDisplay(), m_glxDrawable->getGLXDrawable(), interval);
    }
    else if (m_glxVisual.getGlxDisplay().isGlxExtensionSupported("GLX_MESA_swap_control"))
    {
        PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA = reinterpret_cast<PFNGLXSWAPINTERVALMESAPROC>(
            TCU_CHECK_GLX(glXGetProcAddress(reinterpret_cast<const GLubyte *>("glXSwapIntervalMESA"))));

        glXSwapIntervalMESA(interval);
    }
}

ContextType GlxRenderContext::getType(void) const
{
    return m_type;
}

void GlxRenderContext::postIterate(void)
{
    m_glxDrawable->swapBuffers();
    m_glxDrawable->processEvents();
    m_glxDisplay.processEvents();
}

const RenderTarget &GlxRenderContext::getRenderTarget(void) const
{
    return m_renderTarget;
}

const glw::Functions &GlxRenderContext::getFunctions(void) const
{
    return m_functions;
}

const GLXContext &GlxRenderContext::getGLXContext(void) const
{
    return m_GLXContext;
}

const GlxVisual &GlxRenderContext::getGLXVisual(void) const
{
    return m_glxVisual;
}

MovePtr<ContextFactory> createContextFactory(EventState &eventState)
{
    return MovePtr<ContextFactory>(new GlxContextFactory(eventState));
}

} // namespace glx
} // namespace x11
} // namespace lnx
} // namespace tcu
