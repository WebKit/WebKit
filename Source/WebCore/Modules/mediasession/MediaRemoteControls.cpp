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

#include "config.h"
#include "MediaRemoteControls.h"

#if ENABLE(MEDIA_SESSION)

#include "MediaSession.h"

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaRemoteControls);

MediaRemoteControls::MediaRemoteControls(ScriptExecutionContext& context, MediaSession* session)
    : m_scriptExecutionContext(context)
    , m_session(session)
{
}

void MediaRemoteControls::clearSession()
{
    m_session = nullptr;
}

MediaRemoteControls::~MediaRemoteControls() = default;

void MediaRemoteControls::setPreviousTrackEnabled(bool isEnabled)
{
    if (m_previousTrackEnabled == isEnabled)
        return;

    m_previousTrackEnabled = isEnabled;

    if (m_session)
        m_session->controlIsEnabledDidChange();
}

void MediaRemoteControls::setNextTrackEnabled(bool isEnabled)
{
    if (m_nextTrackEnabled == isEnabled)
        return;

    m_nextTrackEnabled = isEnabled;

    if (m_session)
        m_session->controlIsEnabledDidChange();
}

}

#endif /* ENABLE(MEDIA_SESSION) */
