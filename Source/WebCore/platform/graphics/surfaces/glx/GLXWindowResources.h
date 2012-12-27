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

#ifndef GLXWindowResources_h
#define GLXWindowResources_h

#include "X11WindowResources.h"

#if USE(ACCELERATED_COMPOSITING) && USE(GLX)

namespace WebCore {

class SharedGLXResources : public SharedX11Resources {
    WTF_MAKE_NONCOPYABLE(SharedGLXResources);

public:
    static PassRefPtr<SharedGLXResources> create()
    {
        if (!m_staticSharedResource)
            m_staticSharedResource = new SharedGLXResources();
        else
            m_staticSharedResource->ref();

        return adoptRef(m_staticSharedResource);
    }

    PlatformDisplay nativeDisplay()
    {
        return SharedX11Resources::x11Display();
    }

    XVisualInfo* visualInfo()
    {
        if (!m_VisualInfo)
            surfaceContextConfig();

        return m_VisualInfo;
    }

    virtual GLXFBConfig pBufferContextConfig()
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
            GLXFBConfig* temp = glXChooseFBConfig(nativeDisplay(), DefaultScreen(nativeDisplay()), attributes, &numReturned);
            if (numReturned)
                m_pbufferfbConfig = temp[0];
            XFree(temp);
        }

        return m_pbufferfbConfig;
    }

    virtual GLXFBConfig surfaceContextConfig()
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

            m_surfaceContextfbConfig = createConfig(attributes);
        }

        return m_surfaceContextfbConfig;
    }

protected:
    SharedGLXResources()
        : SharedX11Resources()
        , m_pbufferfbConfig(0)
        , m_surfaceContextfbConfig(0)
        , m_VisualInfo(0)
    {
    }

    GLXFBConfig createConfig(const int attributes[])
    {
        int numReturned;
        m_VisualInfo = 0;
        GLXFBConfig* temp = glXChooseFBConfig(nativeDisplay(), DefaultScreen(nativeDisplay()), attributes, &numReturned);

        if (!numReturned)
            return 0;

        GLXFBConfig selectedConfig = 0;
        bool found = false;

        for (int i = 0; i < numReturned; ++i) {
            m_VisualInfo = glXGetVisualFromFBConfig(nativeDisplay(), temp[i]);
            if (!m_VisualInfo)
                continue;
#if USE(GRAPHICS_SURFACE)
            if (m_supportsXRenderExtension) {
                XRenderPictFormat* format = XRenderFindVisualFormat(nativeDisplay(), m_VisualInfo->visual);
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

    virtual ~SharedGLXResources()
    {
        if (!m_display)
            return;

        if (m_pbufferfbConfig)
            m_pbufferfbConfig = 0;

        if (m_surfaceContextfbConfig)
            m_surfaceContextfbConfig = 0;

        if (m_VisualInfo) {
            XFree(m_VisualInfo);
            m_VisualInfo = 0;
        }
    }

    GLXFBConfig m_pbufferfbConfig;
    GLXFBConfig m_surfaceContextfbConfig;
    XVisualInfo* m_VisualInfo;
};

}

#endif

#endif
