/**
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "APIUIClient.h"

#include "UserMediaPermissionCheckProxy.h"
#include "UserMediaPermissionRequestProxy.h"
#include "WebPageProxy.h"

namespace API {

void UIClient::checkUserMediaPermissionForOrigin(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, SecurityOrigin&, SecurityOrigin&, WebKit::UserMediaPermissionCheckProxy& request)
{
    request.deny();
}

void UIClient::decidePolicyForUserMediaPermissionRequest(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, SecurityOrigin&, SecurityOrigin&, WebKit::UserMediaPermissionRequestProxy& request)
{
    request.doDefaultAction();
}

void UIClient::createNewPage(WebKit::WebPageProxy&, WebCore::WindowFeatures&&, Ref<NavigationAction>&&, CompletionHandler<void(RefPtr<WebKit::WebPageProxy>&&)>&& completionHandler)
{
    completionHandler(nullptr);
}

void UIClient::decidePolicyForMediaKeySystemPermissionRequest(WebKit::WebPageProxy& page, SecurityOrigin& origin, const WTF::String& keySystem, CompletionHandler<void(bool)>&& completionHandler)
{
    page.requestMediaKeySystemPermissionByDefaultAction(origin.securityOrigin(), WTFMove(completionHandler));
}

} // namespace API
