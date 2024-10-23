/*
 * Copyright (C) 2023,2024 Igalia S.L.
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

#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))

#include <WebCore/ScreenProperties.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>

#if PLATFORM(GTK)
typedef struct _GdkMonitor GdkMonitor;
using PlatformScreen = GdkMonitor;
#elif PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
typedef struct _WPEScreen WPEScreen;
using PlatformScreen = WPEScreen;
#endif

namespace WebKit {

using PlatformDisplayID = uint32_t;

class ScreenManager {
    WTF_MAKE_NONCOPYABLE(ScreenManager);
    friend NeverDestroyed<ScreenManager>;
public:
    static ScreenManager& singleton();

    PlatformDisplayID displayID(PlatformScreen*) const;
    PlatformScreen* screen(PlatformDisplayID) const;
    PlatformDisplayID primaryDisplayID() const { return m_primaryDisplayID; }

    WebCore::ScreenProperties collectScreenProperties() const;

private:
    ScreenManager();

    static PlatformDisplayID generatePlatformDisplayID(PlatformScreen*);

    void addScreen(PlatformScreen*);
    void removeScreen(PlatformScreen*);
    void updatePrimaryDisplayID();
    void propertiesDidChange() const;

    Vector<GRefPtr<PlatformScreen>, 1> m_screens;
    HashMap<PlatformScreen*, PlatformDisplayID> m_screenToDisplayIDMap;
    PlatformDisplayID m_primaryDisplayID { 0 };
};

} // namespace WebKit

#endif // PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
