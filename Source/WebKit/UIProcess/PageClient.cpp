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

#include "config.h"
#include "PageClient.h"

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
#import <WebCore/WebMediaSessionManager.h>
#endif

namespace WebKit {

#if ENABLE(FULLSCREEN_API)
CheckedRef<WebFullScreenManagerProxyClient> PageClient::checkedFullScreenManagerProxyClient()
{
    return fullScreenManagerProxyClient();
}
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
CheckedRef<WebCore::WebMediaSessionManager> PageClient::checkedMediaSessionManager()
{
    return mediaSessionManager();
}
#endif

#if ENABLE(VIDEO)
void PageClient::showCaptionDisplaySettings(WebCore::HTMLMediaElementIdentifier, const WebCore::ResolvedCaptionDisplaySettingsOptions&, CompletionHandler<void(Expected<void, WebCore::ExceptionData>&&)>&& completionHandler)
{
    completionHandler(makeUnexpected<WebCore::ExceptionData>({ WebCore::ExceptionCode::NotSupportedError, "Caption Display Settings are not supported."_s }));
}
#endif

} // namespace WebKit
