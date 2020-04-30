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
#include <wtf/Optional.h>

typedef struct _XDisplay Display;

// It's not possible to forward declare Visual, and including xlib in headers is problematic,
// so we use void* for Visual and provide this macro to get the visual easily.
#define WK_XVISUAL(platformDisplay) (static_cast<Visual*>(platformDisplay.visual()))

namespace WebCore {

class PlatformDisplayX11 final : public PlatformDisplay {
public:
    static std::unique_ptr<PlatformDisplay> create();
    static std::unique_ptr<PlatformDisplay> create(Display*);

    virtual ~PlatformDisplayX11();

    Display* native() const { return m_display; }
    void* visual() const;
    bool supportsXComposite() const;
    bool supportsXDamage(Optional<int>& damageEventBase, Optional<int>& damageErrorBase) const;

private:
    PlatformDisplayX11(Display*, NativeDisplayOwned);

    Type type() const override { return PlatformDisplay::Type::X11; }

#if USE(EGL)
    void initializeEGLDisplay() override;
#endif

    Display* m_display { nullptr };
    mutable Optional<bool> m_supportsXComposite;
    mutable Optional<bool> m_supportsXDamage;
    mutable Optional<int> m_damageEventBase;
    mutable Optional<int> m_damageErrorBase;
    mutable void* m_visual { nullptr };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_PLATFORM_DISPLAY(PlatformDisplayX11, X11)

#endif // PLATFORM(X11)
