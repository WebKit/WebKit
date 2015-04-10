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

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)

#import <WebCore/MediaPlaybackTargetPickerMac.h>
#import <WebCore/Page.h>

std::unique_ptr<WebMediaPlaybackTargetPicker> WebMediaPlaybackTargetPicker::create(WebCore::Page& page)
{
    return std::make_unique<WebMediaPlaybackTargetPicker>(page);
}

WebMediaPlaybackTargetPicker::WebMediaPlaybackTargetPicker(WebCore::Page& page)
    : m_page(&page)
{
}

void WebMediaPlaybackTargetPicker::showPlaybackTargetPicker(const WebCore::FloatRect& rect, bool hasVideo)
{
    targetPicker().showPlaybackTargetPicker(WebCore::IntRect(rect), hasVideo);
}

void WebMediaPlaybackTargetPicker::startingMonitoringPlaybackTargets()
{
    targetPicker().startingMonitoringPlaybackTargets();
}

void WebMediaPlaybackTargetPicker::stopMonitoringPlaybackTargets()
{
    targetPicker().stopMonitoringPlaybackTargets();
}

void WebMediaPlaybackTargetPicker::didChoosePlaybackTarget(Ref<WebCore::MediaPlaybackTarget>&& target)
{
    if (!m_page)
        return;

    m_page->didChoosePlaybackTarget(WTF::move(target));
}

void WebMediaPlaybackTargetPicker::externalOutputDeviceAvailableDidChange(bool available)
{
    if (!m_page)
        return;

    m_page->playbackTargetAvailabilityDidChange(available);
}

void WebMediaPlaybackTargetPicker::invalidate()
{
    m_page = nullptr;
}

WebCore::MediaPlaybackTargetPicker& WebMediaPlaybackTargetPicker::targetPicker()
{
    if (!m_targetPicker)
        m_targetPicker = WebCore::MediaPlaybackTargetPickerMac::create(*this);

    return *m_targetPicker.get();
}

#endif
