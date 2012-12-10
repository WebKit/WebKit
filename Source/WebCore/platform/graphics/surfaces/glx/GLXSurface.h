/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GLXSurface_h
#define GLXSurface_h

#if USE(ACCELERATED_COMPOSITING) && HAVE(GLX)

#include "GLPlatformSurface.h"
#if USE(GRAPHICS_SURFACE)
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#endif
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class SharedX11Resources : public WTF::RefCountedBase {
    WTF_MAKE_NONCOPYABLE(SharedX11Resources);

public:
    static PassRefPtr<SharedX11Resources> create()
    {
        if (!m_staticSharedResource)
            m_staticSharedResource = new SharedX11Resources();
        else
            m_staticSharedResource->ref();

        return adoptRef(m_staticSharedResource);
    }

    void deref()
    {
        if (derefBase()) {
            m_staticSharedResource = 0;
            delete this;
        }
    }

    Window getXWindow()
    {
        if (!m_window) {
            Display* dpy = display();
            XSetWindowAttributes attributes;
            attributes.override_redirect = true;
            m_window = XCreateSimpleWindow(dpy, XDefaultRootWindow(dpy), -1, -1, 1, 1, 0, BlackPixel(dpy, 0), WhitePixel(dpy, 0));
            // From http://tronche.com/gui/x/xlib/window/attributes/
            XChangeWindowAttributes(dpy, m_window, CWOverrideRedirect, &attributes);
            XMapWindow(dpy, m_window);
        }

        return m_window;
    }

    Display* display()
    {
        if (!m_display)
            m_display = XOpenDisplay(0);

        return m_display;
    }

    XVisualInfo* visualInfo()
    {
        if (!m_VisualInfo)
            surfaceContextConfig();

        return m_VisualInfo;
    }

    GLXFBConfig createConfig(const int attributes[])
    {
        int numReturned;
        m_VisualInfo = 0;
        GLXFBConfig* temp = glXChooseFBConfig(display(), DefaultScreen(display()), attributes, &numReturned);

        if (!numReturned)
            return 0;

        GLXFBConfig selectedConfig = 0;
        bool found = false;

        for (int i = 0; i < numReturned; ++i) {
            m_VisualInfo = glXGetVisualFromFBConfig(display(), temp[i]);
            if (!m_VisualInfo)
                continue;
#if USE(GRAPHICS_SURFACE)
            if (m_supportsXRenderExtension) {
                XRenderPictFormat* format = XRenderFindVisualFormat(display(), m_VisualInfo->visual);
                if (format && format->direct.alphaMask > 0) {
                    selectedConfig = temp[i];
                    found = true;
                    break;
                }
            } else if (m_VisualInfo->depth == 32) {
#else
            if (m_VisualInfo->depth == 32) {
#endif
                selectedConfig = temp[i];
                found = true;
            }
        }

        // Did not find any visual supporting alpha, select the first available config.
        if (!found) {
            selectedConfig = temp[0];
            m_VisualInfo = glXGetVisualFromFBConfig(m_display, temp[0]);
        }

        XFree(temp);

        return selectedConfig;
    }

    GLXFBConfig pBufferContextConfig()
    {
        if (!m_pbufferfbConfig) {
            static const int attributes[] = {
                GLX_LEVEL, 0,
                GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
                GLX_RENDER_TYPE,   GLX_RGBA_BIT,
                GLX_RED_SIZE,      1,
                GLX_GREEN_SIZE,    1,
                GLX_BLUE_SIZE,     1,
                GLX_DOUBLEBUFFER,  GL_FALSE,
                None
            };

            int numReturned;
            GLXFBConfig* temp = glXChooseFBConfig(display(), DefaultScreen(display()), attributes, &numReturned);
            if (numReturned)
                m_pbufferfbConfig = temp[0];
            XFree(temp);
        }

        return m_pbufferfbConfig;
    }

    GLXFBConfig surfaceContextConfig()
    {
        if (!m_surfaceContextfbConfig) {
            static int attributes[] = {
                GLX_LEVEL, 0,
                GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
                GLX_RENDER_TYPE,   GLX_RGBA_BIT,
                GLX_RED_SIZE,      1,
                GLX_GREEN_SIZE,    1,
                GLX_BLUE_SIZE,     1,
                GLX_ALPHA_SIZE,    1,
                GLX_DEPTH_SIZE,    1,
                GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
                GLX_DOUBLEBUFFER,  True,
                None
            };
#if USE(GRAPHICS_SURFACE)
            int eventBasep, errorBasep;
            m_supportsXRenderExtension = XRenderQueryExtension(display(), &eventBasep, &errorBasep);
#endif
            m_surfaceContextfbConfig = createConfig(attributes);
        }

        return m_surfaceContextfbConfig;
    }

    bool isXRenderExtensionSupported()
    {
        return m_supportsXRenderExtension;
    }

private:
    SharedX11Resources()
        : m_supportsXRenderExtension(false)
        , m_markForDeletion(false)
        , m_window(0)
        , m_display(0)
        , m_pbufferfbConfig(0)
        , m_surfaceContextfbConfig(0)
        , m_VisualInfo(0)
    {
    }

    ~SharedX11Resources()
    {
        if (!m_display)
            return;

        if (m_window) {
            XUnmapWindow(m_display, m_window);
            XDestroyWindow(m_display, m_window);
            m_window = 0;
        }

        if (m_pbufferfbConfig)
            m_pbufferfbConfig = 0;

        if (m_surfaceContextfbConfig)
            m_surfaceContextfbConfig = 0;

        if (m_VisualInfo) {
            XFree(m_VisualInfo);
            m_VisualInfo = 0;
        }

        XCloseDisplay(m_display);
        m_display = 0;
    }

    static SharedX11Resources* m_staticSharedResource;
    bool m_supportsXRenderExtension;
    bool m_markForDeletion;
    Window m_window;
    Display* m_display;
    GLXFBConfig m_pbufferfbConfig;
    GLXFBConfig m_surfaceContextfbConfig;
    XVisualInfo* m_VisualInfo;
};

class GLXSurface : public GLPlatformSurface {
    WTF_MAKE_NONCOPYABLE(GLXSurface);

public:
    GLXSurface();
    virtual ~GLXSurface();

protected:
    XVisualInfo* visualInfo();
    Window xWindow();
    GLXFBConfig pBufferConfiguration();
    GLXFBConfig transportSurfaceConfiguration();
    bool isXRenderExtensionSupported();

private:
    RefPtr<SharedX11Resources> m_sharedResources;
};

#if USE(GRAPHICS_SURFACE)
class GLXTransportSurface : public GLXSurface {
    WTF_MAKE_NONCOPYABLE(GLXTransportSurface);

public:
    GLXTransportSurface();
    virtual ~GLXTransportSurface();
    PlatformSurfaceConfig configuration();
    void swapBuffers();
    void setGeometry(const IntRect& newRect);
    void destroy();

private:
    void initialize();
    void freeResources();
};
#endif

class GLXPBuffer : public GLXSurface {
    WTF_MAKE_NONCOPYABLE(GLXPBuffer);

public:
    GLXPBuffer();
    virtual ~GLXPBuffer();
    PlatformSurfaceConfig configuration();
    void destroy();

private:
    void initialize();
    void freeResources();
};

}

#endif

#endif
