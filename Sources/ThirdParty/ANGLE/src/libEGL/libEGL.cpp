//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// libEGL.cpp: Implements the exported EGL functions.

#include <exception>

#include "common/debug.h"
#include "libGLESv2/Context.h"

#include "libEGL/main.h"
#include "libEGL/Display.h"


bool validate(egl::Display *display)
{
    if (display == EGL_NO_DISPLAY)
    {
        return error(EGL_BAD_DISPLAY, false);
    }

    if (!display->isInitialized())
    {
        return error(EGL_NOT_INITIALIZED, false);
    }

    return true;
}

bool validate(egl::Display *display, EGLConfig config)
{
    if (!validate(display))
    {
        return false;
    }

    if (!display->isValidConfig(config))
    {
        return error(EGL_BAD_CONFIG, false);
    }

    return true;
}

bool validate(egl::Display *display, gl::Context *context)
{
    if (!validate(display))
    {
        return false;
    }

    if (!display->isValidContext(context))
    {
        return error(EGL_BAD_CONTEXT, false);
    }

    return true;
}

bool validate(egl::Display *display, egl::Surface *surface)
{
    if (!validate(display))
    {
        return false;
    }

    if (!display->isValidSurface(surface))
    {
        return error(EGL_BAD_SURFACE, false);
    }

    return true;
}

extern "C"
{
EGLint __stdcall eglGetError(void)
{
    TRACE("()");

    EGLint error = egl::getCurrentError();

    if (error != EGL_SUCCESS)
    {
        egl::setCurrentError(EGL_SUCCESS);
    }

    return error;
}

EGLDisplay __stdcall eglGetDisplay(EGLNativeDisplayType display_id)
{
    TRACE("(EGLNativeDisplayType display_id = 0x%0.8p)", display_id);

    try
    {
        // FIXME: Return the same EGLDisplay handle when display_id already created a display

        if (display_id == EGL_DEFAULT_DISPLAY)
        {
            return new egl::Display((HDC)NULL);
        }
        else
        {
            // FIXME: Check if display_id is a valid display device context

            return new egl::Display((HDC)display_id);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_NO_DISPLAY);
    }

    return EGL_NO_DISPLAY;
}

EGLBoolean __stdcall eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLint *major = 0x%0.8p, EGLint *minor = 0x%0.8p)",
          dpy, major, minor);

    try
    {
        if (dpy == EGL_NO_DISPLAY)
        {
            return error(EGL_BAD_DISPLAY, EGL_FALSE);
        }

        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!display->initialize())
        {
            return error(EGL_NOT_INITIALIZED, EGL_FALSE);
        }

        if (major) *major = 1;
        if (minor) *minor = 4;

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglTerminate(EGLDisplay dpy)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p)", dpy);

    try
    {
        if (dpy == EGL_NO_DISPLAY)
        {
            return error(EGL_BAD_DISPLAY, EGL_FALSE);
        }

        egl::Display *display = static_cast<egl::Display*>(dpy);

        display->terminate();

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

const char *__stdcall eglQueryString(EGLDisplay dpy, EGLint name)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLint name = %d)", dpy, name);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return NULL;
        }

        switch (name)
        {
          case EGL_CLIENT_APIS:
            return success("OpenGL_ES");
          case EGL_EXTENSIONS:
            return success("");
          case EGL_VENDOR:
            return success("TransGaming Inc.");
          case EGL_VERSION:
            return success("1.4 (git-devel "__DATE__" " __TIME__")");
        }

        return error(EGL_BAD_PARAMETER, (const char*)NULL);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, (const char*)NULL);
    }

    return NULL;
}

EGLBoolean __stdcall eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLConfig *configs = 0x%0.8p, "
          "EGLint config_size = %d, EGLint *num_config = 0x%0.8p)",
          dpy, configs, config_size, num_config);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        if (!num_config)
        {
            return error(EGL_BAD_PARAMETER, EGL_FALSE);
        }

        const EGLint attribList[] =    {EGL_NONE};

        if (!display->getConfigs(configs, attribList, config_size, num_config))
        {
            return error(EGL_BAD_ATTRIBUTE, EGL_FALSE);
        }

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, const EGLint *attrib_list = 0x%0.8p, "
          "EGLConfig *configs = 0x%0.8p, EGLint config_size = %d, EGLint *num_config = 0x%0.8p)",
          dpy, attrib_list, configs, config_size, num_config);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        if (!num_config)
        {
            return error(EGL_BAD_PARAMETER, EGL_FALSE);
        }

        const EGLint attribList[] =    {EGL_NONE};

        if (!attrib_list)
        {
            attrib_list = attribList;
        }

        display->getConfigs(configs, attrib_list, config_size, num_config);

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, EGLint attribute = %d, EGLint *value = 0x%0.8p)",
          dpy, config, attribute, value);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display, config))
        {
            return EGL_FALSE;
        }

        if (!display->getConfigAttrib(config, attribute, value))
        {
            return error(EGL_BAD_ATTRIBUTE, EGL_FALSE);
        }

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLSurface __stdcall eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, EGLNativeWindowType win = 0x%0.8p, "
          "const EGLint *attrib_list = 0x%0.8p)", dpy, config, win, attrib_list);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display, config))
        {
            return EGL_NO_SURFACE;
        }

        HWND window = (HWND)win;

        if (!IsWindow(window))
        {
            return error(EGL_BAD_NATIVE_WINDOW, EGL_NO_SURFACE);
        }

        if (attrib_list)
        {
            while (*attrib_list != EGL_NONE)
            {
                switch (attrib_list[0])
                {
                  case EGL_RENDER_BUFFER:
                    switch (attrib_list[1])
                    {
                      case EGL_BACK_BUFFER:
                        break;
                      case EGL_SINGLE_BUFFER:
                        return error(EGL_BAD_MATCH, EGL_NO_SURFACE);   // Rendering directly to front buffer not supported
                      default:
                        return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
                    }
                    break;
                  case EGL_VG_COLORSPACE:
                    return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
                  case EGL_VG_ALPHA_FORMAT:
                    return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
                  default:
                    return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
                }
            }
        }

        if (display->hasExistingWindowSurface(window))
        {
            return error(EGL_BAD_ALLOC, EGL_NO_SURFACE);
        }

        EGLSurface surface = (EGLSurface)display->createWindowSurface(window, config);

        return success(surface);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }

    return EGL_NO_SURFACE;
}

EGLSurface __stdcall eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, const EGLint *attrib_list = 0x%0.8p)",
          dpy, config, attrib_list);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display, config))
        {
            return EGL_NO_SURFACE;
        }

        UNIMPLEMENTED();   // FIXME

        return success(EGL_NO_DISPLAY);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }

    return EGL_NO_SURFACE;
}

EGLSurface __stdcall eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, EGLNativePixmapType pixmap = 0x%0.8p, "
          "const EGLint *attrib_list = 0x%0.8p)", dpy, config, pixmap, attrib_list);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display, config))
        {
            return EGL_NO_SURFACE;
        }

        UNIMPLEMENTED();   // FIXME

        return success(EGL_NO_DISPLAY);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }

    return EGL_NO_SURFACE;
}

EGLBoolean __stdcall eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p)", dpy, surface);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        if (surface == EGL_NO_SURFACE)
        {
            return error(EGL_BAD_SURFACE, EGL_FALSE);
        }

        display->destroySurface((egl::Surface*)surface);

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint attribute = %d, EGLint *value = 0x%0.8p)",
          dpy, surface, attribute, value);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        if (surface == EGL_NO_SURFACE)
        {
            return error(EGL_BAD_SURFACE, EGL_FALSE);
        }

        egl::Surface *eglSurface = (egl::Surface*)surface;

        switch (attribute)
        {
          case EGL_VG_ALPHA_FORMAT:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_VG_COLORSPACE:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_CONFIG_ID:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_HEIGHT:
            *value = eglSurface->getHeight();
            break;
          case EGL_HORIZONTAL_RESOLUTION:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_LARGEST_PBUFFER:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_MIPMAP_TEXTURE:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_MIPMAP_LEVEL:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_MULTISAMPLE_RESOLVE:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_PIXEL_ASPECT_RATIO:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_RENDER_BUFFER:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_SWAP_BEHAVIOR:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_TEXTURE_FORMAT:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_TEXTURE_TARGET:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_VERTICAL_RESOLUTION:
            UNIMPLEMENTED();   // FIXME
            break;
          case EGL_WIDTH:
            *value = eglSurface->getWidth();
            break;
          default:
            return error(EGL_BAD_ATTRIBUTE, EGL_FALSE);
        }

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglBindAPI(EGLenum api)
{
    TRACE("(EGLenum api = 0x%X)", api);

    try
    {
        switch (api)
        {
          case EGL_OPENGL_API:
          case EGL_OPENVG_API:
            return error(EGL_BAD_PARAMETER, EGL_FALSE);   // Not supported by this implementation
          case EGL_OPENGL_ES_API:
            break;
          default:
            return error(EGL_BAD_PARAMETER, EGL_FALSE);
        }

        egl::setCurrentAPI(api);

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLenum __stdcall eglQueryAPI(void)
{
    TRACE("()");

    try
    {
        EGLenum API = egl::getCurrentAPI();

        return success(API);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglWaitClient(void)
{
    TRACE("()");

    try
    {
        UNIMPLEMENTED();   // FIXME

        return success(0);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglReleaseThread(void)
{
    TRACE("()");

    try
    {
        eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_CONTEXT, EGL_NO_SURFACE, EGL_NO_SURFACE);

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLSurface __stdcall eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLenum buftype = 0x%X, EGLClientBuffer buffer = 0x%0.8p, "
          "EGLConfig config = 0x%0.8p, const EGLint *attrib_list = 0x%0.8p)",
          dpy, buftype, buffer, config, attrib_list);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display, config))
        {
            return EGL_NO_SURFACE;
        }

        UNIMPLEMENTED();   // FIXME

        return success(EGL_NO_SURFACE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }

    return EGL_NO_SURFACE;
}

EGLBoolean __stdcall eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint attribute = %d, EGLint value = %d)",
          dpy, surface, attribute, value);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        UNIMPLEMENTED();   // FIXME

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint buffer = %d)", dpy, surface, buffer);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        UNIMPLEMENTED();   // FIXME

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint buffer = %d)", dpy, surface, buffer);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        UNIMPLEMENTED();   // FIXME

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLint interval = %d)", dpy, interval);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        display->setSwapInterval(interval);

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLContext __stdcall eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, EGLContext share_context = 0x%0.8p, "
          "const EGLint *attrib_list = 0x%0.8p)", dpy, config, share_context, attrib_list);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display, config))
        {
            return EGL_NO_CONTEXT;
        }

        EGLContext context = display->createContext(config, static_cast<gl::Context*>(share_context));

        return success(context);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_NO_CONTEXT);
    }

    return EGL_NO_CONTEXT;
}

EGLBoolean __stdcall eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLContext ctx = 0x%0.8p)", dpy, ctx);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        if (ctx == EGL_NO_CONTEXT)
        {
            return error(EGL_BAD_CONTEXT, EGL_FALSE);
        }

        display->destroyContext((gl::Context*)ctx);

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLSurface draw = 0x%0.8p, EGLSurface read = 0x%0.8p, EGLContext ctx = 0x%0.8p)",
          dpy, draw, read, ctx);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);
        gl::Context *context = static_cast<gl::Context*>(ctx);
        IDirect3DDevice9 *device = display->getDevice();

        if (!device || device->TestCooperativeLevel() != D3D_OK)
        {
            return error(EGL_CONTEXT_LOST, EGL_FALSE);
        }

        if (ctx != EGL_NO_CONTEXT && !validate(display, context))
        {
            return EGL_FALSE;
        }

        if ((draw != EGL_NO_SURFACE && !validate(display, static_cast<egl::Surface*>(draw))) ||
            (read != EGL_NO_SURFACE && !validate(display, static_cast<egl::Surface*>(read))))
        {
            return EGL_FALSE;
        }

        if (draw != read)
        {
            UNIMPLEMENTED();   // FIXME
        }

        egl::setCurrentDisplay(dpy);
        egl::setCurrentDrawSurface(draw);
        egl::setCurrentReadSurface(read);

        glMakeCurrent(context, display, static_cast<egl::Surface*>(draw));

        return success(EGL_TRUE);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLContext __stdcall eglGetCurrentContext(void)
{
    TRACE("()");

    try
    {
        EGLContext context = glGetCurrentContext();

        return success(context);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_NO_CONTEXT);
    }

    return EGL_NO_CONTEXT;
}

EGLSurface __stdcall eglGetCurrentSurface(EGLint readdraw)
{
    TRACE("(EGLint readdraw = %d)", readdraw);

    try
    {
        if (readdraw == EGL_READ)
        {
            EGLSurface read = egl::getCurrentReadSurface();
            return success(read);
        }
        else if (readdraw == EGL_DRAW)
        {
            EGLSurface draw = egl::getCurrentDrawSurface();
            return success(draw);
        }
        else
        {
            return error(EGL_BAD_PARAMETER, EGL_NO_SURFACE);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }

    return EGL_NO_SURFACE;
}

EGLDisplay __stdcall eglGetCurrentDisplay(void)
{
    TRACE("()");

    try
    {
        EGLDisplay dpy = egl::getCurrentDisplay();

        return success(dpy);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_NO_DISPLAY);
    }

    return EGL_NO_DISPLAY;
}

EGLBoolean __stdcall eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLContext ctx = 0x%0.8p, EGLint attribute = %d, EGLint *value = 0x%0.8p)",
          dpy, ctx, attribute, value);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        UNIMPLEMENTED();   // FIXME

        return success(0);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglWaitGL(void)
{
    TRACE("()");

    try
    {
        UNIMPLEMENTED();   // FIXME

        return success(0);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglWaitNative(EGLint engine)
{
    TRACE("(EGLint engine = %d)", engine);

    try
    {
        UNIMPLEMENTED();   // FIXME

        return success(0);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p)", dpy, surface);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        if (surface == EGL_NO_SURFACE)
        {
            return error(EGL_BAD_SURFACE, EGL_FALSE);
        }

        egl::Surface *eglSurface = (egl::Surface*)surface;

        if (eglSurface->swap())
        {
            return success(EGL_TRUE);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

EGLBoolean __stdcall eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target)
{
    TRACE("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLNativePixmapType target = 0x%0.8p)", dpy, surface, target);

    try
    {
        egl::Display *display = static_cast<egl::Display*>(dpy);

        if (!validate(display))
        {
            return EGL_FALSE;
        }

        UNIMPLEMENTED();   // FIXME

        return success(0);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, EGL_FALSE);
    }

    return EGL_FALSE;
}

__eglMustCastToProperFunctionPointerType __stdcall eglGetProcAddress(const char *procname)
{
    TRACE("(const char *procname = \"%s\")", procname);

    try
    {
        struct Extension
        {
            const char *name;
            __eglMustCastToProperFunctionPointerType address;
        };

        static const Extension eglExtensions[] =
        {
            {"", NULL},
        };

        for (int ext = 0; ext < sizeof(eglExtensions) / sizeof(Extension); ext++)
        {
            if (strcmp(procname, eglExtensions[ext].name) == 0)
            {
                return (__eglMustCastToProperFunctionPointerType)eglExtensions[ext].address;
            }
        }

        return glGetProcAddress(procname);
    }
    catch(std::bad_alloc&)
    {
        return error(EGL_BAD_ALLOC, (__eglMustCastToProperFunctionPointerType)NULL);
    }

    return NULL;
}
}
