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

#ifndef XUniquePtr_h
#define XUniquePtr_h

#if PLATFORM(X11)
#include "PlatformDisplayX11.h"
#include <X11/Xutil.h>

#if USE(GLX)
typedef struct __GLXcontextRec* GLXContext;
extern "C" void glXDestroyContext(::Display*, GLXContext);
#endif

namespace WebCore {

template<typename T>
struct XPtrDeleter {
    void operator()(T* ptr) const
    {
        XFree(ptr);
    }
};

template<typename T>
using XUniquePtr = std::unique_ptr<T, XPtrDeleter<T>>;

template<> struct XPtrDeleter<XImage> {
    void operator() (XImage* ptr) const
    {
        XDestroyImage(ptr);
    }
};

template<> struct XPtrDeleter<_XGC> {
    void operator() (_XGC* ptr) const
    {
        XFreeGC(downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native(), ptr);
    }
};
// Give a name to this to avoid having to use the internal struct name.
using XUniqueGC = XUniquePtr<_XGC>;

#if USE(GLX)
template<> struct XPtrDeleter<__GLXcontextRec> {
    void operator() (__GLXcontextRec* ptr)
    {
        glXDestroyContext(downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native(), ptr);
    }
};
// Give a name to this to avoid having to use the internal struct name.
using XUniqueGLXContext = XUniquePtr<__GLXcontextRec>;
#endif

} // namespace WebCore

#endif // PLATFORM(X11)

#endif // XUniquePtr_h
