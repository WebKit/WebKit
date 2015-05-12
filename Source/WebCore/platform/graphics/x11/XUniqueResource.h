/*
 * Copyright (C) 2015 Igalia S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XUniqueResource_h
#define XUniqueResource_h

#if PLATFORM(X11)

#if USE(GLX)
typedef unsigned long GLXPbuffer;
typedef unsigned long GLXPixmap;
#endif

namespace WebCore {

enum class XResource {
    Colormap,
#if PLATFORM(GTK)
    Damage,
#endif
    Pixmap,
    Window,
#if USE(GLX)
    GLXPbuffer,
    GLXPixmap,
#endif
};

template <XResource T> class XUniqueResource {
public:
    XUniqueResource()
    {
    }

    XUniqueResource(unsigned long resource)
        : m_resource(resource)
    {
    }

    XUniqueResource(XUniqueResource&& uniqueResource)
        : m_resource(uniqueResource.release())
    {
    }

    XUniqueResource& operator=(XUniqueResource&& uniqueResource)
    {
        reset(uniqueResource.release());
        return *this;
    }

    ~XUniqueResource()
    {
        reset();
    }

    unsigned long get() const { return m_resource; }
    unsigned long release() { return std::exchange(m_resource, 0); }

    void reset(unsigned long resource = 0)
    {
        std::swap(m_resource, resource);
        deleteXResource(resource);
    }

    explicit operator bool() const { return m_resource; }

private:
    static void deleteXResource(unsigned long resource);

    unsigned long m_resource { 0 };
};

using XUniqueColormap = XUniqueResource<XResource::Colormap>;
#if PLATFORM(GTK)
using XUniqueDamage = XUniqueResource<XResource::Damage>;
#endif
using XUniquePixmap = XUniqueResource<XResource::Pixmap>;
using XUniqueWindow = XUniqueResource<XResource::Window>;
#if USE(GLX)
using XUniqueGLXPbuffer = XUniqueResource<XResource::GLXPbuffer>;
using XUniqueGLXPixmap = XUniqueResource<XResource::GLXPixmap>;
#endif

} // namespace WebCore

#endif // PLATFORM(X11)

#endif // XUniqueResource_h
