/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "WPEWebView.h"
#include <wtf/glib/GRefPtr.h>

struct wpe_input_keyboard_event;

namespace WebKit {
class TouchGestureController;
}

namespace WKWPE {

class ViewLegacy final : public View {
public:
    static Ref<View> create(struct wpe_view_backend* backend, const API::PageConfiguration& configuration)
    {
        return adoptRef(*new ViewLegacy(backend, configuration));
    }
    ~ViewLegacy();

#if ENABLE(FULLSCREEN_API)
    bool setFullScreen(bool);
#endif

#if ENABLE(TOUCH_EVENTS)
    WebKit::TouchGestureController& touchGestureController() const { return *m_touchGestureController; }
#endif

#if ENABLE(GAMEPAD)
    static WebKit::WebPageProxy* platformWebPageProxyForGamepadInput();
#endif

private:
    ViewLegacy(struct wpe_view_backend*, const API::PageConfiguration&);

    struct wpe_view_backend* backend() const override { return m_backend; }
    void synthesizeCompositionKeyPress(const String&, std::optional<Vector<WebCore::CompositionUnderline>>&&, std::optional<WebKit::EditingRange>&&) override;
    void callAfterNextPresentationUpdate(CompletionHandler<void()>&&) override;

    void setViewState(OptionSet<WebCore::ActivityState>);
    void handleKeyboardEvent(struct wpe_input_keyboard_event*);

    struct wpe_view_backend* m_backend { nullptr };
    bool m_horizontalScrollActive { false };
    bool m_verticalScrollActive { false };
#if ENABLE(TOUCH_EVENTS)
    std::unique_ptr<WebKit::TouchGestureController> m_touchGestureController;
#endif
};

} // namespace WKWPE
