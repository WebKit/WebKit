/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebVideoFullscreenInterfaceMac.h"

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#import "IntRect.h"
#import "WebVideoFullscreenChangeObserver.h"
#import "WebVideoFullscreenModel.h"

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebVideoFullscreenInterfaceMacAdditions.mm>
#endif

namespace WebCore {

WebVideoFullscreenInterfaceMac::~WebVideoFullscreenInterfaceMac()
{
}

void WebVideoFullscreenInterfaceMac::setWebVideoFullscreenModel(WebVideoFullscreenModel* model)
{
    m_videoFullscreenModel = model;
}

void WebVideoFullscreenInterfaceMac::setWebVideoFullscreenChangeObserver(WebVideoFullscreenChangeObserver* observer)
{
    m_fullscreenChangeObserver = observer;
}

void WebVideoFullscreenInterfaceMac::setMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode | mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}

void WebVideoFullscreenInterfaceMac::clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode & ~mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}

#if !USE(APPLE_INTERNAL_SDK)
void WebVideoFullscreenInterfaceMac::setupFullscreen(NSView&, const IntRect&, NSWindow *, HTMLMediaElementEnums::VideoFullscreenMode, bool)
{
}

void WebVideoFullscreenInterfaceMac::enterFullscreen()
{
}

void WebVideoFullscreenInterfaceMac::exitFullscreen(const IntRect&, NSWindow *)
{
}

void WebVideoFullscreenInterfaceMac::cleanupFullscreen()
{
}

void WebVideoFullscreenInterfaceMac::invalidate()
{
}

void WebVideoFullscreenInterfaceMac::preparedToReturnToInline(bool, const IntRect&, NSWindow *)
{
}

void WebVideoFullscreenInterfaceMac::setExternalPlayback(bool, ExternalPlaybackTargetType, WTF::String)
{
}

bool supportsPictureInPicture()
{
    return false;
}
#endif

}

#endif
