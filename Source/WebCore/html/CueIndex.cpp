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

#include "CueIndex.h"

#include "TextTrackCue.h"
#include "TextTrackCueList.h"

namespace WebCore {

CueSet CueSet::difference(const CueSet&) const
{
    // FIXME(62883): Implement.
    return CueSet();
}

CueSet CueSet::unionSet(const CueSet&) const
{
    // FIXME(62883): Implement.
    return CueSet();
}

void CueSet::add(const TextTrackCue&)
{
    // FIXME(62883): Implement.
}

bool CueSet::contains(const TextTrackCue&) const
{
    // FIXME(62883): Implement.
    return false;
}

void CueSet::remove(const TextTrackCue&)
{
    // FIXME(62883): Implement.
}

bool CueSet::isEmpty() const
{
    // FIXME(62883): Implement.
    return false;
}

int CueSet::size() const
{
    // FIXME(62883): Implement.
    return 0;
}

void CueIndex::fetchNewCuesFromLoader(CueLoader*)
{
    // FIXME(62883): Implement.
}

void CueIndex::removeCuesFromIndex(const TextTrackCueList*)
{
    // FIXME(62883): Implement.
}

CueSet CueIndex::visibleCuesAtTime(double) const
{
    // FIXME(62855): Implement.
    return CueSet();
}

void CueIndex::add(TextTrackCue*)
{
    // FIXME(62890): Implement.
}

void CueIndex::remove(TextTrackCue*)
{
    // FIXME(62890): Implement.
}

} // namespace WebCore

#endif
