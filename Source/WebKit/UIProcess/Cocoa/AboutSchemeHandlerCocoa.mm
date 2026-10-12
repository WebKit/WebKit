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

#import "config.h"
#import "AboutSchemeHandler.h"

#import "WebPrivacyHelpers.h"
#import <WebCore/ResourceResponse.h>
#import <WebCore/SharedBuffer.h>

namespace WebKit {

#if ENABLE(HEAVY_AD_UNLOADING)
class AdTaggingListHandler final : public AboutSchemeHandler::OpaquePathHandler {
public:
    static constexpr auto path = "ad-tagging-list"_s;

    void loadContent(URL requestURL, CompletionHandler<void(WebCore::ResourceResponse&&, Ref<WebCore::SharedBuffer>&&)>&& handler) final
    {
        ResourceMonitorURLsController::singleton().getSource([requestURL = WTF::move(requestURL), handler = WTF::move(handler)](String&& source) mutable {
            auto data = WebCore::SharedBuffer::create(source.span8());

            WebCore::ResourceResponse response(WTF::move(requestURL), "text/plain"_s, data->size(), "UTF-8"_s);
            handler(WTF::move(response), WTF::move(data));
        });
    }
};
#endif

void AboutSchemeHandler::platformInitialize()
{
#if ENABLE(HEAVY_AD_UNLOADING)
    registerHandler(AdTaggingListHandler::path, makeUnique<AdTaggingListHandler>());
#endif
}

} // namespace WebKit
