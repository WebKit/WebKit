/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <gtk/gtk.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

typedef struct _WebKitWebViewBase WebKitWebViewBase;

namespace WebKit {

class ToplevelWindow {
    WTF_MAKE_NONCOPYABLE(ToplevelWindow); WTF_MAKE_FAST_ALLOCATED;
public:
    static ToplevelWindow* forGtkWindow(GtkWindow*);
    explicit ToplevelWindow(GtkWindow*);
    ~ToplevelWindow();

    void addWebView(WebKitWebViewBase*);
    void removeWebView(WebKitWebViewBase*);

    GtkWindow* window() const { return m_window; }
    bool isActive() const;
    bool isFullscreen() const;
    bool isMinimized() const;
    GdkMonitor* monitor() const;
    bool isInMonitor() const;

private:
    void connectSignals();
    void disconnectSignals();
#if USE(GTK4)
    void connectSurfaceSignals();
    void disconnectSurfaceSignals();
#endif

    void notifyIsActive(bool);
    void notifyState(uint32_t, uint32_t);
    void notifyMonitorChanged(GdkMonitor*);

    GtkWindow* m_window { nullptr };
    HashSet<WebKitWebViewBase*> m_webViews;
#if USE(GTK4)
    GdkToplevelState m_state { static_cast<GdkToplevelState>(0) };
    HashSet<GdkMonitor*> m_monitors;
#endif
};

} // namespace WebKit
