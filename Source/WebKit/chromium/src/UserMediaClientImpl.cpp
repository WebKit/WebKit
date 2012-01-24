/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#if ENABLE(MEDIA_STREAM)

#include "UserMediaClientImpl.h"

#include "WebUserMediaClient.h"
#include "WebUserMediaRequest.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "platform/WebMediaStreamSource.h"
#include <wtf/RefPtr.h>

using namespace WebCore;

namespace WebKit {

UserMediaClientImpl::UserMediaClientImpl(WebViewImpl* webView)
    : m_client(webView->client() ? webView->client()->userMediaClient() : 0)
{
}

void UserMediaClientImpl::pageDestroyed()
{
}

void UserMediaClientImpl::requestUserMedia(PassRefPtr<UserMediaRequest> prpRequest, const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources)
{
    if (m_client) {
        RefPtr<UserMediaRequest> request = prpRequest;

        // FIXME: Cleanup when the chromium code has switched to the split sources implementation.
        MediaStreamSourceVector combinedSources;
        combinedSources.append(audioSources);
        combinedSources.append(videoSources);
        m_client->requestUserMedia(PassRefPtr<UserMediaRequest>(request.get()), combinedSources);

        m_client->requestUserMedia(request.release(), audioSources, videoSources);
    }
}

void UserMediaClientImpl::cancelUserMediaRequest(UserMediaRequest* request)
{
    if (m_client)
        m_client->cancelUserMediaRequest(WebUserMediaRequest(request));
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
