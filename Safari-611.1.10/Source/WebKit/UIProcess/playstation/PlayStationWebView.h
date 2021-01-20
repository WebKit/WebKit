/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#include "APIObject.h"
#include "PageClientImpl.h"
#include "WKView.h"
#include "WebPageProxy.h"

namespace WebKit {

class PlayStationWebView : public API::ObjectImpl<API::Object::Type::View> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<PlayStationWebView> create(const API::PageConfiguration&);
    virtual ~PlayStationWebView();

    WebPageProxy* page() { return m_page.get(); }

    bool isActive() const;
    bool isFocused() const;
    bool isVisible() const;

private:
    PlayStationWebView(const API::PageConfiguration&);

    std::unique_ptr<WebKit::PageClientImpl> m_pageClient;
    RefPtr<WebPageProxy> m_page;

    bool m_active { false };
    bool m_focused { false };
    bool m_visible { false };
#if ENABLE(FULLSCREEN_API)
    bool m_isFullScreen { false };
#endif
};

} // namespace WebKit
