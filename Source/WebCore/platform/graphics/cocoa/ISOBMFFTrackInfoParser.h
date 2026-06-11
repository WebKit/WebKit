/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include <span>
#include <wtf/MediaTime.h>
#include <wtf/Vector.h>

namespace WebCore {

// Lightweight ISO-BMFF moov-content walker. Reads enough of the moov box
// to identify per-track empty edits (elst entry 0 with media_time = -1)
// and report each track's empty-edit duration as a MediaTime in the
// movie timescale.
class ISOBMFFTrackInfoParser {
public:
    struct TrackEditInfo {
        uint64_t trackID { 0 };
        MediaTime emptyEditOffset; // invalid MediaTime if no empty edit on this track
    };

    // Walks a complete moov box body (the bytes after its 8/16-byte box header).
    // Returns one TrackEditInfo per trak whose elst entry 0 is an empty edit.
    // On any malformation, returns whatever was parsed up to the failure point.
    static Vector<TrackEditInfo> parseMoovBody(std::span<const uint8_t>);
};

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
