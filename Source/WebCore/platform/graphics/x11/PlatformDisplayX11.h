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

#pragma once

#if PLATFORM(X11)

#include "PlatformDisplay.h"
#include <optional>

typedef struct _XDisplay Display;

namespace WebCore {

class PlatformDisplayX11 final : public PlatformDisplay {
public:
    static std::unique_ptr<PlatformDisplay> create();
#if PLATFORM(GTK)
    static std::unique_ptr<PlatformDisplay> create(GdkDisplay*);
#endif

    virtual ~PlatformDisplayX11();

    ::Display* native() const { return m_display; }

private:
    explicit PlatformDisplayX11(::Display*);
#if PLATFORM(GTK)
    explicit PlatformDisplayX11(GdkDisplay*);

    void sharedDisplayDidClose() override;
#endif

    Type type() const override { return PlatformDisplay::Type::X11; }

#if USE(EGL)
#if PLATFORM(GTK)
    EGLDisplay gtkEGLDisplay() override;
#endif
    void initializeEGLDisplay() override;
#endif

#if USE(LCMS)
    cmsHPROFILE colorProfile() const override;
#endif

#if USE(ATSPI)
    String platformAccessibilityBusAddress() const override;
#endif

    ::Display* m_display { nullptr };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_PLATFORM_DISPLAY(PlatformDisplayX11, X11)

#endif // PLATFORM(X11)
