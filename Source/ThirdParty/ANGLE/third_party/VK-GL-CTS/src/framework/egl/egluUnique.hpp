#ifndef _EGLUUNIQUE_HPP
#define _EGLUUNIQUE_HPP
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

#include "egluDefs.hpp"
#include "eglwDefs.hpp"

namespace eglw
{
class Library;
}

namespace eglu
{

class UniqueDisplay
{
public:
    UniqueDisplay(const eglw::Library &egl, eglw::EGLDisplay display);
    ~UniqueDisplay(void);

    eglw::EGLDisplay operator*(void) const
    {
        return m_display;
    }
    operator bool(void) const;

private:
    const eglw::Library &m_egl;
    eglw::EGLDisplay m_display;

    // Disabled
    UniqueDisplay &operator=(const UniqueDisplay &);
    UniqueDisplay(const UniqueDisplay &);
};

class UniqueSurface
{
public:
    UniqueSurface(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLSurface surface);
    ~UniqueSurface(void);

    eglw::EGLSurface operator*(void) const
    {
        return m_surface;
    }
    operator bool(void) const;

private:
    const eglw::Library &m_egl;
    eglw::EGLDisplay m_display;
    eglw::EGLSurface m_surface;

    // Disabled
    UniqueSurface &operator=(const UniqueSurface &);
    UniqueSurface(const UniqueSurface &);
};

class UniqueContext
{
public:
    UniqueContext(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLContext context);
    ~UniqueContext(void);

    eglw::EGLContext operator*(void) const
    {
        return m_context;
    }
    operator bool(void) const;

private:
    const eglw::Library &m_egl;
    eglw::EGLDisplay m_display;
    eglw::EGLContext m_context;

    // Disabled
    UniqueContext operator=(const UniqueContext &);
    UniqueContext(const UniqueContext &);
};

class ScopedCurrentContext
{
public:
    ScopedCurrentContext(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLSurface draw,
                         eglw::EGLSurface read, eglw::EGLContext context);
    ~ScopedCurrentContext(void);

private:
    const eglw::Library &m_egl;
    eglw::EGLDisplay m_display;
};

class UniqueImage
{
public:
    UniqueImage(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLImage image);
    ~UniqueImage(void);

    eglw::EGLImage operator*(void) const
    {
        return m_image;
    }
    operator bool(void) const;

private:
    const eglw::Library &m_egl;
    eglw::EGLDisplay m_display;
    eglw::EGLImage m_image;

    // Disabled
    UniqueImage operator=(const UniqueImage &);
    UniqueImage(const UniqueImage &);
};

} // namespace eglu

#endif // _EGLUUNIQUE_HPP
