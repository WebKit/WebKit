/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_SESSION)

#include "APIObject.h"
#include "WebContextSupplement.h"
#include "WebMediaSessionFocusManagerClient.h"
#include "WebPageProxy.h"

namespace WebKit {

typedef std::pair<WebPageProxy*, uint64_t> FocusedMediaElement;

class WebMediaSessionFocusManager : public API::ObjectImpl<API::Object::Type::MediaSessionFocusManager>, public WebContextSupplement {
public:
    static const char* supplementName();

    static Ref<WebMediaSessionFocusManager> create(WebProcessPool*);

    void initializeClient(const WKMediaSessionFocusManagerClientBase*);

    bool valueForPlaybackAttribute(WKMediaSessionFocusManagerPlaybackAttribute) const;
    void updatePlaybackAttributesFromMediaState(WebPageProxy*, uint64_t, WebCore::MediaProducer::MediaStateFlags);
    void setVolumeOfFocusedMediaElement(double);

    void setFocusedMediaElement(WebPageProxy&, uint64_t);
    void clearFocusedMediaElement();

    using API::Object::ref;
    using API::Object::deref;

private:
    explicit WebMediaSessionFocusManager(WebProcessPool*);

    // WebContextSupplement
    void refWebContextSupplement() override;
    void derefWebContextSupplement() override;

    void updatePlaybackAttribute(WKMediaSessionFocusManagerPlaybackAttribute, bool);

    std::unique_ptr<FocusedMediaElement> m_focusedMediaElement;
    WKMediaSessionFocusManagerPlaybackAttributes m_playbackAttributes { 0 };
    WebMediaSessionFocusManagerClient m_client;
};

} // namespace WebKit

#endif // ENABLE(MEDIA_SESSION)
