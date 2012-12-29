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

#ifndef X11WindowResources_h
#define X11WindowResources_h

#if USE(ACCELERATED_COMPOSITING)

#include "IntRect.h"

#include <opengl/GLDefs.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#if USE(GRAPHICS_SURFACE)
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#endif

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

    Display* x11Display()
    {
        if (!m_display) {
            m_display = XOpenDisplay(0);
            if (!m_display) {
                LOG_ERROR("Failed to make connection with X");
                return 0;
            }
#if USE(GRAPHICS_SURFACE)
            if (m_display) {
                int eventBasep, errorBasep;
                m_supportsXRenderExtension = XRenderQueryExtension(m_display, &eventBasep, &errorBasep);
            }
#endif
        }

        return m_display;
    }

    Window getXWindow()
    {
        if (!m_window) {
            Display* dpy = x11Display();
            XSetWindowAttributes attributes;
            attributes.override_redirect = true;
            m_window = XCreateSimpleWindow(dpy, XDefaultRootWindow(dpy), -1, -1, 1, 1, 0, BlackPixel(dpy, 0), WhitePixel(dpy, 0));
            // From http://tronche.com/gui/x/xlib/window/attributes/
            XChangeWindowAttributes(dpy, m_window, CWOverrideRedirect, &attributes);
            XMapWindow(dpy, m_window);

            if (!m_window) {
                LOG_ERROR("Failed to create offscreen root window.");
                return 0;
            }
        }

        return m_window;
    }

    bool isXRenderExtensionSupported() const
    {
        return m_supportsXRenderExtension;
    }

protected:
    SharedX11Resources()
        : m_supportsXRenderExtension(false)
        , m_window(0)
        , m_display(0)
    {
    }

    virtual ~SharedX11Resources()
    {
        if (!m_display)
            return;

        if (m_window) {
            XUnmapWindow(m_display, m_window);
            XDestroyWindow(m_display, m_window);
            m_window = 0;
        }

        XCloseDisplay(m_display);
        m_display = 0;
    }

    static SharedX11Resources* m_staticSharedResource;
    bool m_supportsXRenderExtension;
    Window m_window;
    Display* m_display;
};

class X11OffScreenWindow {
    WTF_MAKE_NONCOPYABLE(X11OffScreenWindow);

public:
    X11OffScreenWindow();
    virtual ~X11OffScreenWindow();
    void createOffscreenWindow(uint32_t*);
    void destroyWindow(const uint32_t);
    void reSizeWindow(const IntRect&, const uint32_t);
    Display* nativeSharedDisplay() const;
#if USE(EGL)
    bool setVisualId(const EGLint);
#endif
    void setVisualInfo(XVisualInfo*);
    bool isXRenderExtensionSupported() const;

private:
    RefPtr<SharedX11Resources> m_sharedResources;
    XVisualInfo* m_configVisualInfo;
};

}

#endif

#endif
