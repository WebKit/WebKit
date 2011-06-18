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

#include "LoadableTextTrackImpl.h"

#include "TextTrackCueList.h"

namespace WebCore {

LoadableTextTrackImpl::LoadableTextTrackImpl(const String& kind, const String& label, const String& language, bool isDefault)
    : TextTrackPrivateInterface(kind, label, language)
    , m_isDefault(isDefault)
{
}

LoadableTextTrackImpl::~LoadableTextTrackImpl()
{
}

PassRefPtr<TextTrackCueList> LoadableTextTrackImpl::cues() const
{
    // FIXME(62885): Implement.
    return 0;
}

PassRefPtr<TextTrackCueList> LoadableTextTrackImpl::activeCues() const
{
    // FIXME(62885): Implement.
    return 0;
}

void LoadableTextTrackImpl::fetchNewestCues(Vector<TextTrackCue*>&)
{
    // FIXME(62885): Implement.
}

void LoadableTextTrackImpl::load(const String&)
{
    // FIXME(62881): Implement.
}

void LoadableTextTrackImpl::newCuesLoaded()
{
    // FIXME(62881): Implement.
}

} // namespace WebCore

#endif
