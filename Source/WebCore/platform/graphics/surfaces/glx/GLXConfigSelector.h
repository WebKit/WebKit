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

#ifndef GLXConfigSelector_h
#define GLXConfigSelector_h

#include "X11WindowResources.h"

#if USE(ACCELERATED_COMPOSITING) && USE(GLX)

namespace WebCore {

class GLXConfigSelector {
    WTF_MAKE_NONCOPYABLE(GLXConfigSelector);

public:
    GLXConfigSelector(Display* xDisplay, bool supportsXRenderExtension)
        : m_pbufferFBConfig(0)
        , m_surfaceContextFBConfig(0)
        , m_visualInfo(0)
        , m_sharedDisplay(xDisplay)
        , m_supportsXRenderExtension(supportsXRenderExtension)
    {
    }

    virtual ~GLXConfigSelector()
    {
    }

    XVisualInfo* visualInfo() const
    {
        return m_visualInfo;
    }

    GLXFBConfig pBufferContextConfig()
    {
        if (!m_pbufferFBConfig) {
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

            int numAvailableConfigs;
            GLXFBConfig* temp = glXChooseFBConfig(m_sharedDisplay, DefaultScreen(m_sharedDisplay), attributes, &numAvailableConfigs);
            if (numAvailableConfigs)
                m_pbufferFBConfig = temp[0];
            XFree(temp);
        }

        return m_pbufferFBConfig;
    }

    GLXFBConfig surfaceContextConfig()
    {
        if (!m_surfaceContextFBConfig) {
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

            m_surfaceContextFBConfig = createConfig(attributes);
        }

        return m_surfaceContextFBConfig;
    }

    void reset()
    {
        m_pbufferFBConfig = 0;
        m_surfaceContextFBConfig = 0;
    }

private:
    GLXFBConfig createConfig(const int attributes[])
    {
        int numAvailableConfigs;
        m_visualInfo = 0;
        GLXFBConfig* temp = glXChooseFBConfig(m_sharedDisplay, DefaultScreen(m_sharedDisplay), attributes, &numAvailableConfigs);

        if (!numAvailableConfigs)
            return 0;

        GLXFBConfig selectedConfig = 0;
        bool found = false;

        for (int i = 0; i < numAvailableConfigs; ++i) {
            m_visualInfo = glXGetVisualFromFBConfig(m_sharedDisplay, temp[i]);
            if (!m_visualInfo)
                continue;
#if USE(GRAPHICS_SURFACE)
            if (m_supportsXRenderExtension) {
                XRenderPictFormat* format = XRenderFindVisualFormat(m_sharedDisplay, m_visualInfo->visual);
                if (format && format->direct.alphaMask > 0) {
                    selectedConfig = temp[i];
                    found = true;
                    break;
                }
            } else if (m_visualInfo->depth == 32) {
#else
            if (m_visualInfo->depth == 32) {
#endif
                selectedConfig = temp[i];
                found = true;
            }
        }

        // Did not find any visual supporting alpha, select the first available config.
        if (!found) {
            selectedConfig = temp[0];
            m_visualInfo = glXGetVisualFromFBConfig(m_sharedDisplay, temp[0]);
        }

        XFree(temp);

        return selectedConfig;
    }

    GLXFBConfig m_pbufferFBConfig;
    GLXFBConfig m_surfaceContextFBConfig;
    XVisualInfo* m_visualInfo;
    Display* m_sharedDisplay;
    bool m_supportsXRenderExtension;
};

}

#endif

#endif
