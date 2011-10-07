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

LoadableTextTrack::LoadableTextTrack(const String& kind, const String& label, const String& language, bool isDefault)
    : TextTrack(kind, label, language)
    , m_isDefault(isDefault)
{
}

LoadableTextTrack::~LoadableTextTrack()
{
}

void LoadableTextTrack::load(const String& url, ScriptExecutionContext* context)
{
    return m_parser.load(url, context, this);
}

bool LoadableTextTrack::supportsType(const String& url)
{
    return m_parser.supportsType(url);
}

void LoadableTextTrack::newCuesParsed()
{
    // FIXME(62883): Fetch new cues from parser and temporarily store to give to CueLoaderClient when fetchNewCuesFromLoader is called.
}

void LoadableTextTrack::trackLoadStarted()
{
    setReadyState(TextTrack::Loading);
}

void LoadableTextTrack::trackLoadError()
{
    setReadyState(TextTrack::Error);
}

void LoadableTextTrack::trackLoadCompleted()
{
    setReadyState(TextTrack::Loaded);
}

void LoadableTextTrack::newCuesLoaded()
{
    // FIXME(62885): Tell the client to fetch the latest cues.
}

void LoadableTextTrack::fetchNewestCues(Vector<TextTrackCue*>&)
{
    // FIXME(62885): Implement.
}

} // namespace WebCore

#endif
