/*
 * Copyright (C) 2012 Igalia S.L.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef  RedirectedXCompositeWindow_h
#define  RedirectedXCompositeWindow_h

#if USE(REDIRECTED_XCOMPOSITE_WINDOW)

#include <WebCore/IntSize.h>
#include <WebCore/RefPtrCairo.h>
#include <WebCore/XUniqueResource.h>
#include <functional>

typedef struct _GdkWindow GdkWindow;
typedef struct _XDisplay Display;
typedef unsigned long Window;

namespace WebKit {

class RedirectedXCompositeWindow {
public:
    static std::unique_ptr<RedirectedXCompositeWindow> create(GdkWindow*, std::function<void()> damageNotify);
    ~RedirectedXCompositeWindow();

    Window windowID() const { return m_window.get(); }
    void resize(const WebCore::IntSize&);
    cairo_surface_t* surface();

private:
    RedirectedXCompositeWindow(GdkWindow*, std::function<void()> damageNotify);
    void cleanupPixmapAndPixmapSurface();

    Display* m_display;
    WebCore::IntSize m_size;
    WebCore::XUniqueWindow m_window;
    WebCore::XUniqueWindow m_parentWindow;
    WebCore::XUniquePixmap m_pixmap;
    WebCore::XUniqueDamage m_damage;
    RefPtr<cairo_surface_t> m_surface;
    bool m_needsNewPixmapAfterResize;
};

} // namespace WebKit

#endif // USE(REDIRECTED_XCOMPOSITE_WINDOW)

#endif // RedirectedXCompositeWindow_h
