/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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

#ifndef JSTextTrackCustom_h
#define JSTextTrackCustom_h

#if ENABLE(VIDEO_TRACK)
#include "JSTextTrack.h"

#include "HTMLMediaElement.h"
#include "HTMLTrackElement.h"
#include "JSNodeCustom.h"
#include "LoadableTextTrack.h"

using namespace JSC;

namespace WebCore {

inline void* root(TextTrack* track)
{
    // If this track corresponds to a <track> element, return that element's root.
    if (track->trackType() == TextTrack::TrackElement) {
        if (HTMLTrackElement* trackElement = static_cast<LoadableTextTrack*>(track)->trackElement())
            return root(trackElement);
    }

    // No, return the media element's root if it has one.
    if (track->mediaElement())
        return root(track->mediaElement());

    // No track element and no media element, return the text track.
    return track;
}

}

#endif
#endif
