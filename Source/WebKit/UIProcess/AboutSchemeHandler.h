/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "WebURLSchemeHandler.h"
#include <wtf/HashMap.h>

namespace WebKit {

class AboutSchemeHandler final : public WebURLSchemeHandler {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class OpaquePathHandler {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual ~OpaquePathHandler() = default;
        virtual void loadContent(URL, CompletionHandler<void(WebCore::ResourceResponse&&, Ref<WebCore::SharedBuffer>&&)>&&) = 0;
    };

    static AboutSchemeHandler& singleton();

    void registerHandler(const String& opaquePath, std::unique_ptr<OpaquePathHandler>&&);
    bool canHandleURL(const URL&) const;

    static constexpr auto scheme = "about"_s;
    static constexpr auto blank = "blank"_s;

private:
    friend MainThreadNeverDestroyed<AboutSchemeHandler>;

    class EmptyPathHandler final : public OpaquePathHandler {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        void loadContent(URL url, CompletionHandler<void(WebCore::ResourceResponse&&, Ref<WebCore::SharedBuffer>&&)>&& handler)
        {
            WebCore::ResourceResponse response(url, "text/html"_s, 0, "UTF-8"_s);
            handler(WTFMove(response), WebCore::SharedBuffer::create());
        }
    };

    AboutSchemeHandler();

    void platformInitialize();

    void platformStartTask(WebPageProxy&, WebURLSchemeTask&) final;
    void platformStopTask(WebPageProxy&, WebURLSchemeTask&) final { }

    OpaquePathHandler* handlerForURL(URL&) const;

    HashMap<String, std::unique_ptr<OpaquePathHandler>> m_handlers;
};

} // namespace WebKit
