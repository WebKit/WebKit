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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebMediaPlaybackTargetPicker.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#import <WebCore/MediaPlaybackTarget.h>
#import <WebCore/Page.h>
#import <WebCore/WebMediaSessionManager.h>

std::unique_ptr<WebMediaPlaybackTargetPicker> WebMediaPlaybackTargetPicker::create(WebCore::Page& page)
{
    return makeUnique<WebMediaPlaybackTargetPicker>(page);
}

WebMediaPlaybackTargetPicker::WebMediaPlaybackTargetPicker(WebCore::Page& page)
    : m_page(&page)
{
}

void WebMediaPlaybackTargetPicker::addPlaybackTargetPickerClient(uint64_t contextId)
{
    WebCore::WebMediaSessionManager::shared().addPlaybackTargetPickerClient(*this, contextId);
}

void WebMediaPlaybackTargetPicker::removePlaybackTargetPickerClient(uint64_t contextId)
{
    WebCore::WebMediaSessionManager::shared().removePlaybackTargetPickerClient(*this, contextId);
}

void WebMediaPlaybackTargetPicker::showPlaybackTargetPicker(uint64_t contextId, const WebCore::FloatRect& rect, bool hasVideo)
{
    WebCore::WebMediaSessionManager::shared().showPlaybackTargetPicker(*this, contextId, WebCore::IntRect(rect), hasVideo, m_page ? m_page->useDarkAppearance() : false);
}

void WebMediaPlaybackTargetPicker::playbackTargetPickerClientStateDidChange(uint64_t contextId, WebCore::MediaProducer::MediaStateFlags state)
{
    WebCore::WebMediaSessionManager::shared().clientStateDidChange(*this, contextId, state);
}

void WebMediaPlaybackTargetPicker::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    WebCore::WebMediaSessionManager::shared().setMockMediaPlaybackTargetPickerEnabled(enabled);
}

void WebMediaPlaybackTargetPicker::setMockMediaPlaybackTargetPickerState(const String& name, WebCore::MediaPlaybackTargetContext::State state)
{
    WebCore::WebMediaSessionManager::shared().setMockMediaPlaybackTargetPickerState(name, state);
}

void WebMediaPlaybackTargetPicker::setPlaybackTarget(uint64_t contextId, Ref<WebCore::MediaPlaybackTarget>&& target)
{
    if (!m_page)
        return;

    m_page->setPlaybackTarget(contextId, WTFMove(target));
}

void WebMediaPlaybackTargetPicker::externalOutputDeviceAvailableDidChange(uint64_t contextId, bool available)
{
    if (!m_page)
        return;

    m_page->playbackTargetAvailabilityDidChange(contextId, available);
}

void WebMediaPlaybackTargetPicker::setShouldPlayToPlaybackTarget(uint64_t contextId, bool shouldPlay)
{
    if (!m_page)
        return;

    m_page->setShouldPlayToPlaybackTarget(contextId, shouldPlay);
}

void WebMediaPlaybackTargetPicker::invalidate()
{
    m_page = nullptr;
    WebCore::WebMediaSessionManager::shared().removeAllPlaybackTargetPickerClients(*this);
}

#endif
