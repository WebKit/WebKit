/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#if HAVE(WINDOW_CAPTURE)

#include <wtf/CompletionHandler.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakObjCPtr.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS NSObject;
OBJC_CLASS NSString;
OBJC_CLASS NSWindow;
OBJC_CLASS WKTabCapturePickerOverlay;

namespace WebCore {
class SecurityOriginData;
}

namespace WebKit {

class WebPageProxy;

class TabCapturePicker : public RefCountedAndCanMakeWeakPtr<TabCapturePicker> {
    WTF_MAKE_TZONE_ALLOCATED(TabCapturePicker);
public:
    using Callback = CompletionHandler<void(RefPtr<WebPageProxy>&&)>;
    static Ref<TabCapturePicker> create(WebPageProxy& requestingPage, const WebCore::SecurityOriginData& requestingOrigin, Callback&&);
    ~TabCapturePicker();

    void start();
    void cancel();

    bool hasRequestingPage(WebPageProxy& page) const { return m_requestingPage == &page; }

    void scheduleOverlaysUpdate();

private:
    TabCapturePicker(WebPageProxy&, const WebCore::SecurityOriginData&, Callback&&);

    void updateOverlays();
    void installOverlay(WebPageProxy&);
    void selectWindow(NSWindow *);
    void updateTints();
    void finish(RefPtr<WebPageProxy>&&);
    void stop();

    struct Overlay {
        RetainPtr<WKTabCapturePickerOverlay> view;
        WeakPtr<WebPageProxy> page;
    };

    WeakPtr<WebPageProxy> m_requestingPage;
    RetainPtr<NSString> m_requestingOriginName;
    Callback m_callback;

    Vector<Overlay> m_overlays;
    RetainPtr<NSObject> m_windowWillCloseObserver;
    WeakObjCPtr<NSWindow> m_selectedWindow;
    bool m_isOverlaysUpdateScheduled { false };
};

} // namespace WebKit

#endif // HAVE(WINDOW_CAPTURE)
