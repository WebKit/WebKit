/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WKCACFViewWindow_h
#define WKCACFViewWindow_h

#include "HeaderDetection.h"

#if HAVE(WKQCA)

#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

typedef struct _WKCACFView* WKCACFViewRef;

namespace WebKit {

// FIXME: Move this class down to WebCore. (Maybe it can even replace some of WKCACFViewLayerTreeHost.)
class WKCACFViewWindow {
    WTF_MAKE_NONCOPYABLE(WKCACFViewWindow);
public:
    // WKCACFViewWindow will destroy its HWND when this message is received.
    static const UINT customDestroyMessage = WM_USER + 1;

    WKCACFViewWindow(WKCACFViewRef, HWND parentWindow, DWORD additionalStyles);
    ~WKCACFViewWindow();

    void setDeletesSelfWhenWindowDestroyed(bool deletes) { m_deletesSelfWhenWindowDestroyed = deletes; }

    HWND window() const { return m_window; }

private:
    LRESULT onCustomDestroy(WPARAM, LPARAM);
    LRESULT onDestroy(WPARAM, LPARAM);
    LRESULT onEraseBackground(WPARAM, LPARAM);
    LRESULT onNCDestroy(WPARAM, LPARAM);
    LRESULT onPaint(WPARAM, LPARAM);
    LRESULT onPrintClient(WPARAM, LPARAM);
    static void registerClass();
    static LRESULT CALLBACK staticWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(UINT, WPARAM, LPARAM);

    HWND m_window;
    RetainPtr<WKCACFViewRef> m_view;
    bool m_deletesSelfWhenWindowDestroyed;
};

} // namespace WebKit

#endif // HAVE(WKQCA)

#endif // WKCACFViewWindow_h
