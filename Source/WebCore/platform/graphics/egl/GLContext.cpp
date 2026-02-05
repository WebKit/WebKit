/*
 * Copyright (C) 2012,2023 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GLContext.h"

#include "GLDisplay.h"
#include "GraphicsContextGL.h"
#include "Logging.h"
#include "PlatformDisplay.h"
#include <wtf/StringPrintStream.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/text/StringToIntegerConversion.h>

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GLContext);

const char* GLContext::errorString(int statusCode)
{
    static_assert(sizeof(int) >= sizeof(EGLint), "EGLint must not be wider than int");
    switch (statusCode) {
#define CASE_RETURN_STRING(name) case name: return #name
        // https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglGetError.xhtml
        CASE_RETURN_STRING(EGL_SUCCESS);
        CASE_RETURN_STRING(EGL_NOT_INITIALIZED);
        CASE_RETURN_STRING(EGL_BAD_ACCESS);
        CASE_RETURN_STRING(EGL_BAD_ALLOC);
        CASE_RETURN_STRING(EGL_BAD_ATTRIBUTE);
        CASE_RETURN_STRING(EGL_BAD_CONTEXT);
        CASE_RETURN_STRING(EGL_BAD_CONFIG);
        CASE_RETURN_STRING(EGL_BAD_CURRENT_SURFACE);
        CASE_RETURN_STRING(EGL_BAD_DISPLAY);
        CASE_RETURN_STRING(EGL_BAD_SURFACE);
        CASE_RETURN_STRING(EGL_BAD_MATCH);
        CASE_RETURN_STRING(EGL_BAD_PARAMETER);
        CASE_RETURN_STRING(EGL_BAD_NATIVE_PIXMAP);
        CASE_RETURN_STRING(EGL_BAD_NATIVE_WINDOW);
        CASE_RETURN_STRING(EGL_CONTEXT_LOST);
#undef CASE_RETURN_STRING
    default: return "Unknown EGL error";
    }
}

const char* GLContext::lastErrorString()
{
    return errorString(eglGetError());
}

IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage-in-libc-call")
bool GLContext::getEGLConfig(EGLDisplay display, EGLConfig* config, int surfaceType)
{
    std::array<EGLint, 4> rgbaSize = { 8, 8, 8, 8 };
    if (const char* environmentVariable = getenv("WEBKIT_EGL_PIXEL_LAYOUT")) {
        if (!strcmp(environmentVariable, "RGB565"))
            rgbaSize = { 5, 6, 5, 0 };
        else
            WTFLogAlways("Unknown pixel layout %s, falling back to RGBA8888", environmentVariable);
    }

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib / Windows ports.
    EGLint attributeList[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, rgbaSize[0],
        EGL_GREEN_SIZE, rgbaSize[1],
        EGL_BLUE_SIZE, rgbaSize[2],
        EGL_ALPHA_SIZE, rgbaSize[3],
        EGL_STENCIL_SIZE, 8,
        EGL_SURFACE_TYPE, surfaceType,
        EGL_DEPTH_SIZE, 0,
        EGL_NONE
    };
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    EGLint count;
    if (!eglChooseConfig(display, attributeList, nullptr, 0, &count)) {
        RELEASE_LOG_INFO(Compositing, "Cannot get count of available EGL configurations: %s.", lastErrorString());
        return false;
    }

    EGLint numberConfigsReturned;
    Vector<EGLConfig> configs(count);
    if (!eglChooseConfig(display, attributeList, reinterpret_cast<EGLConfig*>(configs.mutableSpan().data()), count, &numberConfigsReturned) || !numberConfigsReturned) {
        RELEASE_LOG_INFO(Compositing, "Cannot get available EGL configurations: %s.", lastErrorString());
        return false;
    }

    auto index = configs.findIf([&](EGLConfig value) {
        EGLint redSize, greenSize, blueSize, alphaSize;
        eglGetConfigAttrib(display, value, EGL_RED_SIZE, &redSize);
        eglGetConfigAttrib(display, value, EGL_GREEN_SIZE, &greenSize);
        eglGetConfigAttrib(display, value, EGL_BLUE_SIZE, &blueSize);
        eglGetConfigAttrib(display, value, EGL_ALPHA_SIZE, &alphaSize);
        return redSize == rgbaSize[0] && greenSize == rgbaSize[1]
            && blueSize == rgbaSize[2] && alphaSize == rgbaSize[3];
    });

    if (index != notFound) {
        *config = configs[index];
        return true;
    }

    RELEASE_LOG_INFO(Compositing, "Could not find suitable EGL configuration out of %zu checked.", configs.size());
    return false;
}
IGNORE_CLANG_WARNINGS_END

std::unique_ptr<GLContext> GLContext::createWindowContext(GLDisplay& display, Target target, GLNativeWindowType window, EGLContext sharingContext)
{
    EGLDisplay eglDisplay = display.eglDisplay();
    EGLConfig config;
    if (!getEGLConfig(eglDisplay, &config, EGL_WINDOW_BIT)) {
        RELEASE_LOG_INFO(Compositing, "Cannot obtain EGL window context configuration: %s\n", lastErrorString());
        return nullptr;
    }

    EGLContext context = createContextForEGLVersion(eglDisplay, config, sharingContext);
    if (context == EGL_NO_CONTEXT) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL window context: %s\n", lastErrorString());
        return nullptr;
    }

    EGLSurface surface = EGL_NO_SURFACE;
    switch (target) {
#if USE(WPE_RENDERER)
    case Target::WPE:
        surface = createWindowSurfaceWPE(eglDisplay, config, window);
        break;
#endif // USE(WPE_RENDERER)
#if USE(GBM)
    case Target::GBM:
#endif
    case Target::Surfaceless:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    default:
        break;
    }

    if (surface == EGL_NO_SURFACE) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL window surface: %s. Retrying with fallback.", lastErrorString());
        // EGLNativeWindowType changes depending on the EGL implementation, reinterpret_cast
        // would work for pointers, and static_cast for numeric types only; so use a plain
        // C cast expression which works in all possible cases.
        surface = eglCreateWindowSurface(eglDisplay, config, (EGLNativeWindowType) window, nullptr);
    }

    if (surface == EGL_NO_SURFACE) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL window surface: %s\n", lastErrorString());
        eglDestroyContext(eglDisplay, context);
        return nullptr;
    }

    return makeUnique<GLContext>(display, context, surface, config);
}

std::unique_ptr<GLContext> GLContext::createSurfacelessContext(GLDisplay& display, Target target, EGLContext sharingContext)
{
    EGLDisplay eglDisplay = display.eglDisplay();
    const char* extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);
    if (!GLContext::isExtensionSupported(extensions, "EGL_KHR_surfaceless_context") && !GLContext::isExtensionSupported(extensions, "EGL_KHR_surfaceless_opengl")) {
        RELEASE_LOG_INFO(Compositing, "Cannot create surfaceless EGL context: required extensions missing.");
        return nullptr;
    }

    EGLConfig config;
    if (!getEGLConfig(eglDisplay, &config, target == Target::Surfaceless ? EGL_PBUFFER_BIT : EGL_WINDOW_BIT)) {
        RELEASE_LOG_INFO(Compositing, "Cannot obtain EGL surfaceless configuration: %s\n", lastErrorString());
        return nullptr;
    }

    EGLContext context = createContextForEGLVersion(eglDisplay, config, sharingContext);
    if (context == EGL_NO_CONTEXT) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL surfaceless context: %s\n", lastErrorString());
        return nullptr;
    }

    return makeUnique<GLContext>(display, context, EGL_NO_SURFACE, config);
}

std::unique_ptr<GLContext> GLContext::createPbufferContext(GLDisplay& display, EGLContext sharingContext)
{
    EGLDisplay eglDisplay = display.eglDisplay();
    EGLConfig config;
    if (!getEGLConfig(eglDisplay, &config, EGL_PBUFFER_BIT)) {
        RELEASE_LOG_INFO(Compositing, "Cannot obtain EGL Pbuffer configuration: %s\n", lastErrorString());
        return nullptr;
    }

    EGLContext context = createContextForEGLVersion(eglDisplay, config, sharingContext);
    if (context == EGL_NO_CONTEXT) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL Pbuffer context: %s\n", lastErrorString());
        return nullptr;
    }

    static const int pbufferAttributes[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLSurface surface = eglCreatePbufferSurface(eglDisplay, config, pbufferAttributes);
    if (surface == EGL_NO_SURFACE) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL Pbuffer surface: %s\n", lastErrorString());
        eglDestroyContext(eglDisplay, context);
        return nullptr;
    }

    return makeUnique<GLContext>(display, context, surface, config);
}

std::unique_ptr<GLContext> GLContext::createOffscreenContext(GLDisplay& display, Target target, EGLContext sharingContext)
{
    if (auto context = createSurfacelessContext(display, target, sharingContext))
        return context;

    switch (target) {
#if USE(WPE_RENDERER)
    case Target::WPE:
        if (auto context = createWPEContext(display, sharingContext))
            return context;
        break;
#endif
#if USE(GBM)
    case Target::GBM:
#endif
    case Target::Surfaceless:
        // Do not fallback to pbuffers.
        RELEASE_LOG_INFO(Compositing, "Could not create EGL surfaceless context: %s.", lastErrorString());
        return nullptr;
    default:
        break;
    }

    RELEASE_LOG_INFO(Compositing, "Could not create platform context: %s. Using Pbuffer as fallback.", lastErrorString());
    if (auto context = createPbufferContext(display, sharingContext))
        return context;

    RELEASE_LOG_INFO(Compositing, "Could not create Pbuffer context: %s.", lastErrorString());
    return nullptr;
}

std::unique_ptr<GLContext> GLContext::create(GLDisplay& display, Target target, GLContext* sharingGLContext, GLNativeWindowType window)
{
    RELEASE_ASSERT(display.eglDisplay() != EGL_NO_DISPLAY);

    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL context: error binding OpenGL ES API (%s)\n", lastErrorString());
        return nullptr;
    }

    EGLContext eglSharingContext = sharingGLContext ? sharingGLContext->m_context : EGL_NO_CONTEXT;
    auto context = window ? createWindowContext(display, target, window, eglSharingContext) : createOffscreenContext(display, target, eglSharingContext);
    if (!context)
        RELEASE_LOG_INFO(Compositing, "Could not create EGL context.");
    return context;
}

static GLContext::Target targetForPlatformDisplay(PlatformDisplay& platformDisplay)
{
    switch (platformDisplay.type()) {
    case PlatformDisplay::Type::Surfaceless:
        return GLContext::Target::Surfaceless;
#if USE(WPE_RENDERER)
    case PlatformDisplay::Type::WPE:
        return GLContext::Target::WPE;
#endif
#if USE(GBM)
    case PlatformDisplay::Type::GBM:
        return GLContext::Target::GBM;
#endif
    default:
        break;
    }
    return GLContext::Target::Default;
}

std::unique_ptr<GLContext> GLContext::create(PlatformDisplay& platformDisplay, GLNativeWindowType window)
{
    return GLContext::create(platformDisplay.glDisplay(), targetForPlatformDisplay(platformDisplay), platformDisplay.sharingGLContext(), window);
}

std::unique_ptr<GLContext> GLContext::createOffscreen(PlatformDisplay& platformDisplay)
{
    return GLContext::create(platformDisplay.glDisplay(), targetForPlatformDisplay(platformDisplay), platformDisplay.sharingGLContext());
}

std::unique_ptr<GLContext> GLContext::createSharing(PlatformDisplay& platformDisplay)
{
    return GLContext::create(platformDisplay.glDisplay(), targetForPlatformDisplay(platformDisplay));
}

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
static void logGLDebugMessage(GLenum source, GLenum type, GLuint identifier, GLenum severity, GLsizei, const GLchar* message, const void*)
{
    static constexpr auto sourceName = [](GLenum source) -> const char* {
        switch (source) {
        case GL_DEBUG_SOURCE_API_KHR:
            return "API call";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR:
            return "Window System";
        case GL_DEBUG_SOURCE_SHADER_COMPILER_KHR:
            return "Shader Compiler";
        case GL_DEBUG_SOURCE_THIRD_PARTY_KHR:
            return "Third Party";
        case GL_DEBUG_SOURCE_APPLICATION_KHR:
            return "Application";
        case GL_DEBUG_SOURCE_OTHER_KHR:
        default:
            return "Other";
        };
    };

    static constexpr auto typeName = [](GLenum type) -> const char* {
        switch (type) {
        case GL_DEBUG_TYPE_ERROR_KHR:
            return "Error";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR:
            return "Deprecated Behaviour";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR:
            return "Undefined Behaviour";
        case GL_DEBUG_TYPE_PORTABILITY_KHR:
            return "Non-portable";
        case GL_DEBUG_TYPE_PERFORMANCE_KHR:
            return "Performance";
        case GL_DEBUG_TYPE_MARKER_KHR:
            return "Marker";
        case GL_DEBUG_TYPE_PUSH_GROUP_KHR:
            return "Group Push";
        case GL_DEBUG_TYPE_POP_GROUP_KHR:
            return "Group Pop";
        case GL_DEBUG_TYPE_OTHER_KHR:
        default:
            return "Other";
        }
    };

    static constexpr auto logLevel = [](GLenum severity) -> WTFLogLevel {
        switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH_KHR:
            return WTFLogLevel::Error;
        case GL_DEBUG_SEVERITY_MEDIUM_KHR:
            return WTFLogLevel::Warning;
        case GL_DEBUG_SEVERITY_LOW_KHR:
            return WTFLogLevel::Info;
        case GL_DEBUG_SEVERITY_NOTIFICATION_KHR:
        default:
            return WTFLogLevel::Debug;
        }
    };

    RELEASE_LOG_WITH_LEVEL(GLContext, logLevel(severity), "%s (%s) [id=%ld]: %s", sourceName(source), typeName(type), identifier, message);
    if (type == GL_DEBUG_TYPE_ERROR_KHR && LOG_CHANNEL(GLContext).level >= WTFLogLevel::Debug) {
        WTF::StringPrintStream backtraceStream;
        WTFReportBacktraceWithPrefixAndPrintStream(backtraceStream, "#");
        RELEASE_LOG(GLContext, "Backtrace leading to error:\n%s", backtraceStream.toString().utf8().data());
    }
}

bool GLContext::enableDebugLogging()
{
    const char* glExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    const bool backtraceOnError = LOG_CHANNEL(GLContext).level >= WTFLogLevel::Debug;

#if USE(LIBEPOXY)
    if ((!epoxy_is_desktop_gl() && glVersion() >= 320) || isExtensionSupported(glExtensions, "GL_KHR_debug") || isExtensionSupported(glExtensions, "GL_ARB_debug_output")) {
        glDebugMessageCallbackKHR(logGLDebugMessage, nullptr);
        glEnable(backtraceOnError ? GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR : GL_DEBUG_OUTPUT_KHR);
        return true;
    }
#else
    // Assume EGL/GLES2+, which is the case for platforms that do not use Epoxy.
    PFNGLDEBUGMESSAGECALLBACKKHRPROC debugMessageCallback = nullptr;
    if (glVersion() >= 320)
        debugMessageCallback = reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKKHRPROC>(eglGetProcAddress("glDebugMessageCallback"));
    else if (isExtensionSupported(glExtensions, "GL_KHR_debug"))
        debugMessageCallback = reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKKHRPROC>(eglGetProcAddress("glDebugMessageCallbackKHR"));

    if (debugMessageCallback) {
        debugMessageCallback(logGLDebugMessage, nullptr);
        glEnable(backtraceOnError ? GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR : GL_DEBUG_OUTPUT_KHR);
        return true;
    }
#endif

    return false;
}

static inline bool shouldEnableDebugLogging()
{
    return LOG_CHANNEL(GLContext).state != WTFLogChannelState::Off;
}
#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

GLContext::GLContext(GLDisplay& display, EGLContext context, EGLSurface surface, EGLConfig config)
    : m_display(display)
    , m_context(context)
    , m_surface(surface)
    , m_config(config)
{
    RELEASE_ASSERT(context != EGL_NO_CONTEXT);

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    if (shouldEnableDebugLogging()) [[unlikely]] {
        GLContext* previousContext = nullptr;
        if (!isCurrent()) {
            previousContext = current();
            makeContextCurrent();
        }

        if (!enableDebugLogging()) {
            static std::once_flag onceFlag;
            std::call_once(onceFlag, []() {
                RELEASE_LOG_FAULT(GLContext, "No debug logging support, neither GL_KHR_debug, GL_ARB_debug_output, nor GLES 3.2+ are available");
            });
        }

        if (previousContext)
            previousContext->makeContextCurrent();
    }
#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

#if ENABLE(MEDIA_TELEMETRY)
    if (m_surface != EGL_NO_SURFACE) {
        MediaTelemetryReport::singleton().reportWaylandInfo(*this, MediaTelemetryReport::WaylandAction::InitGfx,
            MediaTelemetryReport::WaylandGraphicsState::GfxInitialized, MediaTelemetryReport::WaylandInputsState::InputsInitialized);
    }
#endif
}

GLContext::~GLContext()
{
    if (auto display = m_display.get()) {
        EGLDisplay eglDisplay = display->eglDisplay();
        if (m_context) {
            eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(eglDisplay, m_context);
        }

        if (m_surface)
            eglDestroySurface(eglDisplay, m_surface);
    }

#if USE(WPE_RENDERER)
    destroyWPETarget();
#endif

#if ENABLE(MEDIA_TELEMETRY)
    if (m_surface != EGL_NO_SURFACE) {
        MediaTelemetryReport::singleton().reportWaylandInfo(*this, MediaTelemetryReport::WaylandAction::DeinitGfx,
            MediaTelemetryReport::WaylandGraphicsState::GfxNotInitialized, MediaTelemetryReport::WaylandInputsState::InputsInitialized);
    }
#endif
}

RefPtr<GLDisplay> GLContext::display() const
{
    return m_display.get();
}

EGLContext GLContext::createContextForEGLVersion(EGLDisplay eglDisplay, EGLConfig config, EGLContext sharingContext)
{
    EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
        EGL_CONTEXT_OPENGL_DEBUG, shouldEnableDebugLogging() ? EGL_TRUE : EGL_FALSE,
#endif
        EGL_NONE,
    };
    return eglCreateContext(eglDisplay, config, sharingContext, contextAttributes);
}

bool GLContext::makeCurrentImpl()
{
    ASSERT(m_context);
    auto display = m_display.get();
    return display ? eglMakeCurrent(display->eglDisplay(), m_surface, m_surface, m_context) : false;
}

bool GLContext::unmakeCurrentImpl()
{
    auto display = m_display.get();
    return display ? eglMakeCurrent(display->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) : false;
}

unsigned GLContext::glVersion() const
{
    return version();
}

bool GLContext::makeContextCurrent()
{
    if (isCurrent())
        return true;

    // ANGLE doesn't know anything about non-ANGLE contexts, and does
    // nothing in MakeCurrent if what it thinks is current hasn't changed.
    // So, when making a native context current we need to unmark any previous
    // ANGLE context to ensure the next MakeCurrent does the right thing.
    auto* context = currentContext();
    bool isSwitchingFromANGLE = context && context->type() == GLContextWrapper::Type::Angle;
    if (isSwitchingFromANGLE)
        context->unmakeCurrentImpl();

    auto display = m_display.get();
    if (display && eglMakeCurrent(display->eglDisplay(), m_surface, m_surface, m_context)) {
        didMakeContextCurrent();
        return true;
    }

    // If we failed to make the native context current, restore the previous ANGLE one.
    if (isSwitchingFromANGLE)
        context->makeCurrentImpl();

    return false;
}

bool GLContext::unmakeContextCurrent()
{
    if (!isCurrent())
        return true;

    auto display = m_display.get();
    if (display && eglMakeCurrent(display->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
        didUnmakeContextCurrent();
        return true;
    }

    return false;
}

GLContext* GLContext::current()
{
    auto* context = currentContext();
    if (context && context->type() == GLContextWrapper::Type::Native)
        return static_cast<GLContext*>(context);
    return nullptr;
}

void GLContext::swapBuffers()
{
    if (m_surface == EGL_NO_SURFACE)
        return;

    if (auto display = m_display.get())
        eglSwapBuffers(display->eglDisplay(), m_surface);
}

GCGLContext GLContext::platformContext() const
{
    return m_context;
}

IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage-in-libc-call")
bool GLContext::isExtensionSupported(const char* extensionList, const char* extension)
{
    if (!extensionList)
        return false;

    ASSERT(extension);
    int extensionLen = strlen(extension);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib / Windows ports.
    const char* extensionListPtr = extensionList;
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    while ((extensionListPtr = strstr(extensionListPtr, extension))) {
        if (extensionListPtr[extensionLen] == ' ' || extensionListPtr[extensionLen] == '\0')
            return true;
        extensionListPtr += extensionLen;
    }
    return false;
}
IGNORE_CLANG_WARNINGS_END

unsigned GLContext::versionFromString(const char* versionStringAsChar)
{
    auto versionString = String::fromLatin1(versionStringAsChar);
    Vector<String> versionStringComponents = versionString.split(' ');

    Vector<String> versionDigits;
    if (versionStringComponents[0] == "OpenGL"_s) {
        // If the version string starts with "OpenGL" it can be GLES 1 or 2. In GLES1 version string starts
        // with "OpenGL ES-<profile> major.minor" and in GLES2 with "OpenGL ES major.minor". Version is the
        // third component in both cases.
        versionDigits = versionStringComponents[2].split('.');
    } else {
        // Version is the first component. The version number is always "major.minor" or
        // "major.minor.release". Ignore the release number.
        versionDigits = versionStringComponents[0].split('.');
    }

    return parseIntegerAllowingTrailingJunk<unsigned>(versionDigits[0]).value_or(0) * 100 + parseIntegerAllowingTrailingJunk<unsigned>(versionDigits[1]).value_or(0) * 10;
}

unsigned GLContext::version() const
{
    if (!m_version) {
        auto* versionString = reinterpret_cast<const char*>(::glGetString(GL_VERSION));
        m_version = versionFromString(versionString);
    }

    return m_version;
}

const GLContext::GLExtensions& GLContext::glExtensions() const
{
    static std::once_flag flag;
    std::call_once(flag, [this] {
        const char* extensionsString = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        m_glExtensions.OES_texture_npot = isExtensionSupported(extensionsString, "GL_OES_texture_npot");
        m_glExtensions.EXT_unpack_subimage = isExtensionSupported(extensionsString, "GL_EXT_unpack_subimage");
        m_glExtensions.APPLE_sync = isExtensionSupported(extensionsString, "GL_APPLE_sync");
        m_glExtensions.OES_packed_depth_stencil = isExtensionSupported(extensionsString, "GL_OES_packed_depth_stencil");
    });
    return m_glExtensions;
}

GLContext::ScopedGLContext::ScopedGLContext(std::unique_ptr<GLContext>&& context)
    : m_context(WTF::move(context))
{
    auto eglContext = eglGetCurrentContext();
    m_previous.glContext = GLContext::current();
    if (!m_previous.glContext || m_previous.glContext->platformContext() != eglContext) {
        m_previous.context = eglContext;
        if (m_previous.context != EGL_NO_CONTEXT) {
            m_previous.display = eglGetCurrentDisplay();
            m_previous.readSurface = eglGetCurrentSurface(EGL_READ);
            m_previous.drawSurface = eglGetCurrentSurface(EGL_DRAW);
        }
    }
    m_context->makeContextCurrent();
}

GLContext::ScopedGLContext::~ScopedGLContext()
{
    m_context = nullptr;

    if (m_previous.context != EGL_NO_CONTEXT)
        eglMakeCurrent(m_previous.display, m_previous.drawSurface, m_previous.readSurface, m_previous.context);
    else if (m_previous.glContext)
        m_previous.glContext->makeContextCurrent();
}

GLContext::ScopedGLContextCurrent::ScopedGLContextCurrent(GLContext& context)
    : m_context(context)
{
    auto eglContext = eglGetCurrentContext();
    m_previous.glContext = GLContext::current();
    if (!m_previous.glContext || m_previous.glContext->platformContext() != eglContext) {
        m_previous.context = eglContext;
        if (m_previous.context != EGL_NO_CONTEXT) {
            m_previous.display = eglGetCurrentDisplay();
            m_previous.readSurface = eglGetCurrentSurface(EGL_READ);
            m_previous.drawSurface = eglGetCurrentSurface(EGL_DRAW);
        }
    }
    m_context.makeContextCurrent();
}

GLContext::ScopedGLContextCurrent::~ScopedGLContextCurrent()
{
    if (m_previous.glContext && m_previous.context == EGL_NO_CONTEXT) {
        m_previous.glContext->makeContextCurrent();
        return;
    }

    m_context.unmakeContextCurrent();

    if (m_previous.context)
        eglMakeCurrent(m_previous.display, m_previous.drawSurface, m_previous.readSurface, m_previous.context);
}

#if ENABLE(MEDIA_TELEMETRY)
EGLDisplay GLContext::eglDisplay() const
{
    auto display = m_display.get();
    return display ? display->eglDisplay() : EGL_NO_DISPLAY;
}

EGLConfig GLContext::eglConfig() const
{
    EGLConfig config = nullptr;
    auto display = m_display.get();
    if (!display)
        return nullptr;

    if (!getEGLConfig(display->eglDisplay(), &config, WindowSurface)) {
        WTFLogAlways("Cannot obtain EGL window context configuration: %s\n", lastErrorString());
        config = nullptr;
        ASSERT_NOT_REACHED();
    }

    return config;
}

EGLSurface GLContext::eglSurface() const
{
    return m_surface;
}

EGLContext GLContext::eglContext() const
{
    return m_context;
}

unsigned GLContext::windowWidth() const
{
    return parseInteger<unsigned>(StringView::fromLatin1(std::getenv("WPE_INIT_VIEW_WIDTH"))).value_or(1920);
}

unsigned GLContext::windowHeight() const
{
    return parseInteger<unsigned>(StringView::fromLatin1(std::getenv("WPE_INIT_VIEW_HEIGHT"))).value_or(1080);
}
#endif

} // namespace WebCore
