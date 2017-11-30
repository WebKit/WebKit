//
// Copyright(c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// entry_points_egl.cpp : Implements the EGL entry points.

#include "libGLESv2/entry_points_egl.h"

#include "common/debug.h"
#include "common/version.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/Thread.h"
#include "libANGLE/queryutils.h"
#include "libANGLE/validationEGL.h"
#include "libGLESv2/global_state.h"
#include "libGLESv2/proc_table.h"

#include <EGL/eglext.h>

namespace egl
{

namespace
{

bool CompareProc(const ProcEntry &a, const char *b)
{
    return strcmp(a.first, b) < 0;
}

void ClipConfigs(const std::vector<const Config *> &filteredConfigs,
                 EGLConfig *output_configs,
                 EGLint config_size,
                 EGLint *num_config)
{
    EGLint result_size = static_cast<EGLint>(filteredConfigs.size());
    if (output_configs)
    {
        result_size = std::max(std::min(result_size, config_size), 0);
        for (EGLint i = 0; i < result_size; i++)
        {
            output_configs[i] = const_cast<Config *>(filteredConfigs[i]);
        }
    }
    *num_config = result_size;
}

}  // anonymous namespace

// EGL 1.0
EGLint EGLAPIENTRY GetError(void)
{
    EVENT("()");
    Thread *thread = GetCurrentThread();

    EGLint error = thread->getError();
    thread->setError(NoError());
    return error;
}

EGLDisplay EGLAPIENTRY GetDisplay(EGLNativeDisplayType display_id)
{
    EVENT("(EGLNativeDisplayType display_id = 0x%0.8p)", display_id);

    return Display::GetDisplayFromNativeDisplay(display_id, AttributeMap());
}

EGLBoolean EGLAPIENTRY Initialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLint *major = 0x%0.8p, EGLint *minor = 0x%0.8p)", dpy,
          major, minor);
    Thread *thread = GetCurrentThread();

    Display *display = static_cast<Display *>(dpy);
    if (dpy == EGL_NO_DISPLAY || !Display::isValidDisplay(display))
    {
        thread->setError(EglBadDisplay());
        return EGL_FALSE;
    }

    Error error = display->initialize();
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    if (major)
        *major = 1;
    if (minor)
        *minor = 4;

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY Terminate(EGLDisplay dpy)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p)", dpy);
    Thread *thread = GetCurrentThread();

    Display *display = static_cast<Display *>(dpy);
    if (dpy == EGL_NO_DISPLAY || !Display::isValidDisplay(display))
    {
        thread->setError(EglBadDisplay());
        return EGL_FALSE;
    }

    if (display->isValidContext(thread->getContext()))
    {
        thread->setCurrent(nullptr);
    }

    Error error = display->terminate();
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(NoError());
    return EGL_TRUE;
}

const char *EGLAPIENTRY QueryString(EGLDisplay dpy, EGLint name)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLint name = %d)", dpy, name);
    Thread *thread = GetCurrentThread();

    Display *display = static_cast<Display *>(dpy);
    if (!(display == EGL_NO_DISPLAY && name == EGL_EXTENSIONS))
    {
        Error error = ValidateDisplay(display);
        if (error.isError())
        {
            thread->setError(error);
            return nullptr;
        }
    }

    const char *result;
    switch (name)
    {
        case EGL_CLIENT_APIS:
            result = "OpenGL_ES";
            break;
        case EGL_EXTENSIONS:
            if (display == EGL_NO_DISPLAY)
            {
                result = Display::GetClientExtensionString().c_str();
            }
            else
            {
                result = display->getExtensionString().c_str();
            }
            break;
        case EGL_VENDOR:
            result = display->getVendorString().c_str();
            break;
        case EGL_VERSION:
            result = "1.4 (ANGLE " ANGLE_VERSION_STRING ")";
            break;
        default:
            thread->setError(EglBadParameter());
            return nullptr;
    }

    thread->setError(NoError());
    return result;
}

EGLBoolean EGLAPIENTRY GetConfigs(EGLDisplay dpy,
                                  EGLConfig *configs,
                                  EGLint config_size,
                                  EGLint *num_config)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLConfig *configs = 0x%0.8p, "
        "EGLint config_size = %d, EGLint *num_config = 0x%0.8p)",
        dpy, configs, config_size, num_config);
    Thread *thread = GetCurrentThread();

    Display *display = static_cast<Display *>(dpy);

    Error error = ValidateGetConfigs(display, config_size, num_config);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    ClipConfigs(display->getConfigs(AttributeMap()), configs, config_size, num_config);

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY ChooseConfig(EGLDisplay dpy,
                                    const EGLint *attrib_list,
                                    EGLConfig *configs,
                                    EGLint config_size,
                                    EGLint *num_config)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, const EGLint *attrib_list = 0x%0.8p, "
        "EGLConfig *configs = 0x%0.8p, EGLint config_size = %d, EGLint *num_config = 0x%0.8p)",
        dpy, attrib_list, configs, config_size, num_config);
    Thread *thread = GetCurrentThread();

    Display *display       = static_cast<Display *>(dpy);
    AttributeMap attribMap = AttributeMap::CreateFromIntArray(attrib_list);

    Error error = ValidateChooseConfig(display, attribMap, config_size, num_config);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    ClipConfigs(display->getConfigs(attribMap), configs, config_size, num_config);

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY GetConfigAttrib(EGLDisplay dpy,
                                       EGLConfig config,
                                       EGLint attribute,
                                       EGLint *value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, EGLint attribute = %d, EGLint "
        "*value = 0x%0.8p)",
        dpy, config, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display      = static_cast<Display *>(dpy);
    Config *configuration = static_cast<Config *>(config);

    Error error = ValidateGetConfigAttrib(display, configuration, attribute);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    QueryConfigAttrib(configuration, attribute, value);

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLSurface EGLAPIENTRY CreateWindowSurface(EGLDisplay dpy,
                                           EGLConfig config,
                                           EGLNativeWindowType win,
                                           const EGLint *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, EGLNativeWindowType win = 0x%0.8p, "
        "const EGLint *attrib_list = 0x%0.8p)",
        dpy, config, win, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    Config *configuration   = static_cast<Config *>(config);
    AttributeMap attributes = AttributeMap::CreateFromIntArray(attrib_list);

    Error error = ValidateCreateWindowSurface(display, configuration, win, attributes);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_SURFACE;
    }

    egl::Surface *surface = nullptr;
    error                 = display->createWindowSurface(configuration, win, attributes, &surface);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_SURFACE;
    }

    return static_cast<EGLSurface>(surface);
}

EGLSurface EGLAPIENTRY CreatePbufferSurface(EGLDisplay dpy,
                                            EGLConfig config,
                                            const EGLint *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, const EGLint *attrib_list = "
        "0x%0.8p)",
        dpy, config, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    Config *configuration   = static_cast<Config *>(config);
    AttributeMap attributes = AttributeMap::CreateFromIntArray(attrib_list);

    Error error = ValidateCreatePbufferSurface(display, configuration, attributes);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_SURFACE;
    }

    egl::Surface *surface = nullptr;
    error                 = display->createPbufferSurface(configuration, attributes, &surface);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_SURFACE;
    }

    return static_cast<EGLSurface>(surface);
}

EGLSurface EGLAPIENTRY CreatePixmapSurface(EGLDisplay dpy,
                                           EGLConfig config,
                                           EGLNativePixmapType pixmap,
                                           const EGLint *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, EGLNativePixmapType pixmap = "
        "0x%0.8p, "
        "const EGLint *attrib_list = 0x%0.8p)",
        dpy, config, pixmap, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display      = static_cast<Display *>(dpy);
    Config *configuration = static_cast<Config *>(config);

    Error error = ValidateConfig(display, configuration);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_SURFACE;
    }

    UNIMPLEMENTED();  // FIXME

    thread->setError(NoError());
    return EGL_NO_SURFACE;
}

EGLBoolean EGLAPIENTRY DestroySurface(EGLDisplay dpy, EGLSurface surface)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p)", dpy, surface);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    Error error = ValidateSurface(display, eglSurface);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    if (surface == EGL_NO_SURFACE)
    {
        thread->setError(EglBadSurface());
        return EGL_FALSE;
    }

    error = display->destroySurface(reinterpret_cast<Surface *>(surface));
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY QuerySurface(EGLDisplay dpy,
                                    EGLSurface surface,
                                    EGLint attribute,
                                    EGLint *value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint attribute = %d, EGLint "
        "*value = 0x%0.8p)",
        dpy, surface, attribute, value);
    Thread *thread = GetCurrentThread();

    const Display *display    = static_cast<const Display *>(dpy);
    const Surface *eglSurface = static_cast<const Surface *>(surface);

    Error error = ValidateQuerySurface(display, eglSurface, attribute, value);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    QuerySurfaceAttrib(eglSurface, attribute, value);

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLContext EGLAPIENTRY CreateContext(EGLDisplay dpy,
                                     EGLConfig config,
                                     EGLContext share_context,
                                     const EGLint *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, EGLContext share_context = "
        "0x%0.8p, "
        "const EGLint *attrib_list = 0x%0.8p)",
        dpy, config, share_context, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display             = static_cast<Display *>(dpy);
    Config *configuration        = static_cast<Config *>(config);
    gl::Context *sharedGLContext = static_cast<gl::Context *>(share_context);
    AttributeMap attributes      = AttributeMap::CreateFromIntArray(attrib_list);

    Error error = ValidateCreateContext(display, configuration, sharedGLContext, attributes);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_CONTEXT;
    }

    gl::Context *context = nullptr;
    error = display->createContext(configuration, sharedGLContext, attributes, &context);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_CONTEXT;
    }

    thread->setError(NoError());
    return static_cast<EGLContext>(context);
}

EGLBoolean EGLAPIENTRY DestroyContext(EGLDisplay dpy, EGLContext ctx)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLContext ctx = 0x%0.8p)", dpy, ctx);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    gl::Context *context = static_cast<gl::Context *>(ctx);

    Error error = ValidateContext(display, context);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    if (ctx == EGL_NO_CONTEXT)
    {
        thread->setError(EglBadContext());
        return EGL_FALSE;
    }

    if (context == thread->getContext())
    {
        thread->setCurrent(nullptr);
    }

    error = display->destroyContext(context);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY MakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSurface draw = 0x%0.8p, EGLSurface read = 0x%0.8p, "
        "EGLContext ctx = 0x%0.8p)",
        dpy, draw, read, ctx);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    gl::Context *context = static_cast<gl::Context *>(ctx);

    Error error = ValidateMakeCurrent(display, draw, read, context);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    Surface *readSurface   = static_cast<Surface *>(read);
    Surface *drawSurface   = static_cast<Surface *>(draw);
    Error makeCurrentError = display->makeCurrent(drawSurface, readSurface, context);
    if (makeCurrentError.isError())
    {
        thread->setError(makeCurrentError);
        return EGL_FALSE;
    }

    gl::Context *previousContext = thread->getContext();
    thread->setCurrent(context);

    // Release the surface from the previously-current context, to allow
    // destroyed surfaces to delete themselves.
    if (previousContext != nullptr && context != previousContext)
    {
        auto err = previousContext->releaseSurface(display);
        if (err.isError())
        {
            thread->setError(err);
            return EGL_FALSE;
        }
    }

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLSurface EGLAPIENTRY GetCurrentSurface(EGLint readdraw)
{
    EVENT("(EGLint readdraw = %d)", readdraw);
    Thread *thread = GetCurrentThread();

    if (readdraw == EGL_READ)
    {
        thread->setError(NoError());
        return thread->getCurrentReadSurface();
    }
    else if (readdraw == EGL_DRAW)
    {
        thread->setError(NoError());
        return thread->getCurrentDrawSurface();
    }
    else
    {
        thread->setError(EglBadParameter());
        return EGL_NO_SURFACE;
    }
}

EGLDisplay EGLAPIENTRY GetCurrentDisplay(void)
{
    EVENT("()");
    Thread *thread = GetCurrentThread();

    thread->setError(NoError());
    if (thread->getContext() != nullptr)
    {
        return thread->getContext()->getCurrentDisplay();
    }
    return EGL_NO_DISPLAY;
}

EGLBoolean EGLAPIENTRY QueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLContext ctx = 0x%0.8p, EGLint attribute = %d, EGLint *value "
        "= 0x%0.8p)",
        dpy, ctx, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    gl::Context *context = static_cast<gl::Context *>(ctx);

    Error error = ValidateQueryContext(display, context, attribute, value);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    QueryContextAttrib(context, attribute, value);

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY WaitGL(void)
{
    EVENT("()");
    Thread *thread = GetCurrentThread();

    Display *display = thread->getCurrentDisplay();

    Error error = ValidateDisplay(display);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    // eglWaitGL like calling eglWaitClient with the OpenGL ES API bound. Since we only implement
    // OpenGL ES we can do the call directly.
    error = display->waitClient(thread->getContext());
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY WaitNative(EGLint engine)
{
    EVENT("(EGLint engine = %d)", engine);
    Thread *thread = GetCurrentThread();

    Display *display = thread->getCurrentDisplay();

    Error error = ValidateDisplay(display);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    if (engine != EGL_CORE_NATIVE_ENGINE)
    {
        thread->setError(EglBadParameter() << "the 'engine' parameter has an unrecognized value");
    }

    error = display->waitNative(thread->getContext(), engine);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY SwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p)", dpy, surface);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = (Surface *)surface;

    Error error = ValidateSurface(display, eglSurface);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    if (display->testDeviceLost())
    {
        thread->setError(EglContextLost());
        return EGL_FALSE;
    }

    if (surface == EGL_NO_SURFACE)
    {
        thread->setError(EglBadSurface());
        return EGL_FALSE;
    }

    if (!thread->getContext() || thread->getCurrentDrawSurface() != eglSurface)
    {
        thread->setError(EglBadSurface());
        return EGL_FALSE;
    }

    error = eglSurface->swap(thread->getContext());
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY CopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLNativePixmapType target = "
        "0x%0.8p)",
        dpy, surface, target);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    Error error = ValidateSurface(display, eglSurface);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    if (display->testDeviceLost())
    {
        thread->setError(EglContextLost());
        return EGL_FALSE;
    }

    UNIMPLEMENTED();  // FIXME

    thread->setError(NoError());
    return 0;
}

// EGL 1.1
EGLBoolean EGLAPIENTRY BindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint buffer = %d)", dpy,
          surface, buffer);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    Error error = ValidateSurface(display, eglSurface);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    if (buffer != EGL_BACK_BUFFER)
    {
        thread->setError(EglBadParameter());
        return EGL_FALSE;
    }

    if (surface == EGL_NO_SURFACE || eglSurface->getType() == EGL_WINDOW_BIT)
    {
        thread->setError(EglBadSurface());
        return EGL_FALSE;
    }

    if (eglSurface->getBoundTexture())
    {
        thread->setError(EglBadAccess());
        return EGL_FALSE;
    }

    if (eglSurface->getTextureFormat() == EGL_NO_TEXTURE)
    {
        thread->setError(EglBadMatch());
        return EGL_FALSE;
    }

    gl::Context *context = thread->getContext();
    if (context)
    {
        gl::Texture *textureObject = context->getTargetTexture(GL_TEXTURE_2D);
        ASSERT(textureObject != nullptr);

        if (textureObject->getImmutableFormat())
        {
            thread->setError(EglBadMatch());
            return EGL_FALSE;
        }

        error = eglSurface->bindTexImage(context, textureObject, buffer);
        if (error.isError())
        {
            thread->setError(error);
            return EGL_FALSE;
        }
    }

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY SurfaceAttrib(EGLDisplay dpy,
                                     EGLSurface surface,
                                     EGLint attribute,
                                     EGLint value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint attribute = %d, EGLint "
        "value = %d)",
        dpy, surface, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    Error error = ValidateSurfaceAttrib(display, eglSurface, attribute, value);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    SetSurfaceAttrib(eglSurface, attribute, value);

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY ReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint buffer = %d)", dpy,
          surface, buffer);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    Error error = ValidateSurface(display, eglSurface);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    if (buffer != EGL_BACK_BUFFER)
    {
        thread->setError(EglBadParameter());
        return EGL_FALSE;
    }

    if (surface == EGL_NO_SURFACE || eglSurface->getType() == EGL_WINDOW_BIT)
    {
        thread->setError(EglBadSurface());
        return EGL_FALSE;
    }

    if (eglSurface->getTextureFormat() == EGL_NO_TEXTURE)
    {
        thread->setError(EglBadMatch());
        return EGL_FALSE;
    }

    gl::Texture *texture = eglSurface->getBoundTexture();

    if (texture)
    {
        error = eglSurface->releaseTexImage(thread->getContext(), buffer);
        if (error.isError())
        {
            thread->setError(error);
            return EGL_FALSE;
        }
    }

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY SwapInterval(EGLDisplay dpy, EGLint interval)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLint interval = %d)", dpy, interval);
    Thread *thread = GetCurrentThread();

    Display *display = static_cast<Display *>(dpy);

    Error error = ValidateDisplay(display);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    Surface *draw_surface = static_cast<Surface *>(thread->getCurrentDrawSurface());

    if (draw_surface == nullptr)
    {
        thread->setError(EglBadSurface());
        return EGL_FALSE;
    }

    const egl::Config *surfaceConfig = draw_surface->getConfig();
    EGLint clampedInterval           = std::min(std::max(interval, surfaceConfig->minSwapInterval),
                                      surfaceConfig->maxSwapInterval);

    draw_surface->setSwapInterval(clampedInterval);

    thread->setError(NoError());
    return EGL_TRUE;
}

// EGL 1.2
EGLBoolean EGLAPIENTRY BindAPI(EGLenum api)
{
    EVENT("(EGLenum api = 0x%X)", api);
    Thread *thread = GetCurrentThread();

    switch (api)
    {
        case EGL_OPENGL_API:
        case EGL_OPENVG_API:
            thread->setError(EglBadParameter());
            return EGL_FALSE;  // Not supported by this implementation
        case EGL_OPENGL_ES_API:
            break;
        default:
            thread->setError(EglBadParameter());
            return EGL_FALSE;
    }

    thread->setAPI(api);

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLenum EGLAPIENTRY QueryAPI(void)
{
    EVENT("()");
    Thread *thread = GetCurrentThread();

    EGLenum API = thread->getAPI();

    thread->setError(NoError());
    return API;
}

EGLSurface EGLAPIENTRY CreatePbufferFromClientBuffer(EGLDisplay dpy,
                                                     EGLenum buftype,
                                                     EGLClientBuffer buffer,
                                                     EGLConfig config,
                                                     const EGLint *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLenum buftype = 0x%X, EGLClientBuffer buffer = 0x%0.8p, "
        "EGLConfig config = 0x%0.8p, const EGLint *attrib_list = 0x%0.8p)",
        dpy, buftype, buffer, config, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    Config *configuration   = static_cast<Config *>(config);
    AttributeMap attributes = AttributeMap::CreateFromIntArray(attrib_list);

    Error error =
        ValidateCreatePbufferFromClientBuffer(display, buftype, buffer, configuration, attributes);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_SURFACE;
    }

    egl::Surface *surface = nullptr;
    error = display->createPbufferFromClientBuffer(configuration, buftype, buffer, attributes,
                                                   &surface);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_SURFACE;
    }

    return static_cast<EGLSurface>(surface);
}

EGLBoolean EGLAPIENTRY ReleaseThread(void)
{
    EVENT("()");
    Thread *thread = GetCurrentThread();

    MakeCurrent(EGL_NO_DISPLAY, EGL_NO_CONTEXT, EGL_NO_SURFACE, EGL_NO_SURFACE);

    thread->setError(NoError());
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY WaitClient(void)
{
    EVENT("()");
    Thread *thread = GetCurrentThread();

    Display *display = thread->getCurrentDisplay();

    Error error = ValidateDisplay(display);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    error = display->waitClient(thread->getContext());
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(NoError());
    return EGL_TRUE;
}

// EGL 1.4
EGLContext EGLAPIENTRY GetCurrentContext(void)
{
    EVENT("()");
    Thread *thread = GetCurrentThread();

    gl::Context *context = thread->getContext();

    thread->setError(NoError());
    return static_cast<EGLContext>(context);
}

// EGL 1.5
EGLSync EGLAPIENTRY CreateSync(EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLenum type = 0x%X, const EGLint* attrib_list = 0x%0.8p)",
          dpy, type, attrib_list);
    Thread *thread = GetCurrentThread();

    UNIMPLEMENTED();
    thread->setError(EglBadDisplay() << "eglCreateSync unimplemented.");
    return EGL_NO_SYNC;
}

EGLBoolean EGLAPIENTRY DestroySync(EGLDisplay dpy, EGLSync sync)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLSync sync = 0x%0.8p)", dpy, sync);
    Thread *thread = GetCurrentThread();

    UNIMPLEMENTED();
    thread->setError(EglBadDisplay() << "eglDestroySync unimplemented.");
    return EGL_FALSE;
}

EGLint EGLAPIENTRY ClientWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSync sync = 0x%0.8p, EGLint flags = 0x%X, EGLTime timeout = "
        "%d)",
        dpy, sync, flags, timeout);
    Thread *thread = GetCurrentThread();

    UNIMPLEMENTED();
    thread->setError(EglBadDisplay() << "eglClientWaitSync unimplemented.");
    return 0;
}

EGLBoolean EGLAPIENTRY GetSyncAttrib(EGLDisplay dpy,
                                     EGLSync sync,
                                     EGLint attribute,
                                     EGLAttrib *value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSync sync = 0x%0.8p, EGLint attribute = 0x%X, EGLAttrib "
        "*value = 0x%0.8p)",
        dpy, sync, attribute, value);
    Thread *thread = GetCurrentThread();

    UNIMPLEMENTED();
    thread->setError(EglBadDisplay() << "eglSyncAttrib unimplemented.");
    return EGL_FALSE;
}

EGLImage EGLAPIENTRY CreateImage(EGLDisplay dpy,
                                 EGLContext ctx,
                                 EGLenum target,
                                 EGLClientBuffer buffer,
                                 const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLContext ctx = 0x%0.8p, EGLenum target = 0x%X, "
        "EGLClientBuffer buffer = 0x%0.8p, const EGLAttrib *attrib_list = 0x%0.8p)",
        dpy, ctx, target, buffer, attrib_list);
    Thread *thread = GetCurrentThread();

    UNIMPLEMENTED();
    thread->setError(EglBadDisplay() << "eglCreateImage unimplemented.");
    return EGL_NO_IMAGE;
}

EGLBoolean EGLAPIENTRY DestroyImage(EGLDisplay dpy, EGLImage image)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLImage image = 0x%0.8p)", dpy, image);
    Thread *thread = GetCurrentThread();

    UNIMPLEMENTED();
    thread->setError(EglBadDisplay() << "eglDestroyImage unimplemented.");
    return EGL_FALSE;
}

EGLDisplay EGLAPIENTRY GetPlatformDisplay(EGLenum platform,
                                          void *native_display,
                                          const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLenum platform = %d, void* native_display = 0x%0.8p, const EGLint* attrib_list = "
        "0x%0.8p)",
        platform, native_display, attrib_list);
    Thread *thread = GetCurrentThread();

    Error err = ValidateGetPlatformDisplay(platform, native_display, attrib_list);
    thread->setError(err);
    if (err.isError())
    {
        return EGL_NO_DISPLAY;
    }

    const auto &attribMap = AttributeMap::CreateFromAttribArray(attrib_list);
    if (platform == EGL_PLATFORM_ANGLE_ANGLE)
    {
        return Display::GetDisplayFromNativeDisplay(
            gl::bitCast<EGLNativeDisplayType>(native_display), attribMap);
    }
    else if (platform == EGL_PLATFORM_DEVICE_EXT)
    {
        Device *eglDevice = reinterpret_cast<Device *>(native_display);
        return Display::GetDisplayFromDevice(eglDevice, attribMap);
    }
    else
    {
        UNREACHABLE();
        return EGL_NO_DISPLAY;
    }
}

EGLSurface EGLAPIENTRY CreatePlatformWindowSurface(EGLDisplay dpy,
                                                   EGLConfig config,
                                                   void *native_window,
                                                   const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, void* native_window = 0x%0.8p, "
        "const EGLint* attrib_list = 0x%0.8p)",
        dpy, config, native_window, attrib_list);
    Thread *thread = GetCurrentThread();

    UNIMPLEMENTED();
    thread->setError(EglBadDisplay() << "eglCreatePlatformWindowSurface unimplemented.");
    return EGL_NO_SURFACE;
}

EGLSurface EGLAPIENTRY CreatePlatformPixmapSurface(EGLDisplay dpy,
                                                   EGLConfig config,
                                                   void *native_pixmap,
                                                   const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, void* native_pixmap = 0x%0.8p, "
        "const EGLint* attrib_list = 0x%0.8p)",
        dpy, config, native_pixmap, attrib_list);
    Thread *thread = GetCurrentThread();

    UNIMPLEMENTED();
    thread->setError(EglBadDisplay() << "eglCreatePlatformPixmapSurface unimplemented.");
    return EGL_NO_SURFACE;
}

EGLBoolean EGLAPIENTRY WaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLSync sync = 0x%0.8p, EGLint flags = 0x%X)", dpy, sync,
          flags);
    Thread *thread = GetCurrentThread();

    UNIMPLEMENTED();
    thread->setError(EglBadDisplay() << "eglWaitSync unimplemented.");
    return EGL_FALSE;
}

__eglMustCastToProperFunctionPointerType EGLAPIENTRY GetProcAddress(const char *procname)
{
    EVENT("(const char *procname = \"%s\")", procname);
    Thread *thread = GetCurrentThread();

    ProcEntry *entry =
        std::lower_bound(&g_procTable[0], &g_procTable[g_numProcs], procname, CompareProc);

    thread->setError(NoError());

    if (entry == &g_procTable[g_numProcs] || strcmp(entry->first, procname) != 0)
    {
        return nullptr;
    }

    return entry->second;
}
}
