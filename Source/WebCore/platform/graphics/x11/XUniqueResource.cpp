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

#include "config.h"
#include "XUniqueResource.h"

#if PLATFORM(X11)
#include "PlatformDisplayX11.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#if PLATFORM(GTK)
#include <X11/extensions/Xdamage.h>
#endif

#if USE(GLX)
#if USE(LIBEPOXY)
#include <epoxy/glx.h>
#else
#include <GL/glx.h>
#endif
#endif

namespace WebCore {

static inline Display* sharedDisplay()
{
    return downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
}

template<> void XUniqueResource<XResource::Colormap>::deleteXResource(unsigned long resource)
{
    if (resource)
        XFreeColormap(sharedDisplay(), resource);
}

#if PLATFORM(GTK)
template<> void XUniqueResource<XResource::Damage>::deleteXResource(unsigned long resource)
{
    if (resource)
        XDamageDestroy(sharedDisplay(), resource);
}
#endif

template<> void XUniqueResource<XResource::Pixmap>::deleteXResource(unsigned long resource)
{
    if (resource)
        XFreePixmap(sharedDisplay(), resource);
}

template<> void XUniqueResource<XResource::Window>::deleteXResource(unsigned long resource)
{
    if (resource)
        XDestroyWindow(sharedDisplay(), resource);
}

#if USE(GLX)
template<> void XUniqueResource<XResource::GLXPbuffer>::deleteXResource(unsigned long resource)
{
    if (resource)
        glXDestroyPbuffer(sharedDisplay(), resource);
}

template<> void XUniqueResource<XResource::GLXPixmap>::deleteXResource(unsigned long resource)
{
    if (resource)
        glXDestroyGLXPixmap(sharedDisplay(), resource);
}
#endif // USE(GLX)

} // namespace WebCore

#endif // PLATFORM(X11)
