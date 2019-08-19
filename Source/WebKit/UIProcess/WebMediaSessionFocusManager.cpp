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

#include "config.h"
#include "WebMediaSessionFocusManager.h"

#if ENABLE(MEDIA_SESSION)

#include "WebProcessPool.h"

namespace WebKit {

const char* WebMediaSessionFocusManager::supplementName()
{
    return "WebMediaSessionFocusManager";
}

Ref<WebMediaSessionFocusManager> WebMediaSessionFocusManager::create(WebProcessPool* processPool)
{
    return adoptRef(*new WebMediaSessionFocusManager(processPool));
}

WebMediaSessionFocusManager::WebMediaSessionFocusManager(WebProcessPool* processPool)
    : WebContextSupplement(processPool) { }

// WebContextSupplement

void WebMediaSessionFocusManager::refWebContextSupplement()
{
    API::Object::ref();
}

void WebMediaSessionFocusManager::derefWebContextSupplement()
{
    API::Object::deref();
}

void WebMediaSessionFocusManager::initializeClient(const WKMediaSessionFocusManagerClientBase* client)
{
    m_client.initialize(client);
}

bool WebMediaSessionFocusManager::valueForPlaybackAttribute(WKMediaSessionFocusManagerPlaybackAttribute attribute) const
{
    if (!m_focusedMediaElement)
        return false;

    return m_playbackAttributes & attribute;
}

void WebMediaSessionFocusManager::updatePlaybackAttribute(WKMediaSessionFocusManagerPlaybackAttribute attribute, bool value)
{
    if (value)
        m_playbackAttributes |= attribute;
    else
        m_playbackAttributes &= ~attribute;

    m_client.didChangePlaybackAttribute(this, attribute, value);
}

void WebMediaSessionFocusManager::setVolumeOfFocusedMediaElement(double volume)
{
    if (!m_focusedMediaElement)
        return;

    if (WebPageProxy* proxy = m_focusedMediaElement->first)
        proxy->setVolumeOfMediaElement(volume, m_focusedMediaElement->second);
}

void WebMediaSessionFocusManager::updatePlaybackAttributesFromMediaState(WebPageProxy* proxy, uint64_t elementID, WebCore::MediaProducer::MediaStateFlags flags)
{
    if (m_focusedMediaElement) {
        if (proxy == m_focusedMediaElement->first && elementID == m_focusedMediaElement->second) {
            updatePlaybackAttribute(IsPlaying, flags & WebCore::MediaProducer::IsSourceElementPlaying);
            updatePlaybackAttribute(IsNextTrackControlEnabled, flags & WebCore::MediaProducer::IsNextTrackControlEnabled);
            updatePlaybackAttribute(IsPreviousTrackControlEnabled, flags & WebCore::MediaProducer::IsPreviousTrackControlEnabled);
        }
    }
}

void WebMediaSessionFocusManager::setFocusedMediaElement(WebPageProxy& proxy, uint64_t elementID)
{
    m_focusedMediaElement = makeUnique<FocusedMediaElement>(&proxy, elementID);
}

void WebMediaSessionFocusManager::clearFocusedMediaElement()
{
    m_focusedMediaElement = nullptr;
}

} // namespace WebKit

#endif // ENABLE(MEDIA_SESSION)
