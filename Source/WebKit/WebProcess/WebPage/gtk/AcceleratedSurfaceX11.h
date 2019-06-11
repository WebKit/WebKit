/*
 * Copyright (C) 2012-2016 Igalia S.L.
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

#pragma once

#if PLATFORM(X11)

#include "AcceleratedSurface.h"
#include <WebCore/XUniqueResource.h>

typedef struct _XDisplay Display;
typedef unsigned long Pixmap;
typedef unsigned long Window;

namespace WebKit {

class WebPage;

class AcceleratedSurfaceX11 final : public AcceleratedSurface {
    WTF_MAKE_NONCOPYABLE(AcceleratedSurfaceX11); WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<AcceleratedSurfaceX11> create(WebPage&, Client&);
    ~AcceleratedSurfaceX11();

    uint64_t window() const override { return m_window.get(); }
    uint64_t surfaceID() const override { return m_pixmap.get(); }
    bool hostResize(const WebCore::IntSize&) override;
    bool shouldPaintMirrored() const override { return false; }

    void didRenderFrame() override;

private:
    AcceleratedSurfaceX11(WebPage&, Client&);

    void createPixmap();

    Display* m_display { nullptr };
    WebCore::XUniqueWindow m_window;
    WebCore::XUniqueWindow m_parentWindow;
    WebCore::XUniquePixmap m_pixmap;
};

} // namespace WebKit

#endif // PLATFORM(X11)
