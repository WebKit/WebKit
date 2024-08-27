/*
 * Copyright (C) 2024 Igalia S.L.
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

#include <optional>
#include <wtf/Noncopyable.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct _GdkDisplay GdkDisplay;

namespace WebCore {
class GLDisplay;
}

namespace WebKit {

class Display {
    WTF_MAKE_NONCOPYABLE(Display);
    friend class LazyNeverDestroyed<Display>;
public:
    static Display& singleton();
    ~Display();

    WebCore::GLDisplay* glDisplay() const;
    bool glDisplayIsSharedWithGtk() const { return glDisplay() && !m_glDisplayOwned; }

    bool isX11() const;
    bool isWayland() const;

    String accessibilityBusAddress() const;

private:
    Display();
#if PLATFORM(X11)
    bool initializeGLDisplayX11() const;
    String accessibilityBusAddressX11() const;
#endif
#if PLATFORM(WAYLAND)
    bool initializeGLDisplayWayland() const;
#endif

    GRefPtr<GdkDisplay> m_gdkDisplay;
    mutable std::unique_ptr<WebCore::GLDisplay> m_glDisplay;
    mutable bool m_glInitialized { false };
    mutable bool m_glDisplayOwned { false };
};

} // namespace WebKit
