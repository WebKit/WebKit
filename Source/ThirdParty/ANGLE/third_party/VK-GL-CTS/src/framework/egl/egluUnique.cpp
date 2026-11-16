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
 * \brief EGL unique resources
 *//*--------------------------------------------------------------------*/

#include "egluUnique.hpp"
#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"

namespace eglu
{

using namespace eglw;

UniqueDisplay::UniqueDisplay(const Library &egl, EGLDisplay display) : m_egl(egl), m_display(display)
{
}

UniqueDisplay::~UniqueDisplay(void)
{
    if (m_display != EGL_NO_DISPLAY)
        m_egl.terminate(m_display);
}

UniqueDisplay::operator bool(void) const
{
    return m_display != EGL_NO_DISPLAY;
}

UniqueSurface::UniqueSurface(const Library &egl, EGLDisplay display, EGLSurface surface)
    : m_egl(egl)
    , m_display(display)
    , m_surface(surface)
{
}

UniqueSurface::~UniqueSurface(void)
{
    if (m_surface != EGL_NO_SURFACE)
        m_egl.destroySurface(m_display, m_surface);
}

UniqueSurface::operator bool(void) const
{
    return m_surface != EGL_NO_SURFACE;
}

UniqueContext::UniqueContext(const Library &egl, EGLDisplay display, EGLContext context)
    : m_egl(egl)
    , m_display(display)
    , m_context(context)
{
}

UniqueContext::~UniqueContext(void)
{
    if (m_context != EGL_NO_CONTEXT)
        m_egl.destroyContext(m_display, m_context);
}

UniqueContext::operator bool(void) const
{
    return m_context != EGL_NO_CONTEXT;
}

ScopedCurrentContext::ScopedCurrentContext(const Library &egl, EGLDisplay display, EGLSurface draw, EGLSurface read,
                                           EGLContext context)
    : m_egl(egl)
    , m_display(display)
{
    EGLU_CHECK_CALL(m_egl, makeCurrent(display, draw, read, context));
}

ScopedCurrentContext::~ScopedCurrentContext(void)
{
    m_egl.makeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

UniqueImage::UniqueImage(const Library &egl, EGLDisplay display, EGLImage image)
    : m_egl(egl)
    , m_display(display)
    , m_image(image)
{
}

UniqueImage::~UniqueImage(void)
{
    if (m_image != EGL_NO_IMAGE)
        m_egl.destroyImageKHR(m_display, m_image);
}

UniqueImage::operator bool(void) const
{
    return m_image != EGL_NO_IMAGE;
}

} // namespace eglu
