/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(VIDEO_TRACK)

#include "LoadableTextTrack.h"

namespace WebCore {

LoadableTextTrack::LoadableTextTrack(TextTrackClient* trackClient, TextTrackLoadingClient* loadingClient, const String& kind, const String& label, const String& language, bool isDefault)
    : TextTrack(trackClient, kind, label, language)
    , m_loadingClient(loadingClient)
    , m_isDefault(isDefault)
{
}

LoadableTextTrack::~LoadableTextTrack()
{
}

void LoadableTextTrack::load(const KURL& url, ScriptExecutionContext* context)
{
    if (!m_loader)
        m_loader = TextTrackLoader::create(this, context);
    m_loader->load(url);
}

void LoadableTextTrack::newCuesAvailable(TextTrackLoader* loader)
{
    ASSERT_UNUSED(loader, m_loader == loader);

    // FIXME(62885): Implement.
}

void LoadableTextTrack::cueLoadingStarted(TextTrackLoader* loader)
{
    ASSERT_UNUSED(loader, m_loader == loader);
    
    setReadyState(TextTrack::LOADING);
}

void LoadableTextTrack::cueLoadingCompleted(TextTrackLoader* loader, bool loadingFailed)
{
    ASSERT_UNUSED(loader, m_loader == loader);

    loadingFailed ? setReadyState(TextTrack::HTML_ERROR) : setReadyState(TextTrack::LOADED);

    if (m_loadingClient)
        m_loadingClient->textTrackLoadingCompleted(this, loadingFailed);
}

} // namespace WebCore

#endif
